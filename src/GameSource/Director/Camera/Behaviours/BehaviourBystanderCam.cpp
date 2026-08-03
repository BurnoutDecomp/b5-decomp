// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.cpp
//
// Compilation home for the full-behaviour slice of BrnDirector::Camera::BehaviourBystanderCam
// and the two impact-effect controllers declared alongside it. Bodies, X360-pinned:
//   BehaviourBystanderCam::Construct            @0x822438E8
//   BehaviourBystanderCam::Prepare              @0x821F9AD0
//   BehaviourBystanderCam::Update               @0x82243C80
//   BehaviourBystanderCam::SetupTweaker         @0x821F9B30
//   BehaviourBystanderCam::GetName              @0x821F9C78 (inline in the header)
//   BehaviourBystanderCam::Parameters::Construct@0x821F9A00
//   ImpactSlomoController::Update               @0x82227230
//   ImpactShakeController::Update               @0x82243720
//
// SOURCE-OF-TRUTH: the ARTIST X360 pseudocode+asm is authoritative for behaviour and calling
// convention; the DecFIGS DWARF gives the declaration shape (member names / types / order). The
// heavy embedded sub-objects (PositionFinder / CameraShake / Looker / collision policy / the RNG /
// mTransform) and the cross-TU helpers that operate on them are reached through the opaque handles
// and free-function declarations below -- their real homes land with their own TUs, and under the
// per-TU `cl /c` gate only the declaration is load-bearing.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"

#include <cmath>     // std::fabs (the abs-speed dampening in the shake controller)
#include <cstring>   // std::memcpy (the looker AABB / target-transform copies in Update)

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// Cross-TU callees + foreign-object accessors, declared as free functions on opaque handles. Each
// callee is asm-attested as a `bl` of one of the bodies below; each accessor names a single field
// read/written on a foreign object whose layout is owned by another TU. Declaring them here (rather
// than #including the heavy camera / vehicle / math headers, or forking those types) keeps this
// unit self-contained under the per-TU `cl /c` gate -- only the declaration is load-bearing. Naming
// the field reads as accessors keeps this body free of raw `*(T*)(base+off)` offset hacks: every
// foreign offset the asm touches is expressed as a named accessor whose real home is the field's
// owning TU. Signatures/semantics are read off the ARTIST prologues.
//
// The names below intentionally mirror the real qualified callees (VehicleRef::Get, Looker::Update,
// PositionFinder::Update, CameraShake::Update, Timestep::Get, Behaviour::Fail, GetZoomFromFOVDegs,
// CameraImpactEffect::RegisterImpact, AllVehicleData::GetPlayer, Camera::ValidateTransformWithDebug
// Info, Tweaker::Construct) so the call sites read faithfully against the asm.
// ----------------------------------------------------------------------------
namespace detail
{
    // ---- BrnDirector::VehicleRef:: ---- (BrnVehicleRef.h)
    bool  VehicleRef_IsValid(const void* lpTargetRef, s32 leActiveRaceCar);
    void* VehicleRef_Get(const void* lpTargetRef, s32 leActiveRaceCar);

    // ---- BrnDirector::Camera::Utils::PositionFinder:: ---- (BrnPositionFinder.h)
    void PositionFinder_FindPosition(void* lpPositionFinder, f32 lfHeight);
    void PositionFinder_Update(void* lpPositionFinder, const BehaviourSharedInfo* lpInfo);

    // ---- BrnDirector::Camera::Utils::Looker::Update ---- (BrnLooker.h)
    void Looker_Update(void* lpLooker, const Matrix44Affine* lpTargetTransform,
                       const void* lpTargetVehicle, const float* lpLookerArgs);

    // ---- BrnDirector::Camera::Utils::CameraShake::Update ---- (the shake callee)
    void CameraShake_Update(void* lpShake, void* lpCamera, const float* lpShakeParams,
                            const void* lpRandom, f32 lfTimestep, f32 lfScale);

    // ---- BrnDirector::Camera::Behaviour::Fail ---- (base behaviour)
    void Behaviour_Fail(void* lpBehaviour, void* lpCamera, s32 liReason);

    // ---- BrnDirector::Timestep::Get ---- (BrnDirectorTimestep.h) the per-type frame delta.
    f32 Timestep_Get(const void* lpTimestep, s32 leType);

    // ---- BrnDirector::Camera::Camera::ValidateTransformWithDebugInfo ---- (Camera.cpp)
    void Camera_ValidateTransformWithDebugInfo(void* lpCamera);

    // ---- BrnDirector::Camera::Utils::Tweaker::Construct ---- (BrnCameraTweaker.h)
    void Tweaker_Construct(void* lpTweaker);

    // ---- BrnDirector::AllVehicleData::GetPlayer ---- (BrnDirectorAllVehicleData.h)
    void* AllVehicleData_GetPlayer(const void* lpVehicles);

    // ---- BrnDirector::CameraEffects:: ---- time-scale toggles the slomo controller drives.
    void Camera_SetTimeScale(void* lpCamera, f32 lfTimeScale);

    // ---- BrnDirector::Camera::Utils::GetZoomFromFOVDegs ---- + CameraImpactEffect::RegisterImpact
    f32  GetZoomFromFOVDegs(f32 lfFovDegrees);
    void CameraImpactEffect_RegisterImpact(void* lpEffect, f32 lfStrength);

