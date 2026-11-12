#!/usr/bin/env python3
"""Two-board bench: drives two boards over their USB serial and checks real links.

Each board runs src/bench/bench_main.cpp (one env per role, see platformio.ini,
"TWO-BOARD BENCH").  The two boards are wired crossed on UART2 (GPIO 17 -> 16
both ways).  This script flashes them, then runs the scenarios step by step and
prints one PASS / FAIL line per check, then a summary.

Usage:
    python test/two_boards/bench.py --a COM4 --b COM5 [--no-flash] [A] [B] [C]

    A  convoy: tractor (board a) and trailer (board b), through the TEST coupler
    B  remote link: machine (board a) and remote (board b)
    C  node: main board (board a) and extension board (board b)
    N1 negative: two machines on the remote link must refuse each other
    N2 negative: two tractors hitched together must not couple
    MA MB MC monkey test on A, B or C: random events, safety invariants, recovery (--steps, --seed)

The serial reset lines are used to hold a board in reset (it falls silent).
"""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import sys
import time

import serial

ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD = pathlib.Path(os.environ.get("LOCALAPPDATA", "")) / "Temp" / "PlatformIO" / "build" / "Open_Node_RC_Core"
PIO = [str(pathlib.Path.home() / ".platformio" / "penv" / "Scripts" / "python.exe"), "-m", "platformio"]

SCENARIOS = {
    "A": ("bench_tractor", "bench_trailer"),
    "B": ("bench_machine", "bench_remote"),
    "C": ("bench_node_mainboard", "bench_node_extboard"),
    "N1": ("bench_machine", "bench_machine"),
    "N2": ("bench_tractor", "bench_tractor"),
}

ST_NEVER, ST_ALIVE, ST_LOST = 0, 1, 2
PR_DETACHED, PR_PAIRING, PR_COUPLED, PR_CONFLICT, PR_FAILED = range(5)

RESULTS: list[tuple[str, bool, str]] = []

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


# =============================================================================
# 1. BOARDS
# =============================================================================

def channel_ids(env: str) -> tuple[dict, dict]:
    """Analog and digital channel indexes of an env, read from its generated combus_ids.h."""
    text = (BUILD / env / "combus_generated" / "combus_ids.h").read_text(encoding="utf-8")
    out = []
    for kind in ("Analog", "Digital"):
        m = re.search(rf"enum class {kind}ComBusID\s*:\s*uint8_t\s*\{{(.*?)\}};", text, re.S)
        body = m.group(1) if m else ""
        out.append({n: int(v) for n, v in re.findall(r"(\w+)\s*=\s*(\d+)", body)})
    return out[0], out[1]


def neutral() -> int:
    """CbusNeutral = CbusMaxVal >> 1, with CbusMaxVal = 0xFFFF (combus_res.h)."""
    return 0xFFFF >> 1


