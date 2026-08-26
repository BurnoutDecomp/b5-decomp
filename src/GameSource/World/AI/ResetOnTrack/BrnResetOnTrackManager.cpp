#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnAI::ResetOnTrackManager out-of-line members: Construct (@0x82791A48) and GetAICar
// (@0x82765878), plus the file-scope static perf-mon handles.

namespace BrnAI
{
    // File-scope static perf-mon handles (DWARF BrnResetOnTrackManager.h:349-351;
    // X360 dword_82F3026C / dword_82F30274 / dword_82F30270).
    s32 ResetOnTrackManager::miInitialCoordinatesPM;
    s32 ResetOnTrackManager::muAvoidHNGPM;
    s32 ResetOnTrackManager::muTestLineHNGPM;

    // Never called; pins the request-queue offset from inside the class (offsetof on a private
    // member is only legal within class scope).
    void ResetOnTrackManager::_AssertLayout()
    {
        static_assert(offsetof(ResetOnTrackManager, mResetOnTrackRequestQueue) == 0x000,
                      "ROT queue offset drift");
    }

    // =============================================================================================
    // Construct @0x82791A48   -- the ONLY construction site of this object in the whole image is
    // BrnAI::AIModule::Prepare @0x82798070 stage 3.
    //
    // Store-for-store against the X360 body (offsets are this-relative, r30 == this):
    //   0x82791A5C..0x82791C58   the mRandom (+0x4F0) prime: seed 0xC87CD8C91AD0891B, slot 0 =
    //                            1.0f, then seven AddRandomFloatToBuffer draws through the
    //                            0x5851F42D4C957F2D LCG, then ++muOldestBufferIndex. That whole
    //                            unrolled blob IS CgsNumeric::Random::Construct inlined -- the
    //                            committed body in CgsRandom.h is the same sequence.
    //   0x82791C5C  stw r31, 0x230(this)   mResetOnTrackRequestQueue.Clear()   (miCount = 0)
    //   0x82791C60..0x82791C70               mRecentResets.Construct(): mpData = this+0x260,
    //                            capacity 8, position/count/... = 0
    //   0x82791C74  bl CreateFromHandle(this+0x360, lAISectionData+0x14)
    //                            == mpAISectionData = lAISectionData (the committed
    //                            ResourcePtr::operator=(const ResourcePtr&) idiom exactly:
    //                            rebind from the SOURCE's {mpThis, muThreadId} pair)
    //   0x82791C7C  stw r28,  0x380(this)  mpaAICars = lpaAICars
    //   0x82791C84  stw r31,  0x388(this)  miResetCount = 0
    //   0x82791C94  stw -1,   0x384(this)  mePlayerGlobalRaceCarIndex = -1
    //   0x82791C80..0x82791CF4               the embedded ResetOnTrackDebugComponent's own
    //                            Construct(owner), inlined: owner @+0xC, two 16-deep ring
    //                            buffers (@+0x10 -> data +0x30, @+0x530 -> data +0x550), a
    //                            7-byte flag block @+0x858..0x85E, two zero words @+0x850/0x854
    //                            and 60 @+0x860
    //   0x82791CF8  bl CgsDev::DebugComponent::Register(this + 0x540)
    //   0x82791CFC..             the three "ROT, ..." PerfMonCpu::AddMonitor registrations
    //   tail                     the by-value lAISectionData parameter's ~ResourcePtr (the
    //                            intrusive-list unlink + self-link) -- emitted by the compiler
    //                            here, not written out.
    //
    // ⚠️ [FLAG PC boot gate] THE DEBUG COMPONENT BLOCK AND ITS Register ARE PARKED. Its interior
    // is `u8 mResetOnTrackDebugComponent[0x870]` -- no named members, so writing its two ring
    // buffers would mean poking raw offsets into an opaque blob, and Register() links the object
    // into the global debug list where the debug UI walks it every frame. Constructing it by
    // offset arithmetic and then publishing it is exactly [[valid-pointer-invalid-object]]. It is
    // pure debug surface; nothing on the reset-on-track path reads it. Restore it WITH the
    // component's own named layout.
    // ⚠️ [FLAG PC boot gate] the three PerfMonCpu::AddMonitor calls are parked with it -- their
    // handles are only read by the parked bodies (ComputeInitialCoordinates / AvoidObstacles /
    // TestLineHNG), and registering a monitor nothing starts or stops just adds a permanent
    // empty row to the profiler HUD. The static handles keep their -1-less default (0) exactly as
    // the console's .bss does before registration.
    // =============================================================================================
    void ResetOnTrackManager::Construct(CgsResource::ResourcePtr<AISectionsData> lAISectionData,
                                        AICar* lpaAICars)
    {
        mRandom.Construct();

        mResetOnTrackRequestQueue.Clear();
        mRecentResets.Construct();

        mpAISectionData = lAISectionData;

        mpaAICars                  = lpaAICars;
        mePlayerGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(-1);
        miResetCount               = 0;

        // [FLAG PC boot gate] the ResetOnTrackDebugComponent Construct + Register and the three
        // "ROT, ..." perf monitors -- see the banner.
    }

    // X360 0x82765878. Private helper; called by 17 sites (PlayerIsLookingBackwards,
    // ComputeInitialCoordinatesStandard, ResetAwayFromPlayer, ...).
    //
    // Two range asserts (E_GLOBAL_RACE_CAR_INDEX_0 <= index < E_GLOBAL_RACE_CAR_INDEX_COUNT ==
    // 35), then return &mpaAICars[index]: the X360 forms 0x1560*index + mpaAICars
    // (sizeof(AICar) == 0x1560 == 5472). AICar is opaque here (its full layout is another TU's),
    // so the element address is computed by the X360-attested byte stride rather than by
    // pointer subscript on the incomplete type -- the result is the same &mpaAICars[index].
    //
    // ⛔⛔ THE CONSOLE STRIDE IS ONLY CORRECT AGAINST A CONSOLE-LAID-OUT AICar ARRAY. Nothing in
    // this build supplies one yet (AIModule::Prepare passes a null array -- see the FLAG there),
    // so every caller of this helper is still parked. When the AI-car array lands as a real host
    // member, this MUST become `&mpaAICars[index]` on the host type: an x64 AICar is not 0x1560
    // bytes, and keeping the console constant would walk the array at the wrong pitch --
    // silently, since every address it produces is still inside the allocation.
    AICar* ResetOnTrackManager::GetAICar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        static const u32 KU_AI_CAR_STRIDE = 0x1560;  // sizeof(AICar) == 5472 (X360-attested)
        return reinterpret_cast<AICar*>(
            reinterpret_cast<u8*>(mpaAICars) + KU_AI_CAR_STRIDE * static_cast<u32>(leGlobalRaceCarIndex));
    }
}
