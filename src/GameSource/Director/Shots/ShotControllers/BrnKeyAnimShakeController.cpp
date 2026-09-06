// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.cpp
//
// BrnDirector::KeyAnimShakeController -- reconstructed from BURNOUT_X360_ARTIST.XEX,
// semantic parity (not byte-matching). See the header for why this class matters: it is the
// ONLY consumer of Camera::mEffects' shake request in the whole image.
//
// Bodied here (2 ledger functions):
//   KeyAnimShakeController::Construct @0x8223D240
//   KeyAnimShakeController::Update    @0x8223D488
//
// The X360 source path its assert quotes is
// "..\..\..\GameSource\Director/Shots/ShotControllers/BrnKeyAnimShakeController.cpp", which
// is what puts this file here.
// ============================================================================

#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.h"

#include "GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.h" // the PROCEDURAL arm's method 1
#include "GameSource/Director/Camera/Camera.h"                                  // Camera::Camera / CameraEffects / CameraState
#include "GameSource/Director/BrnDirectorResourceManager.h"                     // GetShakeAnimGroup
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup
#include "GameSource/AttribSys/Generated/classes/proceduralshake.h"             // Attrib::Gen::proceduralshake
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // the ICEANIM park's one-shot notice

#include <cstring>   // std::memcpy (the proceduralshake attribute record is external data)
#include <cstdlib>   // [diag] getenv -- BRN_CAMERA_TRACE

namespace BrnDirector
{
namespace
{
    // The CameraState flag whose set state means "this camera CUT this frame". The same
    // BitArray index 6 InertiaController::Update reads to restart its own history: on a cut
    // the shot clock restarts rather than advancing (X360 0x8223D4F8 `ld r11, 0x140(camera)` /
    // `rlwinm r11,r11,0,25,25` -- the 0x40 mask over the flags word at camera +0x144, which is
    // CameraState::mCurrentFlags' low word).
    const u32 KU_CAMERA_FLAG_CUT_THIS_FRAME = 6;

    // The `iceanim` generated-class key, FULL 64 bits (X360 0x8223D574..0x8223D584:
    // `lis 0xA997 / ori 0xC1EE / lis 0x4644 / ori 0xE379 / insrdi r11, r10, 32,0`). The same
    // doubleword Attrib::Gen::iceanim's own ctor compares against.
    const u64 KU_ICEANIM_CLASS_KEY = 0x4644E379A997C1EEull;

    // ---- the proceduralshake ATTRIBUTE-DATA field map ------------------------------------
    // These stay at their console byte offsets on purpose: they index the SERIALISED
    // AttribSys record the vault ships (a 0x1C-byte data area), not a host struct -- the same
    // rule and the same idiom as BrnBoostShakeController.cpp's cameradefaults curve fields and
    // BrnMainDirector.cpp's KU_*_SOURCE_BOOST_FOV_OFFSET pair.
    //
    // The ROLE of each is read straight off the PerlinShakeController::Update call at
    // 0x8223D96C..0x8223D98C, whose seven float registers are loaded in this order:
    //     f1 = mfShotRunningTime      f2 = data+0x0C   f3 = data+0x14   f4 = data+0x00
    //     f5 = data+0x10              f6 = data+0x18   f7 = data+0x04
    // against the declared parameter list
    //     Update(camera, lfTime, lfRollScale, lfPitchScale, lfYawScale,
    //                           lfRollFreqScale, lfPitchFreqScale, lfYawFreqScale)
    // -- so the record is three (scale, frequency-scale) pairs plus the method selector, and
    // the generated class's own name list ("Pitch/Roll/Yaw Frequency + Scale + ShakeMethod")
    // is the independent corroboration of that reading.
    const u32 KU_PS_YAW_SCALE_OFFSET        = 0x00;
    const u32 KU_PS_YAW_FREQ_SCALE_OFFSET   = 0x04;
    const u32 KU_PS_SHAKE_METHOD_OFFSET     = 0x08;   // `lwz r10, 8(r11)`; == 1 -> Perlin
    const u32 KU_PS_ROLL_SCALE_OFFSET       = 0x0C;
    const u32 KU_PS_ROLL_FREQ_SCALE_OFFSET  = 0x10;
    const u32 KU_PS_PITCH_SCALE_OFFSET      = 0x14;
    const u32 KU_PS_PITCH_FREQ_SCALE_OFFSET = 0x18;

