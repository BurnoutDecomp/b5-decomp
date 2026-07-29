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
// incrementally from BURNOUT_X360_ARTIST.XEX; member names cross-checked against the
// DecFIGS DWARF (GameSource/World/EnvironmentManager/BrnEnvironmentManager.h). ONLY
// members provably touched by a reconstructed function are named, at their asm-attested
// offsets, with everything else left as explicit padding rather than fabricated.
//
// SKY WAVE 2026-07-29 -- header-policy promotion. The class was previously an opaque
// padding stub that could not carry a by-name body (the ledger flagged exactly this as
// the blocker for the whole 12-function TU). It is now named out to 0x1C70 from the
// seven X360 bodies that touch it: Construct @0x827CA408, Prepare @0x827D49A8,
// Update @0x827D6060, GenerateShaderConstants @0x827D0098, GenerateEffects @0x827BE698,
// Enable/DisableJunkyardLightingSetup @0x827B0F98 / @0x827B10E8, plus the already-
// committed PerformBlend / SetupUpdateFromToolBlend / DiscardCurrSeason / UpdateFromTool.
//
// Three prior names were WRONG and are corrected here (Construct's stores decide it):
//     0x444 muCurrSeasonRef  -> meStreamOutStage    (Construct writes 7 = E_STREAMOUT_DONE)
//     0x448 mbCurrSeason     -> muStreamOutTarget
//     0x4DC muDiscardSeason  -> miCurrSeason
//     0x4F8 miBlendState     -> meBlendMode         (Update compares == 2 = E_BLENDMODE_PAUSED)
//     0x500 miSavedBlendState-> meBlendModePaused
//     0x504 mfCurrTimeOfDay  -> mfTimeOfDay
// Regions still opaque: the CgsModule receiver queue body, CgsNumeric::Random's internals,
// the per-season / dictionary / colour-cube ResourcePtrs, the AsyncOp, and the
// GlobalIrradianceManager (its own header owns that; only its 0x160 extent is pinned here,
// by mGlobalIrradianceManager@0x700 vs miToolUpdateFrameCounter@0x860).
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

        // ---- ADDITIVE (attested by WorldModule::Update @0x827D63E8) ----
        // The per-frame environment tick: (player speed, the update output the
        // fog/lighting outputs land in, the frame camera position). DWARF
        // BrnEnvironmentManager.h:386; body gated in WorldLinkStubs.cpp until
        // this module's own TU lands.
        void Update( f32 lfPlayerSpeed, BrnWorldIO::UpdateOutputBuffer* lpOutput,
                     Vector3 lCameraPosition );

        // ---- ADDITIVE (WorldModule pre-scene spine @0x827BD1F0: the time-of-day
        //      copy into the traffic pre-scene input reads +0x504 directly) ----
        f32 GetCurrTimeOfDay() const { return mfTimeOfDay; }

        // Inlined on the X360 to a single `stfs f, 0x504(this)`; spelled out here because
        // Update / Enable / DisableJunkyardLightingSetup all go through it.
        void SetTimeOfDay_Seconds( f32 lfSeconds ) { mfTimeOfDay = lfSeconds; }

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

    // ---- One junkyard lighting setup: a world position and the key-light direction to
    //      force while the camera is nearest to it. Stride 0x20, attested by
    //      EnableJunkyardLightingSetup @0x827B0F98 (`addi r11,r31,0x1A50` then
    //      `lvx v0, r11, -0x10` -> position @+0x00, direction @+0x10).
    struct JunkyardLighting
    {
        Vector3 mJunkyardPosition;      // +0x00
        Vector3 mKeyLightDirection;     // +0x10
    };

    // The most setups ReadJunkyardLightingData will load (the 0x1A40..0x1C40 span / 0x20).
    enum { KU_MAX_JUNKYARD_LIGHTING_SETUPS = 16 };

    // --- named members proven by the stage machines (Prepare @0x827D49A8 switches on
    //     mePrepareStage; WorldModule::Release @0x827BCE58 stage 8 pokes the pair;
    //     both names are the IDA-applied struct names in the Prepare body) ---
    s32            mePrepareStage;              // 0x000  staged-prepare cursor (0 = START)
    s32            meReleaseStage;              // 0x004  release latch (1 = RELEASING)
    // 0x008 mReceiverQueue -- a CgsModule EventReceiverQueue<1024,16>. Construct seeds
    // +0x08 = &mabReceiverQueueBuffer[0], +0x10 = 1024 (capacity), +0x14 = 16 (stride)
    // and then Clears it. Only those three words are attested, so the queue is modelled
    // as its head words plus the raw buffer rather than a fabricated class.
    void*          mpReceiverQueueBuffer;       // 0x008  = &maReceiverQueueStorage[0]
    u8             mPad00C[4];                  // 0x00C
    u32            muReceiverQueueCapacity;     // 0x010  = 1024
    u32            muReceiverQueueStride;       // 0x014  = 16
    u8             mPad018[8];                  // 0x018
    u8             maReceiverQueueStorage[0x400];   // 0x020
    s32            miResourceCnt;               // 0x420  outstanding resource requests Prepare waits on
    u8             maDictionaryPtr[0x20];       // 0x424  ResourcePtr<Dictionary> (target type un-homed)
    // --- named members proven by DiscardCurrSeason (0x827B0E50) / UpdateFromTool (0x827B0DA8) ---
    // NOTE: 0x444 / 0x448 were previously mis-named muCurrSeasonRef / mbCurrSeason. Construct
    // @0x827CA408 writes 7 (== E_STREAMOUT_DONE, the mirror of meStreamInStage) into 0x444 and 0
    // into 0x448, so the pair is the stream-OUT half of the season swap. DiscardCurrSeason
    // therefore reads as "begin streaming the current season out", which is exactly its name.
    s32            meStreamOutStage;            // 0x444  0 = START (set by DiscardCurrSeason), 7 = DONE
    u8             muStreamOutTarget;           // 0x448  season index being streamed out
    u8             mPad449[3];                  // 0x449
    EStreamInStage meStreamInStage;             // 0x44C  asserted == E_STREAMIN_DONE
    u8             muStreamInTarget;            // 0x450  Prepare: = (u8)miCurrSeason
    u8             mPad451[0xF];                // 0x451
    u8             maRandom[0x2C];              // 0x460  CgsNumeric::Random (8-float ring + u64 seed + index)
    u8             mPad48C[4];                  // 0x48C
    u8             maSeasonPtrs[2][0x20];       // 0x490  ResourcePtr<Keyframe> per season
    s32            maiSeasons[2];               // 0x4D0  Construct: { -1, 0 }
    s32            miSeasonCurrentlyPlaying;    // 0x4D8
    s32            miCurrSeason;                // 0x4DC  (was muDiscardSeason) -> muStreamOutTarget/muStreamInTarget
    f32            mfSeasonBlend;               // 0x4E0
    f32            mfSeasonBlendDelta;          // 0x4E4
    s32            maiLocations[2];             // 0x4E8
    s32            miCurrLocation;              // 0x4F0
    f32            mfLocationBlend;             // 0x4F4
    s32            meBlendMode;                 // 0x4F8  (was miBlendState) 2 = E_BLENDMODE_PAUSED
    u8             mPad4FC[4];                  // 0x4FC
    s32            meBlendModePaused;           // 0x500  (was miSavedBlendState) meBlendMode saved across a tool blocking op
    // --- named members proven by PerformBlend (0x827B0EB8) / SetupUpdateFromToolBlend (0x827C5018) ---
    f32            mfTimeOfDay;                 // 0x504  current time-of-day, SECONDS of day
    f32            mfTimeOfDayDelta;            // 0x508
    f32            mfCloudDelta;                // 0x50C
    f32            mfSunRigRotation;            // 0x510  degrees
    f32            mfSunTiltAtHorizon;          // 0x514  degrees
    f32            mfSunTiltAtMidday;           // 0x518  degrees
    s32            meSetupSeasonsBlendStage;    // 0x51C  Construct: 4 (E_SETUPSEASONSBLEND_DONE)
    BlendFrame     mBlendFrame;                 // 0x520  the live 4-keyframe blend frame
    ScatteringData mScattering;                 // 0x540  blended scattering target (PerformBlend dst @+0x540)
    u8             mPad5E8[0x8];                // 0x5E8
    LightingData   mLighting;                   // 0x5F0  blended lighting target   (PerformBlend dst @+0x5F0)
    u8             mPad674[0xC];                // 0x674
    CloudsData     mClouds;                     // 0x680  blended clouds target     (PerformBlend dst @+0x680)
    u8             mPad6EC[4];                  // 0x6EC
    f32            mfCloudDistanceCurve;        // 0x6F0  Construct: 1.0f
    bool           mbSetScattColsFromSky;       // 0x6F4
    bool           mbSetIrradianceFromSky;      // 0x6F5
    u8             mPad6F6[0xA];                // 0x6F6
    u8             maGlobalIrradianceManager[0x160];  // 0x700  GlobalIrradianceManager (3 matrices @+0x00/0x40/0x80,
                                               //        average colour @+0xC0, frame coeffs @+0xD0)
    s32            miToolUpdateFrameCounter;    // 0x860  reset by UpdateFromTool; 30-frame gate in SetupUpdateFromToolBlend
    u8             mPad864[0xC];                // 0x864
    Keyframe       maToolKeyframes[4];          // 0x870  four tool-parsed keyframe slots (4 * 0x240)
    // 0x1170: an unnamed bool -- Construct clears it, GenerateEffects @0x827BE698 gates all four
    // of its per-frame copies on it (false = take the blended keyframe, true = take the defaults).
    // FLAG: no DWARF member exists at this offset, so the NAME is not recovered; the behaviour is.
    bool           mbUseDefaultEffects;         // 0x1170
    u8             mPad1171[3];                 // 0x1171
    u8             maDefColourCubePtr[0x20];    // 0x1174  ResourcePtr<ColourCube>
    u32            muDefTintData;               // 0x1194  Keyframe::TintData used when mbUseDefaultEffects
    bool           mbOverrideSeason;            // 0x1198  debug var "Override season"
    u8             mPad1199[3];                 // 0x1199
    s32            miOverrideCurrentSeason;     // 0x119C
    s32            miOverrideNextSeason;        // 0x11A0  debug var "Season to use"   [1,99]
    s32            miOverrideKeyframe;          // 0x11A4  debug var "Keyframe to use" [1,99]
    u8             mPad11A8[8];                 // 0x11A8
    Vector3        mCloud0Disp;                 // 0x11B0  accumulated cloud-layer-0 UV displacement
    f32            mfWhiteLevel;                // 0x11C0  debug var "HDR white level" [0.025, 1.0]
    f32            mrTimeStep;                  // 0x11C4
    u8             maAsyncOp[0x48];             // 0x11C8  CgsFileSys AsyncOp
    u8             mPad1210_[0x28];             // 0x1210  (incl. mpFileHandle @0x1210)
    Vector3        mOverrideKeyLightDirection;  // 0x1220
    f32            mfTimeBeforeEnteringJunkyard;// 0x1230
    bool           mbOverrideKeyLightDirection; // 0x1234
    bool           mbJunkyardLightingSetup;     // 0x1235
    u8             mPad1236[0xA];               // 0x1236
    char           macJunkyardLightingBuffer[0x800];  // 0x1240  the raw junkyardlighting.dat text
    JunkyardLighting maJunkyardLighting[KU_MAX_JUNKYARD_LIGHTING_SETUPS];  // 0x1A40
    u32            muNumJunkyardLightingSetupsLoaded;  // 0x1C40
    u8             mPad1C44[0xC];               // 0x1C44
    Vector3        mCurrentCameraPosition;      // 0x1C50  written every Update
    f32            mfTimeOfDayUpperBound;       // 0x1C60
    f32            mfTimeOfDayLowerBound;       // 0x1C64
    f32            mfSunElevTodLBound;          // 0x1C68  sun-elevation time-of-day clamp floor
    f32            mfSunElevTodUBound;          // 0x1C6C  sun-elevation time-of-day clamp cap
};
}
}