class Board:
    """One board of the bench: its console, its channels, its reset line."""

    def __init__(self, port: str, env: str, name: str):
        self.port, self.env, self.name = port, env, name
        self.analog, self.digital = channel_ids(env)
        self.ser = serial.Serial()
        self.ser.port, self.ser.baudrate, self.ser.timeout = port, 115200, 0.05
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.open()
        self.log: list[str] = []
        self.raw = ""                 # everything the board printed, to find crashes
        self.boots = 0                # boots caused by the bench (reset line)
        self.last_up = 0              # uptime of the last status line, to see a restart
        self.up_resets = 0            # restarts seen by the uptime, not caused by the bench
        self.max_lp = 0               # longest loop of the board, in ms
        self.max_cb = 0               # longest ComBus time of a loop (RX + TX), in us
        self.cb_avgs: list[int] = []  # mean ComBus time between two status lines, in us

    # --- reset line ---
    def hold(self, on: bool) -> None:
        """Hold the board in reset (on) or release it (off): EN is driven by RTS."""
        self.ser.dtr = False
        self.ser.rts = on
        if not on:
            self.boots += 1
            self.last_up = 0
            self.wait_line("READY", 5.0)

    def reboot(self) -> None:
        self.hold(True)
        time.sleep(0.1)
        self.hold(False)

    # --- console ---
    def lines(self) -> list[str]:
        out = []
        raw = self.ser.read(4096)
        while raw:
            for l in raw.decode("utf-8", errors="replace").splitlines():
                if l.strip():
                    out.append(l.strip())
            raw = self.ser.read(4096)
        self.log.extend(out)
        return out

    def wait_line(self, prefix: str, timeout: float) -> str:
        end = time.time() + timeout
        buf = ""
        while time.time() < end:
            chunk = self.ser.read(4096).decode("utf-8", errors="replace")
            self.raw += chunk
            buf += chunk
                # The colour codes are removed from the whole buffer: one may be split
                # between two reads, and the logs carry them without a newline
            for l in ANSI.sub("", buf).splitlines():
                if l.startswith(prefix):
                    self.log.append(l)
                    return l
        raise TimeoutError(f"{self.name}: no '{prefix}' line in {timeout} s")

    def drain(self) -> None:
        """Read what is waiting, keep it in raw (never thrown away: a crash must be seen)."""
        data = self.ser.read(self.ser.in_waiting or 1)
        while data:
            self.raw += data.decode("utf-8", errors="replace")
            data = self.ser.read(self.ser.in_waiting) if self.ser.in_waiting else b""

    def crashes(self) -> int:
        """Boots not caused by the bench, plus panics."""
        resets = max(0, self.raw.count("rst:0x") - self.boots)
        return max(resets, self.up_resets) + self.raw.count("Guru Meditation")

    def probe(self) -> str:
        """After a missing answer: does the board answer again, did it restart?"""
        for k in range(6):
            try:
                s = self.status()
                return f"answers again after {k * 0.5:.1f} s, up={s['up']} ms"
            except TimeoutError:
                time.sleep(0.5)
        return "silent for 6 s (frozen, or its console is)"

    def cmd(self, line: str, timeout: float = 1.0) -> None:
        self.drain()
        self.ser.write((line + "\n").encode())
        self.wait_line("OK " + line, timeout)

    def status(self) -> dict:
        self.drain()
        self.ser.write(b"s\n")
        line = self.wait_line("S ", 1.0)
        f = dict(kv.split("=", 1) for kv in line[2:].split())
        st = {k: int(f[k]) for k in ("st", "ct", "nx", "pr", "age", "den")}
        st["up"], st["lp"] = int(f.get("up", -1)), int(f.get("lp", -1))
        st["cbmax"] = int(f.get("cbmax", -1))
        if 0 <= st["up"] < self.last_up:
            self.up_resets += 1
        self.last_up = max(st["up"], 0)
        self.max_lp = max(self.max_lp, st["lp"])
        if "cbmax" in f:
            self.max_cb = max(self.max_cb, int(f["cbmax"]))
            self.cb_avgs.append(int(f["cbavg"]))
        st["A"] = [int(v) for v in f["A"].split(",")] if f["A"] else []
        st["D"] = [c == "1" for c in f["D"]]
        return st

    # --- channels ---
    def a(self, name: str, raw: int) -> None:
        self.cmd(f"a {self.analog[name]} {raw}")

    def d(self, name: str, on: bool) -> None:
        self.cmd(f"d {self.digital[name]} {1 if on else 0}")

    def ga(self, name: str) -> int:
        return self.status()["A"][self.analog[name]]

    def gd(self, name: str) -> bool:
        return self.status()["D"][self.digital[name]]


# =============================================================================
# 2. CHECKS
# =============================================================================

def check(label: str, ok: bool, detail: str = "") -> bool:
    RESULTS.append((label, ok, detail))
    print(f"{'PASS' if ok else 'FAIL'}  {label}" + (f"   [{detail}]" if detail else ""), flush=True)
    return ok


def until(fn, timeout: float, period: float = 0.05):
    """Poll fn() until it is true or the timeout expires; return (ok, elapsed s, last value)."""
    t0 = time.time()
    last = None
    while time.time() - t0 < timeout:
        last = fn()
        if last:
            return True, time.time() - t0, last
        time.sleep(period)
    return False, time.time() - t0, last


def check_until(label: str, fn, timeout: float, show=None) -> bool:
    ok, dt, _ = until(fn, timeout)
    detail = f"{dt:.2f} s" + (f", {show()}" if show else "")
    return check(label, ok, detail)


def stable(b: Board) -> bool:
    s = b.status()
    return s["ct"] == 1 and s["st"] == ST_ALIVE


# =============================================================================
# 3. SCENARIOS
# =============================================================================