    // The shake METHOD selector value that selects the Perlin-noise shake; anything else takes
    // the procedural CameraShake wobble (X360 `cmpwi cr6, r10, 1` / `bne`).
    const s32 KI_SHAKE_METHOD_PERLIN = 1;

    inline f32 ReadFloat(const u8* lpData, u32 luByteOffset)
    {
        f32 lfValue = 0.0f;
        std::memcpy(&lfValue, lpData + luByteOffset, sizeof(f32));
        return lfValue;
    }

    inline s32 ReadInt(const u8* lpData, u32 luByteOffset)
    {
        s32 liValue = 0;
        std::memcpy(&liValue, lpData + luByteOffset, sizeof(s32));
        return liValue;
    }
}

// ----------------------------------------------------------------------------
// Construct @0x8223D240
//
// Store map, in the asm's own order:
//   stw   r4, 0x758(r3)                     mpResourceManager = the argument
//   stfs  0.0, 0x790(r3)                    mfShotRunningTime = 0
//   stfs  0.0, 0(r3) / 4 / 8 / 12           mShake.Construct()      (the four wobble words)
//   stb   0,   0x75C(r3)                    mu8LastShakeType  = 0
//   stfs  0.06 / 0.0 / 1.15 / 0.11 @0x10..  mShakeParams.Construct() (the seed defaults --
//                                           the SAME four rodata literals nineteen other
//                                           shake-block seeds load, already committed as
//                                           CameraShake::Parameters::Construct)
//   the +0x760 block                        mRandom.Construct()     (default seed, ring slot 0
//                                           = 1.0f, seven refill draws, index += 1 & 7)
// Every one of those is an already-committed inline Construct on this build, so the body is
// the five named calls rather than a transcription of the flattened stores.
// ----------------------------------------------------------------------------
void KeyAnimShakeController::Construct(const DirectorResourceManager* lpResourceManager)
{
    mpResourceManager = lpResourceManager;
    mfShotRunningTime = 0.0f;
    mShake.Construct();
    mu8LastShakeType  = 0;
    mShakeParams.Construct();
    mRandom.Construct();
}

// ----------------------------------------------------------------------------
// Update @0x8223D488
//
// THE WALK, gate by gate (asm addresses are the branch that decides each):
//
//   0x8223D4D4  if (camera.mEffects.mfShakeAmplitude == 0.0f) return;
//                 -- an EXACT float compare against flt_82001CC0, not an epsilon.
//   0x8223D4E0  if (camera.mEffects.mu8ShakeType == 0) return;
//                 -- type 0 is "no shake"; the type is a ONE-BASED shot index.
//   0x8223D4F4  if (type > mpResourceManager->GetShakeAnimGroup().Num_ShotList()) return;
//                 -- `cmplw r30, r3` + `bgt`: an UNSIGNED compare, and it is > not >=,
//                    which is what makes the index one-based.
//   0x8223D51C  the shot clock: on a camera CUT (state flag 6) mfShotRunningTime = 0,
//                 otherwise += the timestep. Both arms run BEFORE the shot is resolved.
//   0x8223D554  the shot itself: GetAttributePointer("ShotList", type - 1), with
//                 Attrib::DefaultDataArea(0x18) as the null-element fallback -- i.e. exactly
//                 the committed shotgroup::GetShotListElement / ShotList(index).
//   0x8223D570  a LOCAL COPY of the element's RefSpec (the copy ctor @0x82803560 AddRefs the
//                 resolved collection), Clean()ed at the end if the resolve pinned one.
//   0x8223D594  ICEANIM arm   (class key 0x4644E379_A997C1EE)  -- FLAGGED, see below.
//   0x8223D948  PROCEDURAL arm (class key 0x88C5A4BD_B8FDFFFF) -- REPRODUCED.
//   0x8223D9C8  anything else -> CGS_ASSERT(false, "Unsupported ShakeType found in ShakeGroup")
//                 (BrnKeyAnimShakeController.cpp:151 -- `li r5, 0x97`).
//   0x8223D9EC  mu8LastShakeType = type, in ALL THREE arms (the `stb r26` is after the join).
//
// ⚠️ [FLAG PC bring-up] THE ICEANIM ARM IS NOT TRANSCRIBED, and it is a NAMED park, not a
//   silent drop. That arm (0x8223D598..0x8223D92C) re-binds mShakeTake through
//   DirectorResourceManager::GetKeyAnimFromGuid + ICE::ICETake::SetDataPointers whenever the
//   requested type changes, computes a take frame index from
//   (mfShakeFrequency * mfShotRunningTime * 30) modulo the take's key count, samples six
//   ICETake channels (22/23/24 rotation, 25/26/27 translation) through
//   ICETake::GetValueFloat, and composes an axis-angle rotation onto the camera rows with a
//   ~140-instruction VMX quaternion pipeline. Three of its four dependencies have no
//   reconstructed home on this build (ICETake::GetValueFloat / SetDataPointers /
//   GetKeyAnimFromGuid are declaration-only or unhomed, and mShakeTake is a sized opaque
//   sub-object), so transcribing it would mean inventing the take runtime -- which is what
//   this project forbids. It announces itself ONCE at runtime instead of failing quietly.
//   ⛔ DELETE-WHEN: the ICE take runtime (ICETake::GetValueFloat + DirectorResourceManager::
//   GetKeyAnimFromGuid) lands. Then swap OpaqueICETake for the real ICE::ICETake and
//   transcribe the arm.
//   CONSEQUENCE WHILE PARKED: a shake shot authored as an ICE key-anim take does nothing; a
//   shake shot authored as a proceduralshake record works exactly as the console's.
//
// ⚠️ ONE DELIBERATE DEVIATION, flagged: `mpResourceManager != 0`. The console dereferences it
//   unconditionally (`lwz r11, 0x758(r27)`), because MainDirector::Construct always seeds it.
//   It is guarded here for the same reason BoostShakeController guards its cameradefaults
//   pointer: on this build the director's construct order is still a bring-up variable, and a
//   null here means the finaliser ran before Construct -- a bring-up bug to find, not a camera
//   one. DELETE with the bring-up path.
// ----------------------------------------------------------------------------
void KeyAnimShakeController::Update(f32 lfTimestep, Camera::Camera* lpCamera)
{
    Camera::CameraEffects& lrEffects = lpCamera->GetEffects();

    // Gate 1 -- no amplitude, no shake.
    if (lrEffects.mfShakeAmplitude == 0.0f)
        return;

    // Gate 2 -- shake type 0 is "none". The type is a ONE-BASED index into the shot list.
    const u32 luShakeType = static_cast<u32>(lrEffects.mu8ShakeType);
    if (luShakeType == 0)
        return;

    if (mpResourceManager == 0)   // [FLAG PC bring-up] -- see the banner.
        return;

    // Gate 3 -- an out-of-range type is silently ignored (the console does NOT assert here;
    // the assert is further down, for a shot of an unsupported CLASS).
    const Attrib::Gen::shotgroup& lrShakeGroup = mpResourceManager->GetShakeAnimGroup();
    if (luShakeType > lrShakeGroup.Num_ShotList())
        return;

    // The shot clock. A cut restarts the shake; otherwise it runs on.
    if (lpCamera->GetState().IsFlagSet(KU_CAMERA_FLAG_CUT_THIS_FRAME))
    {
        mfShotRunningTime = 0.0f;
    }
    else
    {
        mfShotRunningTime += lfTimestep;
    }

    // The shot this type names, and the local ref the console takes on it.
    const Attrib::RefSpec* lpShotElement = lrShakeGroup.ShotList(luShakeType - 1u);
    Attrib::RefSpec        lShotRef(*lpShotElement);

    if (lShotRef.GetClassKey() == KU_ICEANIM_CLASS_KEY)
    {
        // [FLAG PC bring-up] the ICE key-anim shake arm -- parked, and LOUD. See the banner.
        static bool sbAnnounced = false;
        if (!sbAnnounced && CgsDev::Log::gpDebugPrint != 0)
        {
            sbAnnounced = true;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] KeyAnimShakeController: shake type " << luShakeType
                << " is an ICEANIM take; the take-runtime arm @0x8223D598 is not"
                   " reconstructed, so this shake does nothing.\n";
        }
    }
    else if (lShotRef.GetClassKey() == Attrib::Gen::proceduralshake::KU_PROCEDURALSHAKE_CLASS_KEY)
    {
        const Attrib::Gen::proceduralshake lShakeRecord(lShotRef, 0);
        const u8* const lpData = static_cast<const u8*>(lShakeRecord.GetLayoutPointer());

        // [DIAG BRN_CAMERA_TRACE] [FLAG PC witness] -- NOT IN THE X360 BINARY. The one line
        // that says the console arm was REACHED with real authored data: the shake type, the
        // authored method, and the amplitude the camera asked for. First 32 only.
        // DELETE-WHEN: the camera-shake bring-up closes.
        {
            static const bool sbOn = (getenv("BRN_CAMERA_TRACE") != 0);
            static s32 siLines = 0;
            if (sbOn && siLines < 32 && CgsDev::Log::gpDebugPrint != 0 && lpData != 0)
            {
                ++siLines;
                *CgsDev::Log::gpDebugPrint
                    << "[cam] shake applied type=" << luShakeType
                    << " method=" << ReadInt(lpData, KU_PS_SHAKE_METHOD_OFFSET)
                    << " amp=" << lrEffects.mfShakeAmplitude
                    << " freq=" << lrEffects.mfShakeFrequency
                    << " t=" << mfShotRunningTime << "\n";
            }
        }

        // The console has no null test: proceduralshake's ctor gives the instance a 0x1C-byte
        // default data area when the resolve produced none, so lpData is never null there.
        // [FLAG PC bring-up] guarded anyway -- a null here means the attribute vault did not
        // load, which is a resource bug to find rather than a fault to take. DELETE with the
        // bring-up path.
        if (lpData != 0)
        {
            if (ReadInt(lpData, KU_PS_SHAKE_METHOD_OFFSET) == KI_SHAKE_METHOD_PERLIN)
            {
                PerlinShakeController::Update(lpCamera, mfShotRunningTime,
                                              ReadFloat(lpData, KU_PS_ROLL_SCALE_OFFSET),
                                              ReadFloat(lpData, KU_PS_PITCH_SCALE_OFFSET),
                                              ReadFloat(lpData, KU_PS_YAW_SCALE_OFFSET),
                                              ReadFloat(lpData, KU_PS_ROLL_FREQ_SCALE_OFFSET),
                                              ReadFloat(lpData, KU_PS_PITCH_FREQ_SCALE_OFFSET),
                                              ReadFloat(lpData, KU_PS_YAW_FREQ_SCALE_OFFSET));
            }
            else
            {
                // The wobble shake, driven by the camera's OWN request: the frequency
                // multiplies the timestep, the amplitude scales the resulting angle
                // (`lfs f0, 0x118(camera)` / `fmuls f1, f0, f30` and `lfs f2, 0x114(camera)`).
                mShake.Update(lpCamera->mTransform, mShakeParams, mRandom,
                              lrEffects.mfShakeFrequency * lfTimestep,
                              lrEffects.mfShakeAmplitude);
            }
        }
    }
    else
    {
        CGS_ASSERT(false, "Unsupported ShakeType found in ShakeGroup");   // .cpp:151
    }

    mu8LastShakeType = static_cast<u8>(luShakeType);

    if (lShotRef.HasResolvedCollection())
        lShotRef.Clean();
}

}