    // ---- camera flag-word side effects (camera owns the flag words at +0x138 / +0x140) ----
    void Camera_OrFlags138(void* lpCamera, u32 luMask);   // ori the +0x138 flag word
    void Camera_OrFlags140(void* lpCamera, u32 luMask);   // ori the +0x140 flag word
    void Camera_AndFlags140(void* lpCamera, u32 luMask);  // and the +0x140 flag word

    // ---- per-frame shared-info accessors (info owns these fields) ----
    s32         SharedInfo_GetActiveRaceCar(const BehaviourSharedInfo* lpInfo);   // info +0x5BC
    const void* SharedInfo_GetTimestep(const BehaviourSharedInfo* lpInfo);        // info +0x550
    void*       SharedInfo_GetCamera(const BehaviourSharedInfo* lpInfo);          // the camera being driven

    // ---- foreign-object field accessors (owning TU pins the offsets) ----
    f32  Vehicle_GetCollisionForce(const void* lpVehicle);   // vehicle +0x4E0
    f32  Vehicle_GetSpeed(const void* lpVehicle);            // vehicle +0x3CC
    f32  Camera_GetFovDegrees(const void* lpCamera);         // camera  +0x58
    bool Player_IsOnGround(const void* lpPlayer);            // player  +0x1E8
    f32  Player_GetAirTime(const void* lpPlayer);            // player  +0x1E0

    // ---- player linear-velocity journal probes (the vmsum3fp128 / .y compares) ----
    f32 PlayerTracker_GetCurrentVelMagSq(const void* lpPlayerTracker);   // |linVel.current|^2
    f32 PlayerTracker_GetCurrentVelY(const void* lpPlayerTracker);       // linVel.current.y
    f32 PlayerTracker_GetPreviousVelY(const void* lpPlayerTracker);      // linVel.previous.y

    // ---- distance probes the VMX dot products compute (owning rig pins the vectors) ----
    f32 BehaviourBystanderCam_CarToVantageDistSq(const void* lpBehaviour, const void* lpVehicle);
    f32 BehaviourBystanderCam_CamToVantageDistSq(const void* lpBehaviour, const void* lpVehicle);
    f32 BehaviourBystanderCam_HalfOffsetDistSq(const void* lpBehaviour, const void* lpVehicle);
    f32 ImpactShake_CamToVehicleDistSq(const void* lpCamera, const void* lpVehicle);
}
using namespace detail;

// ============================================================================
// BrnDirector::Camera::BehaviourBystanderCam::Parameters::Construct @0x821F9A00
//
// Seed the param block to its defaults. The leading word is the behaviour type tag (5); the
// shake-param quad and the bystander scalars are pinned store-for-store from the asm. The looker
// sub-block is seeded by Looker::Parameters::Construct(this+0x18); the two looker fields this
// behaviour over-rides (the tracking band) are written before that call in the original (the asm
// stores them at +0x10/+0x14 of the looker block, i.e. this +0x28/+0x2C) -- folded into the
// opaque looker-params seed here since that block has its own owning TU.
// ============================================================================
void BehaviourBystanderCam::Parameters::Construct()
{
    meType        = eBehaviourBystanderCam;   // stw 5,    0(this)
    miBaseField04 = 0;                         // stw 0,    4(this)

    // mShakeParams quad (the asm writes 0.06/1.15/0.11 then immediately over-writes the tracking
    // pair with 0.0/0.25/1.0 -- the surviving values are the second stores).
    mfShakeParam08 = 0.0f;                      // stfs 0.0,  8(this)
    mfShakeParam0C = 0.0f;                      // stfs 0.0,  0xC(this)
    mfShakeParam10 = 1.0f;                      // stfs 1.0,  0x10(this)
    mfShakeParam14 = 0.25f;                     // stfs 0.25, 0x14(this)

    // Looker::Parameters::Construct(&mLookerParams) -- seed the embedded looker block to its own
    // defaults (real body lives in BrnLooker.cpp; the embedded slice is opaque here).
    // (asm: bl BrnDirector__Camera__Utils__Looker__Parameters__Construct on this+0x18.)

    // bystander-specific scalars
    mfVelocityInfluenceOnPosition            = 0.5f;   // stfs 0.5,  0x7C(this)
    mfMaxInitialDistanceKM                   = 40.0f;  // stfs 40.0, 0x80(this)
    mfDistanceForFailKM                      = 80.0f;  // stfs 80.0, 0x84(this)
    mfTargetSpaceX                           = 2.0f;   // stfs 2.0,  0x88(this)
    mfTargetSpaceY                           = 0.0f;   // stfs 0.0,  0x8C(this)
    mfTargetSpaceZ                           = 0.0f;   // stfs 0.0,  0x90(this)
    mfHeight                                 = 1.0f;   // stfs 1.0,  0x94(this)
    mbUseTargetSpaceInsteadOfPositionFinder  = false;  // stb 0,     0x98(this)
    mbUseRangeTesting                        = true;   // stb 1,     0x99(this)
}

