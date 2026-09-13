// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraShakeICEController.cpp
//
// Compilation home for BrnDirector::Camera::Utils::CameraShakeICEController:
//   ::Construct @0x8223EBF0  (186 asm lines)  -- COMPLETE
//   ::Update    @0x8223EEC8  (~590 asm lines) -- HEAD + THE THREE GATES; the ICE take arm
//                                                is a documented, LOUD partial (see below)
// (::GetMatrix is a one-line accessor and lives inline in BrnCameraShake.h, which is where
//  both console builds put it -- it has no standalone symbol on either.)
//
// ⭐⭐ WHY THIS FILE EXISTS -- IT IS A LINK-CLOSURE SPLIT, exactly like the sibling
// BrnCameraShakeUpdate.cpp. The DWARF home of this class is BrnCameraShake.cpp, and that TU
// also carries the three `CameraShake::Parameters::Serialise<S>` explicit instantiations,
// whose DebugMenuSerialiser / TextFileWriteSerialiser / TextFileReadSerialiser bodies are all
// out-of-line in TUs that are not on the build list. Mounting the whole file would open three
// unresolved externals to close two. Same precedent as BrnCameraShakeUpdate.cpp and
// BrnCameraTweakerConstruct.cpp.
//
// ⚠️⚠️ WHAT ::Construct RETIRES, AND WHY IT IS NOT COSMETIC.
// Until this TU landed, `CameraShakeICEController::Construct` resolved to an EMPTY `{}` in
// Director/DirectorLinkStubs.cpp (group E). Its committed justification was
//   "unlike its neighbour it is a real (non-inlined) console call whose body was never dumped,
//    so there is nothing to transcribe"
// and that is FALSE: 0x8223EBF0 is a fully exported 186-line function in
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8223EBF0.json, and the whole body is transcribed
// below. That is the FOURTEENTH stale gate on this campaign, and the second in this class's
// immediate neighbourhood (CameraSphericalRotationController::Construct was the previous one).
// ⇒ WHEN A GATE NAMES A REASON, TEST THE REASON. "Never dumped" is a claim about the export
//   set, and the export set is one `ls` away.
//
// ⭐⭐ AND THE STUB WAS ARMED, NOT MERELY INCOMPLETE. The second half of its own note --
// "safe because the pools placement-new every behaviour with `new (slot) T()`, so the
// sub-object starts zeroed" -- is exactly the danger. `Construct`'s real job is to set
// mMatrix to the IDENTITY. Zero-initialised, mMatrix is the ALL-ZERO matrix, and
// BehaviourGameplayExternal::Update multiplies GetMatrix()'s result into the camera transform
// (it inlines the accessor as four `lvx128` off mBoostShake at 0x82241C70..0x82241C8C). An
// all-zero matrix does not "do nothing" -- it ANNIHILATES the transform, collapsing the chase
// camera to the origin with an empty basis. BehaviourGameplayExternal::Prepare already calls
// mBoostShake.Construct() (BrnBehaviourGameplayExternal.cpp:260), so the zeroed matrix was
// already sitting in the object, waiting for the one caller that reads it to land.
// ⇒ this is the same shape as the RaceCarState::operator= silent-drop stub: a stub whose
//   "not on the live path" excuse expires the instant that path lights up.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"      // THE home for this class
#include "GameSource/AttribSys/Generated/classes/iceanim.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <cmath>
#include <cstdlib>
#include "GameSource/Director/BrnDirectorResourceManager.h"       // GetShakeAnimGroup() (+0x518)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"     // Attrib::Gen::shotgroup::Num_ShotList
#include "GameShared/GameClasses/Numeric/CgsRandom.h"             // Random::Construct / ::RandomFloat
#include "GameShared/GameClasses/Development/Log/CgsLog.h"        // [diag] CgsDev::Log::gpDebugPrint
#include "rw/math/vpu/types.h"                                    // Matrix44Affine (complete)
#include "rw/math/fpu/scalar_operation.h"                         // Clamp -- the console's fsel pair

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    typedef rw::math::vpu::Vector3 Vector3;

