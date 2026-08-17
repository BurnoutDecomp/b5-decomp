#pragma once

// ============================================================================
// BrnWorld::ActiveRaceCar -- the "active" (simulated, in-range) half of a race car.
//
// A RaceCar (BrnRaceCar.h) is the always-resident global slot; when it comes into
// range it is paired with an ActiveRaceCar that owns the physics/AI state. The two
// reference each other: RaceCar::mpActiveRaceCar <-> ActiveRaceCar::mpRaceCar.
//
// ---- 2026-07-31 RENDER WAVE: this header is now the SINGLE definition ------
// BrnRaceCarEntityModule.h used to carry its own opaque `class ActiveRaceCar {
// u8 maPlaceholderBytes[0x1CD0]; }` stand-in so the module's two array accessors
// could compile -- a genuine ODR fork (two definitions of BrnWorld::ActiveRaceCar
// in one program). The module header now includes THIS one and the placeholders
// are gone.
//
// ---- SOURCES ---------------------------------------------------------------
// MEMBER NAMES/TYPES/ORDER: the DecFIGS DWARF for BrnActiveRaceCar.h, which carries
// the class 78/78 and RenderParams 32/32. Every DWARF member below is gated on X360
// attestation: the byte offsets in the comments are the ones the ARTIST asm bakes in,
// and every single one of them lands exactly where the DWARF order puts it. The
// cross-checks that pin it (all from BURNOUT_X360_ARTIST.XEX):
//   RaceCarEntityModule::GenerateDispatchLists @0x822E79F8
//     &maActiveRaceCars[i] + 8768  -> mRenderParams                (2016 == 0x7E0)
//     ...              + 8701/8698 -> mbRenderThisFrame / mbIsInCarSelectOnline
//     ...              + 12252     -> mRenderParams.mfDeformationSquared (+3484)
//     ...              + 13888     -> mRenderParams.mLOD               (+5120)
//     ...              + 13894     -> mRenderParams.mbIsEngineOff      (+5126)
//     ...              + 13899     -> mRenderParams.mbIsHidden         (+5131)
//     GetPhysicsState()+ 1098      -> mPhysicsState.mbCrashing
//   ActiveRaceCar::IsActive  @0x822A1FB8   muState (+1856), mpRaceCar (+1776)
//   ActiveRaceCar::Attach    @0x822BEEE0   mpRaceCar (+1776), muState = 1,
//                                          RaceCarState::Clear(this + 224)
//   RenderRaceCar            @0x822CF6A0   the whole RenderParams read surface
//   the replay call site @0x822E82E8 `mulli r11, r30, 0x14A0` -> sizeof(RenderParams)
//                                          == 5280 (5272 rounded to the 16-byte align)
//
// ---- THREE MIS-ATTRIBUTIONS CORRECTED (2026-07-31) -------------------------
// The previous revision homed six members on ActiveRaceCar that are not its members:
//   maWheelPhysicsTransformA[6] @+0x310  ->  mPhysicsState.maWheelTransforms[4]
//   mau8WheelOnGroundA[4]       @+0x526  ->  mPhysicsState.mabWheelExists[4]
//   mbIsCrashing                @+0x52A  ->  mPhysicsState.mbCrashing
//   maWheelPhysicsTransformB[6] @+0x1020 ->  mRenderParams.mWheelTransforms[6]
//   mau8WheelOnGroundB[4]       @+0x1560 ->  mRenderParams.mabWheelExists[6]
//   mbBraking                   @+0x1BE7 ->  mRenderParams.mbIsBraking
// (mPhysicsState is at +224, mRenderParams at +2016; subtract and every one lands.)
// The [6] on the two physics arrays was wrong too -- RaceCarState carries [4].
//
// ---- x64 ------------------------------------------------------------------
// Parity is by NAMED MEMBER, not byte offset. mpRaceCar is a real pointer, so the
// members after it sit 4 bytes later than the console; nothing reads this class by
// offset, so that is fine and the old offsetof static_asserts are retired (the
// console offsets survive as the comments above/below, which is what a later wave
// needs). RenderParams itself IS byte-identical on x64 -- see its own banner.
// ============================================================================

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex
#include "BrnCommonTypes.h"                 // Matrix44Affine, Vector2/3/4, Vector3Plus
#include "GameShared/GameClasses/Containers/CgsBitArray.h"       // CgsContainers::BitArray<96>
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"      // CgsContainers::FixedRingBuffer<T,N>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Graphics/CgsModel.h"             // CgsGraphics::Model::State
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"      // CgsSceneManager::VolumeInstanceId
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnDetachedPartRenderEvent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // ERaceCarType (IsPlayer)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // EActiveRaceCarEngineState
#include "GameShared/GameClasses/System/Timer/CgsFrameInterpolation.h" // ⚠️ FLAG PC QoL: PoseTrack (the render-pose interpolator, by value)

namespace BrnPhysics { namespace Vehicle { struct VehicleInputInterface; } }
namespace CgsWorld { struct WorldMap2D; }   // UpdatePhysicsState forwards it to RaceCar::UpdatePositioningData

namespace BrnWorld
{
class RaceCar;

// DWARF BrnActiveRaceCar.h:58/:60. The 96 is the width of RenderParams' body-part
// BitArray; the 12 is the glass-volume ceiling.
const u32 KU_MAX_BODY_PARTS_PER_RACE_CAR    = 96;
const u32 KU_MAX_GLASS_VOLUMES_PER_RACE_CAR = 12;

// The deformation system's per-car verlet-point budget: the width of
// RenderParams::maVerletOffsets, which the X360 DEBUG_OverrideScratchAmount
// @0x822A21B0 walks 128 times (0x40..0x830, 16-byte stride).
const u32 KU_MAX_RACE_CAR_VERLET_POINTS     = 128;

// DWARF BrnActiveRaceCar.h:101. The crash bookkeeping ActiveRaceCar embeds by value;
// Attach @0x822BEEE0 zeroes the whole 80 bytes with ten 64-bit stores at this+0x540.
struct alignas(16) RaceCarCrashData
{
    Matrix44Affine mInitialTransform;   // DWARF :103
    bool           mbFirstCrash;        // DWARF :104
};

// DWARF BrnActiveRaceCar.h:1019. The depth of ActiveRaceCar::mPrevTransforms.
const s32 KI_PREV_TRANSFORM_BUFFER_SIZE = 4;

class ActiveRaceCar
{
public:
    // X360: returns meActiveRaceCarIndex (this+0x748). DWARF BrnActiveRaceCar.h:736.
    EActiveRaceCarIndex GetActiveRaceCarIndex() const;

    // X360: returns mpRaceCar (this+0x6F0). DWARF BrnActiveRaceCar.h:742.
    RaceCar* GetGlobalRaceCar() const;

    // X360 0x822A1F10: mpRaceCar != NULL. DWARF BrnActiveRaceCar.h:763.
    bool IsAttached() const;

    // X360 0x822A1FB8: muState == E_STATE_ACTIVE (the asm compares against 3, and
    // asserts muState < E_STATE_COUNT (4) and "Active ActiveRaceCar without a RaceCar").
    bool IsActive() const;

    // X360 inlined -- muState == E_STATE_ATTACHED, i.e. attached but still waiting for its
    // resources. UpdateStreaming @0x822FEFE0 tests it (`*(car + 1856) == 1`) and
    // OnRaceCarResourcesLoaded @0x822FEBF8 asserts it by this name
    // ("lpActiveRaceCar->IsWaitingForLoad()").
    bool IsWaitingForLoad() const { return muState == E_STATE_ATTACHED; }

