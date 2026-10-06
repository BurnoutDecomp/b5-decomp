#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "playtest_pc_freeburn_identity.inc"

static unsigned guChecks, guFailures;
static void Check(bool lbPassed, const char* lpcLabel) {
    ++guChecks; if (!lbPassed) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
}
int main() {
    _putenv_s("BRN_HARNESS_SLOT", "0");
    _putenv_s("BP_LAN", ""); _putenv_s("BP_LAN_NAME", ""); _putenv_s("BP_LAN_XUID", "");
    _putenv_s("USERNAME", "Driver"); _putenv_s("COMPUTERNAME", "FIRST-PC");
    Identity lDefault = Resolve();
    Check(lDefault.mbLan, "Freeburn service enabled without harness environment");
    Check(std::strcmp(lDefault.mac, "Driver") == 0, "local persona visible by default");
    Check(lDefault.mu64Xuid && lDefault.muIdent, "nonzero login and lobby identities");
    _putenv_s("COMPUTERNAME", "SECOND-PC");
    Identity lPeer = Resolve();
    Check(lPeer.mu64Xuid != lDefault.mu64Xuid && lPeer.muIdent != lDefault.muIdent,
          "same account on two PCs remains two distinct peers");
    _putenv_s("BP_LAN", "0"); Check(!Resolve().mbLan, "explicit offline opt-out");
    _putenv_s("BP_LAN", "1"); Check(Resolve().mbLan, "existing LAN opt-in still supported");
    _putenv_s("BP_LAN_NAME", "LanDriver");
    Identity lNamed = Resolve();
    _putenv_s("COMPUTERNAME", "THIRD-PC");
    Check(Resolve().mu64Xuid == lNamed.mu64Xuid, "explicit persona keeps established deterministic identity");
    _putenv_s("BP_LAN_XUID", "0x1122334455667788");
    Check(Resolve().mu64Xuid == 0x1122334455667788ull, "explicit full-width XUID override");
    _putenv_s("BP_LAN_XUID", "garbage");
    Check(Resolve().mu64Xuid == lNamed.mu64Xuid, "invalid override uses shared persona identity");
    _putenv_s("BP_LAN_NAME", "12345678901234567890");
    Check(std::strcmp(Resolve().mac, "123456789012345") == 0, "persona respects 15-character platform contract");
    _putenv_s("BP_LAN_NAME", ""); _putenv_s("USERNAME", "");
    Check(std::strcmp(Resolve().mac, "Player") == 0, "missing account keeps nonempty fallback");
    _putenv_s("BP_LAN", "");
    Check(CgsPcNetLanEnabled() && CgsPcNetIdentityXuid() && CgsPcNetIdentityLobbyIdent(), "public service adapter sees default identity");
    std::printf("PlaytestPcFreeburnIdentity: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
