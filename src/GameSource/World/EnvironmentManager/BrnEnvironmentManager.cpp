#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"  // ParseEnvironmentFile
#include "SharedClasses/World/BrnEnvironmentUtil.h"                       // ComputeKeyLightDirection
#include "SharedClasses/Graphics/BrnEffectsData.h"                        // BrnEffectsFrame + BrnEffects::{Bloom,Vignette,Tint}Data
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "GameSource/World/BrnWorldModuleIO.h"               // BrnWorldIO::UpdateOutputBuffer (LockForWrite / GetEffectsEnvironmentInterface)
#include "SharedClasses/World/BrnEnvironmentTimeLine.h"      // TimeLine + TimeLine::LocationData (group envdata's header)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog -- the [env] bring-up diagnostic

// ---- the streaming half (envstream, wave 2026-08-16) -----------------------------------
#include "SharedClasses/World/BrnEnvironmentDictionary.h"   // Dictionary + SeasonData/LocationData
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"        // RequestInterface<4096>
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // AcquireResourceRequest/Response
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID::HashString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // CgsCore::SPrintf

#include <cmath>     // sqrtf / std::cos / std::sin
#include <cstdio>    // std::snprintf (bring-up diagnostic) / fopen / fscanf / fclose (the console's d:\Season.txt override)
#include <cstring>   // memset / memcpy / strcmp
#include <cstdlib>   // strtod (ReadJunkyardLightingData)

namespace
{
    // ---- X360 rodata, all dumped from BURNOUT_X360_ARTIST.XEX with headless IDA 9.3 --------
    // The times are SECONDS OF DAY and the sun-rig angles are DEGREES, which is what makes
    // these readable: 46800 = 13:00, 64800 = 18:00, 28800..61200 = 08:00..17:00,
    // 32400..57600 = 09:00..16:00.
    const f32 KF_DEF_SEASON_BLEND_DELTA        = 0.019999999f;   // flt_82005574
    const f32 KF_DEF_TIME_OF_DAY               = 46800.0f;       // flt_820CA580  (13:00)
    const f32 KF_DEF_TIME_OF_DAY_DELTA         = 54.0f;          // flt_820CA584
    const f32 KF_DEF_CLOUD_DELTA               = 15.0f;          // flt_820047C4
    const f32 KF_SUN_RIG_ROTATION              = 45.0f;          // flt_820CA58C  (degrees)
    const f32 KF_SUN_TILT_AT_HORIZON           = 20.0f;          // flt_820CA5A8  (degrees)
    const f32 KF_SUN_TILT_AT_MIDDAY            = 50.0f;          // flt_820138DC  (degrees)
    const f32 KF_DEF_TIME_OF_DAY_UPPER_BOUND   = 61200.0f;       // flt_820CAB98  (17:00)
    const f32 KF_DEF_TIME_OF_DAY_LOWER_BOUND   = 28800.0f;       // flt_820CC768  (08:00)
    const f32 KF_SUN_ELEV_TOD_LOWER_BOUND      = 32400.0f;       // flt_8201C214  (09:00)
    const f32 KF_SUN_ELEV_TOD_UPPER_BOUND      = 57600.0f;       // flt_820CC764  (16:00)
    const f32 KF_DEF_WHITE_LEVEL               = 0.5f;           // flt_82001DA0
    const f32 KF_JUNKYARD_TIME_OF_DAY_SECONDS  = 64800.0f;       // flt_82F307F0  (18:00)
    const f32 KF_JUNKYARD_NEAREST_DISTANCE_SENTINEL = 100000.0f; // flt_820080E8

    // CalcKeyLightDirection's time-of-day -> elevation-angle mapping.
    const f32 KF_SUN_ELEVATION_ZERO_SECONDS    = 23400.0f;       // flt_820CAA7C  (06:30)
    const f32 KF_RADIANS_PER_SECOND_OF_DAY     = 7.2722054e-5f;  // flt_820CAA78  (2*pi / 86400)

    // (The hand-rolled KU_RANDOM_* / KU_RECEIVER_QUEUE_* constants and the two local
    // E_STREAMOUT_DONE / E_SETUPSEASONSBLEND_DONE s32s that used to live here are RETIRED
    // 2026-08-16 -- Construct now calls mReceiverQueue.Construct() / mRandom.Construct(),
    // and the stage enumerators are the class's own DWARF enums. See the note in Construct.)

    // ---- STEP 9 (group envblend): the rest of the two stage machines + the blend rodata ----
    // EBlendMode, DecFIGS BrnEnvironmentManager.h:333. The DecFIGS enum stops at PAUSED = 2;
    // the X360 SetupBlend @0x827D4FE8 switches on FOUR values and the committed UpdateFromTool
    // @0x827B0DA8 writes the fourth, so it is spelled as a reserved constant, NOT invented as a
    // fifth enumerator.
    const s32 E_BLENDMODE_TIMEOFDAY = 0;
    const s32 E_BLENDMODE_SEASONS   = 1;
    const s32 E_BLENDMODE_PAUSED    = 2;
    const s32 KI_BLENDMODE_TOOL     = 3;

    // ESetupSeasonsBlendStage, DecFIGS BrnEnvironmentManager.h:360 (DONE == 4 is above).
    const s32 E_SETUPSEASONSBLEND_START        = 0;
    const s32 E_SETUPSEASONSBLEND_WF_STREAMOUT = 1;
    const s32 E_SETUPSEASONSBLEND_WF_STREAMIN  = 2;
    const s32 E_SETUPSEASONSBLEND_BLEND        = 3;

    // ---- rodata the blend slice loads, all dumped from the ARTIST image --------------------
    const f32 KF_ZERO               = 0.0f;         // flt_82001CC0
    const f32 KF_ONE                = 1.0f;         // flt_82001C98
    const f32 KF_DEGREES_TO_RADIANS = 0.017453292f; // flt_820CA158 (Update's cloud-direction angle)
    // Update's cloud-layer wind divisor: the console reciprocates flt_820CA5B0[0] == 3.0f with
    // vrefp + two Newton-Raphson refinements and multiplies, which is a plain divide by 3 here.
    const f32 KF_CLOUD_WIND_DIVISOR = 3.0f;         // flt_820CA5B0

    // ---- GenerateEffects @0x827BE698: the weights its "no keyframe / defaults" arm writes ---
    // A slot with no bracketing keyframe contributes NOTHING to the arbitrator's bloom /
    // vignette blend (weight 0), but it still contributes its default colour cube at a
    // quarter weight -- one full unit shared across the layer's four slots.
    const f32 KF_DEF_EFFECTS_LAYER_WEIGHT      = 0.0f;   // flt_82001CC0 (bloom + vignette weight)
    const f32 KF_DEF_EFFECTS_LAYER_TINT_WEIGHT = 0.25f;  // flt_82003F40 (tint weight)

    // K_DEFAULT_JUNKYARD_KEY_LIGHT_DIRECTION (X360 unk_8300F410). This is MUTABLE .data, not
    // rodata -- its bytes are zero in the image and its real initial value is written by a
    // static-init TU outside this wave. EnableJunkyardLightingSetup seeds the override with it
    // and then immediately overwrites it from the nearest loaded junkyard setup, so it only
    // matters when no setups are loaded.
    // FLAG PC-platform leaf: static-init initialiser for this .data global not reconstructed.
    Vector3 GetDefaultJunkyardKeyLightDirection()
    {
        Vector3 lDirection;
        lDirection.x = 0.0f;
        lDirection.y = 0.0f;
        lDirection.z = 0.0f;
        lDirection.w = 0.0f;
        return lDirection;
    }
}

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::EnvironmentManager::UpdateFromTool             @ 0x827B0DA8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::DiscardCurrSeason          @ 0x827B0E50
//   BrnWorld::EnvironmentSettings::EnvironmentManager::ResourcePointerAssertThingy@ 0x827C30E8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::PerformBlend               @ 0x827B0EB8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::SetupUpdateFromToolBlend   @ 0x827C5018
//
// The baked asserts fire the plain-string Begin/Fire/End sequence (rendered here as
// CGS_ASSERT); the X360-baked file/line are discarded per project convention. The typo
// "Tyring" is reproduced verbatim from the ARTIST rodata.

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x827B0DA8. Tool-driven blend/pause transition.
//   meBlendMode >= 3  -> already in a blocking op: on unpause (lbPause == false) restore
//                         the saved state (asserting the state really is the reserved 3),
//                         and report the blocking state (true).
//   meBlendMode <  3  -> on pause (lbPause == true) stash the current state, enter the
//                         reserved blocking state 3, and reset the tool-update frame gate;
//                         report not-blocking (false).
bool EnvironmentManager::UpdateFromTool(bool lbPause)
{
    // The tool's "blocking operation" state is the value 3 -- one PAST the DWARF EBlendMode's
    // last enumerator (E_BLENDMODE_PAUSED == 2). The X360 stores the literal 3 into the same
    // word, so the enum simply has an unnamed reserved state; spelled with an explicit cast
    // (the value is inside the enum's underlying bit range, so this is well defined) rather
    // than by inventing an enumerator name the DWARF does not have.
    const EBlendMode KE_BLENDMODE_TOOL_BLOCKING = static_cast<EBlendMode>(3);

    if (meBlendMode >= KE_BLENDMODE_TOOL_BLOCKING)
    {
        if (!lbPause)
        {
            CGS_ASSERT(meBlendMode == KE_BLENDMODE_TOOL_BLOCKING, "Tyring to unpause a blocking op");
            meBlendMode = meBlendModePaused;
        }
        return true;
    }

    if (lbPause)
    {
        meBlendModePaused        = meBlendMode;
        meBlendMode              = KE_BLENDMODE_TOOL_BLOCKING;
        miToolUpdateFrameCounter = 0;
    }
    return false;
}

// @ 0x827B0E50. Commit the pending season swap once the stream-in has finished.
void EnvironmentManager::DiscardCurrSeason()
{
    CGS_ASSERT(meStreamInStage == E_STREAMIN_DONE, "meStreamInStage == E_STREAMIN_DONE");

    // Rewind the stream-OUT stage machine to START and point it at the season that is
    // being discarded. (0x444/0x448 were previously mis-named muCurrSeasonRef /
    // mbCurrSeason; Construct writes E_STREAMOUT_DONE into 0x444, which is what
    // identifies the pair -- see the header banner.)
    const s32 liCurrSeason = miCurrSeason;
    meStreamOutStage  = E_STREAMOUT_START;
    muStreamOutTarget = static_cast<u8>(liCurrSeason);
}

// ResourcePointerAssertThingy is now the one-line member TEMPLATE in BrnEnvironmentManager.h
// (@0x827C30E8 for T = Keyframe, @0x827C3048 for T = Dictionary -- the X360 emitted one
// out-of-line copy of CgsResource::ResourcePtr<T>::GetMemoryResource()'s null-resource assert,
// CgsResourcePtr.h line 581, per instantiated T). The manager reaches it with T = TimeLine for
// maSeasonPtrs (SetupTimeOfDayBlend / SetupSeasonsBlend / SetupBlend / Prepare), so a single
// Keyframe-only out-of-line body can no longer serve every call site.

// @ 0x827B0EB8. Blend the four source keyframes of a blend frame into the manager's
// current-environment sub-blocks. Each of scattering / lighting / clouds is the
// per-element weighted sum A0*w0 + A1*w1 + A2*w2 + A3*w3 of the four sources'
// corresponding sub-block (the four keyframes are the bracketing time-of-day/season
// pair pairs; the weights come from the blend frame). The X360 loads all four weights
// fresh for each sub-block call; the interleaved (value, weight) argument order matches
// the four GPR (value) + four FPR (weight) register split in the asm.
void EnvironmentManager::PerformBlend(BlendFrame& lrBlendFrame)
{
    const Keyframe& lrK0 = *lrBlendFrame.mapKeyframes[0];
    const Keyframe& lrK1 = *lrBlendFrame.mapKeyframes[1];
    const Keyframe& lrK2 = *lrBlendFrame.mapKeyframes[2];
    const Keyframe& lrK3 = *lrBlendFrame.mapKeyframes[3];

    const float lfW0 = lrBlendFrame.mafWeights[0];
    const float lfW1 = lrBlendFrame.mafWeights[1];
    const float lfW2 = lrBlendFrame.mafWeights[2];
    const float lfW3 = lrBlendFrame.mafWeights[3];

    mScattering.SetToBlend(lrK0.mScattering, lfW0, lrK1.mScattering, lfW1,
                           lrK2.mScattering, lfW2, lrK3.mScattering, lfW3);
    mLighting.SetToBlend(lrK0.mLighting, lfW0, lrK1.mLighting, lfW1,
                         lrK2.mLighting, lfW2, lrK3.mLighting, lfW3);
    mClouds.SetToBlend(lrK0.mClouds, lfW0, lrK1.mClouds, lfW1,
                       lrK2.mClouds, lfW2, lrK3.mClouds, lfW3);
}

// @ 0x827C5018. Tool blend setup. Once every 30 frames (miToolUpdateFrameCounter gate)
// re-parse the light-setup text file into a fresh keyframe and stamp four copies of it
// into the manager's tool-keyframe slots; then point the blend frame at those four slots
// with an equal 0.25 weight each. Returns whether the file was re-read this frame.
bool EnvironmentManager::SetupUpdateFromToolBlend(BlendFrame& lrBlendFrame)
{
    const bool lbReloaded = (miToolUpdateFrameCounter == 0);
    miToolUpdateFrameCounter = (miToolUpdateFrameCounter + 1) % 30;

    if (lbReloaded)
    {
        Keyframe lKeyframe;
        lKeyframe.Construct();

        // Scratch out-params the parser fills; only the keyframe is kept (the colour-cube
        // name/weight arrays and the debug name are discarded on the tool path).
        char  lacColourCubes[4][256];
        float lafColourCubeWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        char  lacName[256];
        memset(lacColourCubes, 0, sizeof(lacColourCubes));

        ParseEnvironmentFile(mfTimeOfDay,
                             lacColourCubes,
                             lafColourCubeWeights,
                             lKeyframe.mBloom,
                             lKeyframe.mVignette,
                             lacName,
                             lKeyframe.mScattering,
                             lKeyframe.mLighting,
                             lKeyframe.mClouds,
                             "d:\\LightSetup.txt");

        maToolKeyframes[0] = lKeyframe;
        maToolKeyframes[1] = lKeyframe;
        maToolKeyframes[2] = lKeyframe;
        maToolKeyframes[3] = lKeyframe;
    }

    for (int liIndex = 0; liIndex < 4; ++liIndex)
    {
        lrBlendFrame.mapKeyframes[liIndex] = &maToolKeyframes[liIndex];
        lrBlendFrame.mafWeights[liIndex]   = 0.25f;
    }

    return lbReloaded;
}

// Request the manager's release (destub wave 2026-07-26). INLINED on the X360 into
// WorldModule::Release @0x827BCE58 stage 8 (`a1[498648] = 0; a1[498649] = 1;` on the
// embedded manager): rewind the staged-prepare cursor to START and raise the release
// latch. Member pair per the IDA-applied struct names in Prepare @0x827D49A8.
void EnvironmentManager::BeginRelease()
{
    mePrepareStage = E_PREPARE_START;
    meReleaseStage = E_RELEASE_DONE;   // the release latch the X360 raises (stage word 1)
}

}   // namespace EnvironmentSettings   -- closed so the three X360 file-scope tweakable globals can
}   // namespace BrnWorld              -- be defined where the DWARF says they live (see below).

// ---------------------------------------------------------------------------------------------
// gfSpecularScale -- X360 0x82F307E8, GLOBAL scope, defined in THIS translation unit.
//
// The DecFIGS DWARF for this very file is explicit about all three of the environment tweakables:
//   references/DecFIGS/dwarfdump/GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp:889
//     namespace :: {  extern float32_t gfBloomLuminanceScale;   // BrnEnvironmentManager.cpp:37
//                     extern float32_t gfBloomThresholdScale;   // BrnEnvironmentManager.cpp:38
//                     extern float32_t gfSpecularScale;     }   // BrnEnvironmentManager.cpp:39
// i.e. `namespace ::` == the global namespace, and the definitions sit at lines 37-39 of
// BrnEnvironmentManager.cpp. They are GLOBALS, not constants: EnvironmentManager::Construct
// @0x827CA408 takes the address of each (`lis/addi ... gfSpecularScale@l` @0x827CA914) to register
// it as a CgsDev debug variable, and this function is the only reader of the specular one --
//   $ grep -rl "gfSpecularScale" .ida-exports/BURNOUT_X360_ARTIST.XEX/
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x827D0098.json   <- GenerateShaderConstants (this read)
//   .ida-exports/BURNOUT_X360_ARTIST.XEX/0x827CA408.json   <- Construct (address-of)
//   $ grep -rn "gfSpecularScale" b5-decomp/src/     # only three COMMENTS, no definition anywhere
// so homing it here creates no split-brain. Value 1.0f in the shipped image (conductor idat dump,
// also recorded in BrnWorldModule.cpp:4290).
//
// FLAG (not fixed here, out of this group's scope): gfBloomLuminanceScale / gfBloomThresholdScale
// are ALREADY defined in GameSource/Graphics/BrnRendererModulePostFx.cpp, which its own banner calls
// a provisional home ("WHEN the environment manager's debug registration is reconstructed, declare
// `extern f32 ...` there rather than minting a second copy"). The DWARF above says their real home
// is this file. They are NOT moved here now because that file is on the build list and a second
// definition would be an LNK2005; the move belongs with EnvironmentManager::Construct's debug-
// variable registration.
// ---------------------------------------------------------------------------------------------
f32 gfSpecularScale = 1.0f;   // X360 0x82F307E8

namespace BrnWorld
{
namespace EnvironmentSettings
{

namespace
{
    // flt_82001C98 (0x3F800000) -- the numerator of the ScattCoeffs reciprocal. The console does a
    // real `lfs`/`fdivs`, not a vrefp estimate, so this is an exact 1.0f divide.
    const f32 KF_SCATT_RANGE_NUMERATOR = 1.0f;

    // flt_820CD130 (0x3903126F) -- exactly the float nearest 1/8000. Converts a cloud layer's world
    // -space scale into the shader's UV scale.
    const f32 KF_CLOUD_SCALE_TO_UV = 0.000125000006f;

    // The X360 builds every 16-byte constant by writing lanes into a stack quad and storing it with
    // one stvx128. These helpers are that quad-build spelled out; they are templates only because
    // rw::math::vpu::Vector2 / Vector3 / Vector4 are distinct types with the same four lanes.
    template <typename T>
    inline void SetQuad(T& lrOut, f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        lrOut.x = lfX;  lrOut.y = lfY;  lrOut.z = lfZ;  lrOut.w = lfW;
    }

    template <typename T>
    inline void CopyQuad(T& lrOut, const f32 lafLanes[4])
    {
        lrOut.x = lafLanes[0];  lrOut.y = lafLanes[1];
        lrOut.z = lafLanes[2];  lrOut.w = lafLanes[3];
    }

