#pragma once

#include "BrnCommonTypes.h"   // Vector3
#include "types.hpp"

#include "GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvLightingData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvCloudsData.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentKeyframe.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"   // CgsResource::ResourcePtr<Keyframe>
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                // CgsNumeric::Random
#include "GameSource/World/EnvironmentManager/BrnGlobalIrradianceManager.h"  // embedded BY VALUE @0x700

namespace BrnWorldIO { struct UpdateOutputBuffer; }
class BrnEffectsFrame;

// The two IN-PLACE environment resources the manager streams (DWARF
// SharedClasses/World/BrnEnvironment{Dictionary,TimeLine}.h). Pointer-only use here --
// the complete types live in their own headers (owned by this wave's `envdata` group)
// and are included by BrnEnvironmentManager.cpp, which walks them by name.
namespace BrnWorld { namespace EnvironmentSettings { struct Dictionary; struct TimeLine; } }
namespace rw { namespace graphics { namespace postfx { class ColourCube; } } }

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

private:
        // ---- the STREAMING half (envstream, wave 2026-08-16). All four are PRIVATE in the
        //      DecFIGS DWARF (BrnEnvironmentManager.h:179 / :183 / :189 / :243); Prepare
        //      @0x827D49A8 and SetupTimeOfDayBlend / SetupSeasonsBlend are their only callers.
        // @0x827D2EB0 -- unload the season bundle + its colour cubes, then request the next
        // season. Returns true only in E_STREAMOUT_DONE.
        bool StreamOut( BrnWorldIO::UpdateOutputBuffer* lpOutput );
        // @0x827D31E8 -- load the season's colour-cube bundle + season bundle, then acquire the
        // season TimeLine resource into maSeasonPtrs[ muStreamInTarget ].
        bool StreamIn( BrnWorldIO::UpdateOutputBuffer* lpOutput );
        // @0x827C4EA0 -- pick the season index the NEXT stream-in will use.
        void RequestNextSeason();
        // @0x827BEBD8 -- parse ENVIRONMENTSETTINGS/JUNKYARDLIGHTING.DAT's
        // "{pos} {dir}" text into maJunkyardLighting[] / muNumJunkyardLightingSetupsLoaded.
        void ReadJunkyardLightingData( const char* lpcBuffer, u32 luSize );
