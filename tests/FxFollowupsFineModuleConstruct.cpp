// FX-FOLLOWUPS (crash parity 2026-09-25, item 2 step D, commit 1): the fine intersection-test module's
// construction, before it is mounted --
//   rw::collision::VolumeLineQuery::GetResourceDescriptor @0x82BB3838 / Initialize @0x82BB3888 (no host bodies
//     before: the vendor header declared them and nothing defined them)
//   CgsSceneManager::FineIntersectionTestModule::Construct @0x828B0BF0 (the unmounted TU): its VolumeVolumeQuery
//     buffer at the host size, and the VolumeLineQuery descriptor into a whole 5-entry block (it was one u32).
// run_fxfollowups_fine_module_construct.py compiles the revision's CgsFineIntersectionTestModule.cpp, VolumeQuery.cpp
// and VolumeBBoxQuery.cpp (their headers shadowed from the revision) beside this file, with the production
// PrimitiveBatchIntersect pasted in (fxfu_fine.inc) so VolumeQuery.cpp links; nothing here runs a query.
//
// The console, from the ARTIST listings:
//   * VLQ descriptor: 5 x {0, 1}, then entry 0 = {0x1B0 * results + 0x80 * volumes + 0x110 + 0x2880, 16};
//   * VLQ Initialize: null block -> null; else m_stackMax = volumes, m_primBufferSize = m_resBufferSize =
//     m_instVolMax = results, and five carves from base + header: stack VolRefs (0x80 * volumes), primitive
//     VolRefs (0x80 * results), instanced Volumes (0x60 * results), VolumeLineSegIntersectResults (0xD0 *
//     results), then the spatial-map query memory; nothing else is written;
//   * Construct: prepare stage 0, release stage 2, both managers null; VVQ descriptor(100, 100) <= its buffer
//     (else "VolumeVolumeQueryMem is too small"), Initialize({buffer, 0, 0, 0, 0}, 100, 100); VLQ descriptor
//     (100, 100) <= 67584 (else "VolumeLineQueryMem is too small"), Initialize({line buffer, ...}, 100, 100).
// On the host the header is sizeof(VolumeLineQuery) rounded to 16 (0x130; console 0x110) and the VVQ buffer is
// 0x49600 (console 0x49000).
#include "types.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <new>
#include <string>
#include <vector>

// The CgsSceneManager side only: the fine module's header carries the BrnCommonTypes vpu vocabulary, so the
// rwcollision headers with the vendor one (VolumeBBoxQuery.hpp / GPInstance.hpp) live in
// FxFollowupsFineModuleConstructVendor.cpp. VolumeQuery.hpp is the light header both sides share.
#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"
#include "vendor/renderware/collision/VolumeQuery.hpp"

using namespace rw::collision;

// FxFollowupsFineModuleConstructVendor.cpp
bool FxVvqCarveInside(const void* lpQueryHandle, const void* lpBuffer, size_t luBufferSize, u32* lpuDescriptorOut);
void FxVlqStrides(size_t* lpuVolRef, size_t* lpuVolume, size_t* lpuResult);

static std::vector<std::string> gaAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcLabel)
{
    ++guChecks;
    if (!lbPass) ++guFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
}

static int CountAsserts(const char* lpcText)
{
    int liCount = 0;
    for (const std::string& lrMessage : gaAsserts)
        if (lrMessage.find(lpcText) != std::string::npos)
            ++liCount;
    return liCount;
}