def scenario_a(t: Board, r: Board) -> None:
    """Convoy: tractor t (socket) and trailer r (plug), through the TEST coupler."""
    n = neutral()
    print("\n=== A. Convoy: tractor (socket) <-> trailer (plug), TEST coupler ===")

    st, sr = t.status(), r.status()
    check("A0 unhitched: both DETACHED, no contract", st["pr"] == PR_DETACHED and sr["pr"] == PR_DETACHED
          and st["ct"] == 0 and sr["ct"] == 0, f"tractor pr={st['pr']} ct={st['ct']}, trailer pr={sr['pr']} ct={sr['ct']}")
    check("A0 unhitched: hitch contributor healthy on both",
          not t.gd("TEST_HITCH_LINK_FAIL") and not r.gd("TEST_HITCH_LINK_FAIL"))

    # Hitch: both triggers together
    t.d("TEST_HITCHED", True)
    r.d("TEST_HITCHED", True)
    check_until("A1 hitched: both COUPLED", lambda: t.status()["pr"] == PR_COUPLED and r.status()["pr"] == PR_COUPLED,
                4.0, lambda: f"tractor pr={t.status()['pr']}, trailer pr={r.status()['pr']}")
    check_until("A1 hitched: contract STABLE and link alive on both", lambda: stable(t) and stable(r), 3.0)
    check("A1 hitched: hitch contributor healthy on both",
          not t.gd("TEST_HITCH_LINK_FAIL") and not r.gd("TEST_HITCH_LINK_FAIL"))

    # Data both ways, through the tables
    t.a("TEST_BRAKE_BUS", 1234)
    t.d("TEST_LIGHTS", True)
    check_until("A2 brake tractor -> trailer", lambda: r.ga("TEST_BRAKE_BUS") == 1234, 1.0)
    check_until("A2 lights tractor -> trailer", lambda: r.gd("TEST_LIGHTS"), 1.0)
    r.a("TEST_TILT_ANGLE", 4321)
    r.a("TEST_AXLE_LOAD", 777)
    check_until("A2 tilt trailer -> tractor", lambda: t.ga("TEST_TILT_ANGLE") == 4321, 1.0)
    check_until("A2 axle load trailer -> tractor (channel injected on the tractor)",
                lambda: t.ga("TEST_AXLE_LOAD") == 777, 1.0)
    r.a("TEST_BRAKE_BUS", 9)
    time.sleep(0.3)
    check("A2 the trailer does not overwrite the brake of the tractor", t.ga("TEST_BRAKE_BUS") == 1234,
          f"tractor brake={t.ga('TEST_BRAKE_BUS')}")

    # Shared fault, both_or, without echo
    t.d("TEST_COUPLED_FAIL", False)
    r.d("TEST_COUPLED_FAIL", True)
    check_until("A3 fault raised by the trailer seen by the tractor", lambda: t.gd("TEST_COUPLED_FAIL"), 1.0)
    r.d("TEST_COUPLED_FAIL", False)
    check_until("A3 fault cleared by the trailer: falls back on both (no echo)",
                lambda: not t.gd("TEST_COUPLED_FAIL") and not r.gd("TEST_COUPLED_FAIL"), 1.5)
    t.d("TEST_COUPLED_FAIL", True)
    check_until("A3 fault raised by the tractor seen by the trailer", lambda: r.gd("TEST_COUPLED_FAIL"), 1.0)
    t.d("TEST_COUPLED_FAIL", False)
    check_until("A3 fault cleared by the tractor: falls back on both",
                lambda: not t.gd("TEST_COUPLED_FAIL") and not r.gd("TEST_COUPLED_FAIL"), 1.5)

    # Short drop of the trigger: no unhitching
    t.d("TEST_HITCHED", False)
    time.sleep(0.1)
    t.d("TEST_HITCHED", True)
    time.sleep(0.5)
    check("A4 short drop of the trigger (100 ms < releaseMs): still COUPLED", t.status()["pr"] == PR_COUPLED,
          f"tractor pr={t.status()['pr']}")

    # Loss: the trailer falls silent (held in reset)
    r.hold(True)
    check_until("A5 trailer silent: link LOST on the tractor", lambda: t.status()["st"] == ST_LOST, 1.5)
    check_until("A5 trailer silent: tilt back to neutral on the tractor", lambda: t.ga("TEST_TILT_ANGLE") == n, 1.0,
                lambda: f"tilt={t.ga('TEST_TILT_ANGLE')}")
    check_until("A5 trailer silent: hitch contributor at fault on the tractor",
                lambda: t.gd("TEST_HITCH_LINK_FAIL"), 1.5)
    check("A5 trailer silent: own brake of the tractor untouched", t.ga("TEST_BRAKE_BUS") == 1234)

    # The trailer comes back: both re-hitch
    r.hold(False)
    t.d("TEST_HITCHED", False)
    check_until("A6 unhitched: tractor DETACHED", lambda: t.status()["pr"] == PR_DETACHED, 1.5)
    check("A6 unhitched: hitch contributor healthy on the tractor", not t.gd("TEST_HITCH_LINK_FAIL"))
    t.d("TEST_HITCHED", True)
    r.d("TEST_HITCHED", True)
    check_until("A6 re-hitched: both COUPLED and STABLE again",
                lambda: t.status()["pr"] == PR_COUPLED and r.status()["pr"] == PR_COUPLED and stable(t) and stable(r), 5.0)
    check_until("A6 re-hitched: the trailer receives the brake again", lambda: r.ga("TEST_BRAKE_BUS") == 1234, 1.0)

    # Unhitch: the link goes off, received values back to safe
    t.d("TEST_HITCHED", False)
    r.d("TEST_HITCHED", False)
    check_until("A7 unhitched: both DETACHED", lambda: t.status()["pr"] == PR_DETACHED and r.status()["pr"] == PR_DETACHED, 1.5)
    check_until("A7 unhitched: brake back to neutral on the trailer", lambda: r.ga("TEST_BRAKE_BUS") == n, 1.0,
                lambda: f"brake={r.ga('TEST_BRAKE_BUS')}")
    check("A7 unhitched: hitch contributor healthy on both",
          not t.gd("TEST_HITCH_LINK_FAIL") and not r.gd("TEST_HITCH_LINK_FAIL"))


