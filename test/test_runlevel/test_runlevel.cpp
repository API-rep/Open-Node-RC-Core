/******************************************************************************
 * @file test_runlevel.cpp
 * @brief Runlevel chain of the Volvo: HALT held until a reboot (link rework S3, brick 1).
 *
 * @details The real chain (kRunlevelProcs, runlevel_config.h) runs through the
 *   real runner (proc_chain_step), one call = one cycle.  The levers
 *   (FAILSAFE_HARD, FAILSAFE_SOFT, KEY_ACTIVE) and RUNLEVEL are written
 *   directly on the bus, as their sources would.
 *
 *   Rows of the table validated by the owner (2026-10-27):
 *     1  RUNNING, hard fault → HALT;
 *     2  HALT, hard fault cleared → stays HALT;
 *     3  HALT, fault cleared, key pressed → stays HALT (was STARTING);
 *     4  soft fault raised during HALT, hard fault cleared → stays HALT (was IDLE);
 *     5  reboot (RUNLEVEL back to its init value IDLE) → IDLE, the key starts again;
 *     7  IDLE, key pressed → STARTING;
 *     8  RUNNING, soft fault rising edge → IDLE.
 *   Row 6 (unknown value → HALT) is in main.cpp: build and review only.
 *
 *   No jumper needed.  Run: pio test -e test_runlevel
 *****************************************************************************/

#include <Arduino.h>
#include <unity.h>
#include <iterator>   // std::size

#include "combus.h"
#include <core/system/combus/combus_defs.h>
#include <core/system/combus/processors/proc_chain.h>
#include <core/system/combus/processors/base/cb_runlevel.h>

  // The chain under test, with the context its umbrella (proc_config.cpp) gives it.
#include <machines/config/machines/volvo_A60H_bruder/combus/processors/input/runlevel_config.h>


// =============================================================================
// FIXTURE
// =============================================================================

static CbChain s_chain = {
    .name      = "runlevel",
    .inCh      = AnalogComBusID::RUNLEVEL,
    .outCh     = AnalogComBusID::RUNLEVEL,
    .procs     = kRunlevelProcs,
    .procCount = static_cast<uint8_t>(std::size(kRunlevelProcs)),
};


static void setRunLevel(RunLevel rl) { comBus.analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value = static_cast<uint16_t>(rl); }
static RunLevel runLevel(void)       { return static_cast<RunLevel>(comBus.analogBus[static_cast<uint8_t>(AnalogComBusID::RUNLEVEL)].value); }
static void setHard(bool v)          { comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::FAILSAFE_HARD)].value = v; }
static void setSoft(bool v)          { comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::FAILSAFE_SOFT)].value = v; }
static void setKey(bool v)           { comBus.digitalBus[static_cast<uint8_t>(DigitalComBusID::KEY_ACTIVE)].value = v; }

  /** One cycle of the runlevel chain. */
static void cycle(void) { proc_chain_step(s_chain, comBus); }


  /** Fresh start: levers at rest, edge memories cleared, RUNLEVEL at its init value. */
void setUp(void) {
    setHard(false);
    setSoft(false);
    setKey(false);
    gRunlevelOnceState   = {};
    gRemoteLinkOnceState = {};
    setRunLevel(RunLevel::IDLE);
}

void tearDown(void) {}


  /** From IDLE, the key brings the machine to RUNNING (as main.cpp would after STARTING). */
static void startRunning(void) {
    setKey(true);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::STARTING), static_cast<int>(runLevel()));
    setRunLevel(RunLevel::RUNNING);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::RUNNING), static_cast<int>(runLevel()));
}


// =============================================================================
// ROWS
// =============================================================================

void test_row1_hard_fault_gives_halt(void) {
    startRunning();
    setHard(true);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::HALT), static_cast<int>(runLevel()));
}


void test_row2_halt_stays_when_hard_fault_clears(void) {
    startRunning();
    setHard(true);
    cycle();
    setHard(false);
    for (int i = 0; i < 5; i++) cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::HALT), static_cast<int>(runLevel()));
}


void test_row3_key_does_not_leave_halt(void) {
    startRunning();
    setHard(true);
    cycle();
    setHard(false);
    cycle();
    setKey(false);   // key off ...
    cycle();
    setKey(true);    // ... and on again
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::HALT), static_cast<int>(runLevel()));
}


void test_row4_soft_fault_does_not_turn_halt_into_idle(void) {
    startRunning();
    setHard(true);
    cycle();
    setSoft(true);   // soft fault raised while the hard one holds
    cycle();
    setHard(false);  // hard fault clears, the soft one stays
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::HALT), static_cast<int>(runLevel()));
}


void test_row5_reboot_leaves_halt(void) {
    startRunning();
    setHard(true);
    cycle();
    setHard(false);
    cycle();

    setUp();         // reboot: init.cpp writes RUNLEVEL = IDLE, the chain states restart at zero
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::IDLE), static_cast<int>(runLevel()));
    setKey(true);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::STARTING), static_cast<int>(runLevel()));
}


void test_row7_key_starts_from_idle(void) {
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::IDLE), static_cast<int>(runLevel()));
    setKey(true);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::STARTING), static_cast<int>(runLevel()));
}


void test_row8_soft_fault_edge_gives_idle(void) {
    startRunning();
    setSoft(true);
    cycle();
    TEST_ASSERT_EQUAL(static_cast<int>(RunLevel::IDLE), static_cast<int>(runLevel()));
}


// =============================================================================
// RUNNER
// =============================================================================

void setup() {
    delay(2000u);
    UNITY_BEGIN();

    RUN_TEST(test_row1_hard_fault_gives_halt);
    RUN_TEST(test_row2_halt_stays_when_hard_fault_clears);
    RUN_TEST(test_row3_key_does_not_leave_halt);
    RUN_TEST(test_row4_soft_fault_does_not_turn_halt_into_idle);
    RUN_TEST(test_row5_reboot_leaves_halt);
    RUN_TEST(test_row7_key_starts_from_idle);
    RUN_TEST(test_row8_soft_fault_edge_gives_idle);

    UNITY_END();
}

void loop() {}