    inline void StoreQuad(f32 lafLanes[4], const Vector3& lrValue)
    {
        lafLanes[0] = lrValue.x;  lafLanes[1] = lrValue.y;
        lafLanes[2] = lrValue.z;  lafLanes[3] = lrValue.w;
    }

    inline Vector3 MakeScaledVector3(const f32 lafLanes[4], f32 lfScale)
    {
        Vector3 lResult;
        lResult.x = lafLanes[0] * lfScale;  lResult.y = lafLanes[1] * lfScale;
        lResult.z = lafLanes[2] * lfScale;  lResult.w = lafLanes[3] * lfScale;
        return lResult;
    }

    // vmulfp128 against a full splat of the white level: all four lanes.
    template <typename T>
    inline void ScaleQuad(T& lrValue, f32 lfScale)
    {
        lrValue.x *= lfScale;  lrValue.y *= lfScale;
        lrValue.z *= lfScale;  lrValue.w *= lfScale;
    }

    // vmulfp128 against the (w, w, w, 1.0f) vector the vperm/vsldoi trio builds: the .w lane, which
    // carries an exponent or a darkening scalar rather than a colour, is multiplied by 1.0f.
    template <typename T>
    inline void ScaleQuadRGB(T& lrValue, f32 lfScale)
    {
        lrValue.x *= lfScale;  lrValue.y *= lfScale;  lrValue.z *= lfScale;
    }

