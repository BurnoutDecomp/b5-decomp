#pragma once

#include "BrnCommonTypes.h"   // Vector3
#include "types.hpp"

#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<Keyframe>

namespace BrnWorldIO { struct UpdateOutputBuffer; }
class BrnEffectsFrame;

namespace BrnWorld
{
namespace EnvironmentSettings
{
// EnvironmentManager -- the environment-settings blend/update manager. Reconstructed
// incrementally from BURNOUT_X360_ARTIST.XEX. The class is large (the true object
// extends past this partial model); ONLY members provably touched by the functions
// reconstructed so far are named, at their asm-attested offsets, with everything
// else left as explicit padding rather than fabricated. Members added by this wave
// (mfCurrTimeOfDay@0x504, the blended mScattering@0x540 / mLighting@0x5F0 /
// mClouds@0x680 targets, and maToolKeyframes[4]@0x870) are pinned to the offsets
// PerformBlend / SetupUpdateFromToolBlend read/write. The +0x424 ResourcePtr, the
// +0x490 per-season ResourcePtr array, the +0x1174 file-request queue and +0x11C8
// AsyncOp regions named in prior comments remain opaque padding (their asm is not
// in this batch's dossier).
class EnvironmentManager
{
public:
        // ---- ADDITIVE (attested by WorldModule::Construct @0x827CF540, which
        //      virtual-dispatches the fleet lifecycle) ----
        // Declaration-only; the body lands with this module's own TU.
        void Construct();

        // ---- ADDITIVE (attested by WorldModule::Prepare @0x827D53B0 stage 5) ----
        // Declaration-only; the body lands with this module's own TU.
        bool Prepare( BrnWorldIO::UpdateOutputBuffer* lpOutput );

        // ADDITIVE (WorldModule::Release @0x827BCE58 stage 8 pokes the manager's
        // leading prepare/release stage pair to {START, RELEASING} -- the inlined
        // release request: `a1[498648] = 0; a1[498649] = 1;` on the embedded manager
        // at WorldModule+1994592). BODIED in BrnEnvironmentManager.cpp (destub wave
        // 2026-07-26); the member pair is IDA-struct-named in the Prepare body
        // (mePrepareStage / meReleaseStage).
        void BeginRelease();

        // ADDITIVE (WorldModule::GenerateFrustumQueries @0x827DADF8 feeds this to
        // ShadowMap::CalculateShadowMapCameras). Declaration-only.
        Vector3 CalcKeyLightDirection() const;

        // ---- ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
        // Declaration-only; bodies with the env-manager TU.
        void EnableJunkyardLightingSetup();
        void DisableJunkyardLightingSetup();
        void GenerateEffects( BrnEffectsFrame* lpFrame0, BrnEffectsFrame* lpFrame1,
                              BrnEffectsFrame* lpFrame2, BrnEffectsFrame* lpFrame3 );

    // A blend frame: four source keyframes and their four blend weights. Built by
    // SetupTimeOfDayBlend / SetupSeasonsBlend / SetupUpdateFromToolBlend and consumed
    // by PerformBlend. Layout attested by SetupUpdateFromToolBlend's fill loop
    // (pointers @+0x00.., weights @+0x10..) and PerformBlend's reads.
    struct BlendFrame
    {
        Keyframe* mapKeyframes[4];   // 0x00  four bracketing source keyframes
        float     mafWeights[4];     // 0x10  their blend weights (sum to 1)
    };

    // Blend / pause state-machine transition driven by the environment tool. Returns
    // whether the manager is (or has just been put) in a blocking operation.
    // @ 0x827B0DA8
    bool UpdateFromTool(bool lbPause);

    // Commit the pending season swap and clear the current-season ref once the season
    // stream-in has completed. @ 0x827B0E50
    void DiscardCurrSeason();