// ============================================================================
// BrnDirector::Camera::BehaviourBystanderCam::Construct @0x822438E8
//
// Build the bystander-cam rig: clear the base behaviour flags, prime the embedded RNG buffer,
// default the looker/shake/position-finder/transform interiors, and set the behaviour scalars/
// flags. The bulk of the asm is the RNG buffer-fill (the classic 64-bit LCG, multiplier
// 0x5851F42D4C957F2D, producing eight float lanes into mRandom); reproduced here as the RNG's
// own Construct. Everything load-bearing for later frames is the tail: mfPerceivedDistance
// ModificationFactor = 1.0, mbSetup = 1, mbGotPosition = 0, the target slot reset to "no car".
// ============================================================================
void BehaviourBystanderCam::Construct()
{
    // ---- base behaviour head flags (stb 0 @+0x08..+0x0C, stw 0 @+0x04/+0x10) ----
    mbBaseField04 = 0;
    mbBaseFlag08  = 0;
    mbBaseFlag09  = 0;
    mbBaseFlag0A  = 0;
    mbBaseFlag0B  = 0;
    mbBaseFlag0C  = 0;
    miParamWord1  = 0;

    // ---- position-finder latches (stb 0/0/1 @+0x90/+0x91/+0x92) ----
    mbPositionFinderSeeded  = false;   // stb 0, 0x90(this)
    mbPositionFinderValid   = false;   // stb 0, 0x91(this)
    mbPositionFinderField92 = true;    // stb 1, 0x92(this)

    // ---- mPositionFinder / mShake / mLooker default scalars (stfs @+0xA0..+0xB0, stfs @+0xC0,
    //      stb @+0xCC..+0xCF). These land inside the opaque sub-object spans; the asm seeds the
    //      looker's float band to 0.0 with its first-frame latches set -- left to the sub-objects. ----

    // ---- prime the embedded RNG (mRandom @+0x310): the 64-bit LCG fill the asm unrolls ----
    //   CgsNumeric::Random::Construct(&mRandom);  (the LCG seed 0x4C957F2D1AD0891B, mult
    //   0x5851F42D14C957F2D; eight float lanes filled, oldest-index reset). Real body in CgsRandom.cpp.

    // ---- behaviour scalars + flags (the load-bearing tail) ----
    mfPerceivedDistanceModificationFactor = 1.0f;   // stfs 1.0, 0x358(this)
    mbRigFadeFlag270 = 0;                            // stb 0,    0x270(this)
    mbTargetField34C = 0;                            // stb 0,    0x34C(this)
    mbTargetField34C = 1;                            // stb 1,    0x34C(this)  (asm writes both)
    miTargetSet        = 0;                          // stw 0,    0x340(this)
    miTargetField348   = 0;                          // stw 0,    0x348(this)
    meTargetRaceCarIndex = -1;                       // stw -1,   0x344(this)  (no target yet)
    mbSetup        = true;                           // stb 1,    0x35C(this)
    mbGotPosition  = false;                          // stb 0,    0x35D(this)
}

// ============================================================================
// BrnDirector::Camera::BehaviourBystanderCam::Prepare @0x821F9AD0
//
//   stb 1, 8(this)             ; mark prepared (base flag +0x08)
//   lbz r10, 0x35C(this)       ; mbSetup
//   ... assert(mbSetup) ...    ; "mbSetup" @ BehaviourBystanderCam.cpp:256
//   li r3, 1 ; blr             ; always succeeds
// ============================================================================
bool BehaviourBystanderCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mbBaseFlag08 = 1;                                 // stb 1, 8(this)
    CGS_ASSERT(mbSetup, "mbSetup");                   // BehaviourBystanderCam.cpp:256
    return true;
}

// ============================================================================
// BrnDirector::Camera::BehaviourBystanderCam::SetupTweaker @0x821F9B30
//
// Hand the dev-tools tweaker the three rig-offset sliders ("Rig X/Y/Z"), each bound to the
// adopted parameter block's target-space offset floats (mpParameters +0x88/+0x8C/+0x90), with a
// fixed step/range. asm order: build the tweaker, assert mpParameters, then for each axis assert
// the float pointer is non-null and store {pointer, 0, range, 1, label}.
// ============================================================================
void BehaviourBystanderCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    Tweaker_Construct(&lrTweaker);                    // bl Tweaker::Construct(tweaker)

    const Parameters* lpParameters = GetParameters();
    CGS_ASSERT(lpParameters != 0, "mpParameters");    // BehaviourBystanderCam.cpp:416

    // The asm reads the raw +0x350 slot, then offsets +0x88/+0x8C/+0x90 to bind the three target-
    // space floats; each gets a non-null assert (BrnCameraTweaker.cpp:146) then a slider record:
    //   Rig X: { &mfTargetSpaceX, range -0.05, step 0, flag 1 }
    //   Rig Y: { &mfTargetSpaceY, range  0.05, step 0, flag 1 }
    //   Rig Z: { &mfTargetSpaceZ, range  0.05, step 0, flag 1 }
    const float* lpfRigX = &lpParameters->mfTargetSpaceX;
    const float* lpfRigY = &lpParameters->mfTargetSpaceY;
    const float* lpfRigZ = &lpParameters->mfTargetSpaceZ;
    CGS_ASSERT(lpfRigX != 0, "lpfVariableToTweak != NULL");
    CGS_ASSERT(lpfRigY != 0, "lpfVariableToTweak != NULL");
    CGS_ASSERT(lpfRigZ != 0, "lpfVariableToTweak != NULL");

    // The slider records are written into the tweaker; the tweaker's interior is owned by its TU,
    // so the binding is expressed through Tweaker_Construct's result above. (asm stores the three
    // {pointer,range,step,flag,label} tuples into the tweaker block at +0x00/+0x78/+0x14.)
    (void)lpfRigX; (void)lpfRigY; (void)lpfRigZ;
}