    inline void ScaleMatrixRows(Matrix44& lrMatrix, f32 lfScale)
    {
        ScaleQuad(lrMatrix.xAxis, lfScale);
        ScaleQuad(lrMatrix.yAxis, lfScale);
        ScaleQuad(lrMatrix.zAxis, lfScale);
        ScaleQuad(lrMatrix.wAxis, lfScale);
    }
}

// =============================================================================================
// @ 0x827D0098  BrnWorld::EnvironmentSettings::EnvironmentManager::GenerateShaderConstants
//
// Turn the manager's CURRENT (already blended) environment data into the 26 constants the world's
// vertex/pixel programs need. The only caller is WorldModule::SetupShaderConstantsBeforeRendering
// @0x827D1410, which pushes each out-param into ShaderConstantTable / BrnShaderConstantsFrame /
// BrnWorldIO::DispatchOutputBuffer -- see the attestation table above the declaration in the header.
//
// SIGNATURE. The Hex-Rays prototype's 63 arguments are the usual PPC over-count. The real shape is
// `this` + 26 references: r3 = this, r4..r10 = out-params 1..7, then one 8-byte parameter-save slot
// each from entrySP+0x50 upward (the 32-bit reference lands in the LOW word, +4, because the console
// is big-endian) -- so out-param N (N >= 8) is read back with `lwz rX, entrySP+0x54+8*(N-8)`. That is
// exactly 26 slots ending at entrySP+0xE4, and it matches the DWARF declaration
// (BrnEnvironmentManager.h:151 / :398) parameter-for-parameter, including the two trailing Vector3&.
// The parameter NAMES are the DWARF definition's
// (references/DecFIGS/dwarfdump/GameSource/World/EnvironmentManager/BrnEnvironmentManager.cpp:210).
//
// WHAT THE ASM DOES, in order (each line below is one contiguous run of the listing):
//   0x827D00C8-0x827D0190  the four SKY out-params. Each is three or four `lfs` into a stack quad
//                          plus one `stvx128`, i.e. a Vector4/Vector3 assignment.
//   0x827D0194-0x827D01F0  `if (mbSetScattColsFromSky)` (byte 0x6F4): copy the three sky colour
//                          vectors and their six scalars over the scattering ones -- a real WRITE
//                          BACK into mScattering, not a local.
//   0x827D01F4-0x827D02E8  the four SCATTERING out-params + ScattCoeffs
//                          (1/(far-near), near/(far-near), pow, cap); flt_82001C98 == 1.0f.
//   0x827D02EC-0x827D038C  the key light direction. `mbOverrideKeyLightDirection` (0x1234) hands the
//                          stored junkyard direction to BOTH outputs; otherwise ComputeKeyLightDirection
//                          runs TWICE -- first with the RAW time of day (-> lOutputUnbiasedKeyLightDirection)
//                          and then with the time of day clamped into the sun-elevation window by the
//                          fsel pair at 0x827D0350 / 0x827D0358 (-> lOutputKeyLightDirection). The DWARF
//                          confirms the split: the class also has CalcKeyLightDirection() and
//                          CalcKeyLightDirectionUnbiased() (BrnEnvironmentManager.h:154 / :157).
//   0x827D038C-0x827D0468  the straight 16-byte copies out of mLighting / mClouds and the three
//                          Vector2 pairs.
//   0x827D046C-0x827D04C8  lOutputCloud0ScaleAndOffset: two vperms + a vsldoi over the permute masks
//                          unk_82CDA400 / unk_82CDA3C0, decoded byte-by-byte in the comment below.
//   0x827D04CC-0x827D0504  `if (mbSetIrradianceFromSky)` (byte 0x6F5) -> ComputeIrradianceRigFromSky.
//   0x827D0508-0x827D0620  the six fill colours x mLighting.mfAmbientIrradianceScale ->
//                          GlobalIrradianceManager::ComputeIrradiance.
//   0x827D0624-0x827D0714  GetIrradianceMatrix(0/1/2) -> the three Matrix44 out-params, and
//                          mGlobalIrradianceManager+0xC0 (its average colour) -> the 25th out-param.
//   0x827D0718-0x827D08B4  the WHITE LEVEL pass (two different broadcast shapes, see below) and the
//                          gfSpecularScale multiply on the specular colour.
//   0x827D08B8-0x827D08C0  lfOutputWhiteLevel = mfWhiteLevel.
//
// PC DEVIATIONS: none behavioural. This is pure arithmetic on members; every VMX op above is a
// broadcast, a component multiply or a lane shuffle, spelled here as the scalar operation it
// performs. The one representational bridge is flagged inline at the ComputeIrradianceRigFromSky
// call (LightingData still stores its colours as f32[4], so the six Vector3& out-references cannot
// be formed without a cast -- see the FLAG there).
//
// NOTE FOR THE READER (not a deviation): Construct @0x827CA408 leaves mfWhiteLevel at 0.5f
// (flt_82001DA0), so once this function replaces PublishWorldShadingConstantsBringUp the published
// colours are HALF the hard-coded bring-up numbers and HDRConstants becomes (0.5, 2.0, 0, 0) instead
// of (1, 1, 0, 0); the post-fx tonemap divides the white level back out. The bring-up's own banner
// says as much ("with post-FX off, 1.0 is the only non-blowing value").
// =============================================================================================
void EnvironmentManager::GenerateShaderConstants(
        Vector4&  lOutputSkyTopColourDrk,
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
        Vector3&  lOutputUnbiasedKeyLightDirection)
{
    // ---- the SKY half of the scattering block ------------------------------------------------
    // Each of these is four `lfs` into one stack quad + one `stvx128`: the .w lane carries the
    // matching exponent / darkening scalar, which is what the "...ColourPow" / "...ColourDrk"
    // halves of the names refer to.
    SetQuad(lOutputSkyTopColourDrk,
            mScattering.mv3SkyTopColour[0], mScattering.mv3SkyTopColour[1],
            mScattering.mv3SkyTopColour[2], mScattering.mfSkyDrk);
    SetQuad(lOutputSkyHorColourPow,
            mScattering.mv3SkyHorColour[0], mScattering.mv3SkyHorColour[1],
            mScattering.mv3SkyHorColour[2], mScattering.mfSkyHorPow);
    SetQuad(lOutputSkySunColourPow,
            mScattering.mv3SkySunColour[0], mScattering.mv3SkySunColour[1],
            mScattering.mv3SkySunColour[2], mScattering.mfSkySunPow);
    // 0x827D0184 `stw r30, 0(r11)` writes the fourth lane of this one as 0.
    SetQuad(lOutputSkyHorBleedSclPow,
            mScattering.mfSkyHorBleedScl, mScattering.mfSkyHorBleedPow,
            mScattering.mfSkySunBleedPow, 0.0f);

    // ---- debug/tool override: drive the scattering colours from the sky colours ---------------
    // 0x827D0194. FALSE at Construct, and only the environment DebugComponent / the light-setup
    // tool flip it, so this arm is off in the shipped flow -- but it is real console code and it
    // WRITES THROUGH to mScattering (three `stvx128 v, r31, 0x590 / 0x5A0 / 0x5B0` plus six `stfs`),
    // so the members read below see the copy. The vector copies move all four lanes.
    if (mbSetScattColsFromSky)
    {
        for (u32 luLane = 0u; luLane < 4u; ++luLane)
        {
            mScattering.mv3ScattTopColour[luLane] = mScattering.mv3SkyTopColour[luLane];
            mScattering.mv3ScattHorColour[luLane] = mScattering.mv3SkyHorColour[luLane];
            mScattering.mv3ScattSunColour[luLane] = mScattering.mv3SkySunColour[luLane];
        }
        mScattering.mfScattDrk         = mScattering.mfSkyDrk;
        mScattering.mfScattHorPow      = mScattering.mfSkyHorPow;
        mScattering.mfScattSunPow      = mScattering.mfSkySunPow;
        mScattering.mfScattHorBleedScl = mScattering.mfSkyHorBleedScl;
        mScattering.mfScattHorBleedPow = mScattering.mfSkyHorBleedPow;
        mScattering.mfScattSunBleedPow = mScattering.mfSkySunBleedPow;
    }

    // ---- the SCATTERING (fog / aerial perspective) half ---------------------------------------
    SetQuad(lOutputScattTopColourDrk,
            mScattering.mv3ScattTopColour[0], mScattering.mv3ScattTopColour[1],
            mScattering.mv3ScattTopColour[2], mScattering.mfScattDrk);
    SetQuad(lOutputScattHorColourPow,
            mScattering.mv3ScattHorColour[0], mScattering.mv3ScattHorColour[1],
            mScattering.mv3ScattHorColour[2], mScattering.mfScattHorPow);
    SetQuad(lOutputScattSunColourPow,
            mScattering.mv3ScattSunColour[0], mScattering.mv3ScattSunColour[1],
            mScattering.mv3ScattSunColour[2], mScattering.mfScattSunPow);
    SetQuad(lOutputScattHorBleedSclPow,
            mScattering.mfScattHorBleedScl, mScattering.mfScattHorBleedPow,
            mScattering.mfScattSunBleedPow, 0.0f);

    // 0x827D02A8-0x827D02E8. The shader evaluates
    //     CalculateScattering = pow(saturate(d*x - y), z) * w
    // so .x = 1/(far - near), .y = near/(far - near), .z = ScattPow, .w = ScattCap.
    const f32 lfInverseScattRange =
        KF_SCATT_RANGE_NUMERATOR / (mScattering.mafScattDist[1] - mScattering.mafScattDist[0]);
    SetQuad(lOutputScattCoeffs,
            lfInverseScattRange,
            mScattering.mafScattDist[0] * lfInverseScattRange,
            mScattering.mfScattPow,
            mScattering.mfScattCap);

    // ---- the key light (sun) direction --------------------------------------------------------
    // 0x827D02EC. Same shape as CalcKeyLightDirection @0x827B0638, except that this one publishes
    // BOTH the clamped direction (what the world is lit by) and the UNBIASED one (the raw time of
    // day; the caller stores it at BrnShaderConstantsFrame+0x300).
    if (mbOverrideKeyLightDirection)
    {
        lOutputKeyLightDirection         = mOverrideKeyLightDirection;
        lOutputUnbiasedKeyLightDirection = mOverrideKeyLightDirection;
    }
    else
    {
        // fsel pair @0x827D0350 / 0x827D0358: min against the upper bound, then max against the
        // lower bound -- clamp into the sun-elevation window (09:00..16:00 by default), so the
        // published sun never sits low enough to light the world from the side.
        f32 lfClampedTimeOfDay = mfTimeOfDay;
        if (lfClampedTimeOfDay > mfSunElevTodUBound)
        {
            lfClampedTimeOfDay = mfSunElevTodUBound;
        }
        if (lfClampedTimeOfDay < mfSunElevTodLBound)
        {
            lfClampedTimeOfDay = mfSunElevTodLBound;
        }

        // The RAW call goes first in the asm (its result is stored to the sp+0xE4 out-param before
        // the second call is set up). The two calls are independent; the order is kept anyway.
        lOutputUnbiasedKeyLightDirection =
            ComputeKeyLightDirection((mfTimeOfDay - KF_SUN_ELEVATION_ZERO_SECONDS)
                                         * KF_RADIANS_PER_SECOND_OF_DAY,
                                     mfSunRigRotation, mfSunTiltAtHorizon, mfSunTiltAtMidday);
        lOutputKeyLightDirection =
            ComputeKeyLightDirection((lfClampedTimeOfDay - KF_SUN_ELEVATION_ZERO_SECONDS)
                                         * KF_RADIANS_PER_SECOND_OF_DAY,
                                     mfSunRigRotation, mfSunTiltAtHorizon, mfSunTiltAtMidday);
    }

    // ---- lighting + clouds: straight 16-byte copies out of the blended sub-blocks --------------
    // (`lvx128 v0, r31, <offset>` / `stvx128 v0, r0, <out>` -- ALL FOUR lanes, which matters
    // because the white-level pass below multiplies all four of them.)
    CopyQuad(lOutputKeyLightColour,         mLighting.mv3KeyLightColour);
    CopyQuad(lOutputKeyLightSpecularColour, mLighting.mv3SpecularColour);
    CopyQuad(lOutputCloud0LiteColour,       mClouds.mav3LayerLiteColour[0]);
    CopyQuad(lOutputCloud1LiteColour,       mClouds.mav3LayerLiteColour[1]);
    CopyQuad(lOutputCloud0DarkColour,       mClouds.mav3LayerDarkColour[0]);
    CopyQuad(lOutputCloud1DarkColour,       mClouds.mav3LayerDarkColour[1]);

    // The three per-layer scalar pairs; `std r30, 0(rX)` zeroes the upper half of each quad.
    SetQuad(lOutputCloudOpacity,
            mClouds.mafLayerOpacity[0],    mClouds.mafLayerOpacity[1],    0.0f, 0.0f);
    SetQuad(lOutputCloudDensity,
            mClouds.mafLayerDensity[0],    mClouds.mafLayerDensity[1],    0.0f, 0.0f);
    SetQuad(lOutputCloudFeathering,
            mClouds.mafLayerFeathering[0], mClouds.mafLayerFeathering[1], 0.0f, 0.0f);

    // 0x827D046C-0x827D04C8. THE ONE REAL SHUFFLE IN THIS FUNCTION, decoded from the two permute
    // masks in .data (DATA_DUMP.md; vperm takes byte i from (vA||vB)[mask[i] & 0x1F]):
    //     unk_82CDA400 = 08090A0B 1C1D1E1F 00010203 00010203
    //     unk_82CDA3C0 = 00010203 00010203 00010203 14151617
    //   v12 = splat(mCloud0Disp.x)                        (vspltw .., 0)
    //   v13 = splat(mCloud0Disp.y)                        (vspltw .., 1)
    //   vperm v13, v12, v13, unk_82CDA3C0     -> (x, x, x, y)
    //   v0  = splat(mafLayerScale[0] * 1/8000)            (lvlx + vspltw .., 0)
    //   vperm v0,  v0,  v0,  unk_82CDA400     -> (s, s, s, s)     [a no-op on a splat]
    //   vsldoi v0, v13, v0, 8                 -> (v13.w2, v13.w3, v0.w0, v0.w1) == (x, y, s, s)
    // so .xy is the accumulated cloud-layer-0 UV OFFSET and .zw its SCALE -- exactly what the name
    // says. (The same vsldoi-8 idiom re-appears at 0x827D0738 over the white level; decoding it
    // there yields (w, w, w, 1), which is the only shape that leaves the ".w is a power, do not
    // scale it" constants intact -- an independent check that this mask decode is right.)
    const f32 lfCloud0Scale = mClouds.mafLayerScale[0] * KF_CLOUD_SCALE_TO_UV;
    SetQuad(lOutputCloud0ScaleAndOffset,
            mCloud0Disp.x, mCloud0Disp.y, lfCloud0Scale, lfCloud0Scale);

    // ---- the irradiance rig -------------------------------------------------------------------
    // 0x827D04CC. Also FALSE at Construct (only the debug component flips it), so it is off in the
    // shipped flow; reconstructed because it is real console code. It OVERWRITES the six fill
    // colours in mLighting that the ComputeIrradiance block below then reads.
    //
    // ARGUMENT ORDER IS FROM THE ASM, NOT FROM MEMORY ORDER: r3..r8 are loaded at 0x827D0500 /
    // 0x827D04F8 / 0x827D04F0 / 0x827D04E8 / 0x827D04E0 / 0x827D04D8 with this+0x610, +0x620,
    // +0x640, +0x630, +0x650, +0x660 -- note r5 = 0x640 (LEFT fill) and r6 = 0x630 (RIGHT fill),
    // i.e. the third and fourth references are swapped relative to the LightingData member order.
    //
    // [FLAG type-bridge] LightingData still stores its colours as `f32[4]`, so the six `Vector3&`
    // out-references the DWARF declares cannot be formed without a reinterpret cast. The six locals
    // below are seeded from the members and written back, which is identical to passing the members
    // directly (nothing else can observe them across the call). RETIRE when LightingData's members
    // are retyped to Vector3 -- see the CROSS-GROUP note in this group's report; the retype also
    // has to move the three keyframe/manager tail pads, so it is deliberately not done here.
    if (mbSetIrradianceFromSky)
    {
        Vector3 lKeyFillColour;     CopyQuad(lKeyFillColour,    mLighting.mv3KeyFillColour);
        Vector3 lShadowFillColour;  CopyQuad(lShadowFillColour, mLighting.mv3ShadowFillColour);
        Vector3 lLeftFillColour;    CopyQuad(lLeftFillColour,   mLighting.mv3LeftFillColour);
        Vector3 lRightFillColour;   CopyQuad(lRightFillColour,  mLighting.mv3RightFillColour);
        Vector3 lUpFillColour;      CopyQuad(lUpFillColour,     mLighting.mv3UpFillColour);
        Vector3 lDownFillColour;    CopyQuad(lDownFillColour,   mLighting.mv3DownFillColour);

        ComputeIrradianceRigFromSky(lKeyFillColour, lShadowFillColour,
                                    lLeftFillColour, lRightFillColour,
                                    lUpFillColour, lDownFillColour,
                                    lOutputSkyTopColourDrk,      // v1
                                    lOutputSkyHorColourPow,      // v2
                                    lOutputSkySunColourPow,      // v3
                                    lOutputSkyHorBleedSclPow,    // v4
                                    lOutputKeyLightDirection);   // v5

        StoreQuad(mLighting.mv3KeyFillColour,    lKeyFillColour);
        StoreQuad(mLighting.mv3ShadowFillColour, lShadowFillColour);
        StoreQuad(mLighting.mv3LeftFillColour,   lLeftFillColour);
        StoreQuad(mLighting.mv3RightFillColour,  lRightFillColour);
        StoreQuad(mLighting.mv3UpFillColour,     lUpFillColour);
        StoreQuad(mLighting.mv3DownFillColour,   lDownFillColour);
    }

    // 0x827D0508-0x827D0620. The six fill colours, each scaled by the ambient scale (the console
    // splats mfAmbientIrradianceScale into six separate stack quads and issues six vmulfp128s),
    // plus the key light direction. Vector-register order v1..v7 = direction, key, shadow, right,
    // left, up, down -- MEMORY order this time (v4 = 0x630 right, v5 = 0x640 left), the opposite of
    // the ComputeIrradianceRigFromSky call above.
    const f32 lfAmbientIrradianceScale = mLighting.mfAmbientIrradianceScale;
    mGlobalIrradianceManager.ComputeIrradiance(
            lOutputKeyLightDirection,
            MakeScaledVector3(mLighting.mv3KeyFillColour,    lfAmbientIrradianceScale),
            MakeScaledVector3(mLighting.mv3ShadowFillColour, lfAmbientIrradianceScale),
            MakeScaledVector3(mLighting.mv3RightFillColour,  lfAmbientIrradianceScale),
            MakeScaledVector3(mLighting.mv3LeftFillColour,   lfAmbientIrradianceScale),
            MakeScaledVector3(mLighting.mv3UpFillColour,     lfAmbientIrradianceScale),
            MakeScaledVector3(mLighting.mv3DownFillColour,   lfAmbientIrradianceScale));

    // 0x827D0624-0x827D06C4. Three calls, colour channel 0/1/2, each into a 64-byte stack matrix
    // that is then copied out four rows at a time. The console alternates between two stack buffers
    // (var_120 for R and B, var_E0 for G) -- pure temporary reuse, no aliasing.
    {
        Matrix44 lIrradianceMatrix;
        lOutputIrradianceMatrixR = mGlobalIrradianceManager.GetIrradianceMatrix(lIrradianceMatrix, 0u);
        lOutputIrradianceMatrixG = mGlobalIrradianceManager.GetIrradianceMatrix(lIrradianceMatrix, 1u);
        lOutputIrradianceMatrixB = mGlobalIrradianceManager.GetIrradianceMatrix(lIrradianceMatrix, 2u);
    }

    // 0x827D070C. `lvx128 v0, r31, 0x7C0` == mGlobalIrradianceManager + 0xC0 == its average colour
    // (DWARF BrnGlobalIrradianceManager.h:91 mAverageIrradianceColour, accessor at :60).
    lOutputAverageIrradianceColour = mGlobalIrradianceManager.GetAverageIrradianceColour();

    // ---- the white-level pass -----------------------------------------------------------------
    // 0x827D0718-0x827D088C. TWO different broadcasts of mfWhiteLevel, and which constant gets which
    // is load-bearing:
    //   (w, w, w, 1)  for the six sky/scattering Vector4s, so the exponent / darkening scalar in .w
    //                 survives untouched;
    //   (w, w, w, w)  for everything that is a pure colour -- key light, specular, the four cloud
    //                 colours, all twelve irradiance-matrix rows and the average irradiance colour.
    // The two BleedSclPow vectors, ScattCoeffs, the three cloud scalar pairs, the cloud scale/offset
    // and BOTH key-light directions are deliberately NOT scaled.
    const f32 lfWhiteLevel = mfWhiteLevel;

    ScaleQuadRGB(lOutputSkyTopColourDrk,   lfWhiteLevel);
    ScaleQuadRGB(lOutputSkyHorColourPow,   lfWhiteLevel);
    ScaleQuadRGB(lOutputSkySunColourPow,   lfWhiteLevel);
    ScaleQuadRGB(lOutputScattTopColourDrk, lfWhiteLevel);
    ScaleQuadRGB(lOutputScattHorColourPow, lfWhiteLevel);
    ScaleQuadRGB(lOutputScattSunColourPow, lfWhiteLevel);

    ScaleQuad(lOutputKeyLightColour,         lfWhiteLevel);
    ScaleQuad(lOutputKeyLightSpecularColour, lfWhiteLevel);
    ScaleQuad(lOutputCloud0LiteColour,       lfWhiteLevel);
    ScaleQuad(lOutputCloud1LiteColour,       lfWhiteLevel);
    ScaleQuad(lOutputCloud0DarkColour,       lfWhiteLevel);
    ScaleQuad(lOutputCloud1DarkColour,       lfWhiteLevel);

    ScaleMatrixRows(lOutputIrradianceMatrixR, lfWhiteLevel);
    ScaleMatrixRows(lOutputIrradianceMatrixG, lfWhiteLevel);
    ScaleMatrixRows(lOutputIrradianceMatrixB, lfWhiteLevel);

    ScaleQuad(lOutputAverageIrradianceColour, lfWhiteLevel);

    // 0x827D0870-0x827D08B4. One more multiply, on the specular colour only, by the file-scope
    // tweakable gfSpecularScale (X360 0x82F307E8; 1.0f in the shipped image). The console builds the
    // splat through a stack quad whose upper three lanes are zeroed and then `vspltw ..,0`, so it is
    // a full four-lane multiply by the scalar.
    ScaleQuad(lOutputKeyLightSpecularColour, gfSpecularScale);

    // 0x827D08B8. Published last and unscaled: the caller needs it to build HDRConstants
    // (w, 1/w, 0, 0) and to put the white level in .w of the fog colour constant.
    lfOutputWhiteLevel = mfWhiteLevel;
}


// =============================================================================================
// SKY WAVE (2026-07-29): the defaults + key-light + junkyard-override slice.
//
//   Construct                    @ 0x827CA408
//   CalcKeyLightDirection        @ 0x827B0638
//   EnableJunkyardLightingSetup  @ 0x827B0F98
//   DisableJunkyardLightingSetup @ 0x827B10E8
//
// All four were quiet no-op gates in WorldLinkStubs.cpp that FIRE at runtime. Every constant
// below was dumped out of the ARTIST image with headless IDA 9.3 -- none is guessed. Their
// values are all readable as times of day and degrees, which is a good independent check:
//   default time of day 46800 s = 13:00, junkyard 64800 s = 18:00, the time-of-day bounds
//   28800..61200 s = 08:00..17:00, and the sun-elevation clamp 32400..57600 s = 09:00..16:00.
// =============================================================================================

// @ 0x827CA408. Seed every default. The X360 also registers five CgsDev debug variables at
// the end ("Override season", "Season to use", "Keyframe to use", "HDR white level",
// "Bloom luminance scale", under the "Environment" group) through a stack DebugInterface.
// FLAG PC-platform leaf: the CgsDev::DebugInterface registration surface is not reconstructed, so
// the debug-variable registrations are omitted; every field they bind is still seeded below.
void EnvironmentManager::Construct()
{
    // ⭐ CORRECTED 2026-08-16 (envstream). Both of these blocks were hand-rolled against the
    // OLD opaque member model and both were wrong; they are now the two real Construct calls
    // the X360 inlines, whose committed homes already carry the asm attestation.
    //
    // (1) The module event-receiver queue. X360 @0x827CA424-0x827CA440 (r3 == this+8):
    //         stw 0x400, 0x10(r3)   -> miCapacity
    //         stw 0x10,  0x14(r3)   -> miAlignment
    //         stw r3+0x18, 0(r3)    -> mpBuffer  == the embedded 1024-byte buffer
    //         bl  CgsModule::BaseEventReceiverQueue::Clear
    //     == CgsModule::EventReceiverQueue<1024,16>::Construct() exactly. The previous code
    //     wrote 1024/16 at this+0x10/this+0x14, which are the queue's COUNT and START OFFSET,
    //     and never Cleared -- so the queue would have reported 1024 pending events to the
    //     very first Prepare that read it.
    mReceiverQueue.Construct();

    // (2) CgsNumeric::Random. X360 @0x827CA444-0x827CA4C4+ is the standard inlined
    //     Random::Construct (seed, mauIntegerBuffer[0] = 1.0f, seven AddRandomFloatToBuffer
    //     refills, then one index bump) -- byte-identical to the one CgsRandom.h homes.
    //     ⚠ The retired hand-rolled copy carried the seed as 0x1AD0891BC87CD8C9, i.e. THE TWO
    //     32-BIT HALVES THE WRONG WAY ROUND: the asm is
    //         lis r10,0x1AD0 / ori r10,0x891B      -> r10 = 0x1AD0891B
    //         lis r9,-0x3784 / ori r9,0xD8C9       -> r9  = 0xC87CD8C9
    //         insrdi r10, r9, 32, 0                -> r9 goes in the HIGH half
    //     == 0xC87CD8C91AD0891B == CgsNumeric::KU_RANDOM_DEFAULT_SEED. This is the second
    //     sighting of exactly the insrdi mistake CgsRandom.h's KU_RANDOM_MULTIPLIER banner
    //     documents. It also refilled the ring from the NEW seed instead of the old one.
    mRandom.Construct();

    mePrepareStage    = E_PREPARE_START;
    meReleaseStage    = E_RELEASE_DONE;

    meStreamOutStage  = E_STREAMOUT_DONE;
    muStreamOutTarget = 0;
    meStreamInStage   = E_STREAMIN_DONE;
    muStreamInTarget  = 0;

    maiSeasons[0]           = -1;
    maiSeasons[1]           = 0;
    miSeasonCurrentlyPlaying = 0;
    miCurrSeason            = 0;
    mfSeasonBlend           = 0.0f;
    mfSeasonBlendDelta      = KF_DEF_SEASON_BLEND_DELTA;
    maiLocations[0]         = 0;
    maiLocations[1]         = 0;
    miCurrLocation          = 0;
    mfLocationBlend         = 1.0f;
    meBlendMode             = E_BLENDMODE_TIMEOFDAY;

    mfTimeOfDay              = KF_DEF_TIME_OF_DAY;
    mfTimeOfDayDelta         = KF_DEF_TIME_OF_DAY_DELTA;
    mfCloudDelta             = KF_DEF_CLOUD_DELTA;
    mfSunRigRotation         = KF_SUN_RIG_ROTATION;
    mfSunTiltAtHorizon       = KF_SUN_TILT_AT_HORIZON;
    mfSunTiltAtMidday        = KF_SUN_TILT_AT_MIDDAY;
    meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_DONE;

    // BlendFrame::Construct, inlined as a 4-iteration loop.
    for (int liIndex = 0; liIndex < 4; ++liIndex)
    {
        mBlendFrame.mapKeyframes[liIndex] = 0;
        mBlendFrame.mafWeights[liIndex]   = 0.0f;
    }

    mScattering.Construct();
    mLighting.Construct();
    mClouds.Construct();

    mfCloudDistanceCurve   = 1.0f;
    mbSetScattColsFromSky  = false;
    mbSetIrradianceFromSky = false;

    mbUseDefaultEffects     = false;
    mbOverrideSeason        = false;
    miOverrideCurrentSeason = 1;
    miOverrideNextSeason    = 1;
    miOverrideKeyframe      = 1;

    mfTimeOfDayUpperBound = KF_DEF_TIME_OF_DAY_UPPER_BOUND;
    mfTimeOfDayLowerBound = KF_DEF_TIME_OF_DAY_LOWER_BOUND;
    mfSunElevTodLBound    = KF_SUN_ELEV_TOD_LOWER_BOUND;
    mfSunElevTodUBound    = KF_SUN_ELEV_TOD_UPPER_BOUND;

    mbJunkyardLightingSetup     = false;
    mbOverrideKeyLightDirection = false;
    mfWhiteLevel                = KF_DEF_WHITE_LEVEL;

    mCloud0Disp.x = 0.0f;
    mCloud0Disp.y = 0.0f;
    mCloud0Disp.z = 0.0f;
    mCloud0Disp.w = 0.0f;
}

// @ 0x827B0638. The world-space key-light (sun) direction for this frame.
//
// When the junkyard override is active the stored direction is returned verbatim; otherwise
// the time of day is first CLAMPED into the sun-elevation window (09:00..16:00) so the sun
// never sits too low, then mapped to an elevation angle by (t - 23400s) * 2*pi/86400 -- i.e.
// seconds of day measured in radians with zero at 06:30 -- and handed to
// EnvironmentSettings::ComputeKeyLightDirection along with the sun rig's three tuning angles.
Vector3 EnvironmentManager::CalcKeyLightDirection() const
{
    if (mbOverrideKeyLightDirection)
    {
        return mOverrideKeyLightDirection;
    }

    // fsel-pair clamp: min against the upper bound first, then max against the lower bound.
    f32 lfClampedTimeOfDay = mfTimeOfDay;
    if (lfClampedTimeOfDay > mfSunElevTodUBound)
    {
        lfClampedTimeOfDay = mfSunElevTodUBound;
    }
    if (lfClampedTimeOfDay < mfSunElevTodLBound)
    {
        lfClampedTimeOfDay = mfSunElevTodLBound;
    }

    const f32 lfKeyLightElevation =
        (lfClampedTimeOfDay - KF_SUN_ELEVATION_ZERO_SECONDS) * KF_RADIANS_PER_SECOND_OF_DAY;

    return ComputeKeyLightDirection(lfKeyLightElevation,
                                    mfSunRigRotation,
                                    mfSunTiltAtHorizon,
                                    mfSunTiltAtMidday);
}

// @ 0x827B0F98. Enter the junkyard's fixed lighting: pin the time of day to 18:00, force the
// key-light direction, and pick the direction from whichever loaded junkyard setup is nearest
// to the camera.
void EnvironmentManager::EnableJunkyardLightingSetup()
{
    CGS_ASSERT(!mbJunkyardLightingSetup, "!mbJunkyardLightingSetup");

    mfTimeBeforeEnteringJunkyard = mfTimeOfDay;

    f32 lfNearestDistance = KF_JUNKYARD_NEAREST_DISTANCE_SENTINEL;

    mbJunkyardLightingSetup     = true;
    mbOverrideKeyLightDirection = true;
    mfTimeOfDayUpperBound       = KF_JUNKYARD_TIME_OF_DAY_SECONDS;
    SetTimeOfDay_Seconds(KF_JUNKYARD_TIME_OF_DAY_SECONDS);
    mOverrideKeyLightDirection  = GetDefaultJunkyardKeyLightDirection();

    for (u32 luIndex = 0; luIndex < muNumJunkyardLightingSetupsLoaded; ++luIndex)
    {
        const JunkyardLighting& lrSetup = maJunkyardLighting[luIndex];

        // Magnitude(camera - setupPosition). The X360 does vmsum3fp128 + vrsqrtefp with two
        // Newton-Raphson refinements and a vsel that maps a zero-length difference to 0
        // rather than NaN; lowered here to the equivalent scalar length.
        const f32 lfDeltaX = mCurrentCameraPosition.x - lrSetup.mJunkyardPosition.x;
        const f32 lfDeltaY = mCurrentCameraPosition.y - lrSetup.mJunkyardPosition.y;
        const f32 lfDeltaZ = mCurrentCameraPosition.z - lrSetup.mJunkyardPosition.z;
        const f32 lfDistanceSquared =
            (lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY) + (lfDeltaZ * lfDeltaZ);
        const f32 lfDistance = (lfDistanceSquared == 0.0f) ? 0.0f : sqrtf(lfDistanceSquared);

        if (lfNearestDistance > lfDistance)
        {
            lfNearestDistance          = lfDistance;
            mOverrideKeyLightDirection = lrSetup.mKeyLightDirection;
        }
    }
}

// @ 0x827B10E8. Leave the junkyard: restore the saved time of day and the default upper bound,
// and drop both overrides.
void EnvironmentManager::DisableJunkyardLightingSetup()
{
    CGS_ASSERT(mbJunkyardLightingSetup, "mbJunkyardLightingSetup");

    SetTimeOfDay_Seconds(mfTimeBeforeEnteringJunkyard);
    mfTimeOfDayUpperBound       = KF_DEF_TIME_OF_DAY_UPPER_BOUND;
    mbJunkyardLightingSetup     = false;
    mbOverrideKeyLightDirection = false;
}

// ============================================================================================
// @ 0x827BE698. Fill the four WORLD-layer effects frames from the environment blend frame,
// once per dispatch pass.
//
// CALLER: WorldModule::GenerateDispatchLists @0x827D1CE8 line 382 (this tree,
// BrnWorldModule.cpp:3836) hands it the world dispatch input's GetEffectsFrame(0..3) --
// four frames because kau8SlotsPerEffectsLayer[KU_EFFECTS_LAYER_WORLD] == 4
// (byte_8203E110 = 01 04 02, DATA_NOTE.md section 1).
//
// SIGNATURE: five GPR arguments in the X360 prologue -- r3 = this, r4..r7 = the four frames.
// Nothing is returned (both exits are a bare `b __restgprlr_19`; Hex-Rays' `__int64 result`
// is r4's dead tail value). No float argument, so no PPC float-slot skew to resolve.
//
// PER SLOT the console runs one of two arms, selected by
//     `if (mbUseDefaultEffects != 0 || mBlendFrame.mapKeyframes[i] == 0)`
// (asm: `lbz r11,0x1170(r3)` / `bne` -> default arm; then `cmplwi r8,0` / `beq` -> default arm).
// X360 0x1170 == mbUseDefaultEffects and X360 0x520/0x530 == mBlendFrame.mapKeyframes/mafWeights --
// the members the committed BrnEnvironmentManager.h already names (the dossier's "keyframe pointers"
// and "weights" ARE the BlendFrame). Those are GUEST offsets: on the x64 host Keyframe*[4] doubles,
// so mBlendFrame sits at 0x528, mafWeights at +0x20 inside it and mbUseDefaultEffects at 0x1190 --
// nothing here (or anywhere in the tree) reaches them by offset, only by name.
//
//   KEYFRAME arm  -- 32 B from kf+0x10 -> frame+0x20 (BloomData), weight -> frame+0x08;
//                    80 B from kf+0x30 -> frame+0x40 (VignetteData), weight -> frame+0x0C;
//                    one word from kf+0x80 -> frame+0x110 (TintData::mpColourCube -- the resolved
//                    ColourCube* the keyframe's bundle import wrote there), weight -> frame+0x18.
//   DEFAULT  arm  -- the SAME three writes, but the Bloom/Vignette values are built fresh on the
//                    stack from the shared post-fx defaults and the two weights are 0.0f
//                    (flt_82001CC0) while the tint weight is 0.25f (flt_82003F40); the colour
//                    cube comes from the manager's own mpDefTintData (+0x1194).
//
// The default arm is BYTE-FOR-BYTE the inlined BrnEffects::BloomData::Construct() +
// BrnEffects::VignetteData::Construct() -- proven by diffing it against
// BrnEffectsFrame::Construct @0x822791E8, which stores the identical constant set in the
// identical member order:
//     BloomData    +0x00 flt_820A3A14 (kfDefLuminance 0.98f)   +0x04 flt_820A3A98 (kfDefThreshold 0.77f)
//                  +0x10 unk_82FFAED0 (kv4DefScale)
//     VignetteData +0x00 flt_820A3A9C (kfDefAngle 0.0f)        +0x04 flt_820A3AA0 (kfDefSharpness 0.33f)
//                  +0x10 unk_82FFAE20 (kv2DefAmount)           +0x20 unk_82FFAEC0 (kv2DefCentre)
//                  +0x30 unk_82FFB220 (kv4DefInnerColour)      +0x40 unk_82FFB1A0 (kv4DefOuterColour)
// so it is written here as those two Construct() calls rather than as re-spelled literals.
// (The 8 padding bytes at BloomData+0x08 and VignetteData+0x08 are NOT written by either arm --
// the console copies them out of the uninitialised stack slot, which is exactly what a
// default-constructed local struct copy does. No behaviour depends on them.)
//
// It does NOT write the six mbUse* bools, and it does not touch DoF / blur / 2d-tint --
// those belong to the base (layer 0) and fx-events (layer 2) producers.
//
// PC NOTE (no deviation, stated for the reader): EnvironmentManager::Prepare and ::Update are
// still inert gates in WorldLinkStubs.cpp and nothing else writes mBlendFrame, so on this build
// every slot takes the "no keyframe" arm and the world layer contributes weight 0. That is the
// console's own "environment settings not loaded" behaviour, not a stand-in.
// mpDefTintData (+0x1194) is written ONLY by EnvironmentManager::Prepare @0x827D49A8
// (`stw r3, 0x1194(r31)` at 0x827D4C10, the POINTER to the default rw::graphics::postfx::ColourCube
// the ResourcePtr at +0x1174 instances); Construct @0x827CA408 does not touch it.
// ⭐ LIVE 2026-08-16 (group tintdata): Prepare's E_PREPARE_WF_ACQUIRE_DEPENDENCIES arm no longer
// HOLDS that publish -- BrnEffects::TintData::mpColourCube is the DWARF's real pointer now, so the
// console's own line is restored and this arm hands the default cube out. Before Prepare runs (or
// on a build where the acquire fails) it is the zero-initialised BSS of the file-scope static
// gGameModule -- which is also what BrnEffects::TintData::Construct() writes -- so the read stays
// deterministic and the null propagates to a null cube, never to a bogus address.
// ============================================================================================
void EnvironmentManager::GenerateEffects(BrnEffectsFrame* lpFrame0, BrnEffectsFrame* lpFrame1,
                                         BrnEffectsFrame* lpFrame2, BrnEffectsFrame* lpFrame3)
{
    BrnEffectsFrame* const lapFrames[4] = { lpFrame0, lpFrame1, lpFrame2, lpFrame3 };

    for (u32 luSlot = 0; luSlot < 4; ++luSlot)
    {
        BrnEffectsFrame* const lpFrame    = lapFrames[luSlot];
        const Keyframe*  const lpKeyframe = mBlendFrame.mapKeyframes[luSlot];
        const f32              lfWeight   = mBlendFrame.mafWeights[luSlot];

        BrnEffects::TintData lTintData;

        if (mbUseDefaultEffects || lpKeyframe == 0)
        {
            BrnEffects::BloomData lBloomData;
            lBloomData.Construct();

            BrnEffects::VignetteData lVignetteData;
            lVignetteData.Construct();

            lTintData.mpColourCube = mpDefTintData;

            lpFrame->SetBloomData(lBloomData, KF_DEF_EFFECTS_LAYER_WEIGHT);
            lpFrame->SetVignetteData(lVignetteData, KF_DEF_EFFECTS_LAYER_WEIGHT);
            lpFrame->SetTintData(lTintData, KF_DEF_EFFECTS_LAYER_TINT_WEIGHT);
        }
        else
        {
            // The keyframe's +0x80 import slot, widened to the host pointer on read (Ptr32::Get(),
            // the project's low-4 GB convention -- see BrnEnvironmentKeyframe.h). One word in, one
            // pointer out; the console's `lwz r11, 0x80(kf)` / `stw r11, 0x110(frame)` verbatim.
            lTintData.mpColourCube = lpKeyframe->mpColourCube.Get();

            lpFrame->SetBloomData(lpKeyframe->mBloom, lfWeight);
            lpFrame->SetVignetteData(lpKeyframe->mVignette, lfWeight);
            lpFrame->SetTintData(lTintData, lfWeight);
        }
    }
}

// ==============================================================================================
// STEP 9, TASK 2 -- THE BLEND HALF OF "THE ENVIRONMENT MANAGER GOES LIVE" (group envblend).
//
//   BrnWorld::EnvironmentSettings::EnvironmentManager::Update              @ 0x827D6060
//   BrnWorld::EnvironmentSettings::EnvironmentManager::SetupBlend          @ 0x827D4FE8
//   BrnWorld::EnvironmentSettings::EnvironmentManager::SetupTimeOfDayBlend @ 0x827D35C0
//   BrnWorld::EnvironmentSettings::EnvironmentManager::SetupSeasonsBlend   @ 0x827D37A0
//   + FindKeyframes / FirstKeyframe / LastKeyframe -- the three helpers the X360 compiler
//     INLINED into the two Setup*Blend bodies (DWARF BrnEnvironmentManager.h:228/:233/:238),
//     de-inlined here under their DWARF names.
//
// HOW THE FOUR FIT TOGETHER (all of it read off the disassembly, not the pseudocode):
//
//   Update       stamps the frame camera position, publishes the cloud-driven WIND VELOCITY into
//                the update output's EffectsEnvironmentInterface under the buffer's WRITE LOCK,
//                and then runs SetupBlend -> (if it filled the frame) PerformBlend.
//   SetupBlend   is a state machine over meBlendMode: TIMEOFDAY -> (on failure) SEASONS ->
//                (on failure) back to TIMEOFDAY, at most a couple of laps; PAUSED means "no
//                blend this frame"; the reserved value 3 is the environment TOOL's blend.
//   SetupTimeOfDayBlend advances the time of day and brackets it between two keyframes.
//   SetupSeasonsBlend   cross-fades a season swap while streaming, pinned to the first/last
//                keyframe of the day.
//
// FOUR THINGS THE ASM SAYS THAT THE WAVE NOTE'S SKETCH DID NOT:
//  1. THERE IS NO DAY WRAP. SetupTimeOfDayBlend REFLECTS: on crossing either bound the time is
//     clamped to that bound and mfTimeOfDayDelta is NEGATED (0x827D3618-0x827D3634). The day
//     runs 08:00 -> 17:00 -> 08:00 -> ... and each turnaround is what triggers a season swap
//     (SetupSeasonsBlend's completion arm re-seeds the time to the opposite bound and negates
//     the delta again, 0x827D398C-0x827D39B0).
//  2. SLOTS 2/3 ARE THE SECOND *LOCATION*, NOT THE SECOND SEASON. Both Setup*Blend bodies weight
//     slots 0/1 by (1 - mfLocationBlend) and slots 2/3 by mfLocationBlend, over
//     maiLocations[miCurrLocation] and maiLocations[1 - miCurrLocation]. mfSeasonBlend only
//     appears in SetupSeasonsBlend, where it separates slot0/slot2 (current season) from
//     slot1/slot3 (the other season) -- a 2x2 (location x season) corner set.
//  3. Update DOES NOT ADVANCE THE TIME OF DAY. That lives in SetupTimeOfDayBlend. Update's only
//     time-of-day write is the junkyard pin (mbJunkyardLightingSetup -> 18:00).
//  4. THE FRAME DELTA IS mrTimeStep (+0x11C4), NOT the float argument. SetupBlend forwards
//     Update's f1 to SetupTimeOfDayBlend, which then IGNORES it and reloads mrTimeStep
//     (0x827D3600 `lfs f12, 0x11C4(r30)`). WorldModule::Update stages mrTimeStep from the
//     world input's sim TimerStatus -- see the BrnWorldModule.cpp edit this group ships.
// ==============================================================================================

// ---------------------------------------------------------------------------------------------
// The three helpers the X360 compiler inlined. DWARF BrnEnvironmentManager.h:228/:233/:238 makes
// them private members of EnvironmentManager; they touch no member state, and declaring them in
// the header would force it to #include SharedClasses/World/BrnEnvironmentTimeLine.h (group
// envdata's file), so they are file-local here. Promote them with that include.
// ---------------------------------------------------------------------------------------------
namespace
{
    // DWARF :228. Bracket lfTime in the location's ascending keyframe-time array and hand back
    // the two keyframes and their two weights. X360: inlined at 0x827D36BC-0x827D36FC and
    // 0x827D3700-0x827D3748 -- FindKeyframeInds on (mpfKeyframeTimes, muKeyframeCnt, lfTime),
    // then each returned index used to look up mppKeyframes.
    void FindKeyframes( Keyframe*& lrpKeyframeA, f32& lrfWeightA,
                        Keyframe*& lrpKeyframeB, f32& lrfWeightB,
                        const TimeLine::LocationData& lrLocation, f32 lfTime )
    {
        u32 luIndexA = 0u;
        u32 luIndexB = 0u;

        FindKeyframeInds( &luIndexA, &lrfWeightA, &luIndexB, &lrfWeightB,
                          lrLocation.mpfKeyframeTimes, lrLocation.muKeyframeCnt, lfTime );

        lrpKeyframeA = lrLocation.mppKeyframes[ luIndexA ];
        lrpKeyframeB = lrLocation.mppKeyframes[ luIndexB ];
    }

