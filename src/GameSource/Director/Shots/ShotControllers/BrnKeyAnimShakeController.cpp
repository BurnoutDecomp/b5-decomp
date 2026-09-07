// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.cpp
//
// BrnDirector::KeyAnimShakeController -- reconstructed from BURNOUT_X360_ARTIST.XEX,
// semantic parity (not byte-matching). See the header for why this class matters: it is the
// ONLY consumer of Camera::mEffects' shake request in the whole image.
//
// Bodied here (2 ledger functions):
//   KeyAnimShakeController::Construct @0x8223D240
//   KeyAnimShakeController::Update    @0x8223D488   -- BOTH arms, ICEANIM and PROCEDURAL
//
// The X360 source path its assert quotes is
// "..\..\..\GameSource\Director/Shots/ShotControllers/BrnKeyAnimShakeController.cpp", which
// is what puts this file here.
// ============================================================================

#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimShakeController.h"

#include "GameSource/Director/Shots/ShotControllers/BrnPerlinShakeController.h" // the PROCEDURAL arm's method 1
#include "GameSource/Director/Camera/Camera.h"                                  // Camera::Camera / CameraEffects / CameraState
#include "GameSource/Director/BrnDirectorResourceManager.h"                     // GetShakeAnimGroup / GetKeyAnimFromGuid
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup
#include "GameSource/AttribSys/Generated/classes/proceduralshake.h"             // Attrib::Gen::proceduralshake
#include "GameSource/AttribSys/Generated/classes/iceanim.h"                     // Attrib::Gen::iceanim (the ICEANIM arm's shot record)
#include "SDKs/Packages/ICE/ICEData.hpp"                                        // ICE::ICETake / ICE::ICETakeData
#include "rw/math/vpu/types.h"                                                  // Vector3 / Quaternion / Matrix44Affine
#include "rw/math/vpu/vector3_operation.h"                                      // Magnitude, operator*(Vector3, VecFloat)
#include "rw/math/vpu/matrix44affine_operation.h"                               // Mult(Matrix44Affine, Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // the opt-in witnesses

#include <cmath>     // std::sqrt (the quaternion's scalar part)
#include <cstring>   // std::memcpy (the proceduralshake attribute record is external data)
#include <cstdlib>   // [diag] getenv -- BRN_CAMERA_TRACE / BRN_DIRECTOR_TRACE

namespace BrnDirector
{
namespace
{
    namespace rwvpu = rw::math::vpu;

    // The CameraState flag whose set state means "this camera CUT this frame". The same
    // BitArray index 6 InertiaController::Update reads to restart its own history: on a cut
    // the shot clock restarts rather than advancing (X360 0x8223D4F8 `ld r11, 0x140(camera)` /
    // `rlwinm r11,r11,0,25,25` -- the 0x40 mask over the flags word at camera +0x144, which is
    // CameraState::mCurrentFlags' low word).
    const u32 KU_CAMERA_FLAG_CUT_THIS_FRAME = 6;

    // The `iceanim` generated-class key, FULL 64 bits (X360 0x8223D574..0x8223D584:
    // `lis 0xA997 / ori 0xC1EE / lis 0x4644 / ori 0xE379 / insrdi r11, r10, 32,0`). The same
    // doubleword Attrib::Gen::iceanim::ClassKey() returns.
    const u64 KU_ICEANIM_CLASS_KEY = 0x4644E379A997C1EEull;

