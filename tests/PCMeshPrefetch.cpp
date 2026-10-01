#define NOMINMAX
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <xmmintrin.h>
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"

static unsigned suChecks = 0, suFailures = 0, suHints = 0;
static uintptr_t sauAddresses[32];
static void Check(bool lbPass, const char* lpcName)
{
    ++suChecks;
    if (!lbPass) { ++suFailures; std::printf("FAIL: %s\n", lpcName); }
}
static void RecordHint(const char* lpAddress, int liHint)
{
    if (suHints < 32) sauAddresses[suHints] = reinterpret_cast<uintptr_t>(lpAddress);
    ++suHints;
    if (liHint != _MM_HINT_T0) ++suFailures;
}
#define _mm_prefetch RecordHint
#define CGS_ASSERT(lbPass, ...) do { if (!(lbPass)) ++suFailures; } while (0)
#include "pc_mesh_prefetch.inc"
#undef _mm_prefetch

static bool Run(CgsGraphics::DispatchCommand* lpBin, const u64* lpKeys, u32 luIndex, u32 luEnd)
{
    __try { CgsGraphics::PrefetchMeshCommandsPC(lpBin, lpKeys, luIndex, luEnd); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int main()
{
    SYSTEM_INFO lInfo = {}; GetSystemInfo(&lInfo);
    auto* lpPages = static_cast<u8*>(VirtualAlloc(nullptr, lInfo.dwPageSize * 2,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!lpPages) return 2;
    DWORD luOld = 0;
    if (!VirtualProtect(lpPages + lInfo.dwPageSize, lInfo.dwPageSize, PAGE_NOACCESS, &luOld)) return 3;
    auto* lpKeys = reinterpret_cast<u64*>(lpPages + lInfo.dwPageSize) - 4;
    alignas(128) u8 lauPackets[1024] = {};
    alignas(128) u8 lauMeshes[4][320] = {};
    auto* lpBin = reinterpret_cast<CgsGraphics::DispatchCommand*>(lauPackets);
    // Deliberately permuted sorted records. High material bits must not become
    // part of the packet offset; each command carries a real full host pointer.
    const unsigned lauOffsets[] = {16, 48, 32, 0};
    for (unsigned lu = 0; lu < 4; ++lu)
    {
        lpKeys[lu] = 0xabcd000000000000ull | lauOffsets[lu];
        auto* lpPacket = reinterpret_cast<u32*>(lauPackets + lauOffsets[lu] * 16);
        lpPacket[0] = u32(CgsGraphics::DispatchCommand::E_DRAWRENDERABLEMESH)
            << CgsGraphics::DispatchCommand::KU_SIZE_BITS;
        const uintptr_t luMesh = reinterpret_cast<uintptr_t>(lauMeshes[lu] + 16);
        std::memcpy(lpPacket + 2, &luMesh, sizeof(luMesh));
    }
    Check(Run(lpBin, lpKeys + 4, 0, 0) && suHints == 0, "empty range never touches an inaccessible key page");
    Check(Run(lpBin, lpKeys + 2, 0, 2) && suHints == 0, "last two records never read the next key");
    Check(Run(lpBin, lpKeys + 3, 0, 1) && suHints == 0, "single remaining record has no lookahead");
    Check(Run(lpBin, lpKeys, 0, 4) && suHints == 6, "valid lookahead emits one packet and two mesh spans");
    const uintptr_t luPacket2 = reinterpret_cast<uintptr_t>(lauPackets + lauOffsets[2] * 16);
    const uintptr_t luMesh1 = reinterpret_cast<uintptr_t>(lauMeshes[1] + 16);
    Check(sauAddresses[0] == luPacket2 && sauAddresses[1] == luPacket2 + 64,
          "two-ahead packet is selected by the sorted record");
    Check(sauAddresses[2] == luMesh1 && sauAddresses[3] == luMesh1 + 64
          && sauAddresses[4] == luMesh1 + 128 && sauAddresses[5] == luMesh1 + 192,
          "next mesh metadata retains its complete host-width address");
    suHints = 0;
    Check(Run(lpBin, lpKeys, 1, 3) && suHints == 0, "partial dispatch end bounds lookahead even when more keys exist");
    Check(Run(lpBin, lpKeys, 1, 4) && suHints == 6
          && sauAddresses[0] == reinterpret_cast<uintptr_t>(lauPackets)
          && sauAddresses[2] == reinterpret_cast<uintptr_t>(lauMeshes[2] + 16),
          "nonzero dispatch start selects its own future packet and mesh");
    suHints = 0;
    Check(Run(lpBin, lpKeys, 4, 4) && suHints == 0, "exhausted range remains inert");
    VirtualFree(lpPages, 0, MEM_RELEASE);
    std::printf("PCMeshPrefetch: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