def scenario_b(m: Board, rc: Board) -> None:
    """Remote link: machine m (socket) and remote rc (plug)."""
    n = neutral()
    print("\n=== B. Remote link: machine (socket) <-> remote (plug) ===")

    check_until("B1 contract STABLE and link alive on both", lambda: stable(m) and stable(rc), 4.0,
                lambda: f"machine ct={m.status()['ct']} st={m.status()['st']}, remote ct={rc.status()['ct']} st={rc.status()['st']}")
    check_until("B1 machine: NODE_LINK_FAIL healthy", lambda: not m.gd("NODE_LINK_FAIL"), 1.0)

    rc.a("STEERING_BUS", 1111)
    rc.a("THROTTLE_BUS", 2222)
    rc.d("KEY_BTN", True)
    check_until("B2 steering remote -> machine", lambda: m.ga("STEERING_BUS") == 1111, 1.0)
    check_until("B2 throttle remote -> machine", lambda: m.ga("THROTTLE_BUS") == 2222, 1.0)
    check_until("B2 key button remote -> machine", lambda: m.gd("KEY_BTN"), 1.0)

    # The remote falls silent (held in reset)
    rc.hold(True)
    check_until("B3 remote silent: NODE_LINK_FAIL raised on the machine", lambda: m.gd("NODE_LINK_FAIL"), 1.0)
    check_until("B3 remote silent: steering back to neutral", lambda: m.ga("STEERING_BUS") == n, 1.0,
                lambda: f"steering={m.ga('STEERING_BUS')}")
    check("B3 remote silent: key button back to its safe value", not m.gd("KEY_BTN"))
    rc.hold(False)
    check_until("B3 remote back: STABLE again, NODE_LINK_FAIL healthy",
                lambda: stable(m) and not m.gd("NODE_LINK_FAIL"), 4.0)

    # The remote stops sending
    rc.a("STEERING_BUS", 1111)
    rc.cmd("t 0")
    check_until("B4 remote stops sending: NODE_LINK_FAIL raised", lambda: m.gd("NODE_LINK_FAIL"), 1.0)
    rc.cmd("t 1")
    check_until("B4 remote sends again: STABLE, NODE_LINK_FAIL healthy, steering received",
                lambda: stable(m) and not m.gd("NODE_LINK_FAIL") and m.ga("STEERING_BUS") == 1111, 4.0)

    # The remote no longer receives: no ACK, it drops its contract within 1 s and stops sending data
    rc.cmd("r 0")
    check_until("B5 remote deaf: its contract falls back to UNKNOWN within about 1 s",
                lambda: rc.status()["ct"] == 0, 2.0)
    check_until("B5 remote deaf: the machine loses the link", lambda: m.gd("NODE_LINK_FAIL"), 1.5)
    rc.cmd("r 1")
    check_until("B5 remote hears again: STABLE on both", lambda: stable(m) and stable(rc), 4.0)