    // ---- the ICE take's SHAKE channel and its six elements --------------------------------
    // Element indices are indices into ICE::ICEElementDescriptions[] (SDKs/Packages/ICE/
    // ICEData.cpp), whose entries [22]..[27] are, in order, SHAKE_QUAT_X / SHAKE_QUAT_Y /
    // SHAKE_QUAT_Z / SHAKE_POS_X / SHAKE_POS_Y / SHAKE_POS_Z -- every one of them a 16-bit
    // eICE_FIXED element on channel 11, whose name in KAPC_ICE_CHANNEL_NAMES is "SHAKE_DATA".
    // The X360 asm passes the six raw literals 0x18/0x17/0x16 and 0x1B/0x1A/0x19 to the take
    // sampler (0x8223D654..0x8223D850) and reads the key count out of channel 11 directly
    // (`lhz r10, 0x1A4(r27)` == take + 0x184 == mChannels[11], mChannels based at ICETake
    // +0x0D4 with a 16-byte ICEChannel stride: 0x184 - 0xD4 == 0xB0 == 11 * 16).
    const s32 KI_ICE_ELEMENT_SHAKE_QUAT_X = 22;
    const s32 KI_ICE_ELEMENT_SHAKE_QUAT_Y = 23;
    const s32 KI_ICE_ELEMENT_SHAKE_QUAT_Z = 24;
    const s32 KI_ICE_ELEMENT_SHAKE_POS_X  = 25;
    const s32 KI_ICE_ELEMENT_SHAKE_POS_Y  = 26;
    const s32 KI_ICE_ELEMENT_SHAKE_POS_Z  = 27;
    const s32 KI_ICE_CHANNEL_SHAKE_DATA   = 11;

    // The take's authored frame rate: the shot clock (seconds, scaled by the camera's own
    // requested shake frequency) is turned into a take frame index by multiplying by this.
    // X360 0x8223D5F8 `lfs f0, flt_82004F5C` -- the image word 0x41F00000 == 30.0f, the same
    // rodata slot a dozen other reconstructed files already name as 30.0f.
    const f32 KF_ICE_TAKE_FRAMES_PER_SECOND = 30.0f;

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

    // ------------------------------------------------------------------------------------
    // SignedMod -- ICE::ICEMath::SignedMod, which the X360 compiler INLINED into ::Update at
    // 0x8223D60C..0x8223D644 (the DecFIGS dwarfdump for BrnKeyAnimShakeController.cpp names
    // `ICE::ICEMath::SignedMod` in this function's inlined-callee list, which is what says the
    // open-coded loop below is a call and not hand-written arithmetic):
    //     lwz r11,<frame> ; cmpwi r11,0 ; bge .done      -- nothing to do for a non-negative
    //   .neg: add r11, r10, r11 ; cmpwi r11,0 ; blt .neg -- lift it into range by whole periods
    //   .done: divw/mullw/subf                           -- then the plain remainder
    // i.e. a modulo whose result is always in [0, liB), not C's sign-following remainder.
    //
    // DELETE-WHEN: ICE::ICEMath::SignedMod gets a body. It is DECLARED (SDKs/Packages/ICE/
    //   ICEMath.hpp:152) and mounted (ICEMath.cpp) but has NO definition anywhere in the tree,
    //   so calling it by name here would open a fresh unresolved external in the shared exe
    //   link. This file-local copy is the console's own inlined body; the moment ICEMath.cpp
    //   carries it, delete this and call ICE::ICEMath::SignedMod(liA, liB) instead.
    // ------------------------------------------------------------------------------------
    inline s32 SignedMod(s32 liA, s32 liB)
    {
        while (liA < 0)
            liA += liB;
        return liA % liB;
    }