// ============================================================================
// CameraShakeICEController::Construct @0x8223EBF0 / BrnCameraShake.cpp:203  (186 asm lines)
//
// ⭐ CROSS-CHECKED AGAINST THE DecFIGS DWARFDUMP AFTER BEING WRITTEN FROM THE ASM, and the two
// agree: the dwarfdump's call list for this function is exactly
// `CgsNumeric::Random::Construct`, a BRACED BLOCK containing `CgsNumeric::Random::RandomFloat`
// (the eight-slot ring prime's loop), and `rw::math::vpu::Matrix44Affine::SetIdentity`.
// Nothing else -- which is what says the two Parameters seeds below are the sub-objects' own
// inlined Constructs rather than open-coded stores.
//
// Every store in the function is accounted for, by offset:
//   +0x000..+0x03F  mMatrix              four stvx128 of an identity built on the stack
//                                        (three 1.0f from flt_82001C98 on the diagonal, the
//                                        wAxis row all zero -- exactly Matrix44Affine::
//                                        SetIdentity()'s wAxis, which is (0,0,0,0) not
//                                        (0,0,0,1))
//   +0x040..+0x04C  mProceduralShake     four stfs of flt_82001CC0 == 0.0f
//   +0x050          mProceduralShakeParams.mfXYShakeMagnitudeDegs  = flt_820047B8 = 0.06f
//   +0x054            .mfZShakeMagnitudeDegs                       = flt_82001CC0 = 0.0f
//   +0x058            .mfXYWobbleMagnitudeDegs                     = flt_820047BC = 1.15f
//   +0x05C            .mfWobbleCenteringFactor                     = flt_820047C0 = 0.11f
//   +0x798          mu8ActiveShake       stb of 0
//   +0x7A0..+0x7C8  mRandom              the default seed + the eight-slot ring prime
//   +0x7D0          mfShotRunningTime    stfs 0.0f
//   +0x7D4          mfBumpValue          stfs 0.0f
// Nothing writes inside mShakeTake (+0x060..+0x797): the ICETake is left as the pool's
// zero-initialised storage until the first SetDataPointers.
//
// ⭐ THE PARAMS BLOCK IS NOT A NEW SEED -- it is byte-for-byte CameraShake::Parameters::
//   Construct() (0.06 / 0.0 / 1.15 / 0.11, the three rodata literals in that member order),
//   which is already committed in BrnCameraShake.h with three independent witnesses. Written
//   as the call, not as four re-derived stores.
//
// ⚠️ AND A NEGATIVE WORTH RECORDING: `mProceduralShake` / `mProceduralShakeParams` are
//   INITIALISED HERE AND NEVER READ BY ::Update. The whole of 0x8223EEC8..0x8223F7F0 does not
//   touch +0x40..+0x5F once. Whatever the embedded CameraShake is for, the ICE controller's
//   per-frame path is not it -- so do not go looking for a "procedural fallback" arm that
//   consumes them, because there isn't one (the vibration ::Update does compute is a bare
//   scalar random walk on mfBumpValue, not a CameraShake::Update call). The old banner in
//   BrnCameraShake.h that promised such a fallback is corrected.
//
// ⭐ THE RNG SEED IS THE ENGINE DEFAULT, VERIFIED not assumed. The asm builds it as
//       lis r10, 0x1AD0 ; ori r10, r10, 0x891B      -> 0x1AD0891B
//       lis r8,  -0x3784; ori r8,  r8,  0xD8C9      -> 0xC87CD8C9
//       insrdi r10, r8, 32, 0                        -> 0xC87CD8C9_1AD0891B
//   (`insrdi RA,RS,32,0` puts RS in the HIGH half -- the same idiom, and the same trap, as
//   the LCG multiplier two lines further down), and that is exactly
//   CgsNumeric::KU_RANDOM_DEFAULT_SEED. The eight-slot prime that follows -- slot[0] = 1.0f,
//   then SEVEN draws that each store at the NEWLY bumped index, then one final index bump --
//   is `Random::Construct()` instruction for instruction, including the detail that separates
//   it from the scalar draw (AddRandomFloatToBuffer bumps THEN writes; RandomFloat writes the
//   CURRENT slot THEN bumps). Called by name; nothing is poked into the Random by offset.
// ============================================================================
void CameraShakeICEController::Construct()
{
    mMatrix.SetIdentity();
    mProceduralShake.Construct();
    mProceduralShakeParams.Construct();
    mu8ActiveShake = 0;
    mRandom.Construct();
    mfShotRunningTime = 0.0f;
    mfBumpValue       = 0.0f;
}

