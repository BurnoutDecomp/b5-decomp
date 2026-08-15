#include "GameSource/World/EnvironmentManager/BrnEnvironmentManager.h"
#include "GameSource/World/EnvironmentSettings/BrnEnvironmentSettings.h"  // ParseEnvironmentFile
#include "SharedClasses/World/BrnEnvironmentUtil.h"                       // ComputeKeyLightDirection
#include "SharedClasses/Graphics/BrnEffectsData.h"                        // BrnEffectsFrame + BrnEffects::{Bloom,Vignette,Tint}Data
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cmath>     // sqrtf
#include <cstring>   // memset / memcpy

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

    // ---- Construct's inlined CgsNumeric::Random seeding -----------------------------------
    const u64 KU_RANDOM_SEED       = 0x1AD0891BC87CD8C9ull;
    const u64 KU_RANDOM_MULTIPLIER = 0x5851F42D4C957F2Dull;
    const u32 KU_RANDOM_RING_SIZE  = 8u;

    // ---- Construct's event-receiver queue geometry ----------------------------------------
    const u32 KU_RECEIVER_QUEUE_CAPACITY = 1024u;
    const u32 KU_RECEIVER_QUEUE_STRIDE   = 16u;

    // ---- stage-machine terminal values Construct writes -----------------------------------
    const s32 E_STREAMOUT_DONE          = 7;
    const s32 E_SETUPSEASONSBLEND_DONE  = 4;

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
    if (meBlendMode >= 3)
    {
        if (!lbPause)
        {
            CGS_ASSERT(meBlendMode == 3, "Tyring to unpause a blocking op");
            meBlendMode = meBlendModePaused;
        }
        return true;
    }

    if (lbPause)
    {
        meBlendModePaused        = meBlendMode;
        meBlendMode             = 3;
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
    meStreamOutStage  = 0;
    muStreamOutTarget = static_cast<u8>(liCurrSeason);
}

// @ 0x827C30E8. An out-of-line copy of CgsResource::ResourcePtr<T>::GetMemoryResource()
// (CgsResourcePtr.h line 581) that the X360 build emitted separately for the manager's
// per-season keyframe resource pointers: assert the main-memory resource is non-null,
// then return it. The X360 asm reads offset 0 of the passed ResourcePtr and returns it
// after the null-assert -- exactly GetMemoryResource() inlined -- so forwarding to it is
// behaviourally identical (no offset hack, no added behaviour). The plain-string assert
// message matches the baked "Can not instance resource pointer ..." rodata.
Keyframe* EnvironmentManager::ResourcePointerAssertThingy(
        CgsResource::ResourcePtr<Keyframe>& lrResourcePtr)
{
    return lrResourcePtr.GetMemoryResource();
}

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
    mePrepareStage = 0;   // back to E_..PREPARE_START
    meReleaseStage = 1;   // RELEASING
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
    // The module event-receiver queue: 1024 entries of 16 bytes, pointed at the embedded
    // storage, then cleared.
    mpReceiverQueueBuffer   = &maReceiverQueueStorage[0];
    muReceiverQueueCapacity = KU_RECEIVER_QUEUE_CAPACITY;
    muReceiverQueueStride   = KU_RECEIVER_QUEUE_STRIDE;
    memset(maReceiverQueueStorage, 0, sizeof(maReceiverQueueStorage));

    // CgsNumeric::Random, inlined: seed the 64-bit LCG state and prime the 8-entry float ring.
    // The generator is x = x * 0x5851F42D4C957F2D + 1 and each draw is the [1,2) float built
    // by or-ing the top 23 bits of the high word under an exponent of 0 (1.0f | (hi >> 9)).
    {
        u64 luState = KU_RANDOM_SEED;
        f32* const lpafRing = reinterpret_cast<f32*>(&maRandom[0]);
        for (u32 luIndex = 0; luIndex < KU_RANDOM_RING_SIZE; ++luIndex)
        {
            luState = (luState * KU_RANDOM_MULTIPLIER) + 1u;
            const u32 luBits = 0x3F800000u | (static_cast<u32>(luState >> 32) >> 9);
            f32 lfDraw;
            memcpy(&lfDraw, &luBits, sizeof(lfDraw));
            lpafRing[luIndex & (KU_RANDOM_RING_SIZE - 1)] = lfDraw;
        }
        // The u64 state and the ring index follow the eight floats (+0x20 / +0x28).
        memcpy(&maRandom[0x20], &luState, sizeof(luState));
        const u32 luRingIndex = KU_RANDOM_RING_SIZE & (KU_RANDOM_RING_SIZE - 1);
        memcpy(&maRandom[0x28], &luRingIndex, sizeof(luRingIndex));
    }

    mePrepareStage    = 0;
    meReleaseStage    = 1;

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
    meBlendMode             = 0;

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
//                    u32 from kf+0x80  -> frame+0x110 (TintData::muColourCube), weight -> frame+0x18.
//   DEFAULT  arm  -- the SAME three writes, but the Bloom/Vignette values are built fresh on the
//                    stack from the shared post-fx defaults and the two weights are 0.0f
//                    (flt_82001CC0) while the tint weight is 0.25f (flt_82003F40); the colour
//                    cube comes from the manager's own muDefTintData (+0x1194).
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
// FLAG: muDefTintData (+0x1194) is written ONLY by EnvironmentManager::Prepare @0x827D49A8
// (`stw r3, 0x1194(r31)` at 0x827D4C10, the id of the default rw::graphics::postfx::ColourCube
// it creates at +0x1174); Construct @0x827CA408 does not touch it. Prepare is an inert gate on
// this build, so the value read here is the zero-initialised BSS of the file-scope static
// gGameModule -- which is also what BrnEffects::TintData::Construct() writes, so the read is
// deterministic and harmless. It becomes live for free when Prepare lands.
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

            lTintData.muColourCube = muDefTintData;

            lpFrame->SetBloomData(lBloomData, KF_DEF_EFFECTS_LAYER_WEIGHT);
            lpFrame->SetVignetteData(lVignetteData, KF_DEF_EFFECTS_LAYER_WEIGHT);
            lpFrame->SetTintData(lTintData, KF_DEF_EFFECTS_LAYER_TINT_WEIGHT);
        }
        else
        {
            lTintData.muColourCube = lpKeyframe->muColourCube;

            lpFrame->SetBloomData(lpKeyframe->mBloom, lfWeight);
            lpFrame->SetVignetteData(lpKeyframe->mVignette, lfWeight);
            lpFrame->SetTintData(lTintData, lfWeight);
        }
    }
}

}
}