    // ------------------------------------------------------------------------------------
    // Matrix44AffineFromQuaternion -- rw::math::vpu::Matrix44AffineFromQuaternion, the SDK
    // builder the DecFIGS dwarfdump names in this function's inlined-callee list, and which
    // the X360 compiler expanded at 0x8223D7D0..0x8223D824.
    //
    // ⭐ THE CONSOLE FORM IS THE SDK'S OWN sqrt(2) STRENGTH REDUCTION, and it is what
    //   IDENTIFIES the builder (it is how Matrix44AffineFromQuaternion is written, whereas
    //   the sibling ...FromQuaternionTranslation is written as the plain twelve-term form):
    //     vmulfp128 v0, v9, v0        q' = gSqrt2s * q            (rw::math::vpu::detail::gSqrt2s)
    //     vnmsubfp  v13, v0, v13, v0  neg_qq  = 0.5 - q'*q'       (halves == vcfsx(1,1))
    //     vmulfp128 v12, v0, v10      xy_yz_zx = q' * q'.yzxx
    //     vmulfp128 v11, v0, v11      wz_wx_wy = q'.w * q'.zxyx
    //     vaddfp/vsubfp v13/v12       addVec / subVec
    //     vaddfp    v0, v13, v10      diagonalValues = neg_qq + neg_qq.yzxx
    //     vperm128 x3 + vrlimi128 x3  the three rows
    //   Because q'_i q'_j == 2 q_i q_j and (0.5 - 2 q_i^2) + (0.5 - 2 q_j^2) == 1 - 2(q_i^2 +
    //   q_j^2), that expansion is EXACTLY the twelve terms below -- which is the de-optimised
    //   form AGENTS.md asks for (strength-reduction reversal), not a different matrix. Each of
    //   the nine terms was read back out of the vperm/vrlimi lane routing before being written:
    //     row0 = (diag.y, add.x, sub.z)   row1 = (sub.x, diag.z, add.y)   row2 = (add.z, sub.y, diag.x)
    //
    // FLAG (placement, not content) -- this belongs in the vendor header
    //   vendor/renderware/include/rw/math/vpu/matrix44affine_operation.h beside
    //   Matrix44AffineFromAxisRotationAngle, which is where the rest of this family lives.
    //   It is file-local here ONLY because that header is not this agent's to edit; it is in
    //   an anonymous namespace so it can never collide with the real one. Reported to the
    //   conductor. DELETE-WHEN: the vendor header carries it.
    // ------------------------------------------------------------------------------------
    inline rwvpu::Matrix44Affine Matrix44AffineFromQuaternion(const rwvpu::Quaternion& lrQuat)
    {
        const f32 lfXX = lrQuat.x * lrQuat.x;
        const f32 lfYY = lrQuat.y * lrQuat.y;
        const f32 lfZZ = lrQuat.z * lrQuat.z;
        const f32 lfXY = lrQuat.x * lrQuat.y;
        const f32 lfXZ = lrQuat.x * lrQuat.z;
        const f32 lfYZ = lrQuat.y * lrQuat.z;
        const f32 lfWX = lrQuat.w * lrQuat.x;
        const f32 lfWY = lrQuat.w * lrQuat.y;
        const f32 lfWZ = lrQuat.w * lrQuat.z;

        rwvpu::Matrix44Affine lResult;
        lResult.xAxis = rwvpu::Vector3{ 1.0f - 2.0f * (lfYY + lfZZ),
                                               2.0f * (lfXY + lfWZ),
                                               2.0f * (lfXZ - lfWY), 0.0f };
        lResult.yAxis = rwvpu::Vector3{        2.0f * (lfXY - lfWZ),
                                        1.0f - 2.0f * (lfXX + lfZZ),
                                               2.0f * (lfYZ + lfWX), 0.0f };
        lResult.zAxis = rwvpu::Vector3{        2.0f * (lfXZ + lfWY),
                                               2.0f * (lfYZ - lfWX),
                                        1.0f - 2.0f * (lfXX + lfYY), 0.0f };
        // The SDK leaves the translation row ZERO (the console's `vspltisw 0`); ::Update
        // overwrites it with the sampled take position before it composes.
        lResult.wAxis = rwvpu::Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        return lResult;
    }
}

// ----------------------------------------------------------------------------
// Construct @0x8223D240
//
// Store map, in the asm's own order:
//   stw   r4, 0x758(r3)                     mpDirectorResourceManager = the argument
//   stfs  0.0, 0x790(r3)                    mfShotRunningTime = 0
//   stfs  0.0, 0(r3) / 4 / 8 / 12           mProceduralShake.Construct()  (the four wobble words)
//   stb   0,   0x75C(r3)                    mu8ActiveShake    = 0
//   stfs  0.06 / 0.0 / 1.15 / 0.11 @0x10..  mProceduralShakeParams.Construct() (the seed
//                                           defaults -- the SAME four rodata literals nineteen
//                                           other shake-block seeds load, already committed as
//                                           CameraShake::Parameters::Construct)
//   the +0x760 block                        mRandom.Construct()     (default seed, ring slot 0
//                                           = 1.0f, seven refill draws, index += 1 & 7)
// Every one of those is an already-committed inline Construct on this build, so the body is
// the five named calls rather than a transcription of the flattened stores.
//
// NOTHING WRITES INSIDE mShakeTake. The console leaves the take at whatever the pool's
// placement-new left there until ::Update's first SetDataPointers; on the host the embedded
// ICE::ICETake's own default constructor (ICEDataICETake.cpp) runs first, which is strictly
// more defined than the console and is the same treatment the sibling
// CameraShakeICEController::Construct already documents for its identical member.
// ----------------------------------------------------------------------------
void KeyAnimShakeController::Construct(const DirectorResourceManager* lpDirectorResourceManager)
{
    mpDirectorResourceManager = lpDirectorResourceManager;
    mfShotRunningTime = 0.0f;
    mProceduralShake.Construct();
    mu8ActiveShake    = 0;
    mProceduralShakeParams.Construct();
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
//   0x8223D4F4  if (type > mpDirectorResourceManager->GetShakeAnimGroup().Num_ShotList()) return;
//                 -- `cmplw r30, r3` + `bgt`: an UNSIGNED compare, and it is > not >=,
//                    which is what makes the index one-based.
//   0x8223D51C  the shot clock: on a camera CUT (state flag 6) mfShotRunningTime = 0,
//                 otherwise += the timestep. Both arms run BEFORE the shot is resolved.
//   0x8223D554  the shot itself: GetAttributePointer("ShotList", type - 1), with
//                 Attrib::DefaultDataArea(0x18) as the null-element fallback -- i.e. exactly
//                 the committed shotgroup::GetShotListElement / ShotList(index).
//   0x8223D570  a LOCAL COPY of the element's RefSpec (the copy ctor @0x82803560 AddRefs the
//                 resolved collection), Clean()ed at the end if the resolve pinned one.
//   0x8223D594  ICEANIM arm   (class key 0x4644E379_A997C1EE)  -- see THE ICEANIM ARM below.
//   0x8223D948  PROCEDURAL arm (class key 0x88C5A4BD_B8FDFFFF)
//   0x8223D9C8  anything else -> CGS_ASSERT(false, "Unsupported ShakeType found in ShakeGroup")
//                 (BrnKeyAnimShakeController.cpp:151 -- `li r5, 0x97`).
//   0x8223D9EC  mu8ActiveShake = type, in ALL THREE arms (the `stb r26` is after the join).
//
// ---- THE ICEANIM ARM (0x8223D598..0x8223D92C), asm block -> C++ statement ------------------
//   0x8223D598  lbz +0x75C / cmplw / beq      if (mu8ActiveShake != type)  -- rebind on change
//   0x8223D5B0  Attrib::Gen::iceanim::iceanim   iceanim lShakeAnim(lShotRef, 0)       .cpp:98
//   0x8223D5BC  lwz r4, 0xC(mpAttributeData)    lShakeAnim.GetAnimGuid()
//   0x8223D5C0  GetKeyAnimFromGuid              lpShakeTake = ...GetKeyAnimFromGuid(guid) :99
//   0x8223D5D0  ICETake::SetDataPointers        mShakeTake.SetDataPointers(lpShakeTake, false)
//   0x8223D5D8  Attrib::Instance::~Instance     lShakeAnim leaves scope
//   0x8223D5DC  0x118 * 0x790 * 30.0f, fctiwz   liFrame                               .cpp:107
//   0x8223D5F0  lhz +0x1A4 / cmpwi / ble        mShakeTake.GetNumKeys(11), the zero guard
//   0x8223D60C  the SignedMod expansion         liShakeKey = SignedMod(liFrame, keys)  .cpp:108
//   0x8223D654  sub_8252F848 x3 (0x18/0x17/0x16) lvRotation, elements 24/23/22        .cpp:111
//   0x8223D690  0x114 splat, vmulfp             lvRotation *= VecFloat(GetShakeAmplitude())
//   0x8223D6E8  vmsum3fp + rsqrt refine + vsel  Magnitude(lvRotation)
//   0x8223D76C  1.0f - magnitude, sqrt, vsel    the quaternion's scalar part
//   0x8223D774  vperm/vsldoi assembly           Quaternion lRotationQuat               .cpp:117
//   0x8223D7D0  the sqrt(2) expansion           Matrix44AffineFromQuaternion           .cpp:118
//   0x8223D828  sub_8252F848 x3 (0x1B/0x1A/0x19) lvPosition, elements 27/26/25         .cpp:121
//   0x8223D858  0x114 splat, vmulfp             lvPosition *= VecFloat(GetShakeAmplitude())
//   0x8223D874  vspltw/vmaddfp cascade + 4 stvx Mult(lShakeMatrix, camera transform), stored
//   0x8223D928  ValidateTransformWithDebugInfo  the out-of-line validate the store pairs with
//
//   ⭐ THE TAKE SAMPLER IS NAMED, NOT GUESSED. X360 `sub_8252F848(take, s32, u16) -> f32` is
//     ICE::ICETake::GetValueFloat(s32 liChannel, u16 lu16Key) const -- the two-argument
//     overload, homed at SDKs/Packages/ICE/ICEData.cpp:1942 with 0x8252F848 in its own banner.
//     (The one-argument GetValueFloat @0x8252C0B8 reads the already-evaluated mValues[] table
//     instead and is NOT what this arm calls.)
//
//   ⚠️ THE SCALAR PART IS sqrt(1 - |v|), NOT sqrt(1 - |v|^2), AND THAT IS THE BINARY.
//     0x8223D6E8 `vmsum3fp128 v12, v0, v0` is |v|^2; the refined rsqrt chain then multiplies
//     it back by 1/|v| at 0x8223D73C to produce |v| (that product, not the dot, is what
//     0x8223D758 stores and 0x8223D76C subtracts from flt_82001C98 == 1.0f). So the authored
//     (SHAKE_QUAT_X, Y, Z) triple, scaled by the amplitude, is used as the imaginary part
//     directly and the scalar part comes off ONE minus the MAGNITUDE. It is not a unit
//     quaternion in general; it is what the console composes, so it is what is written.
//     The two `vsel`-against-zero guards are reproduced as the two zero tests.
//
// ⚠️ ONE DELIBERATE DEVIATION, flagged: `mpDirectorResourceManager != 0`. The console
//   dereferences it unconditionally (`lwz r11, 0x758(r27)`), because MainDirector::Construct
//   always seeds it. It is guarded here for the same reason BoostShakeController guards its
//   cameradefaults pointer: on this build the director's construct order is still a bring-up
//   variable, and a null here means the finaliser ran before Construct -- a bring-up bug to
//   find, not a camera one. DELETE with the bring-up path.
// ----------------------------------------------------------------------------
void KeyAnimShakeController::Update(f32 lfTimeStep, Camera::Camera* lpCamera)
{
    Camera::CameraEffects& lrEffects = lpCamera->GetEffects();

    // Gate 1 -- no amplitude, no shake.
    if (lrEffects.mfShakeAmplitude == 0.0f)
        return;

    // Gate 2 -- shake type 0 is "none". The type is a ONE-BASED index into the shot list.
    const u32 luShakeType = static_cast<u32>(lrEffects.mu8ShakeType);
    if (luShakeType == 0)
        return;

    if (mpDirectorResourceManager == 0)   // [FLAG PC bring-up] -- see the banner.
        return;

    // Gate 3 -- an out-of-range type is silently ignored (the console does NOT assert here;
    // the assert is further down, for a shot of an unsupported CLASS).
    const Attrib::Gen::shotgroup& lrShakeGroup = mpDirectorResourceManager->GetShakeAnimGroup();
    if (luShakeType > lrShakeGroup.Num_ShotList())
        return;

    // The shot clock. A cut restarts the shake; otherwise it runs on.
    if (lpCamera->GetState().IsFlagSet(KU_CAMERA_FLAG_CUT_THIS_FRAME))
    {
        mfShotRunningTime = 0.0f;
    }
    else
    {
        mfShotRunningTime += lfTimeStep;
    }

    // The shot this type names, and the local ref the console takes on it.
    const Attrib::RefSpec* lpShotElement = lrShakeGroup.ShotList(luShakeType - 1u);
    Attrib::RefSpec        lShotRef(*lpShotElement);

    if (lShotRef.GetClassKey() == KU_ICEANIM_CLASS_KEY)
    {
        // ---- bind: only when the requested type CHANGED --------------------------------
        // mu8ActiveShake still holds LAST frame's type at this point (its store is at the
        // join, after all three arms), so this is a genuine edge test and the take is
        // re-bound once per shake change rather than every frame.
        if (static_cast<u32>(mu8ActiveShake) != luShakeType)
        {
            const Attrib::Gen::iceanim lShakeAnim(lShotRef, 0);                 // .cpp:98
            ICE::ICETakeData* const    lpShakeTake =                            // .cpp:99
                mpDirectorResourceManager->GetKeyAnimFromGuid(lShakeAnim.GetAnimGuid());

            // The console passes the resolved take straight through with lbEdit == 0; a null
            // resolve is passed through too (SetDataPointers' own unbind path), exactly as
            // `mr r4, r3` at 0x8223D5C4 does with no test.
            mShakeTake.SetDataPointers(lpShakeTake, false);

            // [DIAG BRN_DIRECTOR_TRACE] [FLAG PC witness] -- NOT IN THE X360 BINARY. The one
            // line that proves the take was actually BOUND: it fires on the bind edge only,
            // so it is naturally budgeted by the shake-type changes themselves, and it is
            // capped anyway. DELETE-WHEN: the camera-shake bring-up closes.
            {
                static const bool sbOn = (getenv("BRN_DIRECTOR_TRACE") != 0);
                static s32        siLines = 0;
                if (sbOn && siLines < 16 && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[iceshake] bound take guid=" << lShakeAnim.GetAnimGuid()
                        << " type=" << luShakeType
                        << " takeData=" << (lpShakeTake != 0 ? 1 : 0)
                        << " keys=" << static_cast<u32>(
                               mShakeTake.GetNumKeys(KI_ICE_CHANNEL_SHAKE_DATA))
                        << "\n";
                }
            }
        }

        // ---- the shot clock -> a take key ----------------------------------------------
        // The camera's own requested FREQUENCY scales the clock before it is converted, so a
        // frequency of 2 plays the authored take at double speed.
        const s32 liFrame = static_cast<s32>(lrEffects.GetShakeFrequency()      // .cpp:107
                                             * mfShotRunningTime
                                             * KF_ICE_TAKE_FRAMES_PER_SECOND);

        // The key count of the SHAKE_DATA channel, and the console's own zero guard
        // (`cmpwi r10, 0` / `ble` at 0x8223D5F4 -- the count is loaded with `lhz`, so this is
        // an "is it empty" test). Whether the guard belongs to SignedMod or to this caller
        // cannot be told from an inlined copy, so it is reproduced HERE, where it is correct
        // under either reading and cannot divide by zero.
        const u16 lu16NumKeys = mShakeTake.GetNumKeys(KI_ICE_CHANNEL_SHAKE_DATA);
        s32       liShakeKey  = 0;                                              // .cpp:108
        if (lu16NumKeys != 0)
            liShakeKey = SignedMod(liFrame, static_cast<s32>(lu16NumKeys));

        // `clrlwi r30, r11, 16` -- the key is carried to the sampler as a u16.
        const u16 lu16ShakeKey = static_cast<u16>(liShakeKey);

        // The amplitude is broadcast once and used by BOTH triples (the console re-loads
        // camera +0x114 for each, at 0x8223D690 and 0x8223D858).
        const rwvpu::Vector4 lvfShakeAmplitude = { lrEffects.GetShakeAmplitude(),
                                                   lrEffects.GetShakeAmplitude(),
                                                   lrEffects.GetShakeAmplitude(),
                                                   lrEffects.GetShakeAmplitude() };

        // ---- the ROTATION triple -> a quaternion ---------------------------------------
        rwvpu::Vector3 lvRotation =                                             // .cpp:111
        {
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_QUAT_X, lu16ShakeKey),
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_QUAT_Y, lu16ShakeKey),
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_QUAT_Z, lu16ShakeKey),
            0.0f
        };
        lvRotation = lvRotation * lvfShakeAmplitude;