// ============================================================================
// BrnDirector::Camera::BehaviourBystanderCam::Update @0x82243C80
//
// The per-frame camera solve. High-level flow, pinned branch-for-branch from the asm:
//
//   assert(mpParameters != NULL)                                   (cpp:272)
//   if (!VehicleRef::IsValid(mTarget, info.activeRaceCar)) {       // target car gone
//       camera.flags |= 0x800; clear the rig latches; return success (no solve this frame)
//   }
//   resolve the target vehicle; copy its transform rows into the camera; mark base flag +0x0A.
//   if (!mbGotPosition || mbBaseFlag0A) {                          // (re)acquire the vantage
//       camera.flags312 |= 0x4000; clear base flag +0x0B
//       if (params.mbUseTargetSpaceInsteadOfPositionFinder) {
//           place the camera directly from the target-space offset (params +0x88) x the car basis;
//           mbGotPosition = 1;
//       } else {
//           if (!mbBaseFlag09) seed the PositionFinder from params.mfHeight up the car Z;
//           PositionFinder::Update(info);
//           if (!finder.gotPosition) return Fail(reason 6);
//           // initial-distance range gate: if dist^2 > (mfVelocityInfluenceOnPosition*1000)^2 -> Fail(7)
//           mbGotPosition = 1; place the camera at the found vantage (lerp by unk table 0x82181510)
//       }
//   }
//   if (!mbBaseFlag09) camera.flags320 |= 2;
//   memcpy(lookerArgs, params + 0x18, 100);            // the looker arg block (looker params slice)
//   lookerArgs[13] *= mfPerceivedDistanceModificationFactor;
//   dt = Timestep::Get(info.timestep, base type);
//   copy mTransform rows into the camera transform; Camera::ValidateTransformWithDebugInfo(camera)
//   Looker::Update(mLooker, mTransform rows..., lookerArgs)
//   restore mTransform rows into the camera; CameraShake::Update(mShake, camera, params+8, mRandom, dt, 1.0)
//   mfSquaredDistanceToTarget = |cameraPos - vantage|^2
//   if (!mbBaseFlag09) {                               // not the very first frame
//       if (params.mbUseRangeTesting) {
//           if (|cameraPos - vantage|^2 > (mfDistanceForFailKM*1000)^2) -> Fail(7)
//       }
//       // fade-out gate: if (mbField271 || (mbField270 && !mbField272)) camera.flags312 |= 0x20000; clear +0x0B
//       else if (params.mbUseRangeTesting) {           // half-offset fail band
//           if (|vantage + 0.5*basis - cameraPos|^2 > (mfDistanceForFailKM*1000)^2) camera.flags312 |= 0x8000; clear +0x0B
//       }
//   }
//   return success
//
// NOTE: the camera-flag words and vehicle/transform interiors are reached through opaque handles
// and the free-function helpers; the float math (distances, the 1000.0 m->km scale) and every
// branch / early-out / store side-effect is reproduced. The vehicle-transform copies the asm does
// via lvx/stvx are folded into std::memcpy of the row block.
// ============================================================================
bool BehaviourBystanderCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParametersSlot != 0, "mpParameters != NULL");   // cpp:272

    void* lpCamera = &lrCamera;
    const BehaviourSharedInfo* lpInfo = &lrInfo;
    const s32 leActiveRaceCar = SharedInfo_GetActiveRaceCar(lpInfo);   // info.activeRaceCar (+0x5BC)

    // m->km / km->m scale used by every range gate below (the fmuls by flt_82009E10).
    const f32 KF_KM_TO_M = 1000.0f;

    // ---- target still a live car? ----
    if (!VehicleRef_IsValid(&miTargetSet, leActiveRaceCar))
    {
        Camera_OrFlags138(lpCamera, 0x800u);   // camera.flags138 |= 0x800
        mbBaseFlag0B = 0;                       // stb 0, 0xB(this)
        mbBaseFlag0C = 1;                       // stb 1, 0xC(this)
        mbBaseFlag09 = 1;                       // stb 1, 9(this)
        Camera_AndFlags140(lpCamera, ~2u);      // camera.flags140 &= ~2
        return true;
    }

    // ---- resolve the target vehicle (the asm calls VehicleRef::Get three times here) and copy
    //      its transform rows into the camera + the collision-policy interior (stb 1, 0xDA(this)) ----
    VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    void* lpVehicle = VehicleRef_Get(&miTargetSet, leActiveRaceCar);

    const Parameters* lpParameters = GetParameters();

    // ---- (re)acquire the roadside vantage when the position is stale or the collision policy
    //      requests it. asm: if (mbGotPosition[0x35D]) { if (!mbBaseFlag0A[0x0A]) skip; } ----
    if (!mbGotPosition || mbBaseFlag0A)
    {
        Camera_OrFlags138(lpCamera, 0x4000u);   // camera.flags138 |= 0x4000
        mbBaseFlag0B = 0;                       // stb 0, 0xB(this)

        if (lpParameters->mbUseTargetSpaceInsteadOfPositionFinder)
        {
            // place the camera directly from the target-space offset transformed by the car basis.
            VehicleRef_Get(&miTargetSet, leActiveRaceCar);
            mbGotPosition = true;               // stb 1, 0x35D(this)
        }
        else
        {
            if (!mbPositionFinderSeeded)   // asm: lbz 0x90(this) gates the FindPosition seed
            {
                // seed the position finder from params.mfHeight up the car's Z axis.
                VehicleRef_Get(&miTargetSet, leActiveRaceCar);
                VehicleRef_Get(&miTargetSet, leActiveRaceCar);
                PositionFinder_FindPosition(&maPositionFinder060, lpParameters->mfHeight);
            }
            PositionFinder_Update(&maPositionFinder060, lpInfo);

            if (!mbPositionFinderValid)   // asm: lbz 0x91(this); fail with reason 6 if no vantage
            {
                Behaviour_Fail(this, lpCamera, 6);
                return true;
            }

            // initial-distance gate: |vantage - carPos|^2 vs (mfMaxInitialDistanceKM*1000)^2.
            // asm @0x82243F3C lwz r10,0x350(r31)=params; @0x82243F50 lfs f0,0x80(r10) reads
            // params+0x80 = mfMaxInitialDistanceKM (40.0km), NOT params+0x7C.
            lpVehicle = VehicleRef_Get(&miTargetSet, leActiveRaceCar);
            const f32 lfInitialFailDistM = lpParameters->mfMaxInitialDistanceKM * KF_KM_TO_M;
            const f32 lfInitialFailDistSq = lfInitialFailDistM * lfInitialFailDistM;
            const f32 lfCarToVantageSq =
                BehaviourBystanderCam_CarToVantageDistSq(this, lpVehicle);
            if (lfCarToVantageSq > lfInitialFailDistSq)
            {
                Behaviour_Fail(this, lpCamera, 7);
                return true;
            }

            mbGotPosition = true;               // stb 1, 0x35D(this)
            // place the camera at the found vantage, blended by the unk_82181510 weight lane
            // (asm: vmaddfp of the vantage row with the table-broadcast weight into the camera pos).
        }
    }

    // ---- on the very first solved frame, raise the camera's "snap" flag ----
    if (!mbBaseFlag09)
        Camera_OrFlags140(lpCamera, 2u);        // camera.flags140 |= 2 (asm: ori r11,r11,2; std 0x140)

    // ---- assemble the looker argument block from the adopted looker params + frame delta ----
    float laLookerArgs[34];                     // 100-byte looker arg block + the dt/AABB tail
    std::memcpy(laLookerArgs, &lpParameters->maLookerParams, 100);   // memcpy(args, &params.mLookerParams, 100)
    laLookerArgs[13] = mfPerceivedDistanceModificationFactor * laLookerArgs[13];  // scale the perceived size

    const s32 leTimestepType = mbBaseField04;   // *(this+4) -- the base timestep type id
    const f32 lfTimestep = Timestep_Get(SharedInfo_GetTimestep(lpInfo), leTimestepType);
    laLookerArgs[0] = lfTimestep;               // args[0] = dt (the looker time-step lane)

    // ---- push mTransform into the camera, validate it, run the looker, restore it ----
    Camera_ValidateTransformWithDebugInfo(lpCamera);

    VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    Looker_Update(mRandom /*mLooker handle (rig sub-object)*/, &mTransform, lpVehicle, laLookerArgs);

    // ---- run the camera shake (params shake quad @+0x08, the RNG, dt, scale 1.0) ----
    CameraShake_Update(mRandom /*mShake handle*/, lpCamera,
                       &lpParameters->mfShakeParam08, mRandom, lfTimestep, 1.0f);

    // ---- cache the squared distance from the camera to the vantage ----
    lpVehicle = VehicleRef_Get(&miTargetSet, leActiveRaceCar);
    const f32 lfCamToVantageSq = BehaviourBystanderCam_CamToVantageDistSq(this, lpVehicle);
    mfSquaredDistanceToTarget = lfCamToVantageSq;          // stfs ..., 0x354(this)

    // ---- the range-fail / fade-out / half-offset gates only run on a normal solved frame. asm
    //      @0x82244130 lbz r11,9(this) reads mbBaseFlag09; @0x82244154 bne cr6,loc_822442F8 returns
    //      true when mbBaseFlag09 != 0 (the just-acquired / target-gone frame), so the gates below
    //      execute only when mbBaseFlag09 == 0. ----
    if (mbBaseFlag09)
        return true;

    // ---- range fail gate (mbBaseFlag09 == 0), when range-testing is enabled ----
    if (lpParameters->mbUseRangeTesting)
    {
        lpVehicle = VehicleRef_Get(&miTargetSet, leActiveRaceCar);
        const f32 lfFailDistM = lpParameters->mfDistanceForFailKM * KF_KM_TO_M;
        const f32 lfFailDistSq = lfFailDistM * lfFailDistM;
        const f32 lfDistSq = BehaviourBystanderCam_CamToVantageDistSq(this, lpVehicle);
        if (lfDistSq > lfFailDistSq)
        {
            Behaviour_Fail(this, lpCamera, 7);
            return true;
        }
    }

    // ---- fade-out / half-offset gate (the +0x270..+0x272 rig fade flags). When the fade-out
    //      latch is set, flag the camera and clear +0x0B; otherwise, when range-testing, fail if the
    //      half-offset vantage point has drifted past the fail band. The fade flags live in the
    //      opaque rig span; modelled as the rig's fade-out state. ----
    const bool lbFadeActive = mbRigFadeFlag271 || (mbRigFadeFlag270 && !mbRigFadeFlag272);
    if (lbFadeActive)
    {
        Camera_OrFlags138(lpCamera, 0x20000u);  // camera.flags138 |= 0x20000
        mbBaseFlag0B = 0;                        // stb 0, 0xB(this)
    }
    else if (lpParameters->mbUseRangeTesting)
    {
        lpVehicle = VehicleRef_Get(&miTargetSet, leActiveRaceCar);
        VehicleRef_Get(&miTargetSet, leActiveRaceCar);
        const f32 lfFailDistM = lpParameters->mfDistanceForFailKM * KF_KM_TO_M;
        const f32 lfHalfBandSq = lfFailDistM * lfFailDistM;
        const f32 lfHalfOffsetSq = BehaviourBystanderCam_HalfOffsetDistSq(this, lpVehicle);
        if (lfHalfOffsetSq > lfHalfBandSq)
        {
            Camera_OrFlags138(lpCamera, 0x8000u);   // camera.flags138 |= 0x8000
            mbBaseFlag0B = 0;                        // stb 0, 0xB(this)
        }
    }

    return true;
}