int main()
{
    // ============================================================================================================
    // V. VolumeLineQuery's construction entry points
    // ============================================================================================================
    const u32 luHeader = static_cast<u32>((sizeof(VolumeLineQuery) + 15u) & ~static_cast<size_t>(15u));
    {
        u32 lau[12];
        std::memset(lau, 0x5A, sizeof(lau));
        VolumeLineQuery::GetResourceDescriptor(lau, 100, 100);
        std::printf("VLQ descriptor(100, 100) = 0x%X / %u (console 0x10450); host header 0x%X (console 0x110)\n",
                    lau[0], lau[1], luHeader);
        Check(lau[0] == luHeader + 100u * 0x80u + 100u * 0x1B0u + 0x2880u && lau[0] == 0x10470u && lau[1] == 16u
              && lau[2] == 0u && lau[3] == 1u && lau[4] == 0u && lau[5] == 1u && lau[6] == 0u && lau[7] == 1u
              && lau[8] == 0u && lau[9] == 1u && lau[10] == 0x5A5A5A5Au,
              "V1 VolumeLineQuery::GetResourceDescriptor(100, 100) = {header + 0x80*vols + 0x1B0*results + 0x2880, 16} "
              "(host 0x10470), entries 1..4 {0, 1}, nothing past the block");

        VolumeLineQuery::GetResourceDescriptor(lau, 7, 3);
        Check(lau[0] == luHeader + 7u * 0x80u + 3u * 0x1B0u + 0x2880u,
              "V2 VolumeLineQuery::GetResourceDescriptor(7, 3): 0x80 per VOLUME, 0x1B0 per RESULT");
    }
    {
        const u32 luSize = 0x10470u;
        u8* lpBlock = static_cast<u8*>(_aligned_malloc(luSize + 64, 16));
        std::memset(lpBlock, 0xCD, luSize + 64);
        void* lapBuffers[5] = { lpBlock, 0, 0, 0, 0 };
        VolumeLineQuery* lpQuery = static_cast<VolumeLineQuery*>(VolumeLineQuery::Initialize(lapBuffers, 100, 100));
        Check(lpQuery == reinterpret_cast<VolumeLineQuery*>(lpBlock) && lpQuery->m_stackMax == 100u
              && lpQuery->m_primBufferSize == 100u && lpQuery->m_resBufferSize == 100u && lpQuery->m_instVolMax == 100u,
              "V3 VolumeLineQuery::Initialize: the block is the query; stackMax = volumes, prim / res / instVol capacities = results");

        const u8* lpStack = reinterpret_cast<const u8*>(lpQuery->m_stackVRefBuffer);
        const u8* lpPrim  = reinterpret_cast<const u8*>(lpQuery->m_primVRefBuffer);
        const u8* lpInst  = reinterpret_cast<const u8*>(lpQuery->m_instVolPool);
        const u8* lpRes   = reinterpret_cast<const u8*>(lpQuery->m_resBuffer);
        const u8* lpMap   = static_cast<const u8*>(lpQuery->m_spatialMapQueryMem);
        size_t luVolRef = 0, luVolume = 0, luResult = 0;
        FxVlqStrides(&luVolRef, &luVolume, &luResult);
        Check(luVolRef == 0x80 && luVolume == 0x60 && luResult == 0xD0
              && lpStack == lpBlock + luHeader && lpPrim == lpStack + 100 * 0x80 && lpInst == lpPrim + 100 * 0x80
              && lpRes == lpInst + 100 * 0x60 && lpMap == lpRes + 100 * 0xD0 && lpMap + 0x2880 == lpBlock + luSize,
              "V4 VolumeLineQuery::Initialize carves stack VolRefs, primitive VolRefs, Volumes, results, spatial map -- "
              "ending exactly at the descriptor's end");

        u32 luNumInputs, luResCount, luTag;
        std::memcpy(&luNumInputs, &lpQuery->m_numInputs, 4);
        std::memcpy(&luResCount, &lpQuery->m_resCount, 4);
        std::memcpy(&luTag, &lpQuery->m_tag, 4);
        bool lbTailUntouched = true;
        for (u32 li = luHeader; li < luSize + 64; ++li)
            if (lpBlock[li] != 0xCD) { lbTailUntouched = false; break; }
        Check(luNumInputs == 0xCDCDCDCDu && luResCount == 0xCDCDCDCDu && luTag == 0xCDCDCDCDu && lbTailUntouched,
              "V5 VolumeLineQuery::Initialize writes only its nine fields: the rest of the header and every carved byte keep their contents");

        void* lapNull[5] = { nullptr, 0, 0, 0, 0 };
        Check(VolumeLineQuery::Initialize(lapNull, 100, 100) == nullptr,
              "V6 VolumeLineQuery::Initialize({null}) answers null (`lwz r11, 0(r3) ; beq`)");
        _aligned_free(lpBlock);
    }

    // ============================================================================================================
    // F. FineIntersectionTestModule::Construct
    // ============================================================================================================
    {
        using CgsSceneManager::FineIntersectionTestModule;
        FineIntersectionTestModule* lpModule =
            static_cast<FineIntersectionTestModule*>(_aligned_malloc(sizeof(FineIntersectionTestModule), 16));
        std::memset(lpModule, 0xCD, sizeof(FineIntersectionTestModule));
        gaAsserts.clear();
        lpModule->Construct();

        Check(CountAsserts("VolumeVolumeQueryMem is too small") == 0 && CountAsserts("VolumeLineQueryMem is too small") == 0,
              "F1 Construct: both descriptors fit their buffers (no :63 / :73 assert)");
        Check(lpModule->mePrepareStage == FineIntersectionTestModule::E_FINE_INTERSECTION_PREPARE_START
              && lpModule->meReleaseStage == FineIntersectionTestModule::E_FINE_INTERSECTION_RELEASE_DONE
              && lpModule->mpVolumeManager == nullptr && lpModule->mpEntityManager == nullptr,
              "F2 Construct: prepare stage START, release stage DONE, no managers (0x828B0C2C..0x828B0C40)");
        Check(static_cast<void*>(lpModule->mpVolumeVolumeQuery) == lpModule->macVolumeVolumeQueryBuffer
              && static_cast<void*>(lpModule->mpVolumeLineQuery) == lpModule->macVolumeLineQueryBuffer,
              "F3 Construct: each query is built in place at the base of its own buffer");

        u32 luVvqDescriptor = 0;
        const size_t luVvqBuffer = sizeof(lpModule->macVolumeVolumeQueryBuffer);
        const bool lbVvqInside = FxVvqCarveInside(lpModule->mpVolumeVolumeQuery, lpModule->macVolumeVolumeQueryBuffer,
                                                  luVvqBuffer, &luVvqDescriptor);
        std::printf("VVQ descriptor(100, 100) = 0x%X in a 0x%zX buffer\n", luVvqDescriptor, luVvqBuffer);
        Check(lbVvqInside,
              "F4 Construct: the VolumeVolumeQuery's descriptor, sub-queries, result array and instancing scratch all lie inside its buffer");

        const VolumeLineQuery* lpVlq = lpModule->mpVolumeLineQuery;
        Check(static_cast<const u8*>(lpVlq->m_spatialMapQueryMem) + 0x2880
                  <= lpModule->macVolumeLineQueryBuffer + sizeof(lpModule->macVolumeLineQueryBuffer)
              && sizeof(lpModule->macVolumeLineQueryBuffer) == 67584u,
              "F5 Construct: the VolumeLineQuery's carve ends inside the console's 67584-byte buffer");

        Check(offsetof(FineIntersectionTestModule, macVolumeLineQueryBuffer) == luVvqBuffer
              && offsetof(FineIntersectionTestModule, mePrepareStage) == luVvqBuffer + 67584u,
              "F6 the buffers are contiguous and the stages follow them (the console's +0x49000 / +0x59800 contiguity)");
        _aligned_free(lpModule);
    }

    std::printf("FxFollowupsFineModuleConstruct: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
