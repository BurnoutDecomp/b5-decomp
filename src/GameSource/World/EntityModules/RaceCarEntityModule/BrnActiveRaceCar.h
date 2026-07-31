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
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Graphics/CgsModel.h"             // CgsGraphics::Model::State
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // BrnPhysics::Vehicle::RaceCarState
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnDetachedPartRenderEvent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h" // ERaceCarType (IsPlayer)

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

    // X360: returns mbToBePlacedOnTrack (byte this+0x7C4). DWARF BrnActiveRaceCar.h:853.
    bool ToBePlacedOnTrack() const;

    // The render gate GenerateDispatchLists applies to every active slot:
    // `if (*(car + 1949) && !*(car + 1946))` == mbRenderThisFrame && !mbIsInCarSelectOnline.
    // The X360 inlines both loads at the call site; expressed here as the predicate they
    // form so the render leg reads them BY NAME.
    bool ShouldRenderThisFrame() const
    {
        return mbRenderThisFrame && !mbIsInCarSelectOnline;
    }
    void SetRenderThisFrame(bool lbRender) { mbRenderThisFrame = lbRender; }

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

    // X360 0x822A2060: assert IsAttached(), then liState == meRaceStartState.
    bool IsOnRaceStartState(s32 liState) const;

    // X360 0x822A20D8: meRaceStartState is one of the two race-start phases.
    bool IsInAnyRaceStartState() const;

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

        // --- deformation scratch (128 verlet points) ------------------------
        const Vector3Plus* GetVerletOffsets() const { return maVerletOffsets; }
        Vector3Plus*       GetVerletOffsets()       { return maVerletOffsets; }

        // --- wheel render transforms (6 wheels) -----------------------------
        // X360 0x822A3220: ((luWheel+33)<<6)+this == &mWheelTransforms[luWheel].
        Matrix44Affine&       GetWheelTransform(u32 luWheel);
        // X360 0x822A31B8: ((luWheel+39)<<6)+this == &mWheelScaleTransforms[luWheel].
        Matrix44Affine&       GetWheelScaleMatrix(u32 luWheel);
        // X360 0x822CD170: mWheelScaleTransforms[luWheel] = lrScale (the matrix arrived
        // split across the int arg registers on console; clean C++ takes it by ref).
        void                  SetWheelScale(u32 luWheel, const Matrix44Affine& lrScale);

        bool GetWheelExists(u32 luWheel) const { return mabWheelExists[luWheel]; }
        void SetWheelExists(u32 luWheel, bool lbExists) { mabWheelExists[luWheel] = lbExists; }

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

    // DWARF BrnActiveRaceCar.h:63.
    static const s32 KI_MAX_BRAKE_COUNTER = 10;

    // The render snapshot the dispatch leg consumes. Public because
    // RaceCarEntityModule::GenerateDispatchLists takes its ADDRESS directly
    // (`addi r5, r29, 0x7E0`) rather than going through GetRenderParams().
    RenderParams* GetRenderParams()             { return &mRenderParams; }
    const RenderParams* GetRenderParams() const { return &mRenderParams; }

    // X360: RaceCarState* GetPhysicsState() -- &mPhysicsState.
    BrnPhysics::Vehicle::RaceCarState*       GetPhysicsState()       { return &mPhysicsState; }
    const BrnPhysics::Vehicle::RaceCarState* GetPhysicsState() const { return &mPhysicsState; }