    // X360 inlined -- muState == E_STATE_WAITING, the state ActiveRaceCar::OnResourcesLoaded
    // @0x822EB168 stores and the ONLY state RaceCarEntityModule::ResetActiveRaceCar
    // @0x822F4880 will promote to E_STATE_ACTIVE (`else if (*(car + 1856) == 2)`).
    bool IsWaitingForPlacement() const { return muState == E_STATE_WAITING; }

    // ------------------------------------------------------------------------
    // X360 0x822EB168  ActiveRaceCar::OnResourcesLoaded  -- the ATTACHED -> WAITING step.
    // Called only by RaceCarEntityModule::OnRaceCarResourcesLoaded @0x822FEBF8.
    // Vector arg v1 is the initial velocity (Hex-Rays drops it -- vector register).
    // ------------------------------------------------------------------------
    void OnResourcesLoaded( const CgsResource::ResourceHandle& lrDeformationModelHandle,
                            const CgsResource::ResourceHandle& lrGraphicsModelHandle,
                            const Vector3&                     lrInitialVelocity,
                            u64                                luCarAssetAttribKey );

    // ------------------------------------------------------------------------
    // X360 0x822D3EC8  ActiveRaceCar::AddHandlingModel -- hands the freshly-active car to
    // the physics vehicle manager. Only caller: ResetActiveRaceCar's WAITING arm.
    // Signature recovered from the ASM (0x822D3EE4..0x822D4054), not the pseudocode:
    // r4/r5/r6/r7/r8 + v1, with the velocity in a vector register that Hex-Rays drops.
    // ------------------------------------------------------------------------
    void AddHandlingModel( BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
                           u64                   luCarAssetAttribKey,
                           const Matrix44Affine& lrInitialTransform,
                           const Vector3&        lrInitialVelocity,
                           bool                  lbResettingPhysicsState,
                           u8                    lu8CarStrengthStat );

    // X360: returns mbToBePlacedOnTrack (byte this+0x7C4). DWARF BrnActiveRaceCar.h:853.
    bool ToBePlacedOnTrack() const;

    // ------------------------------------------------------------------------
    // X360 0x822BFB58  ActiveRaceCar::RequestPlaceOnTrack -- ask PlaceOnTrackManager to
    // drop this car onto the track at (lPosition, lDirection) with lfSpeed of forward
    // velocity. Latches the request; a second request while one is pending is IGNORED
    // (`if (!mbToBePlacedOnTrack)` guards every store).
    // Vector args v1/v2 are the position and the direction; fp1 is the speed.
    // ------------------------------------------------------------------------
    void RequestPlaceOnTrack( const Vector3& lPosition, const Vector3& lDirection,
                              f32 lfSpeed );

    // X360 inlined at PlaceOnTrackManager::PrePhysicsUpdate (BrnPlaceOnTrackManager.cpp:238,
    // right before ResetActiveRaceCar): clears the pending request.
    void ClearPlaceOnTrack() { mbToBePlacedOnTrack = false; }

    // X360 inlined at RaceCarEntityModule::OnRaceCarResourcesLoaded @0x822FEBF8 -- its two
    // trailing stores (`*(car + 1909) = 0` / `*(car + 1910) = 0`), both named here.
    void SetComingInRange(bool lbOn)    { mbComingInRange = lbOn; }
    void SetJoiningGameMode(bool lbOn)  { mbIsJoiningGameMode = lbOn; }

    const Vector3& GetPlaceOnTrackPosition()  const { return mPlaceOnTrackPosition; }
    const Vector3& GetPlaceOnTrackDirection() const { return mPlaceOnTrackDirection; }
    f32            GetPlaceOnTrackSpeed()     const { return mfPlaceOnTrackSpeed; }

    // The render gate GenerateDispatchLists applies to every active slot:
    // `if (*(car + 1949) && !*(car + 1946))` == mbRenderThisFrame && !mbIsInCarSelectOnline.
    // The X360 inlines both loads at the call site; expressed here as the predicate they
    // form so the render leg reads them BY NAME.
    bool ShouldRenderThisFrame() const
    {
        return mbRenderThisFrame && !mbIsInCarSelectOnline;
    }
    void SetRenderThisFrame(bool lbRender) { mbRenderThisFrame = lbRender; }

    // X360 inlined -- AttachActiveRaceCar @0x822F4DB0 copies the module's own
    // mbIsInGameMode into the slot (`stb r11, 0x777(r31)`) between Prepare and Attach.
    void SetInGameMode(bool lbInGameMode) { mbIsInGameMode = lbInGameMode; }

    // ------------------------------------------------------------------------
    // The four stores RaceCarEntityModule::ResetActiveRaceCar @0x822F4880 INLINES into
    // this object on its WAITING->ACTIVE arm (asm 0x822F4E00..0x822F4E18):
    //     *(car + 1856) = 3    muState = E_STATE_ACTIVE
    //     *(car + 1921) = 1    mbAIToBeActivated
    //     *(car + 1933) = 0    mbChangeCollisionState
    //     *(car + 1935) = 0    mbChangeCullingGroup
    // Expressed as one named setter rather than four pokes from another class; this is
    // the ONLY writer of E_STATE_ACTIVE in the XEX and it is not a bring-up seam.
    // ------------------------------------------------------------------------
    void BecomeActiveForReset()
    {
        muState                 = E_STATE_ACTIVE;
        mbAIToBeActivated       = true;
        mbChangeCollisionState  = false;
        mbChangeCullingGroup    = false;
    }

    // ========================================================================
    // Per-frame state accessors bodied in BrnActiveRaceCar.cpp. These thin wrappers
    // forward to the paired global RaceCar (mpRaceCar) which owns the world
    // pose/identity; the active car owns the live physics/AI snapshot.
    // ========================================================================

    // X360 0x822CCEB8: assert IsAttached() + mpRaceCar != NULL, then forward to
    // RaceCar::GetTransform() on the paired global slot.
    Matrix44Affine GetTransform() const;

    // X360 0x822CD038: forward to RaceCar::GetDirection() on the paired global slot.
    Vector3 GetDirection() const;

    // X360 0x822CD0F8: forward to RaceCar::GetVelocity() on the paired global slot.
    Vector3 GetVelocity() const;

    // X360 0x822B8540: the paired global slot's type is E_RACE_CAR_TYPE_PLAYER.
    bool IsPlayer() const;

    // X360 0x822A2150: assert IsAttached(), then return mPhysicsState.mbCrashing.
    bool IsCrashing() const;

    // ⭐ X360 0x822BFDA0 -- LANDED 2026-08-11 (player-input wave; its consumer is
    // RaceCarEntityModule::ProcessPlayerVehicleInput @0x822FFE30, which zeroes the driver
    // controls while `IsCrashing() && IsWrecked()`). "Wrecked" is NOT just the mbIsWrecked
    // latch: for a PLAYER car it is also derived, every frame, from the physics snapshot.
    // The console body, branch for branch (see the .cpp for the offset->member map).
    bool IsWrecked() const;

    // X360 0x822A2060: assert IsAttached(), then liState == meRaceStartState.
    bool IsOnRaceStartState(s32 liState) const;

    // X360 0x822A20D8: meRaceStartState is one of the two race-start phases.
    bool IsInAnyRaceStartState() const;