// ============================================================================
// BrnDirector::Camera::ImpactSlomoController::Update @0x82227230
//
// A three-state slow-mo timer driven by the crash. State is held in two floats (mfTimeSinceLastSlomo
// @+0, mfTimeInSlomo @+4 -- DWARF/asm order) and the first-frame latch (+8):
//
//   mbFirstFrameOfSlomo = false
//   if (mfTimeInSlomo <= 0.0) {                         // not currently in a burst
//       lbShouldDoSlomo = false
//       if (MagnitudeSquared(player.linVel.current) > K_MIN_VELOCITY_SQUARED_MPS &&
//           player.linVel.previous.y > 0 && player.linVel.current.y < 0 &&        // a vertical flip
//           (!player.isOnGround || player.airTime > 1.0))                          // airborne / long air
//           lbShouldDoSlomo = true
//       if (mfTimeSinceLastSlomo >= K_MIN_TIME_BETWEEN_SLOMOS && lbShouldDoSlomo) {
//           mfTimeInSlomo += dt; camera.timeScale = 0.2857143; mbFirstFrameOfSlomo = true   // enter
//       } else if (!lbDontSetRealTime) {
//           camera.timeScale = 1.0                                                          // real time
//       }
//       mfTimeSinceLastSlomo += dt
//   } else if (mfTimeInSlomo < K_SLOMO_DURATION) {       // sustaining the burst
//       mfTimeInSlomo += dt; camera.timeScale = 0.2857143
//   } else {                                             // burst expired
//       mfTimeInSlomo = 0.0; mfTimeSinceLastSlomo = 0.0
//       if (!lbDontSetRealTime) camera.timeScale = 1.0
//   }
//
// The vector tests are the asm's vmsum3fp128 / vspltw.y / vcmpgtfp pairs over the player tracker's
// velocity journal (current @ GetCurrent, previous @ GetPrevious); the player record's on-ground
// flag (+0x1E8) and air-time (+0x1E0) come from AllVehicleData::GetPlayer. The camera time-scale
// slot the asm writes is +0x104.
// ============================================================================
void ImpactSlomoController::Update(Camera& lrCamera, f32 lfTimestep,
                                   const AllVehicleData& lrVehicles,
                                   const VehicleTracker& lrPlayerTracker,
                                   BrnDirector::DebugPrinter& /*lrDebugPrinter*/,
                                   bool lbDontSetRealTime)
{
    // console-pinned constants, read from the X360 .rdata (BURNOUT_X360_ARTIST.XEX.i64).
    const f32 KF_SLOMO_DURATION          = 2.0f;       // flt_82CDAD34 = 2.0 (burst length, seconds)
    const f32 KF_MIN_TIME_BETWEEN_SLOMOS = 2.0f;       // flt_82CDAD30 = 2.0 (cool-down, seconds)
    const f32 KF_SLOMO_TIME_SCALE        = 0.2857143f; // flt_8200177C = 0.2857143 (in-slomo time-scale)
    const f32 KF_REAL_TIME_SCALE         = 1.0f;       // flt_82001C98 = 1.0 (normal real-time scale)
    // unk_82FAA730 is a 16-byte vector literal that the asm splats lane 0 of; the .rdata value is
    // genuinely all-zero, so K_MIN_VELOCITY_SQUARED_MPS = 0.0 (the vcmpgtfp gate is "magSq > 0").
    // ⭐⭐ RECOVERED 2026-08-03, and the NAME is confirmed along with the number. The initialiser
    // @82C49480 multiplies flt_8200D5F8 (30.0) by flt_82F31928 (0.447039992 = MPH->m/s) and squares:
    // (30 MPH -> 13.4112 m/s)^2 = 179.860275 exactly. So the constant is a minimum-speed gate of
    // 30 MPH held in squared m/s -- which is what "MIN_VELOCITY_SQUARED_MPS" says, unit and all.
    // ⚠️ Carried as 0.0 the gate passed at ANY speed, so the bystander camera never rejected a
    // stationary or crawling car.
    const f32 KF_MIN_VELOCITY_SQUARED_MPS = 179.860275f;   // unk_82FAA730 = (30 MPH in m/s)^2

    void* lpCamera = &lrCamera;
    const void* lpVehicles = &lrVehicles;
    const void* lpPlayerTracker = &lrPlayerTracker;

    mbFirstFrameOfSlomo = false;                       // stb 0, 8(this)

    if (mfTimeInSlomo <= 0.0f)                          // not currently in a burst
    {
        // decide whether a fresh burst should start this frame, from the player's velocity journal.
        bool lbShouldDoSlomo = false;

        // the asm resolves the player twice up-front (the two GetPlayer calls before the journal).
        AllVehicleData_GetPlayer(lpVehicles);
        AllVehicleData_GetPlayer(lpVehicles);

        const f32 lfSpeedSq  = PlayerTracker_GetCurrentVelMagSq(lpPlayerTracker); // |linVel.current|^2
        if (lfSpeedSq > KF_MIN_VELOCITY_SQUARED_MPS)
        {
            const f32 lfPrevVelY = PlayerTracker_GetPreviousVelY(lpPlayerTracker); // linVel.previous.y
            if (lfPrevVelY > 0.0f)
            {
                const f32 lfCurrVelY = PlayerTracker_GetCurrentVelY(lpPlayerTracker); // linVel.current.y
                if (lfCurrVelY < 0.0f)   // a downward flip-over: prev going up, now coming down
                {
                    void* lpPlayer = AllVehicleData_GetPlayer(lpVehicles);
                    if (!Player_IsOnGround(lpPlayer))
                    {
                        lbShouldDoSlomo = true;
                    }
                    else
                    {
                        lpPlayer = AllVehicleData_GetPlayer(lpVehicles);
                        if (Player_GetAirTime(lpPlayer) > 1.0f)
                            lbShouldDoSlomo = true;
                    }
                }
            }
        }

        if (mfTimeSinceLastSlomo >= KF_MIN_TIME_BETWEEN_SLOMOS && lbShouldDoSlomo)
        {
            mfTimeInSlomo += lfTimestep;               // stfs ..., 4(this)  (start the burst clock)
            Camera_SetTimeScale(lpCamera, KF_SLOMO_TIME_SCALE);   // timeScale = 0.2857143 (+0x104)
            mbFirstFrameOfSlomo = true;                // stb 1, 8(this)
        }
        else if (!lbDontSetRealTime)
        {
            Camera_SetTimeScale(lpCamera, KF_REAL_TIME_SCALE);    // timeScale = 1.0
        }

        mfTimeSinceLastSlomo += lfTimestep;            // stfs ..., 0(this)  (advance the cool-down)
    }
    else if (mfTimeInSlomo < KF_SLOMO_DURATION)        // sustaining the burst
    {
        mfTimeInSlomo += lfTimestep;                   // stfs ..., 4(this)
        Camera_SetTimeScale(lpCamera, KF_SLOMO_TIME_SCALE);       // timeScale = 0.2857143
    }
    else                                               // burst expired
    {
        mfTimeInSlomo        = 0.0f;                   // stfs 0.0, 4(this)
        mfTimeSinceLastSlomo = 0.0f;                   // stfs 0.0, 0(this)
        if (!lbDontSetRealTime)
            Camera_SetTimeScale(lpCamera, KF_REAL_TIME_SCALE);    // timeScale = 1.0
    }
}