public:

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

        // ADDITIVE (WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 reads it
        // for BrnShaderConstantsFrame::SetCloudDistanceCurve). The console reads the member
        // DIRECTLY -- `lfsx f30, r23, 0x1E7650` @0x827D1B7C, and WorldModule+0x1E7650 ==
        // mEnvironmentManager (WorldModule+0x1E6F60) + 0x6F0 == mfCloudDistanceCurve
        // (:211, Construct seeds 1.0f) -- so on the console this is either a public member
        // or a fully inlined getter; the recon header keeps the member private, so the read
        // goes through a named accessor rather than a friend declaration.
        f32 GetCloudDistanceCurve() const { return mfCloudDistanceCurve; }

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

        // ---- ADDITIVE (WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410) ----
        // @ 0x827D0098. Produce the world's environment shader constants from the CURRENT
        // (already blended) environment data. DWARF BrnEnvironmentManager.h:151 / :398; the
        // parameter NAMES are the DWARF definition's (dwarfdump .../BrnEnvironmentManager.cpp:210).
        // NOT const: it writes mScattering (the mbSetScattColsFromSky arm), mLighting (the
        // mbSetIrradianceFromSky arm) and mGlobalIrradianceManager (ComputeIrradiance).
        //
        // THE 26 OUT-PARAMETERS, in ABI order. "slot" is where the reference arrives: r4..r10 for
        // the first seven, then one 8-byte parameter-save slot each from entrySP+0x50 up (the
        // 32-bit reference sits in the LOW word, +4, big-endian) -- the callee reads them back with
        // `lwz rX, entrySP+0x54+8*(N-8)`, ending at +0xE4 for #26. "caller" is where
        // SetupShaderConstantsBeforeRendering @0x827D1410 puts the value: SCT(n) =
        // ShaderConstantTable slot n, FRAME+x = BrnShaderConstantsFrame+x, DOB = DispatchOutputBuffer.
        //
        //   #  type       name                            slot      caller
        //   1  Vector4&   lOutputSkyTopColourDrk          r4        FRAME+0x2B0
        //   2  Vector4&   lOutputSkyHorColourPow          r5        SCT(33) SkyReflectionColour, FRAME+0x2C0
        //   3  Vector4&   lOutputSkySunColourPow          r6        FRAME+0x2D0
        //   4  Vector3&   lOutputSkyHorBleedSclPow        r7        FRAME+0x2E0
        //   5  Vector4&   lOutputScattTopColourDrk        r8        (dropped by this caller)
        //   6  Vector4&   lOutputScattHorColourPow        r9        SCT(28) FogColourPlusWhiteLevel
        //                                                           (.xyz + whiteLevel in .w), DOB::SetFogColourPlusWhiteLevel
        //   7  Vector4&   lOutputScattSunColourPow        r10       (dropped by this caller)
        //   8  Vector3&   lOutputScattHorBleedSclPow      sp+0x54   (dropped by this caller)
        //   9  Vector4&   lOutputScattCoeffs              sp+0x5C   SCT(27) ScattCoeffs, DOB::SetFogScattering
        //  10  Vector3&   lOutputKeyLightDirection        sp+0x64   SCT(10), FRAME+0x220, DOB::SetKeyLightDirection
        //  11  Vector3&   lOutputKeyLightColour           sp+0x6C   SCT(9), SCT(12) = min(max(c,0),whiteLevel),
        //                                                           FRAME+0x230, DOB::SetKeyLightColour
        //  12  Vector3&   lOutputKeyLightSpecularColour   sp+0x74   SCT(11)
        //  13  Vector3&   lOutputCloud0LiteColour         sp+0x7C   FRAME+0x260 (.xyz, w=1)
        //  14  Vector3&   lOutputCloud1LiteColour         sp+0x84   (dropped by this caller)
        //  15  Vector3&   lOutputCloud0DarkColour         sp+0x8C   FRAME+0x250 (.xyz, w=1)
        //  16  Vector3&   lOutputCloud1DarkColour         sp+0x94   (dropped by this caller)
        //  17  Vector4&   lOutputCloud0ScaleAndOffset     sp+0x9C   FRAME+0x270
        //  18  Vector2&   lOutputCloudOpacity             sp+0xA4   FRAME+0x2A0 (o0, o1, 0, 0)
        //  19  Vector2&   lOutputCloudDensity             sp+0xAC   FRAME+0x280 (1-d0, 1-d1, 0, 0)
        //  20  Vector2&   lOutputCloudFeathering          sp+0xB4   FRAME+0x290 (1/f0, 1/f1, 0, 0)
        //  21  f32&       lfOutputWhiteLevel              sp+0xBC   SCT(29) HDRConstants (w, 1/w, 0, 0),
        //                                                           .w of SCT(28), FRAME+0x318, DOB::SetWhiteLevel
        //  22  Matrix44&  lOutputIrradianceMatrixR        sp+0xC4   sub_827B0790 -> SCT(18)/SCT(19) quadrics
        //  23  Matrix44&  lOutputIrradianceMatrixG        sp+0xCC   sub_827B0790
        //  24  Matrix44&  lOutputIrradianceMatrixB        sp+0xD4   sub_827B0790
        //  25  Vector3&   lOutputAverageIrradianceColour  sp+0xDC   DOB::SetAverageIrradianceColour
        //  26  Vector3&   lOutputUnbiasedKeyLightDirection sp+0xE4  FRAME+0x300
        //
        // (Five of the 26 are computed and dropped by the shipped caller -- #5, #7, #8, #14, #16.
        // Each `var_XXX` is referenced exactly twice in the caller's asm, once at the argument
        // set-up and once at its consumer; those five appear only once.)
        void GenerateShaderConstants( Vector4&  lOutputSkyTopColourDrk,
                                      Vector4&  lOutputSkyHorColourPow,
                                      Vector4&  lOutputSkySunColourPow,
                                      Vector3&  lOutputSkyHorBleedSclPow,
                                      Vector4&  lOutputScattTopColourDrk,
                                      Vector4&  lOutputScattHorColourPow,
                                      Vector4&  lOutputScattSunColourPow,
                                      Vector3&  lOutputScattHorBleedSclPow,
                                      Vector4&  lOutputScattCoeffs,
                                      Vector3&  lOutputKeyLightDirection,
                                      Vector3&  lOutputKeyLightColour,
                                      Vector3&  lOutputKeyLightSpecularColour,
                                      Vector3&  lOutputCloud0LiteColour,
                                      Vector3&  lOutputCloud1LiteColour,
                                      Vector3&  lOutputCloud0DarkColour,
                                      Vector3&  lOutputCloud1DarkColour,
                                      Vector4&  lOutputCloud0ScaleAndOffset,
                                      Vector2&  lOutputCloudOpacity,
                                      Vector2&  lOutputCloudDensity,
                                      Vector2&  lOutputCloudFeathering,
                                      f32&      lfOutputWhiteLevel,
                                      Matrix44& lOutputIrradianceMatrixR,
                                      Matrix44& lOutputIrradianceMatrixG,
                                      Matrix44& lOutputIrradianceMatrixB,
                                      Vector3&  lOutputAverageIrradianceColour,
                                      Vector3&  lOutputUnbiasedKeyLightDirection );

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

    // ---- the four stage machines + the blend mode. ENUMERATOR NAMES AND VALUES ARE THE
    //      DecFIGS DWARF's, VERBATIM (BrnEnvironmentManager.h:246 / :266 / :284 / :299 /
    //      :333 / :360). Every value is independently confirmed by the X360 bodies that
    //      switch on them: Prepare @0x827D49A8 (16 cases, 0..15), StreamOut @0x827D2EB0
    //      (8 cases), StreamIn @0x827D31E8 (8 cases), Construct @0x827CA408's terminal
    //      seeds (7 / 7 / 4) and the RequestNextSeason assert text
    //      "meStreamOutStage >= E_STREAMOUT_REQUEST_NEXT_SEASON" (== 6).
    enum EPrepareStage
    {
        E_PREPARE_START                          = 0,
        E_PREPARE_LOAD_DEPENDENCIES              = 1,
        E_PREPARE_WF_LOAD_DEPENDENCIES           = 2,
        E_PREPARE_ACQUIRE_DEPENDENCIES           = 3,
        E_PREPARE_WF_ACQUIRE_DEPENDENCIES        = 4,
        E_PREPARE_LOAD_DICTIONARY                = 5,
        E_PREPARE_WF_LOAD_DICTIONARY             = 6,
        E_PREPARE_ACQUIRE_DICTIONARY             = 7,
        E_PREPARE_WF_ACQUIRE_DICTIONARY          = 8,
        E_PREPARE_STREAMIN                       = 9,
        E_PREPARE_WF_STREAMIN                    = 10,
        E_PREPARE_OPEN_JUNKYARD_LIGHTING_DATA    = 11,
        E_PREPARE_WF_OPEN_JUNKYARD_LIGHTING_DATA = 12,
        E_PREPARE_READ_JUNKYARD_LIGHTING_DATA    = 13,
        E_PREPARE_WF_READ_JUNKYARD_LIGHTING_DATA = 14,
        E_PREPARE_DONE                           = 15,
    };

    enum EReleaseStage
    {
        E_RELEASE_START = 0,
        E_RELEASE_DONE  = 1,
    };

    enum EStreamOutStage
    {
        E_STREAMOUT_START                     = 0,
        E_STREAMOUT_DISCARD_SEASON            = 1,
        E_STREAMOUT_UNLOAD_SEASON             = 2,
        E_STREAMOUT_WF_UNLOAD_SEASON          = 3,
        E_STREAMOUT_UNLOAD_SEASON_COLOURCUBES = 4,
        E_STREAMOUT_WF_UNLOAD_SEASON_COLOURCUBES = 5,
        E_STREAMOUT_REQUEST_NEXT_SEASON       = 6,
        E_STREAMOUT_DONE                      = 7,
    };

    enum EStreamInStage
    {
        E_STREAMIN_START                    = 0,
        E_STREAMIN_LOAD_SEASON_COLOURCUBES  = 1,
        E_STREAMIN_WF_LOAD_SEASON_COLOURCUBES = 2,
        E_STREAMIN_LOAD_SEASON              = 3,
        E_STREAMIN_WF_LOAD_SEASON           = 4,
        E_STREAMIN_ACQUIRE_SEASON           = 5,
        E_STREAMIN_WF_ACQUIRE_SEASON        = 6,
        E_STREAMIN_DONE                     = 7,
    };

    enum EBlendMode
    {
        E_BLENDMODE_TIMEOFDAY = 0,
        E_BLENDMODE_SEASONS   = 1,
        E_BLENDMODE_PAUSED    = 2,
    };

    enum ESetupSeasonsBlendStage
    {
        E_SETUPSEASONSBLEND_START       = 0,
        E_SETUPSEASONSBLEND_WF_STREAMOUT = 1,
        E_SETUPSEASONSBLEND_WF_STREAMIN = 2,
        E_SETUPSEASONSBLEND_BLEND       = 3,
        E_SETUPSEASONSBLEND_DONE        = 4,
    };

    // DWARF BrnEnvironmentManager.h:62 -- the macJunkyardLightingBuffer size and the read cap
    // Prepare's "(uint32_t)rw::core::filesys::GetSize(mpFileHandle) <
    // K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE-1" assert compares against (X360 immediate 0x7FF).
    enum { K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE = 2048 };