    // ⭐⭐ X360 0x822A4F50 (163 insns) -- THE IGNITION. The ONLY function in the XEX that
    // ever moves meEngineState away from E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF, and
    // ProcessPlayerVehicleInput @0x822FFE30 publishes a ZERO-FILLED driver record until it
    // reads RUNNING. So: this function is what makes the gas pedal do anything.
    //
    // ---- SIGNATURE FROM THE ASM (PPC float-arg trap) --------------------------------------
    // Hex-Rays prints nine args; the ABI says five. Each float arg burns its GPR slot, so
    // r4/r5/r6 are phantom shadows of f1/f2/f3 and are never read in the body. The call site
    // (ActiveRaceCar::Update @0x822F7E30..0x822F7E48) sets exactly r3, f1, f2, f3, r7, r8:
    //     fmr f1, f31   f31 <- Update's f1   -> lfTimeStep
    //     fmr f2, f29   f29 <- Update's f4   -> lfAcceleration
    //     fmr f3, f28   f28 <- Update's f5   -> lfBraking
    //     mr  r7, r24   r24 <- Update's r10  -> lbIsInOnlineGameMode
    //     lbz r8, arg_57(r1)                 -> lbInCarSelectScreen
    // and the two bools trace one hop further, to RaceCarEntityModule::UpdateActiveCars
    // @0x822FF304/0x822FF318, which loads them from ITS OWN `this`:
    //     lbzx r10, r31, 0x18345 == module + 99141  == mbIsInOnlineGameMode
    //     lbzx r8,  r31, 0x186C9 == module + 100041 == mbInCarSelectScreen
    // Both member names were already pinned in BrnRaceCarEntityModule.h (:760 / :607).
    // ⚠️ NOTHING HERE IS INVENTED: every argument is a load from a named module member.
    void UpdateEngineState(f32 lfTimeStep,
                           f32 lfAcceleration,
                           f32 lfBraking,
                           bool lbIsInOnlineGameMode,
                           bool lbInCarSelectScreen);

    // X360 0x822F78B0 (400 insns) -- PARTIAL SLICE, see the .cpp banner for the full console
    // argument list (13 GPR/stack slots + 5 floats + 2 VMX vectors) and for every dropped call.
    // Only the parameters this slice consumes are declared; each dropped one is named in the
    // banner rather than accepted and ignored (an accepted-and-ignored argument is how a
    // silent-drop stub is born).
    void Update(f32 lfTimeStep,
                f32 lfAcceleration,
                f32 lfBraking,
                bool lbIsInOnlineGameMode,
                bool lbInCarSelectScreen);

    // X360 0x822B8610: for an AI car ramp a braking hysteresis counter
    // (miBrakeChangeCounter, +1 toward +KI_MAX_BRAKE_COUNTER when braking / -2 toward
    // -KI_MAX_BRAKE_COUNTER when not) and publish mRenderParams.mbIsBraking = counter > 0;
    // for non-AI cars publish lbBraking directly.
    void SetBraking(bool lbBraking);

    // X360 0x822B8738: per-wheel copy of the four road wheels' 64-byte physics transforms
    // out of the physics wheel-data snapshot into BOTH the physics state
    // (mPhysicsState.maWheelTransforms/.mabWheelExists) and the render snapshot
    // (mRenderParams.mWheelTransforms/.mabWheelExists). The console does this with
    // unrolled lvx128/stvx128; the faithful C++ is a matrix copy-assign per wheel.
    void UpdateWheelPhysicsState(const void* lpPhysicsWheelData);

    // ⭐⭐ X360 0x822D4418 -- THE PHYSICS->RENDER SEAM. Take one frame's published
    // BrnPhysics::Vehicle::RaceCarState and make it this car's pose. Its only caller is
    // RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics @0x822E87B8, which the
    // PC build has not landed yet, so nothing calls this today -- it is the consumer the
    // physics module's publish side plugs into (see the .cpp banner).
    //
    // ⚠️ SIGNATURE FROM THE ASM, NOT FROM HEX-RAYS. The third argument is NOT a timestep
    // (an earlier scoping pass read it as `dt`): 0x822D4750 does `mr r5, r20 ; lwz r3,
    // 0x6F0(r29) ; mr r4, r27 ; bl RaceCar::UpdatePositioningData`, and that callee's
    // second parameter is a CgsWorld::WorldMap2D* (BrnRaceCar.h:67). It is forwarded
    // untouched, which is exactly why Hex-Rays renders it as a bare `int a3`.
    void UpdatePhysicsState(const BrnPhysics::Vehicle::RaceCarState* lpState,
                            CgsWorld::WorldMap2D* lpWorldMap);

    // ========================================================================
    // BrnWorld::ActiveRaceCar::RenderParams -- the per-car VISUAL snapshot the
    // renderer reads each frame to draw one race car. The physics/IO side fills it;
    // RaceCarEntityModule::RenderRaceCar / SubmitCoronasForRaceCar consume it.
    //
    // LAYOUT: the DWARF member list (32/32, in order) reproduces every X360-asm-proven
    // byte offset exactly, so this is a fully NAMED layout with no filler at all. The
    // one x64 divergence -- maDetachedParts' 8-byte buffer pointer -- is absorbed inside
    // CgsModule::EventQueue's own 16-byte-aligned base subobject (console: ptr/max/count
    // + 4 pad; x64: ptr + max/count), so sizeof and every following member are IDENTICAL
    // on both. The static_asserts that PIN this live in BrnActiveRaceCarRenderParams.cpp.
    // ========================================================================
    class RenderParams
    {
    public:
        typedef CgsModule::EventQueue<DetachedPartRenderEvent, 20> DetachedPartRenderQueue;

        // --- body pose ------------------------------------------------------
        const Matrix44Affine& GetBodyTransform() const { return mBodyTransform; }
        void SetBodyTransform(const Matrix44Affine& lrTransform) { mBodyTransform = lrTransform; }
        // ⚠️ FLAG PC quality-of-life: writable access for the render-pose interpolator,
        // which rewrites this transform in place once per rendered frame. Additive; the
        // console's own producers keep using SetBodyTransform.
        Matrix44Affine& GetBodyTransformForWrite() { return mBodyTransform; }

        // --- deformation scratch (128 verlet points) ------------------------
        const Vector3Plus* GetVerletOffsets() const { return maVerletOffsets; }
        Vector3Plus*       GetVerletOffsets()       { return maVerletOffsets; }

        // --- wheel render transforms (6 wheels) -----------------------------
        // X360 0x822A3220: ((luWheel+33)<<6)+this == &mWheelTransforms[luWheel].
        Matrix44Affine&       GetWheelTransform(u32 luWheel);
        // X360 0x822A31B8: ((luWheel+39)<<6)+this == &mWheelScaleTransforms[luWheel].
        Matrix44Affine&       GetWheelScaleMatrix(u32 luWheel);
        // X360 0x822CD170: mWheelScaleTransforms[luWheel] = diag(lrScale.xyz, 1). The
        // console argument is a scale VECTOR in v1, not a matrix -- see the .cpp banner
        // for the asm that settles it.
        void                  SetWheelScale(u32 luWheel, const Vector3& lrScale);

        bool GetWheelExists(u32 luWheel) const { return mabWheelExists[luWheel]; }
        void SetWheelExists(u32 luWheel, bool lbExists) { mabWheelExists[luWheel] = lbExists; }