    // Stream-in stage of the season resource. Only the terminal E_STREAMIN_DONE value is
    // attested here (the DiscardCurrSeason assert "meStreamInStage == E_STREAMIN_DONE");
    // the earlier streaming stages exist but are not exercised by this TU.
    enum EStreamInStage
    {
        E_STREAMIN_DONE = 7,
    };

private:
    // @ 0x827C30E8. Out-of-line copy of ResourcePtr<Keyframe>::GetMemoryResource()'s
    // null-resource assert (CgsResourcePtr.h line 581); returns the memory resource.
    Keyframe* ResourcePointerAssertThingy(CgsResource::ResourcePtr<Keyframe>& lrResourcePtr);

    // @ 0x827B0EB8. 4-way weighted blend of the four source keyframes' scattering /
    // lighting / clouds sub-blocks into the manager's current-environment targets.
    void PerformBlend(BlendFrame& lrBlendFrame);

    // @ 0x827C5018. Tool path: re-read d:\LightSetup.txt once every 30 frames, then set
    // the blend frame to a uniform 0.25-weight blend of four copies of the parsed
    // keyframe. Returns whether the file was re-read this call.
    bool SetupUpdateFromToolBlend(BlendFrame& lrBlendFrame);

    // --- named members proven by the stage machines (Prepare @0x827D49A8 switches on
    //     mePrepareStage; WorldModule::Release @0x827BCE58 stage 8 pokes the pair;
    //     both names are the IDA-applied struct names in the Prepare body) ---
    s32            mePrepareStage;              // 0x000  staged-prepare cursor (0 = START)
    s32            meReleaseStage;              // 0x004  release latch (1 = RELEASING)
    u8             mPad8[0x43C];                // 0x008  (incl. the +0x424 ResourcePtr region, un-homed)
    // --- named members proven by DiscardCurrSeason (0x827B0E50) / UpdateFromTool (0x827B0DA8) ---
    u32            muCurrSeasonRef;             // 0x444  cleared to 0 by DiscardCurrSeason
    u8             mbCurrSeason;                // 0x448  (byte) set from muDiscardSeason low byte
    u8             mPad449[3];                  // 0x449
    EStreamInStage meStreamInStage;             // 0x44C  asserted == E_STREAMIN_DONE
    u8             mPad450[0x8C];               // 0x450  (incl. the +0x490 per-season ResourcePtr array, un-homed)
    u32            muDiscardSeason;             // 0x4DC  season index copied into mbCurrSeason
    u8             mPad4E0[0x18];               // 0x4E0
    s32            miBlendState;                // 0x4F8  blend/pause state machine (0..3)
    u8             mPad4FC[4];                  // 0x4FC
    s32            miSavedBlendState;           // 0x500  miBlendState saved across a tool blocking op
    // --- named members proven by PerformBlend (0x827B0EB8) / SetupUpdateFromToolBlend (0x827C5018) ---
    f32            mfCurrTimeOfDay;             // 0x504  current time-of-day (parser out-param / sun-dir input)
    u8             mPad508[0x38];               // 0x508
    ScatteringData mScattering;                 // 0x540  blended scattering target (PerformBlend dst @+0x540)
    u8             mPad5E8[0x8];                // 0x5E8
    LightingData   mLighting;                   // 0x5F0  blended lighting target   (PerformBlend dst @+0x5F0)
    u8             mPad674[0xC];                // 0x674
    CloudsData     mClouds;                     // 0x680  blended clouds target     (PerformBlend dst @+0x680)
    u8             mPad6EC[0x174];              // 0x6EC
    s32            miToolUpdateFrameCounter;    // 0x860  reset by UpdateFromTool; 30-frame gate in SetupUpdateFromToolBlend
    u8             mPad864[0xC];                // 0x864
    Keyframe       maToolKeyframes[4];          // 0x870  four tool-parsed keyframe slots (4 * 0x240)
    u8             mPad1170[0x78];              // 0x1170 (incl. the +0x1174 file-request queue + +0x11C8 AsyncOp, un-homed)
};
}
}
