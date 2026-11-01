/******************************************************************************
 * @file dashboard_links.cpp
 * @brief ANSI terminal dashboard — links view (key '7', link rework).
 *****************************************************************************/

#ifdef DEBUG_DASHBOARD

#include "dashboard_links.h"
#include <core/system/debug/dashboard/dashboard.h>
#include <core/system/combus/protocol/combus_links.h>

#include <Arduino.h>
#include <stdio.h>
#include <string.h>


static const ComBus* s_bus = nullptr;


// =============================================================================
// 1. HELPERS
// =============================================================================

static const char* stateName(ComLinkState s) {
	switch (s) {
		case ComLinkState::NEVER_SEEN: return "NEVER";
		case ComLinkState::ALIVE:      return "ALIVE";
		case ComLinkState::LOST:       return "LOST";
	}
	return "?";
}

static const char* scopeName(LinkScope s) {
	return (s == LinkScope::INTRA_NODE) ? "boards" : "node";
}

static const char* roleName(ComLinkRole r) {
	return (r == ComLinkRole::SOCKET) ? "socket" : "plug";
}

	// "infoName = 0/1" of a digital channel, or "-" when the channel is absent.
static void fmtDigital(char* out, size_t size, const std::optional<DigitalComBusID>& ch) {
	if (!ch || !s_bus) { snprintf(out, size, "-"); return; }
	const DigitalComBus& d = s_bus->digitalBus[static_cast<uint8_t>(*ch)];
	snprintf(out, size, "%s = %u", d.infoName ? d.infoName : "?", d.value ? 1u : 0u);
}

	// First bytes of an MD5, enough to tell two contracts apart.
static void fmtMd5(char* out, size_t size, const uint8_t* md5) {
	if (!md5) { snprintf(out, size, "-"); return; }
	snprintf(out, size, "%02X%02X%02X%02X..", md5[0], md5[1], md5[2], md5[3]);
}

static void title(const char* left) {
	char upt[12];
	fmtUptime(upt, sizeof(upt));
	char right[28];
	int  lLen = (int)strlen(left);
	int  rLen = snprintf(right, sizeof(right), "uptime: %s  ", upt);
	dLine("%s%*s%s", left, (int)DashInnerW - lLen - rLen, "", right);
}


// =============================================================================
// 2. HOME PAGE
// =============================================================================

static uint8_t links_detail_count() {
	return (uint8_t)(combus_links_count() + combus_couplings_count());
}

static void render_links_view() {
	const uint8_t nLinks = combus_links_count();
	const uint8_t nCpl   = combus_couplings_count();

	dPre();
	dTop();
	{
		char left[64];
		snprintf(left, sizeof(left), "  [ LINKS ]  %u link(s), %u passive coupler(s)", nLinks, nCpl);
		title(left);
	}
	dMid();

	if (nLinks == 0u && nCpl == 0u) {
		dEmpty();
		dLine("  No link under watch.");
		dEmpty();
		dBot();
		return;
	}

	dLine("  %-2s  %-20s  %-6s  %-5s  %-8s  %-6s  %s", "#", "Name", "Scope", "State", "Contract", "Mode", "Coupler / pairing");
	dMid();
	for (uint8_t i = 0u; i < nLinks; ++i) {
		ComLinkView v;
		if (!combus_links_view(i, v)) { continue; }
		char cpl[40] = "-";
		if (v.peer) {
			if (v.coupling) {
				snprintf(cpl, sizeof(cpl), "%s (%s) %s", v.peer->coupler, roleName(v.peer->role),
				         combus_pairing_state_name(v.pairing.state));
			} else {
				snprintf(cpl, sizeof(cpl), "%s (%s)", v.peer->coupler, roleName(v.peer->role));
			}
		}
		dLine("  %2u  %-20.20s  %-6s  %-5s  %-8s  %-6s  %s", i, v.name ? v.name : "?", scopeName(v.scope),
		      v.receives ? stateName(v.state) : "TX", v.contractStable ? "STABLE" : "UNKNOWN",
		      v.nextMode ? "next" : "stable", cpl);
	}

	if (nCpl > 0u) {
		dMidLabel("passive couplers");
		for (uint8_t i = 0u; i < nCpl; ++i) {
			ComCouplingRuntime c;
			if (!combus_couplings_view(i, c) || !c.dev) { continue; }
			dLine("  %2u  %-20.20s  %-8s  type %u  features 0x%04X", (unsigned)(nLinks + i), c.dev->name,
			      combus_pairing_state_name(c.pairing.state), c.pairing.id.type, c.pairing.id.features);
		}
	}

	dBot();
}


// =============================================================================
// 3. DETAIL PAGES
// =============================================================================

	// Pairing part, common to a coupled link and a passive coupler.