        // ADDITIVE (wheel-render wave): the console has no accessor symbol for the
        // angular-velocity array either -- RenderRaceCar @0x822D1110 emits a bare
        // `lfsx f0, r30, r17` with r17 = this + 0xD8C and r30 = 4*luWheel. This exists
        // so the wheel block reads it BY NAME instead of by offset.
        f32  GetWheelAngularVelocity(u32 luWheel) const { return mafWheelAngularVelocities[luWheel]; }
        void SetWheelAngularVelocity(u32 luWheel, f32 lfValue) { mafWheelAngularVelocities[luWheel] = lfValue; }

        // --- part visibility (96 body parts) --------------------------------
        // X360 0x822B8B60: the inlined BitArray<96>::IsBitSet (field = part/64).
        bool IsPartVisible(u8 lu8Part) const;
        void ChangePartVisibility(u8 lu8Part, bool lbVisible);
        void MakeAllPartsVisible();

        // --- LOD / damage state ---------------------------------------------
        CgsGraphics::Model::State GetLOD() const { return mLOD; }
        void SetLOD(CgsGraphics::Model::State leLOD) { mLOD = leLOD; }
        bool IsDamaged() const     { return mbDamaged; }
        void SetDamaged(bool lbOn) { mbDamaged = lbOn; }
        bool GetCrashing() const   { return mbCrashing; }
        void SetCrashing(bool lbOn){ mbCrashing = lbOn; }
        bool IsEngineOff() const   { return mbIsEngineOff; }
        bool IsBraking() const     { return mbIsBraking; }
        void SetBraking(bool lbOn) { mbIsBraking = lbOn; }
        // ADDITIVE (physics wave 1): the two flags ActiveRaceCar::UpdatePhysicsState
        // @0x822D4418 stores directly (this+0x1BE6 == +5126 and this+0x1BE8 == +5128; the
        // console has no accessor symbol -- it emits bare `stb`s -- so these exist only so
        // the body writes them BY NAME instead of by offset).
        void SetEngineOff(bool lbOn) { mbIsEngineOff = lbOn; }
        bool IsReversing() const     { return mbIsReversing; }
        void SetReversing(bool lbOn) { mbIsReversing = lbOn; }
        bool IsRaceCarHidden() const { return mbIsHidden; }
        void SetRaceCarHidden(bool lbOn) { mbIsHidden = lbOn; }
        u8   GetRenderDamageFlag() const { return mu8RenderDamageFlags; }
        void SetRenderDamageFlag(u8 lu8Flags) { mu8RenderDamageFlags = lu8Flags; }
        f32  GetDeformationSquared() const { return mfDeformationSquared; }
        void SetDeformationSquared(f32 lfValue) { mfDeformationSquared = lfValue; }

        const Vector4& GetPaintColour() const       { return mPaintColour; }
        const Vector4& GetPearlescentColour() const { return mPearlescentColour; }
        void SetPaintColour(const Vector4& lrColour)       { mPaintColour = lrColour; }
        void SetPearlescentColour(const Vector4& lrColour) { mPearlescentColour = lrColour; }

        const DetachedPartRenderQueue& GetDetachedPartQueue() const { return maDetachedParts; }
        DetachedPartRenderQueue&       GetDetachedPartQueue()       { return maDetachedParts; }

        // --- cracked-glass shader params (8 panes) --------------------------
        // X360 0x822A1E30 / 0x822A1D30.
        f32             GetCrackedGlassFractureAmountN(u32 n) const;
        void            SetCrackedGlassFractureAmountN(u32 n, f32 lfValue);
        // X360 0x822A1EA0 / 0x822A1DB0.
        f32             GetCrackedGlassEqualisationFactorN(u32 n) const;
        void            SetCrackedGlassEqualisationFactorN(u32 n, f32 lfValue);
        // X360 0x822B8410 / 0x822B83A0. Storage is a packed 2-float pair (8-byte
        // stride, asm-proven); the API speaks Vector2 by value.
        Vector2         GetCrackedGlassScale(u32 n) const;
        void            SetCrackedGlassScaleFactorsN(u32 n, const Vector2& lrScale);

        // --- blues-and-twos (police strobe) state machine -------------------
        // X360 0x822A1C90: accumulate dt into the two light timers, wrap each, and
        // (when bForce && the flash timer has elapsed) toggle the strobe state.
        bool            RequestBluesAndTwosStateSwitch(f32 lfDeltaTime, bool lbForce);

        // --- lifecycle ------------------------------------------------------
        // X360 0x822E6818: reset to the just-spawned visual state.
        void            Reset();

        // X360 0x822A21B0: DEBUG override -- broadcast lfScratchAmount into the W lane
        // of all 128 verlet offsets (whole-register round-trip per element, see the .cpp).
        void            DEBUG_OverrideScratchAmount(f32 lfScratchAmount);

    private:
        // Packed 8-byte storage for one cracked-glass scale pair. The console array
        // strides by 8 bytes (asm: `8*(n+651)`), so this is NOT the 16-byte
        // rw::math::vpu::Vector2; the accessors convert to/from Vector2 by value.
        // (DWARF types it SmoothStep::Vector2 -- the smooth-step helper's own 2-float
        // pair, whose own home is not reconstructed; the storage shape is what matters.)
        struct GlassScale { f32 x; f32 y; };

        // ---- the DWARF layout, in order (X360 byte offsets in the comments) --
        Matrix44Affine mBodyTransform;                   // +0x000 (0)
        Vector3Plus    maVerletOffsets[KU_MAX_RACE_CAR_VERLET_POINTS]; // +0x040 (64)
        Matrix44Affine mWheelTransforms[6];              // +0x840 (2112)
        Matrix44Affine mWheelScaleTransforms[6];         // +0x9C0 (2496)
        Vector3        maAxlePositions[4];               // +0xB40 (2880)
        Vector4        mPaintColour;                     // +0xB80 (2944)  Reset() = (1,1,1,1)
        Vector4        mPearlescentColour;               // +0xB90 (2960)  Reset() = (1,1,1,1)
        Vector3        maLightLocatorPos[24];            // +0xBA0 (2976)
        s32            maLightLocatorType[24];           // +0xD20 (3360)  BrnPhysics::Deformation::ETagPointType
        bool           mabWheelExists[6];                // +0xD80 (3456)  Reset() zeroes
        s32            miNumLightLocators;               // +0xD88 (3464)
        f32            mafWheelAngularVelocities[4];     // +0xD8C (3468)
        f32            mfDeformationSquared;             // +0xD9C (3484)
        CgsContainers::BitArray<KU_MAX_BODY_PARTS_PER_RACE_CAR> mBodyPartVisibility; // +0xDA0 (3488) 2 x u64
        DetachedPartRenderQueue maDetachedParts;         // +0xDB0 (3504)  20 x 80B, ends +0x1400
        CgsGraphics::Model::State mLOD;                  // +0x1400 (5120)
        bool           mbDamaged;                        // +0x1404 (5124)
        bool           mbCrashing;                       // +0x1405 (5125)
        bool           mbIsEngineOff;                    // +0x1406 (5126)
        bool           mbIsBraking;                      // +0x1407 (5127)
        bool           mbIsReversing;                    // +0x1408 (5128)
        bool           mbIsIndicatingLeft;               // +0x1409 (5129)
        bool           mbIsIndicatingRight;              // +0x140A (5130)
        bool           mbIsHidden;                       // +0x140B (5131)  gates ALL of RenderRaceCar
        f32            mfLightOpacityFlipFlop;           // +0x140C (5132)
        f32            mfLightSwitchTimeOut;             // +0x1410 (5136)
        bool           mbBluesAndTwosCanSwitchState;     // +0x1414 (5140)
        bool           mbBluesAndTwosActive;             // +0x1415 (5141)
        u8             mu8RenderDamageFlags;             // +0x1416 (5142)  AddToBin excludeMeshBits
        u8             mu8Pad1417_;                      // +0x1417 (5143)  align to f32
        f32            mafCrackedGlassFractureAmount[8]; // +0x1418 (5144)
        f32            mafCrackedGlassEqualisationFactor[8]; // +0x1438 (5176)
        GlassScale     mavCrackedGlassScaleFactors[8];   // +0x1458 (5208) .. +0x1498 (5272)
    };                                                   // sizeof == 5280 (16-aligned)

public:
    // muState. Only two ordinals are X360-attested by name-free evidence: Attach sets 1
    // and IsActive() tests == 3, with the assert "muState < E_STATE_COUNT" fixing the
    // count at 4. The two middle ordinals are the resource-wait phases between Attach and
    // fully-live; they are named by role, not by an attested enumerator.
    enum EState : u8
    {
        E_STATE_INACTIVE        = 0,
        E_STATE_ATTACHED        = 1,   // Attach() stores this
        E_STATE_WAITING         = 2,
        E_STATE_ACTIVE          = 3,   // IsActive() tests this
        E_STATE_COUNT           = 4,
    };