    // DWARF :233. X360: inlined at 0x827D39D0-0x827D39F8 -- `lwz r8, 8(entry); lwz r8, 0(r8)`.
    void FirstKeyframe( Keyframe*& lrpKeyframe, const TimeLine::LocationData& lrLocation )
    {
        lrpKeyframe = lrLocation.mppKeyframes[ 0 ];
    }

    // DWARF :238. X360: inlined at 0x827D3910-0x827D3968 -- `slwi r7, cnt, 2; add; lwz r8, -4(r8)`,
    // i.e. mppKeyframes[muKeyframeCnt - 1].
    void LastKeyframe( Keyframe*& lrpKeyframe, const TimeLine::LocationData& lrLocation )
    {
        lrpKeyframe = lrLocation.mppKeyframes[ lrLocation.muKeyframeCnt - 1u ];
    }
}


// =============================================================================================
// @ 0x827D6060. The per-frame environment tick.
//
// SIGNATURE (DWARF :103 `void Update(float32_t, UpdateOutputBuffer*, Vector3)`, confirmed
// against the X360 prologue): r3 = this, f1 = the float (its GPR slot r4 is RESERVED AND
// SKIPPED -- the PPC float-arg rule), r5 = UpdateOutputBuffer*, v1 = the Vector3. The committed
// header already spells the float `lfPlayerSpeed`, which is what WorldModule::Update passes
// (mfLocalPlayerActiveRaceCarSpeed); this body only forwards it to SetupBlend, and every
// downstream reader ignores it. Kept as-is.
//
// STORE-FOR-STORE:
//   0x827D608C  lpOutput->LockForWrite()
//   0x827D6098  stvx128 v127, r31, 0x1C50      mCurrentCameraPosition = lCameraPosition
//   0x827D609C  r3 = lpOutput->GetEffectsEnvironmentInterface()      (the WRITE accessor)
//   0x827D60A0  if (meBlendMode == 2 /*E_BLENDMODE_PAUSED*/) -> publish a ZERO wind velocity
//               else -> the cloud block below, then the junkyard time-of-day pin
//   0x827D63A0  lpOutput->UnlockForWrite()
//   0x827D63B8  if (SetupBlend(mBlendFrame, lfPlayerSpeed, lpOutput)) PerformBlend(mBlendFrame)
//
// THE CLOUD BLOCK (0x827D60AC-0x827D6354) is one long VMX stanza that Hex-Rays cannot render.
// Decoded from the disassembly it is:
//     angle   = mClouds.mfDirectionAngle * (pi/180)                 [flt_820CA158 = 0.0174533]
//     x       = angle - 2*pi * round(angle / (2*pi))                [unk_82000C60 lanes 1 and 3
//                                                                    = 6.28319 and 0.159155]
//     s = sin(x), c = cos(x)  -- evaluated as the two 12-term odd/even Taylor polynomials whose
//     coefficient vectors are unk_82000BD0/BE0/BF0 (1, -1/6, 1/120, -1/5040, 1/9!, ... : SIN)
//     and unk_82000C00/C10/C20 (1, -1/2, 1/24, -1/720, 1/8!, ... : COS). This is the inlined
//     XMScalarSinCos range-reduce + polynomial, so it is written back as cosf/sinf.
//     `vperm v0, v0(cos), v3(sin), unk_82CDA350` with control 00010203 14151617 00010203
//     00010203 packs them as (c, s, c, c) -- so lanes z and w carry the COSINE, reproduced.
//     step    = (c,s,c,c) * mClouds.mafLayerSpeed[0] * mfCloudDelta * mrTimeStep
//     mCloud0Disp += step                                          [lvx/vaddfp/stvx at +0x11B0]
//     wind     = step * (1/3)                                      [vrefp + 2 Newton-Raphson
//                                                                   refinements of flt_820CA5B0[0]
//                                                                   = 3.0, then vmulfp]
//     if (mbJunkyardLightingSetup) mfTimeOfDay = 64800 s (18:00)   [flt_82F307F0]
// The junkyard pin is INSIDE the non-paused arm only (0x827D6358 sits after the VMX block and
// the paused arm branches straight past it to the unlock).
//
// PC NOTE: nothing else in this build writes mCloud0Disp, and the sky/cloud shader that consumes
// it is not wired yet, so the accumulator is currently write-only -- the observable half of this
// arm is the wind velocity, which the effects module reads. Stated, not worked around.
// =============================================================================================
void EnvironmentManager::Update( f32 lfPlayerSpeed, BrnWorldIO::UpdateOutputBuffer* lpOutput,
                                 Vector3 lCameraPosition )
{
    lpOutput->LockForWrite();

    mCurrentCameraPosition = lCameraPosition;

    BrnEffects::EffectsEnvironmentInterface* const lpEffectsEnvironment =
        lpOutput->GetEffectsEnvironmentInterface();

    if ( meBlendMode == E_BLENDMODE_PAUSED )
    {
        // Paused: the clouds do not move, so the published wind is exactly zero
        // (flt_82001CC0 stored into all four lanes, 0x827D6374-0x827D6398).
        Vector2 lWindVelocity;
        lWindVelocity.x = KF_ZERO;
        lWindVelocity.y = KF_ZERO;
        lWindVelocity.z = KF_ZERO;
        lWindVelocity.w = KF_ZERO;
        lpEffectsEnvironment->SetWindVelocity( lWindVelocity );
    }
    else
    {
        const f32 lfAngleRadians = mClouds.GetDirectionAngle() * KF_DEGREES_TO_RADIANS;
        const f32 lfCosAngle     = std::cos( lfAngleRadians );
        const f32 lfSinAngle     = std::sin( lfAngleRadians );

        // The scale chain in the console's multiply order: trig * layer-0 speed, then the
        // clouds' per-frame delta, then the frame time step.
        const f32 lfLayerSpeed = mClouds.GetLayerSpeed( 0 );

        Vector3 lCloudStep;
        lCloudStep.x = ( ( lfCosAngle * lfLayerSpeed ) * mfCloudDelta ) * mrTimeStep;
        lCloudStep.y = ( ( lfSinAngle * lfLayerSpeed ) * mfCloudDelta ) * mrTimeStep;
        lCloudStep.z = lCloudStep.x;   // the vperm control repeats the COSINE in lanes z and w
        lCloudStep.w = lCloudStep.x;

        mCloud0Disp.x += lCloudStep.x;
        mCloud0Disp.y += lCloudStep.y;
        mCloud0Disp.z += lCloudStep.z;
        mCloud0Disp.w += lCloudStep.w;

        Vector2 lWindVelocity;
        lWindVelocity.x = lCloudStep.x / KF_CLOUD_WIND_DIVISOR;
        lWindVelocity.y = lCloudStep.y / KF_CLOUD_WIND_DIVISOR;
        lWindVelocity.z = lCloudStep.z / KF_CLOUD_WIND_DIVISOR;
        lWindVelocity.w = lCloudStep.w / KF_CLOUD_WIND_DIVISOR;
        lpEffectsEnvironment->SetWindVelocity( lWindVelocity );

        if ( mbJunkyardLightingSetup )
        {
            SetTimeOfDay_Seconds( KF_JUNKYARD_TIME_OF_DAY_SECONDS );
        }
    }

    lpOutput->UnlockForWrite();

    if ( SetupBlend( mBlendFrame, lfPlayerSpeed, lpOutput ) )
    {
        PerformBlend( mBlendFrame );
    }

    // ---- [FLAG PC bring-up diagnostic] the one line that proves the chain -------------------
    // Sampled every 500 calls, capped at eight lines: the blend is a per-frame steady state, so
    // a per-frame line would flood BrnGame.log while a one-shot would miss the drift that is the
    // whole point. Reads only state this function has just settled. DELETE with the bring-up.
    {
        static u32 suCalls  = 0u;
        static u32 suPrints = 0u;
        const u32  luFrame  = suCalls++;
        if ( ( luFrame % 500u ) == 0u && suPrints < 8u )
        {
            ++suPrints;

            u32 luHours   = 0u;
            u32 luMinutes = 0u;
            u32 luSeconds = 0u;
            HH_MM_SS( &luHours, &luMinutes, &luSeconds, mfTimeOfDay );

            char lacMsg[ 256 ];
            std::snprintf( lacMsg, sizeof( lacMsg ),
                           "[env] tod=%.1fs (%02u:%02u) blend=[%.2f %.2f %.2f %.2f]"
                           " kf=[%p %p %p %p] season=%d loc=%d mode=%d\n",
                           static_cast<double>( mfTimeOfDay ),
                           static_cast<unsigned>( luHours ),
                           static_cast<unsigned>( luMinutes ),
                           static_cast<double>( mBlendFrame.mafWeights[ 0 ] ),
                           static_cast<double>( mBlendFrame.mafWeights[ 1 ] ),
                           static_cast<double>( mBlendFrame.mafWeights[ 2 ] ),
                           static_cast<double>( mBlendFrame.mafWeights[ 3 ] ),
                           static_cast<const void*>( mBlendFrame.mapKeyframes[ 0 ] ),
                           static_cast<const void*>( mBlendFrame.mapKeyframes[ 1 ] ),
                           static_cast<const void*>( mBlendFrame.mapKeyframes[ 2 ] ),
                           static_cast<const void*>( mBlendFrame.mapKeyframes[ 3 ] ),
                           static_cast<int>( miCurrSeason ),
                           static_cast<int>( miCurrLocation ),
                           static_cast<int>( meBlendMode ) );
            CgsDev::Log::WriteToLog( lacMsg );
        }
    }
}


// =============================================================================================
// @ 0x827D4FE8. The blend-mode state machine.
//
// SIGNATURE (DWARF :197 `bool SetupBlend(BlendFrame&, float32_t, UpdateOutputBuffer*)`): the
// caller loads r3=this, r4=&mBlendFrame, f1=the float, r6=lpOutput (0x827D63A4-0x827D63B8) --
// r5 is RESERVED AND SKIPPED for the float, which is why the float is the SECOND parameter and
// the buffer the third. Hex-Rays' 4-GPR prototype is wrong.
//
// switch (meBlendMode) -- a 4-entry jump table at 0x827D5070 with everything > 3 falling into
// the retry counter:
//   0 E_BLENDMODE_TIMEOFDAY : the debug season/keyframe override arm (mbOverrideSeason), then
//                             SetupTimeOfDayBlend; on success return true. On failure rewind
//                             meSetupSeasonsBlendStage to START, set meBlendMode = SEASONS and
//                             FALL THROUGH (the console does not re-read the switch).
//   1 E_BLENDMODE_SEASONS   : SetupSeasonsBlend; on success return true, else meBlendMode =
//                             TIMEOFDAY and go round again.
//   2 E_BLENDMODE_PAUSED    : return false (Update then skips PerformBlend).
//   3 (reserved, TOOL)      : SetupUpdateFromToolBlend, return true. NOTE: this value is NOT in
//                             the DecFIGS EBlendMode enum (which stops at PAUSED = 2); it is
//                             the X360's reserved blocking value, and the committed
//                             UpdateFromTool @0x827B0DA8 already writes it. Named
//                             KI_BLENDMODE_TOOL rather than invented as an enumerator.
// The loop is UNBOUNDED on the console: the retry counter only fires an assert once it has gone
// round twice (`cmplwi r16,2 / addi r16,r16,1 / blt`, compare BEFORE the increment) and then
// branches back into the switch anyway (0x827D5268 `b loc_827D504C`). Reproduced exactly.
// =============================================================================================
bool EnvironmentManager::SetupBlend( BlendFrame& lrBlendFrame, f32 lfTimeStep,
                                     BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    u32 luRetries = 0u;

    for ( ;; )
    {
        switch ( meBlendMode )
        {
        case E_BLENDMODE_TIMEOFDAY:
            if ( mbOverrideSeason )
            {
                // [FLAG] DEBUG-ONLY ARM, and NOT REACHED in the shipped flow: Construct
                // @0x827CA408 clears mbOverrideSeason and only the EnvironmentSettings
                // DebugComponent sets it.
                // [FLAG BLOCKED: the CgsDev::DebugInterface registration surface is not
                // reconstructed] -- the console builds a stack DebugInterface here and calls
                // sub_8282F910(&iface, &miOverrideKeyframe, 1, muKeyframeCnt), the same
                // "register an int debug variable with min 1 / max = keyframe count" call
                // Construct's own registration block makes (that block is already omitted in
                // this TU with the same FLAG). The registration is ALSO where the [1, cnt]
                // clamp on miOverrideKeyframe lives, so the index below is unclamped exactly
                // as it would be here with the registration dropped. Restore both together.
                const TimeLine* const lpTimeLine =
                    ResourcePointerAssertThingy( maSeasonPtrs[ miCurrSeason ] );
                const TimeLine::LocationData& lrLocation =
                    lpTimeLine->mpLocationDatii[ maiLocations[ miCurrLocation ] ];

                mfTimeOfDay = lrLocation.mpfKeyframeTimes[ miOverrideKeyframe - 1 ];

                if ( miOverrideCurrentSeason != miOverrideNextSeason )
                {
                    CGS_ASSERT( miOverrideNextSeason > 0, "miOverrideNextSeason > 0" );
                    CGS_ASSERT( miOverrideNextSeason
                                    <= static_cast<s32>( mDictionaryPtr.GetMemoryResource()->muSeasonCnt ),
                                "miOverrideNextSeason <= (int32_t)((const Dictionary*)"
                                "mDictionaryPtr.GetMemoryResource())->muSeasonCnt" );
                    miOverrideCurrentSeason = miOverrideNextSeason;
                }
                else if ( SetupTimeOfDayBlend( lrBlendFrame, lpOutput, lfTimeStep ) )
                {
                    return true;
                }
            }
            else if ( SetupTimeOfDayBlend( lrBlendFrame, lpOutput, lfTimeStep ) )
            {
                return true;
            }

            meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_START;
            meBlendMode              = E_BLENDMODE_SEASONS;
            // FALL THROUGH -- the console drops straight into the seasons arm (0x827D51E8)
            // without re-reading meBlendMode.

        case E_BLENDMODE_SEASONS:
            if ( SetupSeasonsBlend( lrBlendFrame, lpOutput ) )
            {
                return true;
            }
            meBlendMode = E_BLENDMODE_TIMEOFDAY;
            break;

        case E_BLENDMODE_PAUSED:
            return false;

        case KI_BLENDMODE_TOOL:
            SetupUpdateFromToolBlend( lrBlendFrame );
            return true;

        default:
            break;
        }

        if ( luRetries++ >= 2u )
        {
            CGS_ASSERT( false, "Incorrect time keys fed to the environment manager" );
        }
    }
}


// =============================================================================================
// @ 0x827D35C0. Advance the time of day, then bracket it.
//
// SIGNATURE (DWARF :207 `bool SetupTimeOfDayBlend(BlendFrame&, UpdateOutputBuffer*, float32_t)`):
// the call site loads r3=this, r4=&mBlendFrame, r5=lpOutput, f1=the float (0x827D510C-0x827D511C)
// -- three GPRs then the FPR, so the float is the THIRD parameter. The body NEVER READS IT
// (0x827D366C overwrites f1 from mfTimeOfDay before the first use); the frame delta it actually
// scales by is mrTimeStep. Kept in the signature because the console passes it.
//
// STORE-FOR-STORE:
//   0x827D35DC  if (StreamOut(lpOutput)) StreamIn(lpOutput);
//   0x827D3604  tod' = mfTimeOfDayDelta * mrTimeStep + mfTimeOfDay;  mfTimeOfDay = tod'
//   0x827D3610  if (tod' <  mfTimeOfDayLowerBound) { mfTimeOfDay = lower; delta = -delta; }
//   0x827D3624  else if (tod' > mfTimeOfDayUpperBound) { mfTimeOfDay = upper; delta = -delta; }
//               -- note BOTH clamps share the single `fneg` at 0x827D3630, and the negation
//                  uses the ORIGINAL delta (f13, loaded at 0x827D35FC).
//   0x827D3638  re-read mfTimeOfDay; if (lower > tod || tod > upper) return false
//   0x827D3650  lpTimeLine = maSeasonPtrs[miCurrSeason].GetMemoryResource()
//   0x827D3690  locA = maiLocations[miCurrLocation], locB = maiLocations[1 - miCurrLocation]
//               (the asm forms the two indices as (miCurrLocation + 314)*4 and
//                (315 - miCurrLocation)*4 off `this` -- i.e. the 2-entry array at +0x4E8)
//   0x827D36C4  FindKeyframes(...slots 0/1..., lpTimeLine->mpLocationDatii[locA], mfTimeOfDay)
//   0x827D370C  FindKeyframes(...slots 2/3..., lpTimeLine->mpLocationDatii[locB], mfTimeOfDay)
//   0x827D374C  w0 *= (1 - mfLocationBlend); w1 *= (1 - mfLocationBlend);
//               w2 *= mfLocationBlend;       w3 *= mfLocationBlend;      return true
//
// GUEST-STRIDE WARNING (rule 1): the console reaches LocationData as `mpLocationDatii + 12*i`
// -- 12 is the GUEST sizeof(LocationData) (u32 + 2 x 4-byte pointers). On x64 it is 24 with
// padding, so the index MUST go through the typed array, which is what the line below does.
// Nothing here reaches a member by offset.
// =============================================================================================
bool EnvironmentManager::SetupTimeOfDayBlend( BlendFrame& lrBlendFrame,
                                              BrnWorldIO::UpdateOutputBuffer* lpOutput,
                                              f32 lfTimeStep )
{
    // The console passes this and never reads it (see the banner); named to keep the DWARF
    // signature honest.
    (void)lfTimeStep;

    if ( StreamOut( lpOutput ) )
    {
        StreamIn( lpOutput );
    }

    const f32 lfTimeOfDayDelta   = mfTimeOfDayDelta;
    const f32 lfAdvancedTimeOfDay = ( lfTimeOfDayDelta * mrTimeStep ) + mfTimeOfDay;

    mfTimeOfDay = lfAdvancedTimeOfDay;

    if ( lfAdvancedTimeOfDay < mfTimeOfDayLowerBound )
    {
        // REFLECT, not wrap: clamp to the bound and reverse the direction of time.
        mfTimeOfDay      = mfTimeOfDayLowerBound;
        mfTimeOfDayDelta = -lfTimeOfDayDelta;
    }
    else if ( lfAdvancedTimeOfDay > mfTimeOfDayUpperBound )
    {
        mfTimeOfDay      = mfTimeOfDayUpperBound;
        mfTimeOfDayDelta = -lfTimeOfDayDelta;
    }

    if ( mfTimeOfDayLowerBound > mfTimeOfDay || mfTimeOfDay > mfTimeOfDayUpperBound )
    {
        return false;
    }

    const TimeLine* const lpTimeLine = ResourcePointerAssertThingy( maSeasonPtrs[ miCurrSeason ] );

    const TimeLine::LocationData& lrLocationA =
        lpTimeLine->mpLocationDatii[ maiLocations[ miCurrLocation ] ];
    const TimeLine::LocationData& lrLocationB =
        lpTimeLine->mpLocationDatii[ maiLocations[ 1 - miCurrLocation ] ];

    FindKeyframes( lrBlendFrame.mapKeyframes[ 0 ], lrBlendFrame.mafWeights[ 0 ],
                   lrBlendFrame.mapKeyframes[ 1 ], lrBlendFrame.mafWeights[ 1 ],
                   lrLocationA, mfTimeOfDay );

    FindKeyframes( lrBlendFrame.mapKeyframes[ 2 ], lrBlendFrame.mafWeights[ 2 ],
                   lrBlendFrame.mapKeyframes[ 3 ], lrBlendFrame.mafWeights[ 3 ],
                   lrLocationB, mfTimeOfDay );

    lrBlendFrame.mafWeights[ 0 ] = ( KF_ONE - mfLocationBlend ) * lrBlendFrame.mafWeights[ 0 ];
    lrBlendFrame.mafWeights[ 1 ] = ( KF_ONE - mfLocationBlend ) * lrBlendFrame.mafWeights[ 1 ];
    lrBlendFrame.mafWeights[ 2 ] = mfLocationBlend * lrBlendFrame.mafWeights[ 2 ];
    lrBlendFrame.mafWeights[ 3 ] = mfLocationBlend * lrBlendFrame.mafWeights[ 3 ];

    return true;
}


// =============================================================================================
// @ 0x827D37A0. The season-swap stage machine + the 2x2 (location x season) corner blend.
//
// SIGNATURE (DWARF :212 `bool SetupSeasonsBlend(BlendFrame&, UpdateOutputBuffer*)`): three GPRs,
// no float (0x827D51E8-0x827D51F4).
//
// The CURRENT season's timeline is fetched BEFORE the switch (0x827D37C0-0x827D37D0) and the
// "other" timeline starts out aliased to it (`mr r29, r28` at 0x827D37E8) -- which is what the
// still-streaming stages and the default arm fall through with.
//
// switch (meSetupSeasonsBlendStage) -- a 5-entry table at 0x827D3810:
//   0 START        : mfSeasonBlend = 0; fall through
//   1 WF_STREAMOUT : stage = 1; if (!StreamOut(lpOutput)) -> the blend tail; else fall through
//   2 WF_STREAMIN  : stage = 2; if (!StreamIn(lpOutput))  -> the blend tail; else fall through
//   3 BLEND        : stage = 3; mfSeasonBlend += (mbOverrideSeason ? 1.0 : mfSeasonBlendDelta)
//                    if (mfSeasonBlend > 1.0):
//                        DiscardCurrSeason(); miCurrSeason = 1 - miCurrSeason;
//                        if (!mbOverrideSeason)
//                            mfTimeOfDay = (mfTimeOfDayDelta > 0) ? upper bound : lower bound;
//                        mfTimeOfDayDelta = -mfTimeOfDayDelta;
//                        stage = DONE; return false;      <-- the ONLY false exit
//                    else the "other" timeline = maSeasonPtrs[1 - miCurrSeason] -> the tail
//                    (asm 0x827D389C: `r11 = miCurrSeason<<5; r11 = this - r11; r3 = r11 + 0x4B0`
//                     == &maSeasonPtrs[1 - miCurrSeason] for miCurrSeason in {0,1})
//   4 DONE         : stage = DONE; return false
//   default        : the blend tail
//
// THE TAIL (0x827D38B4): four corners of (location A/B) x (current/other season), each pinned to
// the FIRST keyframe of the day when time is running BACKWARDS (mfTimeOfDayDelta <= 0) and the
// LAST when it is running forwards, with the separable weights
//   w0 = (1-loc)*(1-season)   w1 = (1-loc)*season   w2 = (1-season)*loc   w3 = season*loc
// (0x827D39FC-0x827D3A4C, in that expression order).
// =============================================================================================
bool EnvironmentManager::SetupSeasonsBlend( BlendFrame& lrBlendFrame,
                                            BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    const TimeLine* const lpCurrTimeLine =
        ResourcePointerAssertThingy( maSeasonPtrs[ miCurrSeason ] );
    const TimeLine*       lpNextTimeLine = lpCurrTimeLine;

    switch ( meSetupSeasonsBlendStage )
    {
    case E_SETUPSEASONSBLEND_START:
        mfSeasonBlend = KF_ZERO;
        // FALL THROUGH

    case E_SETUPSEASONSBLEND_WF_STREAMOUT:
        meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_WF_STREAMOUT;
        if ( !StreamOut( lpOutput ) )
        {
            break;
        }
        // FALL THROUGH

    case E_SETUPSEASONSBLEND_WF_STREAMIN:
        meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_WF_STREAMIN;
        if ( !StreamIn( lpOutput ) )
        {
            break;
        }
        // FALL THROUGH

    case E_SETUPSEASONSBLEND_BLEND:
        meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_BLEND;

        // The debug override drives the cross-fade straight to the end in one frame.
        mfSeasonBlend = mbOverrideSeason ? ( mfSeasonBlend + KF_ONE )
                                         : ( mfSeasonBlendDelta + mfSeasonBlend );

        if ( mfSeasonBlend > KF_ONE )
        {
            DiscardCurrSeason();
            miCurrSeason = 1 - miCurrSeason;

            if ( !mbOverrideSeason )
            {
                // The new season starts at the far end of the day, and time reverses --
                // this is the other half of SetupTimeOfDayBlend's reflection.
                mfTimeOfDay = ( mfTimeOfDayDelta > KF_ZERO ) ? mfTimeOfDayUpperBound
                                                             : mfTimeOfDayLowerBound;
            }
            mfTimeOfDayDelta = -mfTimeOfDayDelta;

            meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_DONE;
            return false;
        }

        lpNextTimeLine = ResourcePointerAssertThingy( maSeasonPtrs[ 1 - miCurrSeason ] );
        break;

    case E_SETUPSEASONSBLEND_DONE:
        meSetupSeasonsBlendStage = E_SETUPSEASONSBLEND_DONE;
        return false;

    default:
        break;
    }

    const TimeLine::LocationData* const lpCurrLocations = lpCurrTimeLine->mpLocationDatii;
    const TimeLine::LocationData* const lpNextLocations = lpNextTimeLine->mpLocationDatii;

    const s32 liLocationA = maiLocations[ miCurrLocation ];
    const s32 liLocationB = maiLocations[ 1 - miCurrLocation ];

    if ( mfTimeOfDayDelta > KF_ZERO )
    {
        LastKeyframe( lrBlendFrame.mapKeyframes[ 0 ], lpCurrLocations[ liLocationA ] );
        LastKeyframe( lrBlendFrame.mapKeyframes[ 1 ], lpNextLocations[ liLocationA ] );
        LastKeyframe( lrBlendFrame.mapKeyframes[ 2 ], lpCurrLocations[ liLocationB ] );
        LastKeyframe( lrBlendFrame.mapKeyframes[ 3 ], lpNextLocations[ liLocationB ] );
    }
    else
    {
        FirstKeyframe( lrBlendFrame.mapKeyframes[ 0 ], lpCurrLocations[ liLocationA ] );
        FirstKeyframe( lrBlendFrame.mapKeyframes[ 1 ], lpNextLocations[ liLocationA ] );
        FirstKeyframe( lrBlendFrame.mapKeyframes[ 2 ], lpCurrLocations[ liLocationB ] );
        FirstKeyframe( lrBlendFrame.mapKeyframes[ 3 ], lpNextLocations[ liLocationB ] );
    }

    lrBlendFrame.mafWeights[ 0 ] = ( KF_ONE - mfLocationBlend ) * ( KF_ONE - mfSeasonBlend );
    lrBlendFrame.mafWeights[ 1 ] = ( KF_ONE - mfLocationBlend ) * mfSeasonBlend;
    lrBlendFrame.mafWeights[ 2 ] = ( KF_ONE - mfSeasonBlend ) * mfLocationBlend;
    lrBlendFrame.mafWeights[ 3 ] = mfSeasonBlend * mfLocationBlend;

    return true;
}


// =============================================================================================
// ENVSTREAM WAVE (2026-08-16): the environment manager's STREAMING half.
//
//   Prepare                    @ 0x827D49A8   (JSON)
//   StreamIn                   @ 0x827D31E8   (JSON)
//   StreamOut                  @ 0x827D2EB0   (HOLES_DUMP.md -- no JSON, dumped by the conductor)
//   RequestNextSeason          @ 0x827C4EA0   (HOLES_DUMP.md)
//   ReadJunkyardLightingData   @ 0x827BEBD8   (JSON)
//
// All five are switch-per-frame stage machines over the DWARF enums now declared in the header
// (EPrepareStage / EStreamInStage / EStreamOutStage), except ReadJunkyardLightingData which is a
// straight text parser. Each stage does ONE step under the output buffer's write lock and the
// function returns; the caller (WorldModule::Prepare stage 5 / SetupTimeOfDayBlend /
// SetupSeasonsBlend) re-enters next frame until the terminal stage returns true.
//
// The four resource-request ids the machines assert on -- 2 (LoadBundle reply), 3 (UnloadBundle
// reply), 4 (AcquireResource reply) -- are the X360 immediates AND the ids the committed PC
// resource pipe posts, so this whole chain is live on PC without a translation layer:
//   CgsResourceBundleLoaderModule.cpp:159   AddEvent(&LoadBundleResponse,   2, ...)
//   CgsResourceBundleLoaderModule.cpp:197   AddEvent(&UnloadBundleResponse, 3, ...)
//   CgsResourceModule.cpp:204               pool tag 6 -> receiver id 4 (acquire reply)
//
// PC BRING-UP SEAMS (both banner-flagged at their definitions, nothing else deviates):
//   * PCBringUpWaitForResources -- a BOUNDED wait. The console waits forever for a reply it is
//     guaranteed to get; on PC a pool id that does not exist, or a bundle the converter has not
//     produced yet, would hang WorldModule::Prepare stage 5 forever (= a black screen, no
//     assert). Times the wait out, logs once, and lets the machine finish so the boot proceeds.
//   * PCBringUpJunkyardFile -- stands in for rw::core::filesys::AsyncOp Open/GetStatus/Read/
//     GetResultHandle/GetResultSize/Close, which Prepare stages 11..14 drive. That whole layer
//     is a LINK STUB on this build (SDKs/EATech/AptRenderLinkStubs.cpp:535
//     `Device* Device::GetInstance(...) { return nullptr; }`), and AsyncOp::Open @ asyncop.cpp:230
//     dereferences the returned Device unconditionally -- so calling it is an immediate AV, not a
//     degraded read. The four console stages are kept intact around a synchronous std::fopen.
// =============================================================================================

namespace
{
    // ---- Prepare's baked resource paths / ids (X360 rodata, verbatim) ----------------------
    // aPostfxColourcu  (Prepare @0x827D4A78)
    const char* const KAC_COLOUR_CUBE_DICTIONARY_BUNDLE = "PostFx/colourcubedictionary.bin";
    // aGamedbBurnout5_12 (Prepare @0x827D4B3C)
    const char* const KAC_DEFAULT_COLOUR_CUBE_RESOURCE =
        "gamedb://burnout5/Playground/PostFx/ColourCubeDictionary/rgb_colourcube.tga.ImageFile?ID=217407";
    // aEnvironmentset_0 (Prepare @0x827D4C2C) -- the console's backslash is kept verbatim.
    const char* const KAC_ENVIRONMENT_DICTIONARY_BUNDLE = "EnvironmentSettings\\Dictionary.Bundle";
    // off_82F307EC (Prepare @0x827D4E4C) -- the pointer the AsyncOp::Open call loads.
    const char* const KAC_JUNKYARD_LIGHTING_DATA_PATH = "ENVIRONMENTSETTINGS/JUNKYARDLIGHTING.DAT";
    // aDSeasonTxt / aR / aS (RequestNextSeason @0x827C4F08 / @0x827C4F00 / @0x827C4F28)
    const char* const KAC_SEASON_OVERRIDE_FILE = "d:\\Season.txt";