private:
    // ========================================================================
    // Layout. Only the members this header's bodied surface touches are homed by
    // name; the rest of the DWARF's 78 are honest `u8 maPadXXXX[N]` spans sized so
    // each named member lands at its X360-asm-proven CONSOLE offset (recorded in the
    // comment). On x64 mpRaceCar is 8 bytes wide, so everything after it sits +4;
    // that is fine -- parity here is by named member (see the banner).
    //
    // The opaque spans hold, per the DWARF in order:
    //   +0    mAddRemoveNetworkCarForCollisionQueue, mCentreOfMassTransform,
    //         mDeformationModelResourcePtr, mGraphicsModelResourcePtr,
    //         mHandlingBodyVolumeId, mBaseDeformationID
    //   +1344 mCrashData, mPrevTransforms, mLastRecordedPosition, mDeformedBBox,
    //         mvfLowestPointWorldSpace
    //   +1780 mDeferredResetPosition/Direction, the reset/invulnerability/creation
    //         timers, mbDriveAwayCheckRequired, mfTimeToStartLineBoostChange
    //   +1860 muPrevAISection, muCurrAISection, meOnlineState, meActiveRaceCarIndex,
    //         mCurrentInAirRotations, mfTimeSinceLastStable, mbCurrentlyRotating,
    //         meEngineState, mfEngineStateTime and the touching/game-mode bool run
    //   +1950 mPlaceOnTrackPosition/Direction/Speed, mbToBePlacedOnTrack,
    //         meBaseDeformationType, mfBaseDeformAmount, mCurrentCullingGroup
    //   after mRenderParams: miDefaultColourIndex, miDefaultColourPalette,
    //         mfIndicatorTime, mbRightIndicatorActive, mbLeftIndicatorActive
    // ========================================================================
    u8 maPad0000[224];                                   // +0x000 (0)    .. +0x0E0 (224)

    // X360 +224 (0xE0): RaceCarState::Clear(this + 224) in Attach @0x822BEEE0.
    BrnPhysics::Vehicle::RaceCarState mPhysicsState;     // +0x0E0 (224)  1120 bytes

    u8 maPad0540[1776 - 224 - 1120];                     // +0x540 (1344) .. +0x6F0 (1776)

    // X360 +0x6F0 (1776). The paired global slot ("mpRaceCar == NULL" assert in Attach).
    RaceCar* mpRaceCar;

    u8 maPad06F4[1848 - 1780];                           // +0x6F4 (1780) .. +0x738 (1848)

    // X360 +0x738 (1848). AI braking hysteresis counter (DWARF miBrakeChangeCounter).
    s32 miBrakeChangeCounter;                            // +0x738 (1848)

    u8 maPad073C[1856 - 1852];                           // +0x73C (1852) .. +0x740 (1856)

    // X360 +0x740 (1856). ActiveRaceCar::IsActive/Attach.
    u8 muState;                                          // +0x740 (1856)

    u8 maPad0741[1864 - 1857];                           // +0x741 (1857) .. +0x748 (1864)

    // X360 +0x748 (1864). DWARF meActiveRaceCarIndex.
    EActiveRaceCarIndex meActiveRaceCarIndex;            // +0x748 (1864)

    u8 maPad074C[1916 - 1868];                           // +0x74C (1868) .. +0x77C (1916)

    // X360 +0x77C (1916). DWARF meRaceStartState.
    ERaceStartState meRaceStartState;                    // +0x77C (1916)

    u8 maPad0780[1946 - 1920];                           // +0x780 (1920) .. +0x79A (1946)

    // X360 +0x79A (1946) / +0x79D (1949). GenerateDispatchLists renders a slot only when
    // (mbRenderThisFrame && !mbIsInCarSelectOnline).
    bool mbIsInCarSelectOnline;                          // +0x79A (1946)
    u8   maPad079B[2];                                   // +0x79B (1947) .. +0x79D (1949)
    bool mbRenderThisFrame;                              // +0x79D (1949)

    u8 maPad079E[1988 - 1950];                           // +0x79E (1950) .. +0x7C4 (1988)

    // X360 +0x7C4 (1988). DWARF mbToBePlacedOnTrack.
    bool mbToBePlacedOnTrack;                            // +0x7C4 (1988)

    u8 maPad07C5[2016 - 1989];                           // +0x7C5 (1989) .. +0x7E0 (2016)

    // X360 +0x7E0 (2016).
    RenderParams mRenderParams;                          // +0x7E0 (2016) 5280 bytes

    // X360 +0x1CA0 (7296) .. +0x1CD0 (7376): miDefaultColourIndex, miDefaultColourPalette,
    // mfIndicatorTime, mbRightIndicatorActive, mbLeftIndicatorActive and the tail padding
    // to the attested 0x1CD0 instance stride.
    u8 maPad1CA0[7376 - 7296];
};
}