    // DWARF BrnActiveRaceCar.h:79. (The previous revision named these by ordinal because
    // it had not read the DWARF enum; ordinals 0 and 1 are exactly the two the X360
    // IsInAnyRaceStartState reports true for, which is what attests them.)
    enum ERaceStartState : s32
    {
        E_RACE_START_STATE_ON_START_LINE = 0,
        E_RACE_START_STATE_ROLLING_START = 1,
        E_RACE_START_STATE_RACING        = 2,
    };

    // DWARF BrnActiveRaceCar.h:1063 types meOnlineState with this enum
    // (BrnActiveRaceCar.h:304 in the DWARF dump). Attach @0x822BEEE0 stores 1.
    enum EOnlineState : s32
    {
        E_ONLINE_STATE_CONNECTING   = 0,
        E_ONLINE_STATE_NORMAL       = 1,
        E_ONLINE_STATE_LOST_CONTACT = 2,
        E_ONLINE_STATE_DISCONNECTED = 3,
        E_ONLINE_STATE_COUNT        = 4,
    };

    // DWARF BrnActiveRaceCar.h:63.
    static const s32 KI_MAX_BRAKE_COUNTER = 10;

    // ========================================================================
    // Lifecycle (pose wave 2026-07-31). All three are console functions.
    // ========================================================================

    // X360 0x822EA9C0: `meActiveRaceCarIndex = leIndex` FIRST (asm stores 0x748 before the
    // two range asserts fire), then the same scalar/flag reset Prepare does.
    void Construct(EActiveRaceCarIndex leActiveRaceCarIndex);

    // X360 0x822EAC28: back to the detached, un-posed state -- identity centre-of-mass
    // transform, no RaceCar, muState = E_STATE_INACTIVE, RenderParams::Reset(). Returns
    // true (`li r3, 1`); the module's Prepare stage 3 sweeps all eight slots with it.
    bool Prepare();

    // X360 0x822BEEE0: bind a global RaceCar to this slot. Seeds mPhysicsState.mTransform
    // from the RaceCar's world transform, makes every body part visible and arms
    // mbRenderThisFrame. lbCarSelectDontStreamAudio is the module's
    // mbCarSelectDontStreamAudio, forwarded by AttachActiveRaceCar (asm r5).
    void Attach(RaceCar* lpRaceCar, bool lbCarSelectDontStreamAudio);

    // X360 0x822B8828: the render body transform,
    // `Mult(mCentreOfMassTransform, mPhysicsState.mTransform)`. The console's only caller
    // is UpdatePhysicsState @0x822D4418, which stores the result into
    // mRenderParams.mBodyTransform.
    void CalcBodyTransform(Matrix44Affine& lrBodyTransform) const;

    // The render snapshot the dispatch leg consumes. Public because
    // RaceCarEntityModule::GenerateDispatchLists takes its ADDRESS directly
    // (`addi r5, r29, 0x7E0`) rather than going through GetRenderParams().
    RenderParams* GetRenderParams()             { return &mRenderParams; }
    const RenderParams* GetRenderParams() const { return &mRenderParams; }

    // ========================================================================
    // ⚠️ FLAG PC QUALITY-OF-LIFE -- NOT X360 FUNCTIONS. THE RENDER-POSE INTERPOLATOR.
    //
    // The simulation now ticks at a fixed 60 Hz while the renderer draws as fast as it
    // can (see CgsFrameInterpolation.h). RenderParams IS the pose the renderer draws, and
    // it is rewritten once per TICK by whichever producer owns the slot --
    // ActiveRaceCar::UpdatePhysicsState for the cars physics owns, the
    // PublishRenderPoseWithoutPhysics/WheelPose stand-ins for the rest. Drawn straight,
    // that steps at 60 Hz inside a 140 Hz frame stream and the car visibly shudders
    // against a world that is moving smoothly with the camera. (MEASURED: the body
    // position holds for two to three consecutive frames and then jumps.)
    //
    // So the tick pose is kept HERE, one tick deep, and RenderParams carries the blend of
    // the last two:
    //   RestoreTickRenderPose()        -- once per simulation tick, BEFORE the producers
    //                                     run. Puts the stored tick pose back into
    //                                     RenderParams.
    //   LatchTickRenderPose()          -- once per simulation tick, AFTER the producer has
    //                                     written RenderParams. Rolls current -> previous
    //                                     and takes the new current.
    //   ApplyRenderPoseInterpolation() -- once per rendered frame, BEFORE the dispatch
    //                                     pass. Writes blend(previous, current, alpha)
    //                                     into RenderParams.
    //
    // WHY THE RESTORE EXISTS. Apply REWRITES RenderParams, so between two ticks the pose in
    // it is a blend, not a tick pose. The latch's source is therefore only trustworthy if
    // the tick pose is put back first -- otherwise, on any tick where no producer rewrote
    // that slot, the latch would take its own blended output as the next tick and the
    // history would drift.
    //
    // HONEST SCOPE, as of 2026-08-17: on this build every ACTIVE slot is written every tick
    // by exactly one producer (the physics readback owns the slots in mUsedRaceCars, the two
    // stand-ins own the rest), so the restore currently changes no value. It is a structural
    // guarantee, not an observed fix, and it stops being belt-and-braces the moment any slot
    // can go a tick unwritten -- a paused simulation, a producer gated on the update set, or
    // the prop module, where only the simulated props will have a producer at all.
    //
    // ⛔ AND A WARNING FOR WHOEVER MEASURES THIS NEXT. Any probe of the display pose must be
    // read AFTER Apply. Sampled before it, a probe alternates between the sub-step's raw
    // latch (on stepping frames) and the previous frame's blend (on zero-step frames), which
    // plots as a sawtooth and reads exactly like broken interpolation. That artifact cost one
    // diagnosis round on 2026-08-17; the interpolation was working at the time.
    //
    // ApplyRenderPoseInterpolation is IDEMPOTENT -- it always blends from the two stored
    // ticks, never from what is currently in RenderParams -- so the multi-pass frame (main
    // view plus three shadow cascades plus the env-map faces) can call it per pass without
    // compounding.
    //
    // WHY NOT IN RenderParams: its layout is X360-pinned down to sizeof == 5280
    // (BrnActiveRaceCarRenderParams.cpp's static_assert block). ActiveRaceCar is not.
    //
    // WHAT IS BLENDED: the body transform and the six WORLD wheel transforms -- every
    // matrix the render leg turns into geometry. NOT the wheel SCALE matrices (a scale,
    // republished only when the wheel scale changes), not the LOD, not the visibility bits,
    // not the light locators: those are discrete per-tick decisions, and blending a
    // decision produces a state the game was never in.
    // ========================================================================
    void RestoreTickRenderPose();
    void LatchTickRenderPose();
    void ApplyRenderPoseInterpolation(f32 lfAlpha);