    // Pool ids -- X360 immediates (Prepare `li r6,0xA` / `li r6,0x10`, StreamIn/StreamOut `li r6,0x10`).
    const s32 KI_POOL_POSTFX      = 10;
    const s32 KI_POOL_ENVIRONMENT = 16;

    // Receiver-queue event ids (see the TU banner for both attestations).
    const s32 KI_EVENT_LOAD_BUNDLE_RESPONSE      = 2;
    const s32 KI_EVENT_UNLOAD_BUNDLE_RESPONSE    = 3;
    const s32 KI_EVENT_ACQUIRE_RESOURCE_RESPONSE = 4;

    // The pool request-queue tag an AcquireResourceRequest is posted under
    // (CgsModule::VariableEventQueue<4096,16>::AddEvent(..., 4, 24) in the X360).
    const s32 KI_REQUEST_ACQUIRE_RESOURCE = 4;

    // Prepare stage 7 builds the dictionary's resource name into a stack buffer and hashes it.
    // The console's slot spans to the top of the frame (Hex-Rays sizes it 384); the only writer,
    // Dictionary::BuildResourceName @0x827B03B8, memcpy's the 15-byte literal "ENV_DICTIONARY".
    // 128 == Dictionary::SeasonData::macResourceName, the sibling name buffer. FLAG: the exact
    // console array size is not attested, only that it is >= 15.
    const s32 KI_ENV_RESOURCE_NAME_MAX = 128;