private:
    // @ 0x827C30E8 (T = Keyframe) / @ 0x827C3048 (T = Dictionary): the X360 emitted one out-of-line
    // copy per instantiated T of CgsResource::ResourcePtr<T>::GetMemoryResource()'s null-resource
    // assert (CgsResourcePtr.h line 581); returns the memory resource. A member TEMPLATE here
    // because the manager reaches it with T = TimeLine for maSeasonPtrs (SetupTimeOfDayBlend /
    // SetupSeasonsBlend / SetupBlend / Prepare) as well as T = Keyframe (envblend R1, step 9).
    template <typename T>
    T* ResourcePointerAssertThingy(CgsResource::ResourcePtr<T>& lrResourcePtr)
    { return lrResourcePtr.GetMemoryResource(); }

    // @ 0x827B0EB8. 4-way weighted blend of the four source keyframes' scattering /
    // lighting / clouds sub-blocks into the manager's current-environment targets.
    void PerformBlend(BlendFrame& lrBlendFrame);

    // @ 0x827C5018. Tool path: re-read d:\LightSetup.txt once every 30 frames, then set
    // the blend frame to a uniform 0.25-weight blend of four copies of the parsed
    // keyframe. Returns whether the file was re-read this call.
    bool SetupUpdateFromToolBlend(BlendFrame& lrBlendFrame);

    // ---- STEP 9 "the environment manager goes live", BLEND half (group envblend) ----
    // The three blend-setup bodies Update @0x827D6060 drives. Signatures are the DecFIGS
    // DWARF's (BrnEnvironmentManager.h:197 / :207 / :212) and each is confirmed against the
    // X360 prologue register assignment -- in particular the PPC float-slot skew, which puts
    // SetupBlend's f32 SECOND (r3=this, r4=BlendFrame&, f1=float [r5 reserved and skipped],
    // r6=UpdateOutputBuffer*) and SetupTimeOfDayBlend's f32 THIRD (r3, r4, r5, f1). Hex-Rays
    // renders both as all-GPR prototypes and is wrong about both.
    //
    // @ 0x827D4FE8. The blend-mode state machine: the time-of-day blend, else the season
    // blend, else the tool blend; E_BLENDMODE_PAUSED reports "nothing to blend". Returns
    // whether the blend frame was filled (Update only PerformBlend()s when it was).
    bool SetupBlend(BlendFrame& lrBlendFrame, f32 lfTimeStep,
                    BrnWorldIO::UpdateOutputBuffer* lpOutput);

    // @ 0x827D35C0. Advance the time of day (REFLECTING off mfTimeOfDayLower/UpperBound --
    // there is no day wrap), then fill the blend frame with the bracketing keyframe pair of
    // the current location (slots 0/1) and of the other location (slots 2/3). Returns false
    // when the advanced time of day is outside the bounds, which hands the machine to
    // SetupSeasonsBlend. NOTE the DWARF's trailing float32_t is passed by the console
    // (SetupBlend forwards its f1) but the body never reads it -- it uses mrTimeStep.
    bool SetupTimeOfDayBlend(BlendFrame& lrBlendFrame,
                             BrnWorldIO::UpdateOutputBuffer* lpOutput, f32 lfTimeStep);

    // @ 0x827D37A0. The season-swap stage machine (stream the old season out, the new one in,
    // then cross-fade over mfSeasonBlend). While it runs the blend frame is pinned to the
    // FIRST or LAST keyframe of the day of each of the four (location x season) corners,
    // chosen by the sign of mfTimeOfDayDelta. Returns false only once the swap completes.
    bool SetupSeasonsBlend(BlendFrame& lrBlendFrame,
                           BrnWorldIO::UpdateOutputBuffer* lpOutput);

    // The three helpers the X360 compiler INLINED into the two Setup*Blend bodies --
    // FindKeyframes / FirstKeyframe / LastKeyframe (DWARF :228 / :233 / :238) -- are
    // de-inlined in BrnEnvironmentManager.cpp under their DWARF names. They are kept
    // FILE-LOCAL there rather than declared here because their TimeLine::LocationData
    // parameter would need this header to #include SharedClasses/World/
    // BrnEnvironmentTimeLine.h, which group `envdata` owns; promote them to private members
    // (the DWARF shape) in the same change that adds that include.

