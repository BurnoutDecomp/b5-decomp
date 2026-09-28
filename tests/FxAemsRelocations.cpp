// Relocation kinds are fixed by ARTIST 82B717C8 and XB1 14096C69E.
// Real bank class relocations have kind 1. The callees receive distinct handles.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/Sound/Playback/CgsSoundPcmTrace.h"
using u8 = uint8_t; using u16 = uint16_t; using u32 = uint32_t;
struct ModuleBank { u32 muTotalSize, muResidentSize, muCsisRelocOffset; int miBankHandle; };
struct CsisRelocationRecord { u32 muTargetOffset, muInterfaceIdOffset; u8 muKind, maPadding[3]; };
int gKind = -1, gCalls = 0;
u16 gSystem = 0, gInterface = 0;
const char* gName = nullptr;
namespace Csis {
struct InterfaceId { u16 muSystemId, muInterfaceId; const char* mpName; };
template<int Kind> struct Handle {
    void* words[2];
    int SetFast(const InterfaceId* id) {
        gKind = Kind; ++gCalls; gSystem = id->muSystemId;
        gInterface = id->muInterfaceId; gName = id->mpName;
        return -1; // Deferred interface subscription does not invalidate a bank.
    }
};
using GlobalVariableHandle = Handle<0>;
using ClassHandle = Handle<1>;
using FunctionHandle = Handle<2>;
}
#include "fx_aems_relocations.inc"
int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool good, const char* name) {
        ++checks; if (!good) { ++failures; std::printf("FAIL %s\n", name); }
    };
    alignas(16) u8 bytes[256] = {};
    ModuleBank* bank = reinterpret_cast<ModuleBank*>(bytes);
    bank->muTotalSize = 256; bank->muResidentSize = 128; bank->muCsisRelocOffset = 128;
    *reinterpret_cast<u32*>(bytes + 128) = 1;
    auto* entry = reinterpret_cast<CsisRelocationRecord*>(bytes + 132);
    entry->muTargetOffset = 64; entry->muInterfaceIdOffset = 160;
    *reinterpret_cast<u16*>(bytes + 160) = 0x73c8;
    *reinterpret_cast<u16*>(bytes + 162) = 0x73c8;
    std::strcpy(reinterpret_cast<char*>(bytes + 164), "TrafficEngineClass");
    for (int kind = 0; kind < 3; ++kind) {
        entry->muKind = static_cast<u8>(kind); gCalls = 0; gKind = -1;
        check(ApplyCsisRelocations(bank, bank), "deferred handle resolution accepted");
        check(gCalls == 1 && gKind == kind, "serialized kind selects correct handle resolver");
        check(gSystem == 0x73c8 && gInterface == 0x73c8 &&
              std::strcmp(gName, "TrafficEngineClass") == 0, "interface id/name preserved");
    }
    gCalls = 0; entry->muTargetOffset = 120;
    check(!ApplyCsisRelocations(bank, bank) && gCalls == 0, "out-of-resident target rejected");
    entry->muTargetOffset = 64; entry->muInterfaceIdOffset = 254;
    check(!ApplyCsisRelocations(bank, bank) && gCalls == 0, "short interface id rejected");
    entry->muInterfaceIdOffset = 160; bank->muCsisRelocOffset = 254;
    check(!ApplyCsisRelocations(bank, bank) && gCalls == 0, "short relocation table rejected");
    std::printf("FxAemsRelocations: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