static void renderPairing(const CouplingDev& dev, const ComLinkPairing& p) {
	char trig[48], coupled[48], det[48], fail[48];
	fmtDigital(trig, sizeof(trig), std::optional<DigitalComBusID>(dev.trigger));
	fmtDigital(coupled, sizeof(coupled), dev.coupled);
	fmtDigital(det, sizeof(det), dev.detector);
	fmtDigital(fail, sizeof(fail), dev.fail);

	dLine("  Coupler device :  %s", dev.name);
	dLine("  Trigger :  %s   |  Coupled :  %s", trig, coupled);
	dLine("  Detector :  %s   |  Fault :  %s", det, fail);
	dLine("  Pairing :  %s   engaged %u   glitch %u   since the edge %lu ms", combus_pairing_state_name(p.state),
	      p.engaged ? 1u : 0u, p.glitch ? 1u : 0u, p.engaged ? (unsigned long)(millis() - p.t0Ms) : 0ul);
	dLine("  Debounce :  engage %u ms / release %u ms   |  age tolerance %u ms",
	      dev.engageMs, dev.releaseMs, dev.ageToleranceMs);
	dLine("  Identity :  type %u   features 0x%04X   (reader %s)", p.id.type, p.id.features,
	      dev.readId ? "present" : "none");
}

static void renderLinkDetail(uint8_t idx) {
	ComLinkView v;
	if (!combus_links_view(idx, v)) { dLine("  (link not available)"); return; }

	dLine("  Scope :  %s   |  Receives :  %s   |  Timeout %lu ms   Grace %lu ms", scopeName(v.scope),
	      v.receives ? "yes" : "no", (unsigned long)v.linkTimeoutMs, (unsigned long)v.startupGraceMs);
	if (v.receives) {
		dLine("  State :  %s   |  Contract :  %s (%s)   |  Last frame :  %lu ms ago", stateName(v.state),
		      v.contractStable ? "STABLE" : "UNKNOWN", v.nextMode ? "next" : "stable", (unsigned long)v.rxAgeMs);
	} else {
		dLine("  State :  TX only   |  Contract :  %s", v.contractStable ? "STABLE" : "UNKNOWN");
	}

	if (v.peer) {
		char ann[16], exp[16];
		fmtMd5(ann, sizeof(ann), v.peer->announced.stable);
		fmtMd5(exp, sizeof(exp), v.peer->expected.stable);
		dMidLabel("coupler");
		dLine("  Coupler :  %s   |  Role :  %s   |  Frame :  A%u + D%u", v.peer->coupler, roleName(v.peer->role),
		      v.peer->analog.count, v.peer->digital.count);
		dLine("  MD5 announced :  %s   |  expected :  %s", ann, exp);
	}

	if (v.coupling) {
		dMidLabel("pairing");
		renderPairing(*v.coupling, v.pairing);
		dLine("  Round :  %u / %u   |  Retries left :  %u", v.pairing.round, kCouplingRounds, v.pairing.retriesLeft);
		for (uint8_t c = 0u; c < v.pairing.nCand; ++c) {
			const PairingCandidate& k = v.pairing.cand[c];
			dLine("  Candidate %u :  %02X:%02X:%02X:%02X:%02X:%02X   rounds 0x%02X", c,
			      k.addr.bytes[0], k.addr.bytes[1], k.addr.bytes[2], k.addr.bytes[3], k.addr.bytes[4],
			      k.addr.bytes[5], k.rounds);
		}
	}
}

static void renderPassiveDetail(uint8_t idx) {
	ComCouplingRuntime c;
	if (!combus_couplings_view(idx, c) || !c.dev) { dLine("  (coupler not available)"); return; }
	dLine("  Passive coupler (no link): its trigger is its detector.");
	dMid();
	renderPairing(*c.dev, c.pairing);
}

static void render_links_detail() {
	const uint8_t idx    = dashboard_detail_index();
	const uint8_t nLinks = combus_links_count();
	const char*   name   = "?";
	if (idx < nLinks) {
		ComLinkView v;
		if (combus_links_view(idx, v) && v.name) { name = v.name; }
	} else {
		ComCouplingRuntime c;
		if (combus_couplings_view((uint8_t)(idx - nLinks), c) && c.dev) { name = c.dev->name; }
	}

	dPre();
	dTop();
	{
		char left[64];
		snprintf(left, sizeof(left), "  [ LINKS ]  #%u - %s detail", idx, name);
		title(left);
	}
	dMid();
	if (idx < nLinks) { renderLinkDetail(idx); }
	else              { renderPassiveDetail((uint8_t)(idx - nLinks)); }
	dBot();
}


// =============================================================================
// 4. REGISTRATION
// =============================================================================

void dashboard_links_register(const ComBus* bus) {
	s_bus = bus;
	dashboard_register_slot('7', "links", render_links_view);
	dashboard_register_detail('7', links_detail_count, render_links_detail);
}


#endif // DEBUG_DASHBOARD

// EOF dashboard_links.cpp