    // The 3 * sizeof(Matrix44) the switch's shared not-finished tail clears (X360 `li r5, 0xC0`).
    const size_t KU_IRRADIANCE_MATRICES_BYTES = 192u;

    // ReadJunkyardLightingData's key-light elevation clamp. X360 rodata (DATA_DUMP.md):
    //   flt_820CBD70 = 3F7D2F1B = +0.989   (loaded @0x827BEC50 into f30)
    //   flt_820CBDB0 = BF7D2F1B = -0.989   (loaded @0x827BEC40 into f29)
    const f32 KF_JUNKYARD_MAX_KEY_LIGHT_Y =  0.98900002f;
    const f32 KF_JUNKYARD_MIN_KEY_LIGHT_Y = -0.98900002f;

    // The X360's shared "Prepare has not finished this frame" tail (def_827D4A14 @0x827D4FCC):
    //     memset(this + 0x700, 0, 0xC0);  return 0;
    // this+0x700 is mGlobalIrradianceManager and 0xC0 is exactly its maIrradianceMatrix[3]
    // (the array stops before mAverageIrradianceColour @+0xC0). GlobalIrradianceManager::Construct
    // has NO standalone export -- it is inlined here -- so this IS that Construct, and the
    // manager's three matrices are held at zero for every frame the environment is not yet
    // prepared. Written against the header's CURRENT opaque byte-array member.
    // (envconst re-typed +0x700 to `BrnWorld::GlobalIrradianceManager mGlobalIrradianceManager` in
    // this same wave, so the memset lands on the manager BY NAME -- the console's `li r5,0xC0 /
    // li r4,0 / addi r3,r31,0x700 / bl memset` @0x827D4FCC zeroes exactly the three 64-byte
    // irradiance matrices at the FRONT of that object, KU_IRRADIANCE_MATRICES_BYTES.)
    bool PrepareNotFinished(BrnWorld::GlobalIrradianceManager& lrGlobalIrradianceManager)
    {
        static_assert(KU_IRRADIANCE_MATRICES_BYTES <= sizeof(BrnWorld::GlobalIrradianceManager),
                      "the console memsets 0xC0 bytes at +0x700 -- must stay inside the manager");
        memset(&lrGlobalIrradianceManager, 0, KU_IRRADIANCE_MATRICES_BYTES);
        return false;
    }

    // Read the type word of the receiver queue's first event. The X360 open-codes this as
    //     lwz r11, 0x14(this)   ; miStartOffset
    //     lwz r10, 0x08(this)   ; mpBuffer
    //     lwzx r11, r11, r10    ; *(mpBuffer + miStartOffset) == the record's [type] word
    // which is exactly BaseEventReceiverQueue::GetFirstEvent's return value.
    s32 FirstEventId(const CgsModule::BaseEventReceiverQueue& lrQueue)
    {
        const CgsModule::Event* lpEventData = 0;
        s32 liSize = 0;
        return lrQueue.GetFirstEvent(&lpEventData, &liSize);
    }

    // The first event's payload as an acquire reply, or null when the queue is empty / the reply
    // is not an acquire. (The console reads the payload at record+8 and the ResourceHandle pair at
    // payload+0x18; on the host the response is read BY NAME off
    // CgsResource::Events::AcquireResourceResponse -- see the report's note that the committed
    // response type is missing the console's echoed mResourceId, which is why the host offset of
    // the handle pair is 0x10 rather than 0x18. Producer and consumer are both host code using
    // the same names, so the pair still matches.)
    const CgsResource::Events::AcquireResourceResponse*
    FirstAcquireResponse(const CgsModule::BaseEventReceiverQueue& lrQueue)
    {
        const CgsModule::Event* lpEventData = 0;
        s32 liSize = 0;
        const s32 liType = lrQueue.GetFirstEvent(&lpEventData, &liSize);
        if (liType != KI_EVENT_ACQUIRE_RESOURCE_RESPONSE || lpEventData == 0)
            return 0;
        return reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEventData);
    }

    // [FLAG PC bring-up] BOUNDED WAIT -- stands in for the console's unbounded
    // `if (mReceiverQueue.GetLength() < miResourceCnt) return false;`.
    //
    // WHY: the X360 is guaranteed a reply for every request it posts. On PC a request can be
    // answered late or not at all (a pool id that was never created, a bundle the asset converter
    // has not produced for platform 4 yet), and an unbounded wait inside WorldModule::Prepare
    // stage 5 is a SILENT INFINITE LOAD -- no assert, no log, a black screen. This keeps the
    // console's wait, but after KU_PC_WAIT_FRAME_LIMIT frames on the SAME stage it logs once and
    // lets the machine run on with whatever it has (the arms that follow are all null-guarded).
    // DELETE-WHEN every request Prepare/StreamIn/StreamOut posts is answered on PC (the proof is
    // the absence of any "[env] ... wait expired" line in BrnGame.log).
    enum EPCWaitResult
    {
        E_PCWAIT_PENDING,   // keep waiting -- return not-finished
        E_PCWAIT_READY,     // the replies arrived: run the console's event-id assert
        E_PCWAIT_EXPIRED    // PC-only: give up on this stage, skip its assert, carry on
    };

    const u32 KU_PC_WAIT_FRAME_LIMIT = 300u;   // 5 s at 60 Hz

    EPCWaitResult PCBringUpWaitForResources(s32 liStage, s32 liQueuedCount, s32 liResourceCnt,
                                            const char* lpcWhat)
    {
        static s32 s_iWaitStage  = -1;
        static u32 s_uWaitFrames = 0;

        if (liQueuedCount >= liResourceCnt)
        {
            s_iWaitStage  = -1;
            s_uWaitFrames = 0;
            return E_PCWAIT_READY;
        }

        if (liStage != s_iWaitStage)
        {
            s_iWaitStage  = liStage;
            s_uWaitFrames = 0;
        }

        if (++s_uWaitFrames < KU_PC_WAIT_FRAME_LIMIT)
            return E_PCWAIT_PENDING;

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            char lacLine[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsCore::SPrintf(lacLine, CgsDev::Assert::KI_MESSAGEBUFFERSIZE,
                             "[env] resource wait EXPIRED at stage %d after %u frames (%s) -- "
                             "continuing without it [FLAG PC bring-up]\n",
                             (s32)liStage, (u32)s_uWaitFrames, lpcWhat);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }
        s_iWaitStage  = -1;
        s_uWaitFrames = 0;
        return E_PCWAIT_EXPIRED;
    }

    // [FLAG PC bring-up] the synchronous stand-in for the four rw::core::filesys::AsyncOp calls
    // Prepare stages 11..14 make:
    //     AsyncOp::Open           @0x82BC00C0   (stage 11)
    //     AsyncOp::GetStatus      @0x82BBE058   (stages 12 + 14)
    //     AsyncOp::GetResultHandle@0x82BBE0C0   (stage 12)
    //     rw::core::filesys::GetSize @0x82BBD700 on that handle (stage 12's size assert)
    //     AsyncOp::Read           @0x82BC01D0   (stage 13)
    //     AsyncOp::GetResultSize  @0x82BBE118 + AsyncOp::Close @0x82BBFB48 (stage 14)
    // rw::core::filesys is not wired on this build -- Device::GetInstance is a link stub that
    // returns nullptr (SDKs/EATech/AptRenderLinkStubs.cpp) and AsyncOp::Open dereferences it, so
    // the console call sequence AVs rather than degrading. The FOUR STAGES ARE KEPT; only the IO
    // primitive under them is replaced, and it writes nothing but the bytes of the real shipped
    // file (build/game/ENVIRONMENTSETTINGS/JUNKYARDLIGHTING.DAT, 738 B). The consumer,
    // EnvironmentManager::ReadJunkyardLightingData, is the REAL reconstruction.
    // DELETE-WHEN rw::core::filesys has a live Device on PC: replace the five calls below with
    // mAsyncOp.Open/GetStatus/GetResultHandle/Read/GetResultSize/Close and restore mpFileHandle.
    struct PCBringUpJunkyardFile
    {
        static std::FILE*& File()
        {
            static std::FILE* s_pFile = 0;
            return s_pFile;
        }

        // Stands in for the AsyncOp's own result-size latch (GetResultSize @0x82BBE118), which
        // Prepare reads one stage AFTER the read is issued.
        static u32& BytesRead()
        {
            static u32 s_uBytesRead = 0u;
            return s_uBytesRead;
        }

        // stage 11 == AsyncOp::Open. Returns false if the file is absent (the console cannot
        // reach that state; on PC it is a missing-asset boot, handled by the caller).
        static bool Open(const char* lpcPath)
        {
            Close();
            File() = std::fopen(lpcPath, "rb");
            return File() != 0;
        }

        // stage 12 == rw::core::filesys::GetSize(mpFileHandle).
        static u32 GetSize()
        {
            if (File() == 0)
                return 0u;
            std::fseek(File(), 0, SEEK_END);
            const long lSize = std::ftell(File());
            std::fseek(File(), 0, SEEK_SET);
            return (lSize > 0) ? static_cast<u32>(lSize) : 0u;
        }

        // stage 13 == AsyncOp::Read(handle, buffer, position 0, count luMax, ...).
        static void Read(char* lpacBuffer, u32 luMax)
        {
            BytesRead() = (File() != 0)
                ? static_cast<u32>(std::fread(lpacBuffer, 1, luMax, File()))
                : 0u;
        }

        // stage 14 == AsyncOp::GetResultSize.
        static u32 GetResultSize() { return BytesRead(); }

        // stage 14 == AsyncOp::Close(mpFileHandle, ...).
        static void Close()
        {
            if (File() != 0)
            {
                std::fclose(File());
                File() = 0;
            }
        }
    };

    // One-shot [FLAG PC bring-up] / boot-proof log line helper (the tree's standard
    // filter-gated debug print). Every call site owns its own `static bool` latch.
    void EnvLogLine(const char* lpcLine)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << lpcLine;
    }
}