        // See the banner: the scalar part is sqrt(1 - MAGNITUDE), and the two vsel guards
        // substitute zero rather than let a degenerate input propagate.
        const f32 lfMagnitude         = rwvpu::Magnitude(lvRotation);
        const f32 lfOneMinusMagnitude = 1.0f - lfMagnitude;
        const f32 lfQuatW = (lfOneMinusMagnitude > 0.0f) ? std::sqrt(lfOneMinusMagnitude)
                                                         : 0.0f;

        const rwvpu::Quaternion lRotationQuat =                                 // .cpp:117
            { lvRotation.x, lvRotation.y, lvRotation.z, lfQuatW };

        rwvpu::Matrix44Affine lShakeMatrix =                                    // .cpp:118
            Matrix44AffineFromQuaternion(lRotationQuat);

        // ---- the POSITION triple -> the shake matrix's translation row ------------------
        rwvpu::Vector3 lvPosition =                                             // .cpp:121
        {
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_POS_X, lu16ShakeKey),
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_POS_Y, lu16ShakeKey),
            mShakeTake.GetValueFloat(KI_ICE_ELEMENT_SHAKE_POS_Z, lu16ShakeKey),
            0.0f
        };
        lvPosition = lvPosition * lvfShakeAmplitude;
        lShakeMatrix.wAxis = lvPosition;

        // ---- compose, in the camera's OWN space, and publish ---------------------------
        // The console never materialises the shake matrix's own wAxis: it feeds lvPosition
        // straight into the fourth row of the product. Mult(lhs, rhs) is exactly that
        // (out.row_i = lhs.row_i.x*rhs.xAxis + .y*rhs.yAxis + .z*rhs.zAxis, and the
        // translation row additionally + rhs.wAxis), which is why the shake rotates and
        // shifts the camera about ITSELF rather than about the world origin.
        lpCamera->SetTransform(rwvpu::Mult(lShakeMatrix, lpCamera->GetTransform()));

        // 0x8223D928 -- the out-of-line validate the four `stvx128` are paired with.
        lpCamera->ValidateTransformWithDebugInfo();

        // [DIAG BRN_DIRECTOR_TRACE] [FLAG PC witness] -- NOT IN THE X360 BINARY. The one line
        // that proves the bound take was SAMPLED with real authored data: the key it landed
        // on, the key count it wrapped against, and the rotation it composed. First 32 only.
        // DELETE-WHEN: the camera-shake bring-up closes.
        {
            static const bool sbOn = (getenv("BRN_DIRECTOR_TRACE") != 0);
            static s32        siLines = 0;
            if (sbOn && siLines < 32 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siLines;
                *CgsDev::Log::gpDebugPrint
                    << "[iceshake] sampled type=" << luShakeType
                    << " frame=" << liFrame << " key=" << liShakeKey
                    << "/" << static_cast<u32>(lu16NumKeys)
                    << " quat=(" << lRotationQuat.x << "," << lRotationQuat.y
                    << "," << lRotationQuat.z << "," << lRotationQuat.w << ")"
                    << " pos=(" << lvPosition.x << "," << lvPosition.y
                    << "," << lvPosition.z << ")"
                    << " amp=" << lrEffects.GetShakeAmplitude()
                    << " t=" << mfShotRunningTime << "\n";
            }
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
                mProceduralShake.Update(lpCamera->mTransform, mProceduralShakeParams, mRandom,
                                        lrEffects.mfShakeFrequency * lfTimeStep,
                                        lrEffects.mfShakeAmplitude);
            }
        }
    }
    else
    {
        CGS_ASSERT(false, "Unsupported ShakeType found in ShakeGroup");   // .cpp:151
    }

    mu8ActiveShake = static_cast<u8>(luShakeType);

    if (lShotRef.HasResolvedCollection())
        lShotRef.Clean();
}

}