    // X360: RaceCarState* GetPhysicsState() -- &mPhysicsState.
    BrnPhysics::Vehicle::RaceCarState*       GetPhysicsState()       { return &mPhysicsState; }
    const BrnPhysics::Vehicle::RaceCarState* GetPhysicsState() const { return &mPhysicsState; }

    // ========================================================================
    // [FLAG PC bring-up] SeedPhysicsStateFromCreateEventBringUp -- NOT an X360 function.
    //
    // CLOSES THE OPEN HALF OF THE CREATE EVENT. AddHandlingModel hands the placement
    // transform to VehicleInputInterface::CreateRaceCar; on the console the vehicle
    // manager answers on the very next tick with a RaceCarState whose mTransform IS that
    // transform (the car has not moved yet), and UpdatePhysicsState @0x822D4418 memcpy's
    // the whole snapshot into mPhysicsState. On this build nothing answers -- the create
    // event has no consumer -- so mPhysicsState.mTransform keeps the value Attach seeded
    // from RaceCar::GetTransform(), i.e. the SPAWN ANCHOR, and the render pose
    // (PublishRenderPoseWithoutPhysicsBringUp -> CalcBodyTransform) publishes that instead
    // of where the car was actually placed.
    //
    // ⛔ MEASURED 2026-08-02: this is what kept the junkyard car 4.534 m in the air even
    // after the place-on-track drop query started returning the real ground. The log said
    // "race car 0 -> E_STATE_ACTIVE at (2986.933105, -3.525000, -2011.417969)" while the
    // very next [uoi] publish still read (2986.933105, 1.009405, -2011.417969).
    //
    // It invents nothing: the value stored is the argument the console's own
    // CreateRaceCar was just given, which is exactly what the console's own readback would
    // write back for a car at rest.
    // DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics + UpdatePhysicsState are wired to
    // a real VehicleOutputInterface::maRaceCarStates.
    // ========================================================================
    void SeedPhysicsStateFromCreateEventBringUp(const Matrix44Affine& lrTransform);

    // ========================================================================
    // [FLAG PC bring-up] SetCentreOfMassTransformBringUp -- NOT an X360 function (seat wave
    // 2026-08-05). Lands the OnResourcesLoaded leg this header's banner lists as missing:
    // console OnResourcesLoaded reads `BrnPhysics::Def(mDeformationModelResourcePtr) + 1552`
    // (= StreamedDeformationSpec::mCarModelSpaceToHandlingBodySpaceTransform, SHIPPED in the
    // bundle: identity rotation, translation (0, -0.740575, +0.170226) for PUSMC01) into
    // mCentreOfMassTransform. The resource-ptr alias half of CreateFromHandle is still not
    // reconstructed, but the promote site (RaceCarEntityModule::ResetActiveRaceCar) already
    // holds the RESIDENT spec through the streamer, so it forwards the very matrix the console
    // reads. CalcBodyTransform then produces body = COM * physics -- the console's own
    // composition -- instead of multiplying by the identity Prepare/Attach left.
    // ⭐ This and the analytic seat LAND TOGETHER: the seat outputs the HANDLING-frame (COM)
    // transform ~1.446 above ground; only with this matrix does the rendered body come back
    // down to model-origin height (~0.7055 above ground, the retail rest pose).
    // DELETE-WHEN OnResourcesLoaded's Def() alias leg lands.
    // ========================================================================
    void SetCentreOfMassTransformBringUp(const Matrix44Affine& lrCarModelSpaceToHandlingBodySpace);

    // ========================================================================
    // ADDITIVE named readers for the per-frame OUTPUT publish (2026-08-01).
    //
    // RaceCarEntityModule::UpdateOutputInterfaces @0x822F5CF8 reads every one of these
    // members DIRECTLY off the slot (`lbz r11, 0x788(r31)` and friends -- the console
    // inlines the loads at the call site, so there is no console accessor to name after).
    // Exposed here so the publish reads them BY NAME instead of re-deriving offsets, in
    // the same spirit as ShouldRenderThisFrame() above. The offsets in the comments are
    // the ARTIST loads that prove each one; they are provenance, never casts.
    // ========================================================================
    RaceCarEntityModuleIO::EActiveRaceCarEngineState GetEngineState() const { return meEngineState; }   // +0x768
    EOnlineState GetOnlineState() const              { return meOnlineState; }                 // +0x744
    bool IsInShowtime() const                        { return mbIsInShowtime; }                // +0x788
    bool IsNotSendingNetworkUpdates() const          { return mbNotSendingNetworkUpdates; }    // +0x798
    bool IsDisconnectedFromNetwork() const           { return mbIsDisconnectedFromNetwork; }   // +0x799
    const Vector3& GetCurrentInAirRotations() const  { return mCurrentInAirRotations; }        // +0x750
    u16  GetCurrentAISection() const                 { return muCurrAISection; }               // +0x73E
    bool HasCrashedIntoWater() const                 { return mbCrashedIntoWater; }            // +0x783
    bool CanDriveAwayFromCrash() const               { return mbCanDriveAwayFromCrash; }       // +0x779

    // ADDITIVE 2026-08-11 (player-input wave), same rule as the block above: three members
    // RaceCarEntityModule::ProcessPlayerVehicleInput @0x822FFE30 reads/writes DIRECTLY off the
    // slot, with no console accessor symbol to name them after --
    //   `lbz r11, 0x789(r27)` / `stb r26, 0x789(r27)`   -> mbTakenDown  (read AND cleared)
    //   `lbz r11, 0x788(r27)`                            -> mbIsInShowtime (IsInShowtime above)
    //   `lfs f0,  0x724(r27)`                            -> mfInvulnerablityTime
    // Exposed so the producer reads them BY NAME instead of re-deriving offsets.
    bool IsTakenDown() const                         { return mbTakenDown; }                   // +0x789
    void SetTakenDown(bool lbTakenDown)              { mbTakenDown = lbTakenDown; }            // +0x789
    f32  GetInvulnerabilityTime() const              { return mfInvulnerablityTime; }          // +0x724

private:
    // ========================================================================
    // Layout (completed by the pose wave 2026-07-31).
    //
    // ORDER + NAMES + TYPES: the DecFIGS DWARF's 78-member list, in declaration order
    // (its `// BrnActiveRaceCar.h:NNNN` comments run monotonically 1026..1128, so the
    // dump IS declaration order). Every member below whose console offset is recorded
    // was independently proven by an ARTIST store/load; the ones without a recorded
    // offset simply fill the gaps that the proven ones bracket. The proof set:
    //   Prepare @0x822EAC28  : 0x90 0x598/59C/5A0 0x6B0 0x6F0 0x728 0x72C 0x73C 0x73E
    //                          0x740 0x750 0x760 0x764 0x768 0x76C 0x770 0x775 0x776
    //                          0x777 0x77A 0x77C 0x781 0x782 0x783 0x784 0x788 0x789
    //                          0x78B 0x78C 0x7A0 0x7B0 0x7C0 0x7C4 0x7E0 0x1C80 0x1C84
    //   Attach  @0x822BEEE0  : 0xD0 0xE0 0x2D0 0x430 0x540 0x724 0x744 0x794 0x798..0x79D
    //                          0x1580/0x1588 0x1BE4
    //   Construct @0x822EA9C0: 0x748
    //   UpdatePhysicsState @0x822D4418 / CalcBodyTransform @0x822B8828 : 0x90, 0x2D0
    //   AttachActiveRaceCar @0x822F4DB0 : 0x777
    //   AddRaceCarToStartingGridOrFreeburnLobby @0x82300B38 : 0x7C8, 0x7CC
    //   OnResourcesLoaded @0x822EB168 : 0x1C90, 0x1CB0
    // 0x1CB0 + sizeof(ResourcePtr) == 0x1CD0 closes the class at the attested stride.
    //
    // ⚠️ ONE DWARF/X360 DIVERGENCE (version drift, NOT a transcription error). The PS3
    // DWARF declares mDeformationModelResourcePtr / mGraphicsModelResourcePtr as members
    // #3 and #4, right after mCentreOfMassTransform. On X360 they are the LAST two
    // members: OnResourcesLoaded creates them at this+0x1C90 / this+0x1CB0, and the DWARF
    // slot at +0x0D0 is occupied by the two VolumeInstanceIds instead (Attach's 8-byte
    // write at this+0xD0 followed by VolumeInstanceId::SetEntityIDEntityIndex). The X360
    // order is what this header follows, because it is what the asm proves.
    //
    // Members whose TYPE is not reconstructed in this tree stay as honest `u8 maPadXXXX`
    // spans sized to the console gap, with the DWARF name/type in the comment. On x64
    // mpRaceCar is 8 bytes wide so everything after it sits +4 -- parity here is by named
    // member (see the banner), the console offsets are documentation.
    // ========================================================================