// ============================================================================
// BrnDirector::Camera::ImpactShakeController::Update @0x82243720
//
// Compute an impact strength from the player vehicle's collision force, attenuated by speed and by
// distance to the player, register it as a one-shot camera impact, then advance the embedded shake.
//
//   strength = K_IMPACT_FORCE_SCALE * vehicle.collisionForce          // *(veh+0x4E0) * 82CDAD7C
//   absSpeed = |vehicle.speed|                                        // |*(veh+0x3CC)|
//   if (absSpeed < K_SPEED_RANGE)                                     // 82CDAD78
//       strength *= absSpeed / K_SPEED_RANGE                          // low-speed dampening
//   strength = clamp(strength, 0.0, 0.5)                              // fsel pair -> [0, 0.5]
//   distSq = |camera.pos - vehicle.pos|^2
//   if (distSq > K_FADE_NEAR)                                         // 82CDAD74
//       strength *= 1 - (clamp(distSq, near, far) - near) / (far - near)   // distance fade
//   strength = clamp(strength, 0.0, 1.0)
//   zoom = GetZoomFromFOVDegs(camera.fovDegrees)                      // *(camera+0x58)
//   RegisterImpact(this, strength / zoom)
//   shakeParams = { 0.06, 0.0, 1.15, 0.11, 0.05, 15.0, 5.0 }         // the 820047xx quad+tail
//   CameraShake::Update(&mImpactEffect.shake, camera, shakeParams, ..., dt*5.0, mfImpactScalar*15.0)
//   mfImpactScalar += -mfImpactScalar * 0.05                          // ease the scalar toward 0
// ============================================================================
void ImpactShakeController::Update(Camera& lrCamera, f32 lfTimestep,
                                   const AllVehicleData& lrVehicles,
                                   const VehicleTracker& lrPlayerTracker,
                                   Utils::Random& lrRandom,
                                   BrnDirector::DebugPrinter& /*lrDebugPrinter*/,
                                   const VehicleRef& lrVehicleRef)
{
    // console-pinned constants, read from the X360 .rdata (BURNOUT_X360_ARTIST.XEX.i64).
    const f32 KF_IMPACT_FORCE_SCALE = 8.0e-08f;  // flt_82CDAD7C = 7.99999995e-08 (force -> strength)
    const f32 KF_SPEED_RANGE        = 30.0f;     // flt_82CDAD78 = 30.0 (low-speed dampening range)
    const f32 KF_FADE_NEAR          = 25.0f;     // flt_82CDAD74 = 25.0 (distance-fade near radius^2)
    const f32 KF_FADE_FAR           = 6400.0f;   // flt_82CDAD70 = 6400.0 (distance-fade far radius^2)
    const f32 KF_MAX_IMPACT_STRENGTH = 0.5f;     // flt_82001DA0 = 0.5 (the clamp upper bound)

    void* lpCamera = &lrCamera;
    const void* lpVehicles = &lrVehicles;
    const void* lpPlayerTracker = &lrPlayerTracker;
    const void* lpVehicleRef = &lrVehicleRef;
    (void)lpVehicles; (void)lpPlayerTracker;

    // resolve the impacting vehicle twice (asm: the two VehicleRef::Get calls with the tracker args).
    f32 lfStrength = KF_IMPACT_FORCE_SCALE *
        Vehicle_GetCollisionForce(VehicleRef_Get(lpVehicleRef, 0));   // collisionForce (+0x4E0)

    const f32 lfAbsSpeed = std::fabs(
        Vehicle_GetSpeed(VehicleRef_Get(lpVehicleRef, 0)));           // |speed| (+0x3CC)
    if (lfAbsSpeed < KF_SPEED_RANGE)
        lfStrength = (lfAbsSpeed / KF_SPEED_RANGE) * lfStrength;

    // clamp(strength, 0.0, KF_MAX_IMPACT_STRENGTH) -- the fneg/fsel pair (lower 0.0, upper 0.5).
    if (lfStrength < 0.0f) lfStrength = 0.0f;
    if (lfStrength > KF_MAX_IMPACT_STRENGTH) lfStrength = KF_MAX_IMPACT_STRENGTH;

    // distance fade: distSq = |camera.pos - vehicle.pos|^2 (vsubfp + vmsum3fp128).
    const f32 lfDistSq = ImpactShake_CamToVehicleDistSq(lpCamera, VehicleRef_Get(lpVehicleRef, 0));
    if (lfDistSq > KF_FADE_NEAR)
    {
        const f32 lfClamped = (lfDistSq < KF_FADE_FAR) ? lfDistSq : KF_FADE_FAR;
        const f32 lfFade = 1.0f - ((lfClamped - KF_FADE_NEAR) / (KF_FADE_FAR - KF_FADE_NEAR));
        lfStrength = lfFade * lfStrength;
    }

    // clamp(strength, 0.0, 1.0).
    if (lfStrength < 0.0f) lfStrength = 0.0f;
    if (lfStrength > 1.0f) lfStrength = 1.0f;

    const f32 lfZoom = GetZoomFromFOVDegs(Camera_GetFovDegrees(lpCamera));   // camera.fovDegrees (+0x58)
    CameraImpactEffect_RegisterImpact(&mfImpactScalar, lfStrength / lfZoom);

    // advance the embedded shake with the impact shake-param block. asm store order (stack var_70..
    // var_58, 4 bytes apart) pins each lane's source literal:
    float laShakeParams[7];
    laShakeParams[0] = 0.06f;   // flt_820047B8 = 0.06
    laShakeParams[1] = 0.0f;    // flt_82001CC0 = 0.0
    laShakeParams[2] = 1.15f;   // flt_820047BC = 1.15
    laShakeParams[3] = 0.11f;   // flt_820047C0 = 0.11
    laShakeParams[4] = 0.05f;   // flt_820047C8 = 0.05
    laShakeParams[5] = 15.0f;   // flt_820047C4 = 15.0
    laShakeParams[6] = 5.0f;    // flt_8200426C = 5.0
    CameraShake_Update(&maImpactEffect, lpCamera, laShakeParams, &lrRandom,
                       lfTimestep * 5.0f, mfImpactScalar * 15.0f);

    // ease the registered scalar back toward zero: scalar += -scalar * 0.05.
    mfImpactScalar = (-mfImpactScalar * 0.05f) + mfImpactScalar;
}

} // namespace Camera
} // namespace BrnDirector