def scenario_c(mb: Board, xb: Board) -> None:
    """Node: main board mb and extension board xb, link inside the node (SYSTEM)."""
    print("\n=== C. Node: main board <-> extension board (SYSTEM) ===")

    check_until("C1 contract STABLE and link alive on both", lambda: stable(mb) and stable(xb), 5.0)

    # Diffusion of SYSTEM channels
    mb.a("ESC_RPM_BUS", 3333)
    check_until("C2 ESC_RPM_BUS main -> extension (produced by the main board only)",
                lambda: xb.ga("ESC_RPM_BUS") == 3333, 1.0)
    xb.d("KEY_ACTIVE", True)
    check_until("C2 KEY_ACTIVE extension -> main (produced by the extension only)", lambda: mb.gd("KEY_ACTIVE"), 1.0)
    mb.a("RUNLEVEL", 2)
    check_until("C2 RUNLEVEL main -> extension (owned by the main board)", lambda: xb.ga("RUNLEVEL") == 2, 1.0)

    # Direction filter: what the main board produces is not overwritten
    xb.a("ESC_RPM_BUS", 5)
    time.sleep(0.3)
    check("C3 the extension does not overwrite ESC_RPM_BUS of the main board", mb.ga("ESC_RPM_BUS") == 3333,
          f"main ESC_RPM_BUS={mb.ga('ESC_RPM_BUS')}")

    # Shared fault
    mb.d("FAILSAFE_HARD", False)
    xb.d("FAILSAFE_HARD", True)
    check_until("C4 FAILSAFE_HARD raised by the extension seen by the main board", lambda: mb.gd("FAILSAFE_HARD"), 1.0)
    xb.d("FAILSAFE_HARD", False)
    check_until("C4 FAILSAFE_HARD cleared: falls back on both",
                lambda: not mb.gd("FAILSAFE_HARD") and not xb.gd("FAILSAFE_HARD"), 1.5)

    # Loss of a board of the node
    check("C5 BOARD_LINK_FAIL healthy while linked", not mb.gd("BOARD_LINK_FAIL"))
    xb.hold(True)
    check_until("C5 extension silent: BOARD_LINK_FAIL raised on the main board", lambda: mb.gd("BOARD_LINK_FAIL"), 1.0)

    # Startup grace: the main board restarts while the extension is still off
    mb.reboot()
    t0 = time.time()
    time.sleep(1.0)
    check("C6 main board restarted, extension off: no BOARD_LINK_FAIL during the startup grace (3 s)",
          not mb.gd("BOARD_LINK_FAIL"), f"{time.time() - t0:.1f} s after boot")
    check_until("C6 grace over, extension never seen: BOARD_LINK_FAIL raised", lambda: mb.gd("BOARD_LINK_FAIL"), 4.0,
                lambda: f"{time.time() - t0:.1f} s after boot")
    xb.hold(False)
    check_until("C6 extension back: STABLE on both, BOARD_LINK_FAIL healthy",
                lambda: stable(mb) and stable(xb) and not mb.gd("BOARD_LINK_FAIL"), 6.0)


def scenario_n_sockets(a: Board, b: Board) -> None:
    """Negative: two machines (two sockets of the remote link) must refuse each other."""
    print("\n=== N1. Negative: two machines on the remote link (socket <-> socket) ===")
    ok, dt, _ = until(lambda: a.status()["ct"] == 1 or b.status()["ct"] == 1, 3.0)
    check("N1 two sockets: no contract after 3 s", not ok, f"{dt:.2f} s")
    check("N1 two sockets: NODE_LINK_FAIL raised on both", a.gd("NODE_LINK_FAIL") and b.gd("NODE_LINK_FAIL"))


def scenario_n_tractors(a: Board, b: Board) -> None:
    """Negative: two tractors (two sockets of the TEST coupler) hitched together must not couple."""
    print("\n=== N2. Negative: two tractors hitched together (socket <-> socket) ===")
    a.d("TEST_HITCHED", True)
    b.d("TEST_HITCHED", True)
    # Each socket hears the requests of the other: CONFLICT at once, never retried
    check_until("N2 two sockets hitched: CONFLICT on both", lambda: a.status()["pr"] == PR_CONFLICT
                and b.status()["pr"] == PR_CONFLICT, 1.5,
                lambda: f"a pr={a.status()['pr']}, b pr={b.status()['pr']}")
    samples = []
    t0 = time.time()
    while time.time() - t0 < 3.0:
        sa, sb = a.status(), b.status()
        samples.append((sa["pr"], sb["pr"], sa["ct"], sb["ct"],
                        sa["D"][a.digital["TEST_HITCH_LINK_FAIL"]], sb["D"][b.digital["TEST_HITCH_LINK_FAIL"]]))
        time.sleep(0.1)
    check("N2 for 3 s: still CONFLICT, no contract, on both",
          all(s[0] == PR_CONFLICT and s[1] == PR_CONFLICT and s[2] == 0 and s[3] == 0 for s in samples),
          f"{len(samples)} samples")
    check("N2 for 3 s: hitch contributor at fault on both, without a gap",
          all(s[4] and s[5] for s in samples), f"{sum(1 for s in samples if not (s[4] and s[5]))} gaps")