    // DWARF mAddRemoveNetworkCarForCollisionQueue --
    // CgsModule::EventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent, 8>.
    u8 maPad0000[144];                                   // +0x000 (0)    .. +0x090 (144)

    // X360 +0x90 (144). Prepare and Attach both store the IDENTITY here;
    // OnResourcesLoaded overwrites it from the vehicle's physics def (`BrnPhysics::Def(
    // mDeformationModelResourcePtr) + 1552`), i.e. it is the authored body-to-chassis
    // offset. CalcBodyTransform multiplies it by mPhysicsState.mTransform.
    Matrix44Affine mCentreOfMassTransform;               // +0x090 (144)  .. +0x0D0 (208)

    // DWARF mHandlingBodyVolumeId / mBaseDeformationID -- CgsSceneManager::VolumeInstanceId
    // (8 bytes each; Attach writes the pair at +0xD0 as one 64-bit store, sets the entity
    // TYPE byte to 1 and then calls VolumeInstanceId::SetEntityIDEntityIndex).
    // NAMED 2026-08-01 (drivable wave): AddHandlingModel @0x822D3EC8 reads the FIRST of the
    // pair with a single 64-bit load (`ld r4, 0xD0(r31)`) and hands it to
    // VehicleInputInterface::CreateRaceCar as lVolumeInstanceId, so it needs a name -- and a
    // TYPE, which CgsVolumeInstanceId.h already homes (8 bytes, same as the console).
    CgsSceneManager::VolumeInstanceId mHandlingBodyVolumeId;  // +0x0D0 (208)
    CgsSceneManager::VolumeInstanceId mBaseDeformationID;     // +0x0D8 (216) .. +0x0E0 (224)

    // X360 +224 (0xE0): RaceCarState::Clear(this + 224) in Attach @0x822BEEE0.
    BrnPhysics::Vehicle::RaceCarState mPhysicsState;     // +0x0E0 (224)  1120 bytes

    // X360 +0x540 (1344). Attach zeroes all 80 bytes with ten 64-bit stores.
    RaceCarCrashData mCrashData;                         // +0x540 (1344) .. +0x590 (1424)

    // X360 +0x590 (1424). Prepare and Attach both Clear() it -- the three dwords the asm
    // zeroes at +0x598/+0x59C/+0x5A0 are RingBuffer::miReadPos / miWritePos / miLength,
    // which is exactly Clear() on a base whose mpData/miMaxLength sit at +0x590/+0x594.
    CgsContainers::FixedRingBuffer<Matrix44Affine, KI_PREV_TRANSFORM_BUFFER_SIZE>
        mPrevTransforms;                                 // +0x590 (1424) .. +0x6B0 (1712)

    // X360 +0x6B0 (1712). Prepare zeroes it.
    Vector3 mLastRecordedPosition;                       // +0x6B0 (1712) .. +0x6C0 (1728)

    // DWARF mDeformedBBox (CgsGeometric::AxisAlignedBox, 32) + mvfLowestPointWorldSpace
    // (rw::math::vpu::VecFloat, 16).
    u8 maPad06C0[48];                                    // +0x6C0 (1728) .. +0x6F0 (1776)

    // X360 +0x6F0 (1776). The paired global slot ("mpRaceCar == NULL" assert in Attach).
    RaceCar* mpRaceCar;                                  // +0x6F0 (1776)

    // Console alignment gap before the next 16-byte-aligned Vector3 (console +0x6F4..0x700).
    u8 maPad06F4[12];

    Vector3 mDeferredResetPosition;                      // +0x700 (1792)
    Vector3 mDeferredResetDirection;                     // +0x710 (1808)
    f32     mfDeferredResetTimer;                        // +0x720 (1824)
    f32     mfInvulnerablityTime;                        // +0x724 (1828)  Attach: -1.0f
    f32     mfTimeSinceCreation;                         // +0x728 (1832)
    f32     mfTimeDriveableInCrash;                      // +0x72C (1836)
    bool    mbDriveAwayCheckRequired;                    // +0x730 (1840)
    f32     mfTimeToStartLineBoostChange;                // +0x734 (1844)

    // X360 +0x738 (1848). AI braking hysteresis counter (DWARF miBrakeChangeCounter).
    s32 miBrakeChangeCounter;                            // +0x738 (1848)

    // Both seeded to 0x7FFF ("no section") by Construct, Prepare and Attach.
    u16 muPrevAISection;                                 // +0x73C (1852)
    u16 muCurrAISection;                                 // +0x73E (1854)

    // X360 +0x740 (1856). ActiveRaceCar::IsActive/Attach.
    u8 muState;                                          // +0x740 (1856)

    EOnlineState        meOnlineState;                   // +0x744 (1860)  Attach: NORMAL

    // X360 +0x748 (1864). DWARF meActiveRaceCarIndex. Construct stores it first thing.
    EActiveRaceCarIndex meActiveRaceCarIndex;            // +0x748 (1864)

    Vector3 mCurrentInAirRotations;                      // +0x750 (1872)
    f32     mfTimeSinceLastStable;                       // +0x760 (1888)
    bool    mbCurrentlyRotating;                         // +0x764 (1892)

    // The DWARF types this BrnWorld::RaceCarEntityModuleIO::EActiveRaceCarEngineState.
    RaceCarEntityModuleIO::EActiveRaceCarEngineState meEngineState;   // +0x768 (1896)
    f32     mfEngineStateTime;                           // +0x76C (1900)