// ---------------------------------------------------------------------------------------------
// @ 0x827D49A8.  bool Prepare(BrnWorldIO::UpdateOutputBuffer*)   [DWARF BrnEnvironmentManager.h:91]
//
// A 16-case switch on mePrepareStage (X360 `cmplwi r10, 0xF` + jumptable jpt_827D4A14). Cases 0
// and 1 share an arm; every other case is both a jumptable target and the fall-through of the one
// before it, so a single call can run several stages when nothing has to be waited for. The
// switch's DEFAULT arm is also the target of every "not ready yet" branch: it clears the global
// irradiance matrices and returns false (see PrepareNotFinished).
//
// STAGES (enumerator names verbatim from the DecFIGS DWARF, BrnEnvironmentManager.h:246):
//   START / LOAD_DEPENDENCIES   drop both junkyard-lighting overrides, LoadBundle the POST-FX
//                               colour-cube dictionary into pool 10
//   WF_LOAD_DEPENDENCIES        wait for 1 reply, assert it is a LoadBundle reply
//   ACQUIRE_DEPENDENCIES        acquire the default rgb_colourcube ImageFile from pool 10
//   WF_ACQUIRE_DEPENDENCIES     wait, bind mDefColourCubePtr, publish it as mDefTintData
//   LOAD_DICTIONARY             LoadBundle "EnvironmentSettings\Dictionary.Bundle" into pool 16
//   WF_LOAD_DICTIONARY          wait, assert
//   ACQUIRE_DICTIONARY          Dictionary::BuildResourceName -> HashString -> acquire, pool 16
//   WF_ACQUIRE_DICTIONARY       wait, bind mDictionaryPtr
//   STREAMIN                    pick this frame's season index, arm the stream-in machine
//   WF_STREAMIN                 drive StreamIn() until it is done, then RequestNextSeason()
//   OPEN/WF_OPEN/READ/WF_READ_JUNKYARD_LIGHTING_DATA   the JUNKYARDLIGHTING.DAT read, then
//                               ReadJunkyardLightingData + the two default time-of-day bounds
//   DONE                        rewind the cursor to LOAD_DICTIONARY, clear the release latch,
//                               return true
//
// ⚠ THE TERMINAL STAGE DOES NOT STORE E_PREPARE_DONE. The X360 case-15 arm is
//     0x827D4FA8  li  r3, 1                 ; return true
//     0x827D4FAC  stw r18, 0(r31)           ; r18 == 5 (li r18,5 @0x827D49F0) == E_PREPARE_LOAD_DICTIONARY
//     0x827D4FB0  stw r22, 4(r31)           ; r22 == 0 == E_RELEASE_START
// i.e. a completed Prepare rewinds itself to LOAD_DICTIONARY so a subsequent world load re-streams
// the dictionary + season WITHOUT re-loading the global post-fx colour-cube dependencies (those
// are process-wide). E_PREPARE_DONE (15) is therefore only ever reached by falling through from
// WF_READ_JUNKYARD_LIGHTING_DATA or by an explicit re-entry with the cursor already at 15; the
// member never holds it. Reproduced exactly.
// ---------------------------------------------------------------------------------------------
bool EnvironmentManager::Prepare( BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    switch ( mePrepareStage )
    {
        case E_PREPARE_START:
        case E_PREPARE_LOAD_DEPENDENCIES:
        {
            mePrepareStage              = E_PREPARE_LOAD_DEPENDENCIES;
            mbJunkyardLightingSetup     = false;   // stb 0, 0x1235
            mbOverrideKeyLightDirection = false;   // stb 0, 0x1234

            lpOutput->LockForWrite();
            miResourceCnt = 1;
            lpOutput->GetResourceRequestResourceInterface()->LoadBundle(
                &mReceiverQueue, /*liEventId*/ 0, KI_POOL_POSTFX,
                KAC_COLOUR_CUBE_DICTIONARY_BUNDLE, /*lbUseHDCache*/ false );
            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_PREPARE_WF_LOAD_DEPENDENCIES:
        {
            mePrepareStage = E_PREPARE_WF_LOAD_DEPENDENCIES;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                mePrepareStage, liCount, miResourceCnt, KAC_COLOUR_CUBE_DICTIONARY_BUNDLE );
            if ( leWait == E_PCWAIT_PENDING )
                return PrepareNotFinished( mGlobalIrradianceManager );
            if ( leWait == E_PCWAIT_READY )
            {
                CGS_ASSERT( liCount > 0 && FirstEventId( mReceiverQueue ) == KI_EVENT_LOAD_BUNDLE_RESPONSE,
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_PREPARE_ACQUIRE_DEPENDENCIES:
        {
            mePrepareStage = E_PREPARE_ACQUIRE_DEPENDENCIES;

            lpOutput->LockForWrite();
            miResourceCnt = 1;

            // X360 0x827D4B50-0x827D4B70: the request is built on the stack as
            // {mpUser@+0, miEventId@+4 = 0, miPoolId@+8 = 10, mResourceId@+0x10 = HashString(name)}
            // and posted with AddEvent(type 4, size 24 == the 32-bit sizeof). The `| 0xA00000000`
            // Hex-Rays shows on the id is the usual fusion artifact of the separate miPoolId store
            // (HashString @0x828D84A8 ends `clrldi r3,32`, so the id's high dword is ZERO) --
            // the same artifact already corrected in WorldModule::LoadAttribSysVault.
            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = 0;
            lRequest.miPoolId  = KI_POOL_POSTFX;
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>( CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>( KAC_DEFAULT_COLOUR_CUBE_RESOURCE ) ) ) ) );
            lRequest.mbCheckRefCount = false;   // the console leaves this stack byte unwritten
            lpOutput->GetResourceRequestResourceInterface()->mRequestQueue.AddEvent(
                &lRequest, KI_REQUEST_ACQUIRE_RESOURCE );

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_PREPARE_WF_ACQUIRE_DEPENDENCIES:
        {
            mePrepareStage = E_PREPARE_WF_ACQUIRE_DEPENDENCIES;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                mePrepareStage, liCount, miResourceCnt, KAC_DEFAULT_COLOUR_CUBE_RESOURCE );
            if ( leWait == E_PCWAIT_PENDING )
                return PrepareNotFinished( mGlobalIrradianceManager );

            const CgsResource::Events::AcquireResourceResponse* lpResponse = 0;
            if ( leWait == E_PCWAIT_READY )
            {
                lpResponse = ( liCount > 0 ) ? FirstAcquireResponse( mReceiverQueue ) : 0;
                CGS_ASSERT( lpResponse != 0, "Invalid event id received\n" );
            }

            // X360 0x827D4BF8-0x827D4C10:
            //     CreateFromHandle(&mDefColourCubePtr, eventPayload + 0x18)
            //     mDefTintData = ResourcePtr<ColourCube>::GetMemoryResource(&mDefColourCubePtr)
            // The console runs both unconditionally (its assert arm falls straight into them with
            // a null payload -- an AV it can never actually reach); the bind is guarded here.
            if ( lpResponse != 0 )
            {
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                mDefColourCubePtr = lHandle;

                // The console's second line, RESTORED 2026-08-16 (group tintdata). It was HELD by
                // step 9 for one reason only -- BrnEffects::TintData::muColourCube was a 32-bit
                // guest word, so publishing a host pointer through it would have truncated it and
                // handed EvalTint a bogus ColourCube* to dereference. That member is now the
                // DWARF's real `rw::graphics::postfx::ColourCube* mpColourCube`
                // (SharedClasses/Graphics/BrnEffectsData.h) and so is mpDefTintData, so the store
                // is the console's own, at full width:
                //     X360 0x827D4C0C `bl CgsResource::ResourcePtr<ColourCube>::GetMemoryResource`
                //     X360 0x827D4C10 `stw r3, 0x1194(r31)`
                mpDefTintData = mDefColourCubePtr.GetMemoryResource();

                static bool s_bLoggedDefaultCube = false;
                if ( !s_bLoggedDefaultCube )
                {
                    s_bLoggedDefaultCube = true;
                    // 128 was too small: KAC_DEFAULT_COLOUR_CUBE_RESOURCE is a ~100-char gamedb://
                    // URI, so the pointer and the newline were cut off and the next log line ran on.
                    char lacLine[256];
                    std::snprintf( lacLine, sizeof( lacLine ),
                                   "[env] default colour cube acquired and PUBLISHED: %s -> %p\n",
                                   KAC_DEFAULT_COLOUR_CUBE_RESOURCE,
                                   static_cast<const void*>( mpDefTintData ) );
                    EnvLogLine( lacLine );
                }
            }
        }
        // fall through

        case E_PREPARE_LOAD_DICTIONARY:
        {
            mePrepareStage = E_PREPARE_LOAD_DICTIONARY;

            lpOutput->LockForWrite();
            miResourceCnt = 1;
            lpOutput->GetResourceRequestResourceInterface()->LoadBundle(
                &mReceiverQueue, /*liEventId*/ 0, KI_POOL_ENVIRONMENT,
                KAC_ENVIRONMENT_DICTIONARY_BUNDLE, /*lbUseHDCache*/ false );
            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_PREPARE_WF_LOAD_DICTIONARY:
        {
            mePrepareStage = E_PREPARE_WF_LOAD_DICTIONARY;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                mePrepareStage, liCount, miResourceCnt, KAC_ENVIRONMENT_DICTIONARY_BUNDLE );
            if ( leWait == E_PCWAIT_PENDING )
                return PrepareNotFinished( mGlobalIrradianceManager );
            if ( leWait == E_PCWAIT_READY )
            {
                CGS_ASSERT( liCount > 0 && FirstEventId( mReceiverQueue ) == KI_EVENT_LOAD_BUNDLE_RESPONSE,
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_PREPARE_ACQUIRE_DICTIONARY:
        {
            mePrepareStage = E_PREPARE_ACQUIRE_DICTIONARY;

            lpOutput->LockForWrite();

            // Dictionary::BuildResourceName @0x827B03B8 is called with ONE register (r3 == the
            // destination buffer, `addi r3, r1, 0x1F0+var_180`), so it is a STATIC member -- the
            // DWARF cannot tell static from non-static (AGENTS.md), the call site can.
            char lacResourceName[KI_ENV_RESOURCE_NAME_MAX];
            Dictionary::BuildResourceName( lacResourceName );

            miResourceCnt = 1;

            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = 0;
            lRequest.miPoolId  = KI_POOL_ENVIRONMENT;   // X360 `li r10,0x10` -> the +8 slot
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>( CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>( lacResourceName ) ) ) ) );
            lRequest.mbCheckRefCount = false;
            lpOutput->GetResourceRequestResourceInterface()->mRequestQueue.AddEvent(
                &lRequest, KI_REQUEST_ACQUIRE_RESOURCE );

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_PREPARE_WF_ACQUIRE_DICTIONARY:
        {
            mePrepareStage = E_PREPARE_WF_ACQUIRE_DICTIONARY;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                mePrepareStage, liCount, miResourceCnt, "ENV_DICTIONARY" );
            if ( leWait == E_PCWAIT_PENDING )
                return PrepareNotFinished( mGlobalIrradianceManager );

            const CgsResource::Events::AcquireResourceResponse* lpResponse = 0;
            if ( leWait == E_PCWAIT_READY )
            {
                lpResponse = ( liCount > 0 ) ? FirstAcquireResponse( mReceiverQueue ) : 0;
                CGS_ASSERT( lpResponse != 0, "Invalid event id received\n" );
            }

            if ( lpResponse != 0 )
            {
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                mDictionaryPtr = lHandle;   // X360 CreateFromHandle(this+0x424, payload+0x18)
            }
        }
        // fall through

        case E_PREPARE_STREAMIN:
        {
            mePrepareStage = E_PREPARE_STREAMIN;

            // maiSeasons[ miCurrSeason ] = miSeasonCurrentlyPlaying   (X360 `stwx r9, 4*(cur+0x134), r31`)
            maiSeasons[ miCurrSeason ] = miSeasonCurrentlyPlaying;
            if ( mbOverrideSeason )
            {
                // the debug var "Season to use" is 1-based (X360 `addi r11, r11, -1`)
                maiSeasons[ miCurrSeason ] = miOverrideCurrentSeason - 1;
            }

            meStreamInStage  = E_STREAMIN_START;
            muStreamInTarget = static_cast<u8>( miCurrSeason );
        }
        // fall through

        case E_PREPARE_WF_STREAMIN:
        {
            mePrepareStage = E_PREPARE_WF_STREAMIN;

            // [FLAG PC bring-up] StreamIn and RequestNextSeason both walk the dictionary's
            // SeasonData strings; the console cannot reach this stage without a bound dictionary.
            // On PC an absent ENVIRONMENTSETTINGS bundle would make GetMemoryResource assert every
            // frame forever, so the pair is skipped with ONE log line and the machine runs on --
            // the season timeline then simply stays unbound and SetupBlend keeps its own guard.
            // DELETE-WHEN the converted DICTIONARY.BUNDLE loads (no such line in BrnGame.log).
            if ( !mDictionaryPtr.HasMemoryResource() )
            {
                static bool s_bLoggedNoDictionary = false;
                if ( !s_bLoggedNoDictionary )
                {
                    s_bLoggedNoDictionary = true;
                    EnvLogLine( "[env] dictionary resource absent -- skipping season StreamIn "
                                "[FLAG PC bring-up]\n" );
                }
            }
            else
            {
                if ( !StreamIn( lpOutput ) )
                    return PrepareNotFinished( mGlobalIrradianceManager );

                RequestNextSeason();
            }
        }
        // fall through

        case E_PREPARE_OPEN_JUNKYARD_LIGHTING_DATA:
        {
            mePrepareStage = E_PREPARE_OPEN_JUNKYARD_LIGHTING_DATA;
            // X360: mAsyncOp.Open(off_82F307EC, 0, 0, 0, 0)   -- see PCBringUpJunkyardFile.
            PCBringUpJunkyardFile::Open( KAC_JUNKYARD_LIGHTING_DATA_PATH );
        }
        // fall through

        case E_PREPARE_WF_OPEN_JUNKYARD_LIGHTING_DATA:
        {
            mePrepareStage = E_PREPARE_WF_OPEN_JUNKYARD_LIGHTING_DATA;

            // X360: if (!mAsyncOp.GetStatus(false)) return notFinished;
            //       assert(mAsyncOp.GetStatus(false) == OPSTATUS_COMPLETE);
            //       mpFileHandle = mAsyncOp.GetResultHandle();
            //       assert((uint32_t)GetSize(mpFileHandle) < K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE-1);
            // The synchronous seam completes in the same call, so only the size assert survives.
            CGS_ASSERT( PCBringUpJunkyardFile::GetSize()
                            < (u32)( K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE - 1 ),
                        "(uint32_t)rw::core::filesys::GetSize(mpFileHandle) < "
                        "K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE-1" );
        }
        // fall through

        case E_PREPARE_READ_JUNKYARD_LIGHTING_DATA:
        {
            mePrepareStage = E_PREPARE_READ_JUNKYARD_LIGHTING_DATA;
            // X360: mAsyncOp.Read(mpFileHandle, macJunkyardLightingBuffer, 0,
            //                     K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE-1, 0, 0, 0)
            // (`li r7, 0x7FF` is the byte count, `addi r5, r31, 0x1240` the destination.)
            PCBringUpJunkyardFile::Read(
                macJunkyardLightingBuffer, (u32)( K_JUNKYARD_LIGHTING_DATA_BUFFER_SIZE - 1 ) );
        }
        // fall through

        case E_PREPARE_WF_READ_JUNKYARD_LIGHTING_DATA:
        {
            mePrepareStage = E_PREPARE_WF_READ_JUNKYARD_LIGHTING_DATA;

            // X360: if (!GetStatus(false)) return notFinished;  assert(status == COMPLETE);
            //       luResultSize = mAsyncOp.GetResultSize();
            //       mAsyncOp.Close(mpFileHandle, 0, 0, 0);
            //       macJunkyardLightingBuffer[luResultSize] = '\0';
            //       ReadJunkyardLightingData(macJunkyardLightingBuffer, luResultSize);
            //       mfTimeOfDayLowerBound = flt_820CC768 (28800 == 08:00)
            //       mfTimeOfDayUpperBound = flt_820CAB98 (61200 == 17:00)
            const u32 luResultSize = PCBringUpJunkyardFile::GetResultSize();
            PCBringUpJunkyardFile::Close();

            macJunkyardLightingBuffer[ luResultSize ] = '\0';
            ReadJunkyardLightingData( macJunkyardLightingBuffer, luResultSize );

            mfTimeOfDayLowerBound = KF_DEF_TIME_OF_DAY_LOWER_BOUND;
            mfTimeOfDayUpperBound = KF_DEF_TIME_OF_DAY_UPPER_BOUND;
        }
        // fall through

        case E_PREPARE_DONE:
        {
            // See the ⚠ in the banner: the cursor rewinds to LOAD_DICTIONARY, it is NOT set to DONE.
            mePrepareStage = E_PREPARE_LOAD_DICTIONARY;
            meReleaseStage = E_RELEASE_START;

            // ---- boot proof (one-shot): what actually streamed. -------------------------------
            static bool s_bLoggedPrepareDone = false;
            if ( !s_bLoggedPrepareDone )
            {
                s_bLoggedPrepareDone = true;

                const Dictionary* lpDictionary = 0;
                if ( mDictionaryPtr.HasMemoryResource() )
                    lpDictionary = mDictionaryPtr.GetMemoryResource();

                const TimeLine* lpTimeLine = 0;
                if ( maSeasonPtrs[ miCurrSeason ].HasMemoryResource() )
                    lpTimeLine = maSeasonPtrs[ miCurrSeason ].GetMemoryResource();

                const s32 liSeason   = maiSeasons[ miCurrSeason ];
                const bool lbSeasonOk = ( lpDictionary != 0 && lpDictionary->mpSeasonDatii != 0 &&
                                          liSeason >= 0 && (u32)liSeason < lpDictionary->muSeasonCnt );
                const bool lbLocOk    = ( lpDictionary != 0 && lpDictionary->mpLocationDatii != 0 &&
                                          miCurrLocation >= 0 &&
                                          (u32)miCurrLocation < lpDictionary->muLocationCnt );

                u32 luKeyframes = 0u;
                f32 lfFirst = 0.0f;
                f32 lfLast  = 0.0f;
                if ( lpTimeLine != 0 && lpTimeLine->mpLocationDatii != 0 &&
                     miCurrLocation >= 0 && (u32)miCurrLocation < lpTimeLine->muLocationCnt )
                {
                    const TimeLine::LocationData& lrLoc = lpTimeLine->mpLocationDatii[ miCurrLocation ];
                    luKeyframes = lrLoc.muKeyframeCnt;
                    if ( lrLoc.mpfKeyframeTimes != 0 && luKeyframes > 0u )
                    {
                        lfFirst = lrLoc.mpfKeyframeTimes[ 0 ];
                        lfLast  = lrLoc.mpfKeyframeTimes[ luKeyframes - 1u ];
                    }
                }

                const u32 KU_ENV_BOOT_LINE_MAX = 512u;   // two 128-char names + the fixed text
                char lacLine[ KU_ENV_BOOT_LINE_MAX ];
                CgsCore::SPrintf( lacLine, KU_ENV_BOOT_LINE_MAX,
                    "[env] prepare done: dictionary seasons=%u locations=%u season=%d '%s' "
                    "location=%d '%s' timeline keyframes=%u times %.0f..%.0f colourcubes=%d "
                    "junkyard setups=%u\n",
                    ( lpDictionary != 0 ) ? lpDictionary->muSeasonCnt   : 0u,
                    ( lpDictionary != 0 ) ? lpDictionary->muLocationCnt : 0u,
                    (s32)liSeason,
                    lbSeasonOk ? lpDictionary->mpSeasonDatii[ liSeason ].macResourceName : "<none>",
                    (s32)miCurrLocation,
                    lbLocOk ? lpDictionary->mpLocationDatii[ miCurrLocation ].macName : "<none>",
                    (u32)luKeyframes, (double)lfFirst, (double)lfLast,
                    (s32)( mDefColourCubePtr.HasMemoryResource() ? 1 : 0 ),
                    (u32)muNumJunkyardLightingSetupsLoaded );
                EnvLogLine( lacLine );
            }

            return true;
        }

        default:
            break;
    }

    return PrepareNotFinished( mGlobalIrradianceManager );
}