# =============================================================================
# 4. MONKEY TEST
# =============================================================================

# Per scenario: what each board receives through the link (must be safe while the
# link is not alive), what it may write, and its link contributor.
MONKEY = {
    "A": {
        "rx": [{"TEST_TILT_ANGLE": "N", "TEST_AXLE_LOAD": "N"}, {"TEST_BRAKE_BUS": "N", "TEST_LIGHTS": False}],
        "wa": [["TEST_BRAKE_BUS"], ["TEST_TILT_ANGLE", "TEST_AXLE_LOAD"]],
        "wd": [["TEST_LIGHTS", "TEST_COUPLED_FAIL"], ["TEST_COUPLED_FAIL"]],
        "fail": [None, None], "hitch": True,
    },
    "B": {
        "rx": [{"STEERING_BUS": "N", "THROTTLE_BUS": "N", "KEY_BTN": False}, {}],
        "wa": [[], ["STEERING_BUS", "THROTTLE_BUS"]],
        "wd": [[], ["KEY_BTN"]],
        "fail": ["NODE_LINK_FAIL", None], "hitch": False,
    },
    "C": {
        "rx": [{}, {}],
        "wa": [["ESC_RPM_BUS"], []],
        "wd": [["FAILSAFE_HARD"], ["KEY_ACTIVE", "FAILSAFE_HARD"]],
        "fail": ["BOARD_LINK_FAIL", "BOARD_LINK_FAIL"], "hitch": False,
    },
}