// ============================================================================
// CameraShakeICEController::Update @0x8223EEC8 / PS3 @0x68840 / BrnCameraShake.cpp:233
//
// The authored take is sampled and blended after the original early-out gates.
// Calling convention verified against ARTIST:
//   r6 lu8ShakeType / f2 lfShakeAmplitude / f3 lfShakeFrequency -- note r5 is SKIPPED, which
//   is the float argument consuming its GPR slot, and is what pins the u8 to the FOURTH
//   parameter rather than the third. The DecFIGS symbol says the same thing outright:
//     _ZN...CameraShakeICEController6UpdateEPKNS_23DirectorResourceManagerEfhff
//     -> Update(const DirectorResourceManager*, f32, u8, f32, f32)
//   and the DecFIGS dwarfdump prints the declaration outright, argument names and all:
//     void CameraShakeICEController::Update(const BrnDirector::DirectorResourceManager*
//              lpDirectorResourceManager, float32_t lfTimestep, uint8_t lu8ShakeType,
//              float32_t lfShakeAmplitude, float32_t lfShakeFrequency)
//   THREE independent witnesses (X360 register map, PS3 mangling, DWARF declaration) for a
//   signature Hex-Rays renders as nine `int`s and a `double`.
//   ⚠️ The two f32 that the committed declaration used to call lfMagnitude / lfFrequency are
//     renamed to the DWARF's own spellings here and in the header.
// ============================================================================
void CameraShakeICEController::Update(const BrnDirector::DirectorResourceManager* lpDirectorResourceManager,
                                      f32 lfTimestep, u8 lu8ShakeType,
                                      f32 lfShakeAmplitude, f32 lfShakeFrequency)
{
    // ---- the two tunables, .cpp:235 / :236 ----------------------------------
    // BOTH NAMES ARE THE DWARF'S OWN, and both are FUNCTION-SCOPE, not file-scope: the
    // DecFIGS dwarfdump lists them as locals of this function at BrnCameraShake.cpp:235 and
    // :236, and the PS3 export mangles them as function-local statics
    //   _ZZN11BrnDirector6Camera5Utils24CameraShakeICEController6Update
    //       EPKNS_23DirectorResourceManagerEfhffE20kfShakeFocalDistance   (and ...E22kfShakeVibrationAmount)
    // The VALUES are the X360's, dumped from the image with scratchpad\fc_flt.py (the reader
    // that uses the per-segment mapping -- NOT a segs[0]-based one).
    // ⚠️ Both live in .data, not .rdata, i.e. they are writable tweakables on the console.
    //   They are NOT dynamically-initialised BSS: their image words ARE the values, and the
    //   check that says so is that the same 16-byte run also reads back 0x82CDAD58/5C/60 as
    //   9.0f / 1.0f / 2 -- the three constants BehaviourGameplayExternal::Update's .cpp:445
    //   call site was already independently attested to use. That check is deliberate; this
    //   subsystem's signature failure is a plausible zero that nothing downstream reports.
    // ⭐ WHAT THEY MEAN. The published bump is `mfBumpValue * 0.01f / kfShakeFocalDistance`:
    //   a small LINEAR displacement (0.01 m per unit of bump) divided by a DISTANCE, giving a
    //   small ANGLE in radians -- the small-angle vibration of a camera sighting a subject
    //   10 m away. That is what a "focal distance" is doing as a divisor, and it is why the
    //   result is added to an EULER ANGLE vector rather than to a position.
    const f32 kfShakeFocalDistance   = 10.0f;   // flt_82CDAD48
    const f32 kfShakeVibrationAmount = 0.07f;   // flt_82CDAD4C

    // ---- the shot clock ------------------------------------------------------
    // Runs UNGATED, before every early-out: `lfs f0, 0x7D0(r30); fadds f0, f1, f0; stfs`.
    mfShotRunningTime += lfTimestep;

    // ---- the procedural vibration: a rate-limited random walk toward r^3 -----
    // ⚠️ THE DRAW IS THE BOUNDED SCALAR OVERLOAD, not a raw ring read. The console emits
    //   `slot - 1.0f` then one `fmsubs f13, f11, 2.0f, 1.0f` (X360 flt_82001D9C == 2.0f);
    //   PS3 emits `fadds f0,f0,f0; fsubs f0,f0,1.0f`. Both are exactly what
    //   Random::RandomFloat(-1,1) folds to, since its `(lfMax - lfMin) * t + lfMin` has
    //   lfMax - lfMin == 2.0f as a compile-time constant. Called by name so the ring
    //   arithmetic (refill the CURRENT slot, THEN bump the index) stays in one place.
    // ⚠️ INFERRED, and flagged: the DWARF also declares `Random::RandomSignedFloat()`, which
    //   would fold to byte-identical code. Nothing on either export distinguishes the two
    //   spellings. RandomFloat(-1,1) is chosen because it is BODIED (CgsRandom.cpp:113) and
    //   RandomSignedFloat is declaration-only -- i.e. the alternative reading would open an
    //   unresolved external, not a different behaviour.
    const f32 lfRandomValue = mRandom.RandomFloat(-1.0f, 1.0f);            // .cpp:240

    // `fmuls f10,f13,f13 ; fmsubs f13,f10,f13,f11` == r*r*r - mfBumpValue, then the fsel pair
    // clamps that DELTA (not the result) to +/- kfShakeVibrationAmount, and the old value is
    // added back. The cube is what keeps the walk quiet most of the time and lets it spike.
    mfBumpValue += rw::math::fpu::Clamp(lfRandomValue * lfRandomValue * lfRandomValue - mfBumpValue,
                                        -kfShakeVibrationAmount,
                                         kfShakeVibrationAmount);

    // ---- .cpp:244 -- the procedural vibration, as a ZXY-Euler triple --------
    // NAME AND TYPE ARE THE DWARF'S (`Vector3 lTargetCameraAnglesRandom`, BrnCameraShake.cpp:244).
    // The console zeroes lanes 1 and 2 with two `vrlimi128` against v126 == 0 and leaves lane 3
    // as Vector3's don't-care, so only PITCH vibrates -- lane 0 of an EulerAnglesZXY triple is
    // pitch (ModifyTargetAngles clamps .x against mrPitchLimit).
    // The bare 0.01f is flt_82002138 / PS3 dword_FF2E38: an anonymous literal on BOTH builds
    // (no DWARF name, no dwarfdump local entry), so it is spelled as one rather than given an
    // invented name. Same address the sibling Parameters::Set banner already lists as 0.01f.
    //
    // ⭐⭐ THIS VALUE IS THE ARM'S INPUT, NOT A FALLBACK. Its ONLY consumer is
    //   `vaddfp128 v1, v1, v118` at 0x8223F6C8, which is INSIDE the arm, after
    //   EulerAnglesZXYFromMatrix44Affine. Computed before the gates, exactly as the console
    //   does, and thrown away when a gate trips -- which is precisely why publishing nothing
    //   on a gated frame is faithful rather than a stand-in.
    Vector3 lTargetCameraAnglesRandom;
    lTargetCameraAnglesRandom.x =
        mfBumpValue * 0.01f * (1.0f / kfShakeFocalDistance) * lfShakeAmplitude;
    lTargetCameraAnglesRandom.y = 0.0f;
    lTargetCameraAnglesRandom.z = 0.0f;
    lTargetCameraAnglesRandom.w = 0.0f;


    // ---- GATE 1 @0x8223F014 / PS3 0x68A88 -----------------------------------
    // `fcmpu cr6, f30, flt_82001CC0` then `beq -> epilogue`. An exact float compare against
    // zero, reproduced as one (not as a magnitude epsilon).
    if (lfShakeAmplitude == 0.0f)
        return;

    // ---- GATE 2 @0x8223F020 / PS3 0x68A90 -----------------------------------
    if (lu8ShakeType == 0)
        return;

    // ---- GATE 3 @0x8223F034 -------------------------------------------------
    // `addi r29, r28, 0x518` is DirectorResourceManager + 1304 == mShakeAnimsGroup, taken by
    // name here. The compare is UNSIGNED (`cmplw`) and the shots are ONE-BASED: the arm's
    // very next statement indexes the shot list at `lu8ShakeType - 1`, which is what makes
    // `>` (rather than `>=`) the correct out-of-range test.
    // ⚠️ NO NULL CHECK ON lpDirectorResourceManager -- the console has none either, and adds
    //   nothing here that it does not do at 0x8223F024. Left faithful deliberately; if this
    //   ever faults, the caller is passing a null manager and THAT is the bug.
    const u32 luNumShots = lpDirectorResourceManager->GetShakeAnimGroup().Num_ShotList();
    if (static_cast<u32>(lu8ShakeType) > luNumShots)
        return;

    // ARTIST 8223F038..8223F7EC: resolve the authored take, wrap its two
    // adjacent 30 Hz keys, interpolate their quaternion rotations, then add
    // the procedural pitch vibration and its focal-plane compensation.
    const Attrib::RefSpec ref(*lpDirectorResourceManager->GetShakeAnimGroup().ShotList(lu8ShakeType - 1));
    CGS_ASSERT(ref.GetClassKey() == static_cast<u64>(Attrib::Gen::iceanim::ClassKey()),
               "lShakeParams.GetClassKey() == Attrib::Gen::iceanim::ClassKey()");
    if (mu8ActiveShake != lu8ShakeType)
    {
        const Attrib::Gen::iceanim anim(ref, 0);
        mShakeTake.SetDataPointers(lpDirectorResourceManager->GetKeyAnimFromGuid(anim.GetAnimGuid()), false);
    }
    const f32 frame = lfShakeFrequency * mfShotRunningTime * 30.0f;
    const s32 wholeFrame = static_cast<s32>(frame);
    const s32 count = mShakeTake.GetNumKeys(11);
    auto key = [count](s32 value) -> u16 {
        if (count <= 0) return 0;
        while (value < 0) value += count;
        return static_cast<u16>(value % count);
    };
    const u16 current = key(wholeFrame), next = key(wholeFrame + 1);
    const f32 t = frame - static_cast<f32>(wholeFrame);
    using namespace rw::math::vpu;
    auto sampleRotation = [&](u16 k) {
        Vector3 v = {mShakeTake.GetValueFloat(22,k), mShakeTake.GetValueFloat(23,k), mShakeTake.GetValueFloat(24,k),0};
        v = v * (lfShakeAmplitude / kfShakeFocalDistance);
        const f32 remainder = 1.0f - Magnitude(v);
        return Quaternion{v.x,v.y,v.z,remainder == 0.0f ? 0.0f : std::sqrt(remainder)};
    };
    Quaternion a = sampleRotation(current), b = sampleRotation(next);
    f32 dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if (dot < 0.0f) { a = {-a.x,-a.y,-a.z,-a.w}; dot = -dot; }
    Quaternion rotation;
    if (dot > std::cos(0.087266468f))
    {
        rotation = {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t};
        const f32 inverseLength = 1.0f / std::sqrt(rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w);
        rotation = {rotation.x*inverseLength,rotation.y*inverseLength,rotation.z*inverseLength,rotation.w*inverseLength};
    }
    else
    {
        const f32 angle = std::acos(dot);
        const f32 from = std::sin((1.0f-t)*angle) / std::sin(angle);
        const f32 to = std::sin(t*angle) / std::sin(angle);
        rotation = {a.x*from+b.x*to,a.y*from+b.y*to,a.z*from+b.z*to,a.w*from+b.w*to};
    }
    mMatrix = Matrix44AffineFromQuaternion(rotation);
    const Vector3 angles = EulerAnglesZXYFromMatrix44Affine(mMatrix, 0, 0.01f) + lTargetCameraAnglesRandom;
    mMatrix.SetIdentity();
    RotateMatrix44AffineByEulerAnglesZXY(mMatrix, angles);
    // These three samples occur in the console even though their values are discarded.
    mShakeTake.GetValueFloat(27,current);
    mShakeTake.GetValueFloat(26,current);
    mShakeTake.GetValueFloat(25,current);
    mMatrix.Pos().y -= std::tan(angles.x) * kfShakeFocalDistance;
    mu8ActiveShake = lu8ShakeType;
    // FLAG PC diagnostic: demonstrate that a nonempty authored take changes the view.
    static const bool trace = std::getenv("BRN_CAMERA_RIG_DIAG") != 0;
    static u32 lines = 0;
    if (trace && lfShakeAmplitude > 0.1f && lines++ < 32 && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[camera-rig] boost-shake type=" << static_cast<u32>(lu8ShakeType)
            << " keys=" << count << " frame=" << frame << " angles=" << angles.x << "," << angles.y << "," << angles.z
            << " lift=" << mMatrix.Pos().y << "\n";
}

// ============================================================================
// CameraShakeICEController::GetMatrix
//
// Bodied INLINE in BrnCameraShake.h -- `return mMatrix;`. It has no standalone symbol on
// either export because both builds inline it at every site; BehaviourGameplayExternal::Update
// inlines it as four `lvx128` off mBoostShake at 0x82241C70..0x82241C8C and multiplies the
// result straight into the camera transform. It is a hard link dependency exactly like
// ::Update, and it is now closed.
// ============================================================================

}
}
}
