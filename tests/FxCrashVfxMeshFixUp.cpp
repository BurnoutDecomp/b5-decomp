// FX-CRASHVFX C3 (crash parity 2026-09-25, CC-15): THE DEBRIS MESH COLLECTION'S FIXUP -- BrnParticle::
// BrnVFXMeshCollectionResourceType::FixUp @0x82678490 over the PORTED PARTICLES.BUNDLE collections, and its FLAG PC
// platform leaf for a bundle converted before the port.
//
// run_fxcrashvfx_mesh_fixup.py hands this fixture the PRODUCTION FixUp (with the constants and dword enums of its
// TU and, when the revision has them, IsUnconvertedPC / ReportUnconvertedPC), extracted from the revision's
// BrnVFXMeshCollectionResourceType.cpp and compiled as members of a class with that name. The fixture supplies the
// two Xbox2CheckPhysicalMemoryFlags statics (recording their argument), the asserts (counted) and the log (captured),
// and places each header at the console run's own load address (0x20000000) so absolute words compare as they are.
//
// The expected bytes are the CONSOLE'S: FxCrashVfxMeshFixUpData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_meshfixup_data.py, which runs 0x82678490 on emu64 over the three X360
// headers with the console's rw::Resource (4-byte lanes: 0 = the header, 2 = the body, 1 = a decoy the console never
// reads) and flips back to little-endian exactly the words tools/assets/bundles/particles_transcode.py ports. Per mesh,
// 4 checks: (1) the ported header after FixUp equals the console's, byte for byte; (2) the vertex then index buffer
// checks got the console's buffers, with the console's assert count; (3) the X360 (big-endian) header is REFUSED --
// untouched, no assert, no fault; (4) IsUnconvertedPC says big-endian for the X360 header and not for the ported one,
// before and after FixUp. Plus one: the refusal said so exactly once, naming the bundle and the command.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/rwcore_structs.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/IndexBuffer.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "fxcrashvfx_meshfixup_config.inc"   // FXMF_HAS_IS_UNCONVERTED (generated)
#include "FxCrashVfxMeshFixUpData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;
static std::vector<std::string> gLog;
static std::vector<std::pair<char, uintptr_t>> gBufferChecks;

static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lrLabel.c_str());
    }
    else
    {
        std::printf("pass  %.110s\n", lrLabel.c_str());
    }
}

static std::string Str(const char* lpcFormat, ...)
{
    char lac[512];
    va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lac, sizeof(lac), lpcFormat, lArgs);
    va_end(lArgs);
    return lac;
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char* lpcText) { gLog.push_back(lpcText ? lpcText : ""); }
}
}

namespace renderengine
{
    u32 VertexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords)
    {
        gBufferChecks.push_back(std::make_pair('v', reinterpret_cast<uintptr_t>(lpHeaderDwords)));
        return 0;
    }
    u32 IndexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords)
    {
        gBufferChecks.push_back(std::make_pair('i', reinterpret_cast<uintptr_t>(lpHeaderDwords)));
        return 0;
    }
}

namespace BrnParticle
{
    class BrnVFXMeshCollectionResourceType
    {
    public:
        void FixUp(void* lpResource, const rw::Resource& lrResource) const;
#if FXMF_HAS_IS_UNCONVERTED
        static bool IsUnconvertedPC(const void* lpResource);
#endif
    };

#include "fxcrashvfx_meshfixup_body.inc"   // the PRODUCTION constants, enums, [IsUnconvertedPC], FixUp
}

using BrnParticle::BrnVFXMeshCollectionResourceType;

// A fault (the pre-port FixUp following a byte-reversed offset) is caught and reported, not fatal.
static int FixUpGuarded(const BrnVFXMeshCollectionResourceType* lpType, void* lpHeader, const rw::Resource* lpResource)
{
    __try
    {
        lpType->FixUp(lpHeader, *lpResource);
        return 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 1;
    }
}

static bool Unconverted(const void* lpHeader)
{
#if FXMF_HAS_IS_UNCONVERTED
    return BrnVFXMeshCollectionResourceType::IsUnconvertedPC(lpHeader);
#else
    (void)lpHeader;
    return false;
#endif
}