// ---------------------------------------------------------------------------------------------
// @ 0x827D31E8.  bool StreamIn(BrnWorldIO::UpdateOutputBuffer*)  [DWARF BrnEnvironmentManager.h:183]
//
// The season stream-IN machine (8 cases over EStreamInStage). It streams the season SLOT named by
// muStreamInTarget: the dictionary's SeasonData for maiSeasons[muStreamInTarget] supplies three
// strings at +0x00 / +0x80 / +0xC0 (macResourceName / macBundle / macColourCubesBundle -- the X360
// forms them as seasonBase + 0 / +128 / +192 with the season stride 256, `slwi r10, r10, 8`).
//   COLOURCUBES : LoadBundle macColourCubesBundle if it is non-empty (the console's strlen != 1
//                 test on the NUL-terminated string), pool 16
//   SEASON      : LoadBundle macBundle, pool 16 -- unconditional
//   ACQUIRE     : acquire HashString(macResourceName) from pool 16
//   WF_ACQUIRE  : bind maSeasonPtrs[muStreamInTarget] from the reply's handle
//
// ⚠ THERE IS NO PER-KEYFRAME LOOP HERE. The season's keyframes ride inside the season bundle and
// their host Keyframe* slots (TimeLine::LocationData::mppKeyframes) are produced by
// TimeLineResourceType::FixUp, not by StreamIn -- StreamIn only binds the TimeLine resource
// pointer. (Grep proof in the report: no function in the ARTIST export references a keyframe
// name format string, and TimeLine::BuildResourceName has no caller anywhere in the image.)
// ---------------------------------------------------------------------------------------------
bool EnvironmentManager::StreamIn( BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    switch ( meStreamInStage )
    {
        case E_STREAMIN_START:
        case E_STREAMIN_LOAD_SEASON_COLOURCUBES:
        {
            meStreamInStage = E_STREAMIN_LOAD_SEASON_COLOURCUBES;

            lpOutput->LockForWrite();
            miResourceCnt = 0;

            const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();
            const Dictionary::SeasonData& lrSeason =
                lpDictionary->mpSeasonDatii[ maiSeasons[ muStreamInTarget ] ];

            if ( lrSeason.macColourCubesBundle[ 0 ] != '\0' )
            {
                const s32 liEventId = miResourceCnt;
                ++miResourceCnt;
                lpOutput->GetResourceRequestResourceInterface()->LoadBundle(
                    &mReceiverQueue, liEventId, KI_POOL_ENVIRONMENT,
                    lrSeason.macColourCubesBundle, /*lbUseHDCache*/ false );
            }

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_STREAMIN_WF_LOAD_SEASON_COLOURCUBES:
        {
            meStreamInStage = E_STREAMIN_WF_LOAD_SEASON_COLOURCUBES;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                100 + meStreamInStage, liCount, miResourceCnt, "season colour-cube bundle" );
            if ( leWait == E_PCWAIT_PENDING )
                return false;
            if ( leWait == E_PCWAIT_READY )
            {
                // X360: assert unless (miResourceCnt <= 0) or (count > 0 and the reply is a load reply).
                CGS_ASSERT( miResourceCnt <= 0 ||
                            ( liCount > 0 &&
                              FirstEventId( mReceiverQueue ) == KI_EVENT_LOAD_BUNDLE_RESPONSE ),
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_STREAMIN_LOAD_SEASON:
        {
            meStreamInStage = E_STREAMIN_LOAD_SEASON;

            lpOutput->LockForWrite();
            miResourceCnt = 0;

            const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();
            const Dictionary::SeasonData& lrSeason =
                lpDictionary->mpSeasonDatii[ maiSeasons[ muStreamInTarget ] ];

            const s32 liEventId = miResourceCnt;
            ++miResourceCnt;
            lpOutput->GetResourceRequestResourceInterface()->LoadBundle(
                &mReceiverQueue, liEventId, KI_POOL_ENVIRONMENT,
                lrSeason.macBundle, /*lbUseHDCache*/ false );

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_STREAMIN_WF_LOAD_SEASON:
        {
            meStreamInStage = E_STREAMIN_WF_LOAD_SEASON;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                100 + meStreamInStage, liCount, miResourceCnt, "season bundle" );
            if ( leWait == E_PCWAIT_PENDING )
                return false;
            if ( leWait == E_PCWAIT_READY )
            {
                CGS_ASSERT( miResourceCnt <= 0 ||
                            ( liCount > 0 &&
                              FirstEventId( mReceiverQueue ) == KI_EVENT_LOAD_BUNDLE_RESPONSE ),
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_STREAMIN_ACQUIRE_SEASON:
        {
            meStreamInStage = E_STREAMIN_ACQUIRE_SEASON;

            lpOutput->LockForWrite();
            miResourceCnt = 0;

            const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();
            const Dictionary::SeasonData& lrSeason =
                lpDictionary->mpSeasonDatii[ maiSeasons[ muStreamInTarget ] ];

            const s32 liEventId = miResourceCnt;
            ++miResourceCnt;

            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser    = &mReceiverQueue;
            lRequest.miEventId = liEventId;
            lRequest.miPoolId  = KI_POOL_ENVIRONMENT;
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>( CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>( lrSeason.macResourceName ) ) ) ) );
            lRequest.mbCheckRefCount = false;
            lpOutput->GetResourceRequestResourceInterface()->mRequestQueue.AddEvent(
                &lRequest, KI_REQUEST_ACQUIRE_RESOURCE );

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_STREAMIN_WF_ACQUIRE_SEASON:
        {
            meStreamInStage = E_STREAMIN_WF_ACQUIRE_SEASON;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                100 + meStreamInStage, liCount, miResourceCnt, "season timeline resource" );
            if ( leWait == E_PCWAIT_PENDING )
                return false;

            const CgsResource::Events::AcquireResourceResponse* lpResponse = 0;
            if ( leWait == E_PCWAIT_READY )
            {
                lpResponse = ( liCount > 0 ) ? FirstAcquireResponse( mReceiverQueue ) : 0;
                CGS_ASSERT( lpResponse != 0, "Invalid event id received\n" );
            }

            if ( lpResponse != 0 )
            {
                // X360: CreateFromHandle(&maSeasonPtrs[muStreamInTarget], payload + 0x18)
                // (`__ROL4__(muStreamInTarget,5)` == the 0x20 guest ResourcePtr stride).
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                maSeasonPtrs[ muStreamInTarget ] = lHandle;
            }
        }
        // fall through

        case E_STREAMIN_DONE:
        {
            meStreamInStage = E_STREAMIN_DONE;
            return true;
        }

        default:
            break;
    }

    return false;
}

// ---------------------------------------------------------------------------------------------
// @ 0x827D2EB0 (HOLES_DUMP.md).  bool StreamOut(BrnWorldIO::UpdateOutputBuffer*)
//                                                          [DWARF BrnEnvironmentManager.h:179]
//
// The mirror of StreamIn over EStreamOutStage: release the season slot muStreamOutTarget's
// timeline resource pointer, unload the season bundle and (if it has one) the season's colour-cube
// bundle, then RequestNextSeason. Callers: SetupTimeOfDayBlend @0x827D35DC and
// SetupSeasonsBlend @0x827D3838 (envblend's functions).
//
// ⚠ NOTE THE ASYMMETRY WITH StreamIn, reproduced verbatim: the SEASON bundle is unloaded
// unconditionally but the COLOUR-CUBE bundle is guarded by the same "string is not empty" test
// StreamIn uses -- i.e. stage 2 has no guard, stage 4 does.
// ---------------------------------------------------------------------------------------------
bool EnvironmentManager::StreamOut( BrnWorldIO::UpdateOutputBuffer* lpOutput )
{
    switch ( meStreamOutStage )
    {
        case E_STREAMOUT_START:
        case E_STREAMOUT_DISCARD_SEASON:
        {
            meStreamOutStage = E_STREAMOUT_DISCARD_SEASON;

            // X360 0x827D2F24-0x827D2FA8. The console builds a fresh, EMPTY BaseResourcePtr on the
            // stack -- {mpResourceMemory, mHandle.*} = 0, mpNext = mpPrev = mpThis = &local,
            // muThreadId = 0 -- and calls CreateFromHandle(&maSeasonPtrs[target], &local.mpThis),
            // i.e. it rebinds the slot from the {mpThis, muThreadId} pair of an empty pointer. That
            // is exactly the tree's ResourcePtr<T>::operator=(const ResourcePtr<T>&) (whose banner
            // documents the same `addi r4, src, 0x14` shape). The three stores that follow the call
            // (unlink from the alias ring, then re-self-link) are the stack pointer's INLINED
            // destructor -- so the whole block is one scoped local plus one assignment.
            {
                CgsResource::ResourcePtr<TimeLine> lEmptyResourcePtr;
                maSeasonPtrs[ muStreamOutTarget ] = lEmptyResourcePtr;
            }
        }
        // fall through

        case E_STREAMOUT_UNLOAD_SEASON:
        {
            meStreamOutStage = E_STREAMOUT_UNLOAD_SEASON;

            lpOutput->LockForWrite();
            miResourceCnt = 0;

            const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();
            const Dictionary::SeasonData& lrSeason =
                lpDictionary->mpSeasonDatii[ maiSeasons[ muStreamOutTarget ] ];

            const s32 liEventId = miResourceCnt;
            ++miResourceCnt;
            lpOutput->GetResourceRequestResourceInterface()->UnloadBundle(
                &mReceiverQueue, liEventId, KI_POOL_ENVIRONMENT, lrSeason.macBundle );

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_STREAMOUT_WF_UNLOAD_SEASON:
        {
            meStreamOutStage = E_STREAMOUT_WF_UNLOAD_SEASON;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                200 + meStreamOutStage, liCount, miResourceCnt, "season bundle unload" );
            if ( leWait == E_PCWAIT_PENDING )
                return false;
            if ( leWait == E_PCWAIT_READY )
            {
                CGS_ASSERT( miResourceCnt <= 0 ||
                            ( liCount > 0 &&
                              FirstEventId( mReceiverQueue ) == KI_EVENT_UNLOAD_BUNDLE_RESPONSE ),
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_STREAMOUT_UNLOAD_SEASON_COLOURCUBES:
        {
            meStreamOutStage = E_STREAMOUT_UNLOAD_SEASON_COLOURCUBES;

            lpOutput->LockForWrite();
            miResourceCnt = 0;

            const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();
            const Dictionary::SeasonData& lrSeason =
                lpDictionary->mpSeasonDatii[ maiSeasons[ muStreamOutTarget ] ];

            if ( lrSeason.macColourCubesBundle[ 0 ] != '\0' )
            {
                const s32 liEventId = miResourceCnt;
                ++miResourceCnt;
                lpOutput->GetResourceRequestResourceInterface()->UnloadBundle(
                    &mReceiverQueue, liEventId, KI_POOL_ENVIRONMENT,
                    lrSeason.macColourCubesBundle );
            }

            mReceiverQueue.Clear();
            lpOutput->UnlockForWrite();
        }
        // fall through

        case E_STREAMOUT_WF_UNLOAD_SEASON_COLOURCUBES:
        {
            meStreamOutStage = E_STREAMOUT_WF_UNLOAD_SEASON_COLOURCUBES;

            const s32 liCount = mReceiverQueue.GetLength();
            const EPCWaitResult leWait = PCBringUpWaitForResources(
                200 + meStreamOutStage, liCount, miResourceCnt, "season colour-cube unload" );
            if ( leWait == E_PCWAIT_PENDING )
                return false;
            if ( leWait == E_PCWAIT_READY )
            {
                CGS_ASSERT( miResourceCnt <= 0 ||
                            ( liCount > 0 &&
                              FirstEventId( mReceiverQueue ) == KI_EVENT_UNLOAD_BUNDLE_RESPONSE ),
                            "Invalid event id received\n" );
            }
        }
        // fall through

        case E_STREAMOUT_REQUEST_NEXT_SEASON:
        {
            meStreamOutStage = E_STREAMOUT_REQUEST_NEXT_SEASON;
            RequestNextSeason();
        }
        // fall through

        case E_STREAMOUT_DONE:
        {
            meStreamOutStage = E_STREAMOUT_DONE;
            return true;
        }

        default:
            break;
    }

    return false;
}

// ---------------------------------------------------------------------------------------------
// @ 0x827C4EA0 (HOLES_DUMP.md).  void RequestNextSeason()   [DWARF BrnEnvironmentManager.h:189]
//
// Arm the stream-IN machine for the OTHER season slot and choose which season index it will load.
// Called by StreamOut @0x827D31C0 and by Prepare @0x827D4E28.
//
// The choice, in the console's priority order:
//   1. d:\Season.txt -- if the dev file exists, its first whitespace-delimited token is matched
//      against each SeasonData::macResourceName; a hit wins outright (and, notably, does NOT move
//      miSeasonCurrentlyPlaying).
//   2. the "Override season" debug var -- miSeasonCurrentlyPlaying = miOverrideNextSeason.
//   3. otherwise advance one season, wrapping on muSeasonCnt (the X360 `twllei r10, 0` is the
//      compiler's divide-by-zero trap on the modulus, not a source-level check).
// The chosen index lands in maiSeasons[ 1 - miCurrSeason ], the slot muStreamInTarget names
// (X360 `4 * (0x135 - miCurrSeason)` == &maiSeasons[1 - miCurrSeason]).
// ---------------------------------------------------------------------------------------------
void EnvironmentManager::RequestNextSeason()
{
    CGS_ASSERT( meStreamOutStage >= E_STREAMOUT_REQUEST_NEXT_SEASON,
                "meStreamOutStage >= E_STREAMOUT_REQUEST_NEXT_SEASON" );

    const s32 liNextSlot = 1 - miCurrSeason;

    meStreamInStage  = E_STREAMIN_START;
    muStreamInTarget = static_cast<u8>( liNextSlot );

    const Dictionary* const lpDictionary = mDictionaryPtr.GetMemoryResource();

    // 1. the artist override file. (The console reads it with the C runtime, exactly like the
    // committed SetupUpdateFromToolBlend reads d:\LightSetup.txt; on PC the open simply fails.)
    std::FILE* const lpSeasonFile = std::fopen( KAC_SEASON_OVERRIDE_FILE, "r" );
    if ( lpSeasonFile != 0 )
    {
        char lacSeasonName[ KI_ENV_RESOURCE_NAME_MAX ];
        lacSeasonName[ 0 ] = '\0';
        std::fscanf( lpSeasonFile, "%s", lacSeasonName );

        const s32 liSeasonCnt = static_cast<s32>( lpDictionary->muSeasonCnt );
        for ( s32 liSeason = 0; liSeason < liSeasonCnt; ++liSeason )
        {
            if ( strcmp( lacSeasonName,
                         lpDictionary->mpSeasonDatii[ liSeason ].macResourceName ) == 0 )
            {
                maiSeasons[ liNextSlot ] = liSeason;
                std::fclose( lpSeasonFile );
                return;
            }
        }
        std::fclose( lpSeasonFile );
    }

    // 2. the debug var.
    if ( mbOverrideSeason )
    {
        miSeasonCurrentlyPlaying = miOverrideNextSeason;
    }
    // 3. the shipped path: advance and wrap.
    else
    {
        miSeasonCurrentlyPlaying =
            static_cast<s32>( static_cast<u32>( miSeasonCurrentlyPlaying + 1 ) %
                              lpDictionary->muSeasonCnt );
    }
    maiSeasons[ liNextSlot ] = miSeasonCurrentlyPlaying;
}

// ---------------------------------------------------------------------------------------------
// @ 0x827BEBD8.  void ReadJunkyardLightingData(const char*, uint32_t)
//                                                          [DWARF BrnEnvironmentManager.h:243]
//
// Parse ENVIRONMENTSETTINGS/JUNKYARDLIGHTING.DAT (a 738-byte text file, shipped) into
// maJunkyardLighting[] / muNumJunkyardLightingSetupsLoaded. The grammar, straight from the asm:
//   '#'  starts a comment that runs to the next '\n' (checked BEFORE everything else, so a '#'
//        inside braces would also comment -- reproduced)
//   '{'  opens a vector; the component index resets to 0
//   ','  advances the component index (asserting it stays < 3, and that a number was read)
//   '}'  closes a vector (asserting a number was read and that exactly two commas were seen)
//   anything else INSIDE braces is fed to strtod and stored at the current component
// The vectors alternate: the FIRST '}' of each pair stores the world POSITION, the SECOND
// normalises the direction, CLAMPS ITS Y into [-0.989, 0.989] and stores it as the key-light
// direction, then bumps the count. (X360 r25 starts at 1 -- the "expect a position next" latch.)
//
// Every constant is X360 rodata: flt_820CBD70 == +0.989, flt_820CBDB0 == -0.989 (DATA_DUMP.md;
// both are the neighbours of the two assert strings this function fires, which is an independent
// cross-check that the dump is aimed at the right bytes).
// ---------------------------------------------------------------------------------------------
void EnvironmentManager::ReadJunkyardLightingData( const char* lpcBuffer, u32 luSize )
{
    muNumJunkyardLightingSetupsLoaded = 0u;

    s32  liComponent      = 0;       // r21  the {x,y,z} index inside the current vector
    bool lbInsideBraces   = false;   // r22
    bool lbInsideComment  = false;   // r20
    bool lbReadANumber    = false;   // r27
    bool lbExpectPosition = true;    // r25  (initialised to 1 -- the FIRST '}' is the position)

    Vector3 lVector;                 // the accumulating vector (X360: a 16-byte stack slot)
    lVector.x = 0.0f;
    lVector.y = 0.0f;
    lVector.z = 0.0f;
    // FLAG (documented deviation): the console never initialises this stack slot, so its W lane
    // carries whatever was there and is normalised along with x/y/z. Nothing reads W of either
    // vector (EnableJunkyardLightingSetup uses x/y/z of the position and copies the direction
    // whole into mOverrideKeyLightDirection, whose W the shader path does not consume), so W is
    // pinned to 0 here instead of reproducing uninitialised stack.
    lVector.w = 0.0f;

    for ( u32 luIndex = 0u; luIndex < luSize; ++luIndex )
    {
        const char lcChar = lpcBuffer[ luIndex ];

        if ( lbInsideComment )
        {
            if ( lcChar == '\n' )
                lbInsideComment = false;
            continue;
        }

        if ( lcChar == '#' )
        {
            lbInsideComment = true;
            continue;
        }

        if ( !lbInsideBraces )
        {
            if ( lcChar == '{' )
            {
                lbInsideBraces = true;
                liComponent    = 0;
                lbReadANumber  = false;
            }
            continue;
        }

        if ( lcChar == ',' )
        {
            ++liComponent;
            CGS_ASSERT( liComponent < 3, "Error parsing junkyard lighting data" );
            CGS_ASSERT( lbReadANumber,   "Error parsing junkyard lighting data" );
            lbReadANumber = false;
            continue;
        }

        if ( lcChar == '}' )
        {
            CGS_ASSERT( lbReadANumber,     "Error parsing junkyard lighting data" );
            CGS_ASSERT( liComponent == 2,  "Error parsing junkyard lighting data" );
            lbInsideBraces = false;

            if ( lbExpectPosition )
            {
                maJunkyardLighting[ muNumJunkyardLightingSetupsLoaded ].mJunkyardPosition = lVector;
                lbExpectPosition = false;
            }
            else
            {
                // Normalise (X360: vmsum3fp128 + vrsqrtefp with two Newton-Raphson refinements,
                // lowered here to the equivalent scalar reciprocal square root).
                const f32 lfLengthSquared =
                    ( lVector.x * lVector.x ) + ( lVector.y * lVector.y ) + ( lVector.z * lVector.z );
                const f32 lfInverseLength = 1.0f / sqrtf( lfLengthSquared );

                Vector3 lDirection;
                lDirection.x = lVector.x * lfInverseLength;
                lDirection.y = lVector.y * lfInverseLength;
                lDirection.z = lVector.z * lfInverseLength;
                lDirection.w = lVector.w * lfInverseLength;

                // Clamp the ELEVATION only (component 1), so a straight-down key light still casts
                // a shadow. X360: vspltw v13, v0, 1 then the two vcmpgtfp./vsel pairs.
                if ( lDirection.y > KF_JUNKYARD_MAX_KEY_LIGHT_Y )
                    lDirection.y = KF_JUNKYARD_MAX_KEY_LIGHT_Y;
                else if ( lDirection.y < KF_JUNKYARD_MIN_KEY_LIGHT_Y )
                    lDirection.y = KF_JUNKYARD_MIN_KEY_LIGHT_Y;

                maJunkyardLighting[ muNumJunkyardLightingSetupsLoaded ].mKeyLightDirection = lDirection;

                ++muNumJunkyardLightingSetupsLoaded;
                CGS_ASSERT( muNumJunkyardLightingSetupsLoaded < KU_MAX_JUNKYARD_LIGHTING_SETUPS,
                            "muNumJunkyardLightingSetupsLoaded < KU_MAX_NUM_JUNKYARDS" );

                lbExpectPosition = true;
            }
            continue;
        }

        // A number. strtod consumes it and reports where it stopped; the loop resumes at the
        // character AFTER the number (X360: `v12 = liFloatLength + v12 - 1` then the ++).
        char* lpcEnd = 0;
        const f32 lfValue = static_cast<f32>( strtod( &lpcBuffer[ luIndex ], &lpcEnd ) );

        CGS_ASSERT( lpcEnd != &lpcBuffer[ luIndex ], "Error parsing junkyard lighting data" );
        lbReadANumber = true;

        const s32 liFloatLength = static_cast<s32>( lpcEnd - &lpcBuffer[ luIndex ] );
        CGS_ASSERT( liFloatLength > 0, "liFloatLength > 0" );
        if ( liFloatLength <= 0 )
            continue;   // (PC guard: the console's assert is non-fatal and would spin here)

        ( &lVector.x )[ liComponent ] = lfValue;
        luIndex += static_cast<u32>( liFloatLength - 1 );
    }
}

}
}