    bool mbEnableEngineSwitchOff;                        // +0x770 (1904)  Prepare: true
    bool mbInsideAISectionSystem;                        // +0x771 (1905)
    bool mbIsTouchingAnotherRaceCar;                     // +0x772 (1906)
    bool mbIsTouchingPlayer;                             // +0x773 (1907)
    bool mbIsTouchingWorld;                              // +0x774 (1908)
    bool mbComingInRange;                                // +0x775 (1909)
    bool mbIsJoiningGameMode;                            // +0x776 (1910)
    bool mbIsInGameMode;                                 // +0x777 (1911)  AttachActiveRaceCar
    bool mbIsWaitingForDeferredReset;                    // +0x778 (1912)
    bool mbCanDriveAwayFromCrash;                        // +0x779 (1913)
    bool mbUncrashedThisFrame;                           // +0x77A (1914)

    // X360 +0x77C (1916). DWARF meRaceStartState.
    ERaceStartState meRaceStartState;                    // +0x77C (1916)

    bool mbIsDoingStartLineBoost;                        // +0x780 (1920)
    bool mbAIToBeActivated;                              // +0x781 (1921)
    bool mbIsWrecked;                                    // +0x782 (1922)
    bool mbCrashedIntoWater;                             // +0x783 (1923)
    f32  mfTimeInWater;                                  // +0x784 (1924)
    bool mbIsInShowtime;                                 // +0x788 (1928)
    bool mbTakenDown;                                    // +0x789 (1929)
    bool mbAddedToScene;                                 // +0x78A (1930)
    bool mbAddedForCollision;                            // +0x78B (1931)
    bool mbWonLastEvent;                                 // +0x78C (1932)
    bool mbChangeCollisionState;                         // +0x78D (1933)
    bool mbCollisionStateToChangeTo;                     // +0x78E (1934)
    bool mbChangeCullingGroup;                           // +0x78F (1935)

    // DWARF CgsSceneManager::InEventAddForCollision::CullingGroup (a 4-byte enum; the
    // type's own home is not reconstructed, so the storage is spelled s32 here).
    s32  mCullingGrouptoChangeTo;                        // +0x790 (1936)
    s32  miFlashFrequency;                               // +0x794 (1940)

    bool mbNotSendingNetworkUpdates;                     // +0x798 (1944)
    bool mbIsDisconnectedFromNetwork;                    // +0x799 (1945)

    // X360 +0x79A (1946) / +0x79D (1949). GenerateDispatchLists renders a slot only when
    // (mbRenderThisFrame && !mbIsInCarSelectOnline).
    bool mbIsInCarSelectOnline;                          // +0x79A (1946)
    bool mbCarSelectOnlineStateChanged;                  // +0x79B (1947)
    bool mbReceivedNetworkDriverControls;                // +0x79C (1948)
    bool mbRenderThisFrame;                              // +0x79D (1949)  Attach: true

    Vector3 mPlaceOnTrackPosition;                       // +0x7A0 (1952)  Prepare: (-1,-1,-1)
    Vector3 mPlaceOnTrackDirection;                      // +0x7B0 (1968)  Prepare: (-1,-1,-1)
    f32     mfPlaceOnTrackSpeed;                         // +0x7C0 (1984)

    // X360 +0x7C4 (1988). DWARF mbToBePlacedOnTrack.
    bool mbToBePlacedOnTrack;                            // +0x7C4 (1988)

    // DWARF BrnPhysics::Deformation::DeformationResetType. ⛔ THE OLD COMMENT HERE SAID
    // "own home not reconstructed" AND IT WAS STALE: BrnDeformationEvents.h:17 has carried
    // the enum for several waves, and this header already pulls it in transitively via
    // BrnVehicleEvents.h. Typed 2026-08-01 (drivable wave) because AddHandlingModel hands
    // this member straight to VehicleInputInterface::CreateRaceCar, which takes the enum.
    // AddRaceCarToStartingGridOrFreeburnLobby writes the pair at +0x7C8/+0x7CC.
    BrnPhysics::Deformation::DeformationResetType meBaseDeformationType;   // +0x7C8 (1992)
    f32 mfBaseDeformAmount;                              // +0x7CC (1996)
    s32 mCurrentCullingGroup;                            // +0x7D0 (2000)

    // X360 +0x7E0 (2016).
    RenderParams mRenderParams;                          // +0x7E0 (2016) 5280 bytes

    s32  miDefaultColourIndex;                           // +0x1C80 (7296) Prepare: -1
    s32  miDefaultColourPalette;                         // +0x1C84 (7300) Prepare: -1
    f32  mfIndicatorTime;                                // +0x1C88 (7304)
    bool mbRightIndicatorActive;                         // +0x1C8C (7308)
    bool mbLeftIndicatorActive;                          // +0x1C8D (7309)

    // DWARF mDeformationModelResourcePtr (ResourcePtr<BrnPhysics::Deformation::
    // StreamedDeformationSpec>) and mGraphicsModelResourcePtr (ResourcePtr<
    // BrnVehicle::GraphicsSpec>) -- 32 bytes each, created by OnResourcesLoaded at
    // this+0x1C90 / this+0x1CB0, closing the class at the attested 0x1CD0 stride.
    //
    // PARTIALLY NAMED 2026-08-01 (drivable wave). The two ResourcePtr WRAPPERS stay opaque
    // -- homing them would drag both spec types into every consumer of this header AND give
    // ActiveRaceCar two non-trivial ctors/dtors (BaseResourcePtr links itself into an
    // intrusive alias list), which the module's 8-slot array has never had. What IS named is
    // the ResourceHandle each wrapper stores at its own +0x14: AddHandlingModel reads exactly
    // those two, with two 64-bit loads (`ld r7, 0x1CA4(r31)` / `ld r8, 0x1CC4(r31)` ==
    // 0x1C90+0x14 and 0x1CB0+0x14), and hands them straight to
    // VehicleInputInterface::CreateRaceCar. OnResourcesLoaded is what fills them.
    // [FLAG] the alias-list binding half of BaseResourcePtr::CreateFromHandle (the resource
    // MEMORY pointer + Propogate) is not reproduced -- see OnResourcesLoaded's banner.
    u8 maPad1C90a[20];                                   // +0x1C90 (7312) .. +0x1CA4 (7332)
    CgsResource::ResourceHandle mDeformationModelHandle; // +0x1CA4 (7332)
    u8 maPad1CAC[24];                                    // +0x1CAC (7340) .. +0x1CC4 (7364)
    CgsResource::ResourceHandle mGraphicsModelHandle;    // +0x1CC4 (7364)
    u8 maPad1CCC[12];                                    // +0x1CCC (7372) .. +0x1CD0 (7376)

    // ---- ⚠️ FLAG PC quality-of-life: the render-pose interpolator's storage ------------
    // PAST the console's 0x1CD0 object stride, deliberately: nothing here exists on the
    // X360 and no offset at or before 0x1CD0 moves. (The only pinned offset in the owning
    // module is maRaceCars @0x250, which precedes maActiveRaceCars -- BrnRaceCarEntityModule.h
    // already records that the array's own 0x1A60 no longer holds.)
    //
    // One CgsSystem::FrameInterpolation::PoseTrack per interpolated transform. The wheels
    // get their own because GetWheelTransform is already a WORLD transform -- spin, steer
    // and suspension travel are in it, not derived from the body.
    static const u32 KU_INTERP_WHEELS = 6u;              // == RenderParams::mWheelTransforms[6]
    CgsSystem::FrameInterpolation::PoseTrack mBodyPoseTrack;
    CgsSystem::FrameInterpolation::PoseTrack maWheelPoseTracks[KU_INTERP_WHEELS];
};
}