int main()
{
    // The console run's own load addresses: the header at kuMfHeaderBase, the body lane kuMfBodyBase (a number to
    // FixUp: it never reads the body).
    void* lpRegion = VirtualAlloc(reinterpret_cast<void*>(static_cast<uintptr_t>(kuMfHeaderBase)), 0x200000,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (lpRegion != reinterpret_cast<void*>(static_cast<uintptr_t>(kuMfHeaderBase)))
    {
        std::printf("FxCrashVfxMeshFixUp: cannot place the header at 0x%08X\n", kuMfHeaderBase);
        return 2;
    }
    unsigned char* const lpHeader = static_cast<unsigned char*>(lpRegion);
    rw::Resource lResource;
    lResource.m_baseResources[0] = reinterpret_cast<void*>(static_cast<uintptr_t>(kuMfHeaderBase));
    lResource.m_baseResources[1] = reinterpret_cast<void*>(static_cast<uintptr_t>(kuMfDecoyLane));
    lResource.m_baseResources[2] = reinterpret_cast<void*>(static_cast<uintptr_t>(kuMfBodyBase));
    BrnVFXMeshCollectionResourceType lType;

    for (int m = 0; m < kiMfMeshCount; ++m)
    {
        const MfMesh& lrMesh = kaMfMeshes[m];
        const std::string lTag = Str("mesh %s", lrMesh.mpcId);

        // (1) + (2): the ported header, fixed up, against the console's fixed-up header.
        std::memset(lpHeader, 0, 0x1000);
        std::memcpy(lpHeader, lrMesh.mpPorted, lrMesh.miBytes);
        const bool lbPortedBefore = Unconverted(lpHeader);
        gBufferChecks.clear();
        const unsigned luAssertsBefore = gAsserts;
        const int liFault = FixUpGuarded(&lType, lpHeader, &lResource);
        const int liAsserts = static_cast<int>(gAsserts - luAssertsBefore);
        int liFirstDiff = -1;
        for (int b = 0; b < lrMesh.miBytes && liFirstDiff < 0; ++b)
            if (lpHeader[b] != lrMesh.mpExpected[b])
                liFirstDiff = b;
        u32 luGot = 0, luWant = 0;
        if (liFirstDiff >= 0)
        {
            std::memcpy(&luGot, lpHeader + (liFirstDiff & ~3), 4);
            std::memcpy(&luWant, lrMesh.mpExpected + (liFirstDiff & ~3), 4);
        }
        Check(liFault == 0 && liFirstDiff < 0,
              lTag + Str(": FixUp over the ported header = the console's over its own, byte for byte (fault %d, first "
                         "difference at +0x%X: 0x%08X, console 0x%08X)", liFault, liFirstDiff, luGot, luWant));
        const bool lbChecks = gBufferChecks.size() == 2 && gBufferChecks[0].first == 'v' && gBufferChecks[1].first == 'i'
                              && gBufferChecks[0].second == kuMfHeaderBase + lrMesh.muVertexCheckOffset
                              && gBufferChecks[1].second == kuMfHeaderBase + lrMesh.muIndexCheckOffset;
        Check(lbChecks && liAsserts == lrMesh.miAsserts,
              lTag + Str(": the vertex then index buffer checks got the console's buffers (+0x%X / +0x%X, %d calls), "
                         "asserts %d / %d", lrMesh.muVertexCheckOffset, lrMesh.muIndexCheckOffset,
                         static_cast<int>(gBufferChecks.size()), liAsserts, lrMesh.miAsserts));
        const bool lbPortedAfter = Unconverted(lpHeader);

        // (3): the X360 header as a pre-port bundle carries it -- refused, untouched.
        std::memset(lpHeader, 0, 0x1000);
        std::memcpy(lpHeader, lrMesh.mpConsole, lrMesh.miBytes);
        const bool lbConsoleUnconverted = Unconverted(lpHeader);
        const unsigned luStaleAsserts = gAsserts;
        const int liStaleFault = FixUpGuarded(&lType, lpHeader, &lResource);
        const bool lbUntouched = std::memcmp(lpHeader, lrMesh.mpConsole, lrMesh.miBytes) == 0;
        Check(liStaleFault == 0 && lbUntouched && gAsserts == luStaleAsserts,
              lTag + Str(": the X360 (big-endian) header is refused -- fault %d, untouched %d, asserts %u",
                         liStaleFault, lbUntouched ? 1 : 0, gAsserts - luStaleAsserts));

        // (4): the predicate itself.
        Check(lbConsoleUnconverted && !lbPortedBefore && !lbPortedAfter,
              lTag + Str(": IsUnconvertedPC -- X360 header %d, ported %d, ported after FixUp %d",
                         lbConsoleUnconverted ? 1 : 0, lbPortedBefore ? 1 : 0, lbPortedAfter ? 1 : 0));
    }

    int liLines = 0;
    for (const std::string& lrLine : gLog)
        if (lrLine.find("PARTICLES.BUNDLE") != std::string::npos
            && lrLine.find("--only PARTICLES.BUNDLE") != std::string::npos)
            ++liLines;
    Check(liLines == 1 && gLog.size() == 1,
          Str("the refusal said so exactly once, naming the bundle and the command (%d matching of %d lines)", liLines,
              static_cast<int>(gLog.size())));

    std::printf("FxCrashVfxMeshFixUp: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