public:
    // Inlined into WorldModule::Update @0x827D63E8 (`stfsx f0, r31, 0x1E8124`, i.e. the
    // embedded manager + 0x11C4). DWARF BrnEnvironmentManager.h:107. This is the frame delta
    // every time-dependent quantity in this class is scaled by: the time-of-day advance in
    // SetupTimeOfDayBlend and the cloud UV scroll in Update.
    void SetCurrentTimeStep(f32 lfTimeStep) { mrTimeStep = lfTimeStep; }

private:

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
    EPrepareStage  mePrepareStage;              // 0x000  :272  staged-prepare cursor
    EReleaseStage  meReleaseStage;              // 0x004  :273  release latch
    // ⭐ CORRECTED 2026-08-16 (envstream). 0x008 is the module's real
    // CgsModule::EventReceiverQueue<1024,16> (DWARF BrnEnvironmentManager.h:277
    // `EventReceiverQueue<1024,16> mReceiverQueue`), and the three words the previous
    // opaque model named were at the WRONG offsets. Construct @0x827CA408 proves the
    // layout (r3 = this+8):
    //     stw 0x400, 0x10(r3)     -> miCapacity  == queue+0x10 == this+0x18
    //     stw 0x10,  0x14(r3)     -> miAlignment == queue+0x14 == this+0x1C
    //     stw (r3+0x18), 0(r3)    -> mpBuffer    == queue+0x18 == this+0x20 (the embedded buffer)
    //     bl  BaseEventReceiverQueue::Clear
    // i.e. the base is the 6-word {mpBuffer, miWriteOffset, miCount, miStartOffset,
    // miCapacity, miAlignment} that CgsBaseEventReceiverQueue.h already homes, and
    // this+0x10 / this+0x14 are miCount / miStartOffset -- NOT capacity/stride. The old
    // model therefore wrote 1024 into the queue's COUNT and 16 into its START OFFSET and
    // never Cleared it. Nothing read the queue before this wave, so the bug was latent;
    // Prepare/StreamIn/StreamOut read miCount and *(mpBuffer+miStartOffset) every frame,
    // which is what surfaced it. Construct now calls mReceiverQueue.Construct().
    // (X360 reads that pin the offsets: Prepare @0x827D4AB0 `lwz r10,0x10(r31)` == the
    // event count; @0x827D4AD0-0x827D4AD8 `lwz r11,0x14(r31)` + `lwz r10,8(r31)` +
    // `lwzx r11,r11,r10` == *(mpBuffer + miStartOffset) == the first event's TYPE word.)
    CgsModule::EventReceiverQueue<1024, 16> mReceiverQueue;   // 0x008 (guest 0x008..0x41F)
    // ⭐ 0x420..0x51C RETYPED FROM THE DWARF 2026-08-16 (envstream). Names are the DecFIGS
    // spellings verbatim (BrnEnvironmentManager.h:278..:369); the guest offsets in the
    // comment column stay as the asm attests them, but NOTHING reaches these by offset --
    // ResourcePtr and Random both widen on the x64 host, so every offset from 0x424 on is
    // guest-only documentation. The four s32 stage cursors become their DWARF enums.
    s32            miResourceCnt;               // 0x420  :278  outstanding resource requests Prepare waits on
    CgsResource::ResourcePtr<Dictionary> mDictionaryPtr;      // 0x424  :282  (guest 0x20)
    // NOTE: 0x444 / 0x448 were previously mis-named muCurrSeasonRef / mbCurrSeason. Construct
    // @0x827CA408 writes 7 (== E_STREAMOUT_DONE, the mirror of meStreamInStage) into 0x444 and 0
    // into 0x448, so the pair is the stream-OUT half of the season swap. DiscardCurrSeason
    // therefore reads as "begin streaming the current season out", which is exactly its name.
    EStreamOutStage meStreamOutStage;           // 0x444  :296
    u8             muStreamOutTarget;           // 0x448  :297  season SLOT (0/1) being streamed out
    u8             mPad449[3];                  // 0x449
    EStreamInStage meStreamInStage;             // 0x44C  :311
    u8             muStreamInTarget;            // 0x450  :312  season SLOT (0/1) being streamed in
    u8             mPad451[0xF];                // 0x451
    CgsNumeric::Random mRandom;                 // 0x460  :314  (guest 0x2C, 16-aligned)
    // maSeasonPtrs is ResourcePtr<TimeLine>[2] -- DWARF :318, and the X360 agrees: the
    // out-of-line ResourcePtr<TimeLine>::GetMemoryResource @0x827C30E8 is called with
    // `addi r3, r11, 0x490` by SetupTimeOfDayBlend @0x827D365C. (The DISTINCT out-of-line
    // instantiation @0x827C3048 is ResourcePtr<Dictionary>::GetMemoryResource -- StreamIn /
    // StreamOut / RequestNextSeason call it with `addi r3, r31, 0x424`.)
    CgsResource::ResourcePtr<TimeLine> maSeasonPtrs[2];       // 0x490  :318  (guest 2 * 0x20)
    s32            maiSeasons[2];               // 0x4D0  :319  season INDEX per slot; Construct { -1, 0 }
    s32            miSeasonCurrentlyPlaying;    // 0x4D8  :320
    s32            miCurrSeason;                // 0x4DC  :325  the slot (0/1) currently blended from
    f32            mfSeasonBlend;               // 0x4E0  :326
    f32            mfSeasonBlendDelta;          // 0x4E4  :326
    s32            maiLocations[2];             // 0x4E8  :328
    s32            miCurrLocation;              // 0x4F0  :330
    f32            mfLocationBlend;             // 0x4F4  :331
    EBlendMode     meBlendMode;                 // 0x4F8  :343  2 = E_BLENDMODE_PAUSED
    u8             mPad4FC[4];                  // 0x4FC  (no DWARF member here; asm-attested gap)
    EBlendMode     meBlendModePaused;           // 0x500  :344  meBlendMode saved across a tool blocking op
    // --- named members proven by PerformBlend (0x827B0EB8) / SetupUpdateFromToolBlend (0x827C5018) ---
    f32            mfTimeOfDay;                 // 0x504  current time-of-day, SECONDS of day
    f32            mfTimeOfDayDelta;            // 0x508
    f32            mfCloudDelta;                // 0x50C
    f32            mfSunRigRotation;            // 0x510  degrees
    f32            mfSunTiltAtHorizon;          // 0x514  degrees
    f32            mfSunTiltAtMidday;           // 0x518  degrees
    ESetupSeasonsBlendStage meSetupSeasonsBlendStage;  // 0x51C  :369  Construct: E_SETUPSEASONSBLEND_DONE
    BlendFrame     mBlendFrame;                 // 0x520  the live 4-keyframe blend frame
    ScatteringData mScattering;                 // 0x540  blended scattering target (PerformBlend dst @+0x540)
    u8             mPad5E8[0x8];                // 0x5E8
    LightingData   mLighting;                   // 0x5F0  blended lighting target   (PerformBlend dst @+0x5F0)
    u8             mPad674[0xC];                // 0x674
    CloudsData     mClouds;                     // 0x680  blended clouds target     (PerformBlend dst @+0x680)
    u8             mPad6EC[4];                  // 0x6EC
    f32            mfCloudDistanceCurve;        // 0x6F0  Construct: 1.0f
    bool           mbSetScattColsFromSky;       // 0x6F4  GenerateShaderConstants @0x827D0098 `lbz r11,0x6F4`
    bool           mbSetIrradianceFromSky;      // 0x6F5  GenerateShaderConstants @0x827D04CC `lbz r11,0x6F5`
    // 0x6F6: NOT a member. The DWARF lists mbSetIrradianceFromSky (BrnEnvironmentManager.h:393) and
    // mGlobalIrradianceManager (:396) as consecutive declarations, so this is the alignment gap the
    // 16-byte-aligned manager forces after the two bools. Kept explicit so the offsets stay readable.
    u8             mPad6F6[0xA];                // 0x6F6  alignment padding
    // 0x700. The real type, per DWARF BrnEnvironmentManager.h:396 `GlobalIrradianceManager
    // mGlobalIrradianceManager;`. GenerateShaderConstants reaches it three ways: `mr r3,r27` with
    // r27 = this+0x700 into ComputeIrradiance / GetIrradianceMatrix, and `lvx128 v0, r31, 0x7C0`
    // == +0xC0 == mAverageIrradianceColour (BrnGlobalIrradianceManager.h:91).
    //
    // SIZE: the console class is 0x160 (0x700..0x860, miToolUpdateFrameCounter) and the DWARF says
    // exactly why -- Matrix44 maIrradianceMatrix[3] @+0x00 (0xC0) + Vector3 mAverageIrradianceColour
    // @+0xC0 + Vector3 maFrameIrradianceCoeffs[9] @+0xD0 (0x90) = 0x160. The COMMITTED
    // BrnGlobalIrradianceManager.h currently declares only the three matrices (0xC0), so this member
    // is 0xA0 short of the console size until the irradiance group grows that class. Nothing in the
    // tree reaches any of these members by offset, so the short type is a fidelity gap, not a bug.
    GlobalIrradianceManager mGlobalIrradianceManager;   // 0x700
    s32            miToolUpdateFrameCounter;    // 0x860  reset by UpdateFromTool; 30-frame gate in SetupUpdateFromToolBlend
    u8             mPad864[0xC];                // 0x864
    Keyframe       maToolKeyframes[4];          // 0x870  four tool-parsed keyframe slots (4 * 0x240)
    // 0x1170: an unnamed bool -- Construct clears it, GenerateEffects @0x827BE698 gates all four
    // of its per-frame copies on it (false = take the blended keyframe, true = take the defaults).
    // FLAG: no DWARF member exists at this offset, so the NAME is not recovered; the behaviour is.
    bool           mbUseDefaultEffects;         // 0x1170
    u8             mPad1171[3];                 // 0x1171
    // ⭐ RETYPED 2026-08-16 (envstream). Prepare @0x827D49A8 stage 4 binds this pointer with
    // CgsResource::BaseResourcePtr::CreateFromHandle (asm 0x827D4BF8-0x827D4C04, `addi r30,
    // r31, 0x1174`), so it must be a real ResourcePtr, not an opaque 0x20 guest blob (which is
    // also the WRONG SIZE on x64 -- a host ResourcePtr is 48 bytes). DWARF :411.
    CgsResource::ResourcePtr<rw::graphics::postfx::ColourCube> mDefColourCubePtr;  // 0x1174 :411
    // ⭐ RETYPED 2026-08-16 (group tintdata). DWARF :412 types this member the same
    // `Keyframe::TintData` (== BrnEffects::TintData) the keyframe carries, and Prepare
    // @0x827D49A8 fills it with `ResourcePtr<ColourCube>::GetMemoryResource(&mDefColourCubePtr)`
    // (`stw r3, 0x1194(r31)` @0x827D4C10) -- i.e. a real ColourCube POINTER, 4 bytes on the
    // guest, 8 here. It used to be a u32 because BrnEffects::TintData::mpColourCube was; that
    // member is now the pointer the DWARF declares, so this one is too and Prepare's publish is
    // no longer HELD. Unlike the KEYFRAME's slot this is a plain runtime member of a
    // heap-constructed manager -- nothing serialises it -- so it simply widens.
    rw::graphics::postfx::ColourCube* mpDefTintData;   // 0x1194  :412  Keyframe::TintData
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