def monkey(sc: str, a: Board, b: Board, steps: int, seed: int) -> None:
    """Random events on the link, safety invariants after each one, then a recovery."""
    import random
    rnd = random.Random(seed)
    cfg = MONKEY[sc]
    boards = [a, b]
    n = neutral()
    held = [False, False]
    violations: list[str] = []
    counts: dict[str, int] = {}
    print(f"\n=== M{sc}. Monkey test, {steps} events, seed {seed} ===", flush=True)

    def invariants(step: int, ev: str) -> None:
        for i, bd in enumerate(boards):
            if held[i]:
                continue
            s = bd.status()
            # Peak ComBus time, by the event that came before it
            if s["cbmax"] > cb_peak.get(ev, (0, ""))[0]:
                cb_peak[ev] = (s["cbmax"], bd.name)
            # Safe values while the link is not alive
            if s["st"] != ST_ALIVE:
                for name, safe in cfg["rx"][i].items():
                    if safe == "N":
                        v = s["A"][bd.analog[name]]
                        if v != n:
                            violations.append(f"step {step} {ev}: {bd.name} {name}={v} while link st={s['st']}")
                    elif s["D"][bd.digital[name]] != safe:
                        violations.append(f"step {step} {ev}: {bd.name} {name} not safe while link st={s['st']}")
            # The contributor of the link: raised whenever the link counts as lost
            f = cfg["fail"][i]
            if f and s["st"] == ST_LOST and not s["D"][bd.digital[f]]:
                violations.append(f"step {step} {ev}: {bd.name} {f} healthy while the link is LOST")
            if sc == "B" and i == 0 and s["st"] != ST_ALIVE and not s["D"][bd.digital["NODE_LINK_FAIL"]]:
                violations.append(f"step {step} {ev}: machine NODE_LINK_FAIL healthy while the link is not alive")

    events = ["hold", "release", "flap_tx", "flap_rx", "tx", "rx", "noise", "write", "write", "write",
              "bad_cmd", "burst", "idle", "stall", "stall"] \
        + (["hitch_bounce", "hitch_drop"] if cfg["hitch"] else [])
    cb_peak: dict[str, tuple[int, str]] = {}

    def live() -> list:
        """The boards not held in reset: a held board cannot take a command."""
        return [x for k, x in enumerate(boards) if not held[k]]

    trace: list[str] = []
    t_start = time.time()
    try:
        for step in range(1, steps + 1):
            ev = rnd.choice(events)
            i = rnd.randrange(2)
            bd = boards[i]
            if held[i] and ev not in ("release", "idle"):
                ev = "release"
            counts[ev] = counts.get(ev, 0) + 1
            trace.append(f"{time.time() - t_start:7.2f} s  step {step:3d}  {ev:12s} {bd.name}")
            if ev == "hold":
                bd.ser.rts = True
                held[i] = True
            elif ev == "release":
                if held[i]:
                    bd.hold(False)
                    held[i] = False
                    if cfg["hitch"]:       # a restarted board is unhitched: hitch it again with its partner
                        for x in live():
                            x.d("TEST_HITCHED", False)
                        time.sleep(0.4)
                        for x in live():
                            x.d("TEST_HITCHED", True)
            elif ev in ("flap_tx", "flap_rx"):
                c = "t" if ev == "flap_tx" else "r"
                for _ in range(rnd.randint(2, 6)):
                    bd.cmd(f"{c} 0")
                    time.sleep(rnd.uniform(0.0, 0.15))
                    bd.cmd(f"{c} 1")
                    time.sleep(rnd.uniform(0.0, 0.15))
            elif ev in ("tx", "rx"):
                c = "t" if ev == "tx" else "r"
                bd.cmd(f"{c} 0")
                time.sleep(rnd.uniform(0.1, 2.0))
                bd.cmd(f"{c} 1")
            elif ev == "noise":
                for _ in range(rnd.randint(1, 4)):
                    bd.cmd(f"g {rnd.randint(1, 255)}")
            elif ev == "write":
                if cfg["wa"][i] and rnd.random() < 0.6:
                    bd.a(rnd.choice(cfg["wa"][i]), rnd.choice([0, 1, n, 65535, rnd.randint(0, 65535)]))
                elif cfg["wd"][i]:
                    bd.d(rnd.choice(cfg["wd"][i]), rnd.random() < 0.5)
            elif ev == "bad_cmd":
                junk = bytes(rnd.randrange(32, 127) for _ in range(60))
                if junk.lstrip()[:1] in (b"x", b"t", b"r"):      # a real bench command, not junk
                    junk = b"#" + junk
                bd.ser.write(rnd.choice([b"zz 1\n", b"a 250 1\n", b"d 999 1\n", b"g 0\n", b"\n\n\n", b"a\n",
                                         junk + b"\n"]))
                time.sleep(0.05)
            elif ev == "burst":
                for _ in range(rnd.randint(5, 20)):
                    if cfg["wa"][i]:
                        bd.a(rnd.choice(cfg["wa"][i]), rnd.randint(0, 65535))
            elif ev == "hitch_bounce":
                bd.d("TEST_HITCHED", False)
                time.sleep(rnd.uniform(0.02, 0.25))      # shorter than releaseMs: must not unhitch
                bd.d("TEST_HITCHED", True)
            elif ev == "hitch_drop":
                for x in live():
                    x.d("TEST_HITCHED", False)
                time.sleep(rnd.uniform(0.4, 1.5))
                for x in live():
                    x.d("TEST_HITCHED", True)
            elif ev == "idle":
                time.sleep(rnd.uniform(0.1, 1.5))
            elif ev == "stall":
                # A module blocks the board: short, the peer must not lose the link;
                # long, it must, and take back its safe values (the invariants)
                dur = rnd.choice([20, 80, 150, 400, 800, 2000])
                peer = boards[1 - i]
                watch = not held[1 - i] and peer.status()["st"] == ST_ALIVE and bd.status()["st"] == ST_ALIVE
                bd.drain()
                bd.ser.write(f"w {dur}\n".encode())
                if watch:
                    time.sleep((dur - 50) / 1000 if dur >= 400 else dur * 0.9 / 1000)
                    sp = peer.status()
                    if dur <= 150 and sp["st"] != ST_ALIVE:
                        violations.append(f"step {step} stall {dur} ms on {bd.name}: {peer.name} lost the link (st={sp['st']})")
                    if dur >= 400 and sp["st"] == ST_ALIVE:
                        violations.append(f"step {step} stall {dur} ms on {bd.name}: {peer.name} still alive")
                bd.wait_line(f"OK w {dur}", dur / 1000 + 1.0)
                trace[-1] += f" {dur} ms" + ("" if watch else " (not watched)")
            time.sleep(rnd.uniform(0.0, 0.4))
            invariants(step, ev)
    except TimeoutError:
        print("--- last events ---")
        print(*trace[-15:], sep="\n")
        for bd in boards:
            print(f"--- probe {bd.name}: {bd.probe()}")
            m = [l for l in ANSI.sub("", bd.raw).splitlines() if l.startswith("READY")]
            print(f"    last READY line: {m[-1] if m else '-'}")
        raise

    # Recovery: everything back to normal, the link must come back and carry data
    for i, bd in enumerate(boards):
        if held[i]:
            bd.hold(False)
            held[i] = False
        bd.cmd("t 1")
        bd.cmd("r 1")
    if cfg["hitch"]:
        for x in boards:
            x.d("TEST_HITCHED", False)
        time.sleep(0.5)
        for x in boards:
            x.d("TEST_HITCHED", True)

    print("events: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
    print("ComBus peak after each event: " + ", ".join(
        f"{k} {v[0]} us ({v[1]})" for k, v in sorted(cb_peak.items(), key=lambda kv: -kv[1][0])))
    for x in boards:
        if x.cb_avgs:
            s = sorted(x.cb_avgs)
            print(f"ComBus time per loop on {x.name}: mean {sum(s) // len(s)} us "
                  f"(median {s[len(s) // 2]}, worst mean {s[-1]}), peak {x.max_cb} us")
    check(f"M{sc} no crash nor unexpected reboot",
          a.crashes() == 0 and b.crashes() == 0, f"a {a.crashes()}, b {b.crashes()}")
    # A loop blocked near the loss timeout (250 ms) would make the link look lost
    check(f"M{sc} longest loop under 100 ms on both", a.max_lp < 100 and b.max_lp < 100,
          f"a {a.max_lp} ms, b {b.max_lp} ms")
    check(f"M{sc} safety invariants held after every event", not violations,
          "; ".join(violations[:5]) + (f" (+{len(violations) - 5})" if len(violations) > 5 else ""))
    check_until(f"M{sc} recovery: STABLE and alive on both", lambda: stable(a) and stable(b), 8.0,
                lambda: f"a ct={a.status()['ct']} st={a.status()['st']}, b ct={b.status()['ct']} st={b.status()['st']}")
    sender, receiver, name = {"A": (a, b, "TEST_BRAKE_BUS"), "B": (b, a, "STEERING_BUS"),
                              "C": (a, b, "ESC_RPM_BUS")}[sc]
    sender.a(name, 4242)
    check_until(f"M{sc} recovery: data flows again ({name})", lambda: receiver.ga(name) == 4242, 1.5)
    for fail, bd in zip(cfg["fail"], boards):
        if fail:
            check_until(f"M{sc} recovery: {fail} healthy on {bd.name}", lambda: not bd.gd(fail), 1.5)


# =============================================================================
# 5. MAIN
# =============================================================================

def flash(env: str, port: str) -> None:
    print(f"flash {env} -> {port}", flush=True)
    log = ROOT / ".pio" / f"bench_flash_{env}.log"
    with open(log, "w", encoding="utf-8") as f:
        rc = subprocess.run(PIO + ["run", "-e", env, "-t", "upload", "--upload-port", port],
                            cwd=ROOT, stdout=f, stderr=subprocess.STDOUT).returncode
    if rc != 0:
        raise RuntimeError(f"flash {env} failed, see {log}")
    log.unlink()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="serial port of board a")
    ap.add_argument("--b", required=True, help="serial port of board b")
    ap.add_argument("--no-flash", action="store_true")
    ap.add_argument("--steps", type=int, default=120, help="monkey test: events per scenario")
    ap.add_argument("--seed", type=int, default=None, help="monkey test: random seed (replay)")
    ap.add_argument("scenarios", nargs="*", default=["A", "B", "C"])
    args = ap.parse_args()

    for sc in args.scenarios:
        env_a, env_b = SCENARIOS[sc.lstrip("M")] if sc.startswith("M") else SCENARIOS[sc]
        if not args.no_flash:
            flash(env_a, args.a)
            flash(env_b, args.b)
        a = Board(args.a, env_a, env_a)
        b = Board(args.b, env_b, env_b)
        try:
            a.reboot()
            b.reboot()
            if sc.startswith("M"):
                seed = args.seed if args.seed is not None else int(time.time())
                monkey(sc[1:], a, b, args.steps, seed)
            else:
                {"A": scenario_a, "B": scenario_b, "C": scenario_c,
                 "N1": scenario_n_sockets, "N2": scenario_n_tractors}[sc](a, b)
        except (TimeoutError, KeyError) as e:
            check(f"{sc} aborted", False, repr(e))
            for x in (a, b):
                x.drain()
                print(f"--- last output of {x.name} ---\n{ANSI.sub('', x.raw)[-1500:]}\n---")
        finally:
            for x in (a, b):
                x.ser.rts = False          # never leave a board held in reset
                x.ser.close()

    passed = sum(1 for _, ok, _ in RESULTS if ok)
    print(f"\n{passed}/{len(RESULTS)} checks passed")
    for label, ok, detail in RESULTS:
        if not ok:
            print(f"  FAIL  {label}   [{detail}]")
    return 0 if passed == len(RESULTS) else 1


if __name__ == "__main__":
    sys.exit(main())
