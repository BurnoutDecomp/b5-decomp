#ifndef BRN_TRAFFIC_PARAM_H
#define BRN_TRAFFIC_PARAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "BrnCommonTypes.h"

// ---------------------------------------------------------------------------
// BrnTraffic::Param  (and the ParamSoaData / ParamPlan it embeds)
//
// DWARF home: GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h.
// A traffic-system per-vehicle parameter block: an accessor-heavy record holding the
// vehicle's position-along (mfParamAlong), behaviour/speed scalars, a small ring of
// "where it has been" history (mauHistorySegments / mauHistoryHulls) and a packed flag
// byte (mxFlags) plus a separate effect/history-state byte (mxEffectAndHistoryState).
//
// Layout reconstructed from the X360 retail XEX (BURNOUT_X360_ARTIST.XEX) member STORE
// OFFSETS, corroborated by the DWARF type listing (BrnTrafficParam.h DWARF). The retail
// build is LATER than the Feb-2007 leak: the leak's flag bit values and method set
// differ (retail adds purgatory/divorce, moves the alive/dying/zombie membership into
// the separate ParamSoaData bit-sets and renumbers mxFlags), so the X360 asm is taken as
// the source of truth wherever it diverges from the leak.
//
// Byte offsets pinned by name from the asm (this == u8*; all offsets verified against the
// store/load instructions):
//   0x00 muHullIndex (u16)         0x02 muSectionIndex (u8)   0x03 muCurrentSegment (u8)
//   0x04 mfParamAlong (f32)        0x08 maPlans[2] (ParamPlan, stride 6)
//   0x14 mfSpeed (f32)            0x18 muNextStopLineIndex   0x19 muVehicleType
//   0x1A mxEffectAndHistoryState  0x1B miBehaviour (s8)
//   0x1C mfStopDist 0x20 mfTargetSpeed 0x24 mfTimeQueueing 0x28 mfNextStopLineParam
//   0x2C mfRandomVal 0x30 mfAcceleration 0x34 mfLastSpeed 0x38 mfMaxAcceleration 0x3C mfFrontDist
//   0x40 mxFlags (u8)             0x41 muCurrentSectionDirection (u8)
//   0x42 mauNeighbourEndRung[2]   0x44 mauNeighbourData[2] (u16)
//   0x48 mSympCrashTarget (EntityId)
//   0x4C mauHistorySegments[6] (u16)  0x58 mauHistoryHulls[6] (u16)
//   0x64 muNextHistoryToWrite (u8) 0x65 muStartSectionIndex 0x66 muStartHullIndex (u16)
//   0x68 mfBackDist (f32)         0x6C muExtraBehaviourFlags (u8)
// sizeof(Param) == 0x70 (DWARF tail field at 0x6C plus the trailing word pad).
// ---------------------------------------------------------------------------

namespace BrnTraffic
{
// KU_PARAM_NUM_SEGMENTS_TO_REMEMBER / KU_PARAM_NUM_PLANS (BrnTrafficConstants.h, leak).
static const u32 KU_PARAM_NUM_SEGMENTS_TO_REMEMBER = 6;
static const u32 KU_PARAM_NUM_PLANS = 2;

// CgsFastBitArray out-of-range assert uses 600 as "max bits"; the SoA sets are
// FastBitArray<601> so a valid traffic param index is in [0, 600).
static const u32 KU_PARAM_MAX_PARAMS = 601;

// ---------------------------------------------------------------------------
// ParamSoaData: three FastBitArray<601> membership sets, one quadword-block each.
// Offsets proven by the asm: mAliveParams @0x00, mDyingParams @0x50, mZombieParams @0xA0.
// ---------------------------------------------------------------------------
struct ParamSoaData
{
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mAliveParams;  // 0x000
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mDyingParams;  // 0x050
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mZombieParams; // 0x0A0

    void Construct();
};

// ---------------------------------------------------------------------------
// ParamPlan: a queued lane / section change (6 bytes; verified by the maPlans stride).
// ---------------------------------------------------------------------------
struct ParamPlan
{
    enum Types
    {
        E_TYPE_NONE,
        E_TYPE_CHANGE_LANE,
        E_TYPE_CHANGE_SECTION,

        E_TYPES_COUNT
    };

    u8 muType;      // 0x00
    u8 muDirection; // 0x01
    union           // 0x02 (4 bytes)
    {
        struct
        {
            u16 muNewHull;
            u8  muNewSection;
        } mChangeSectionData;

        struct
        {
            u16 muNeighbourData;
            u8  muNewSection;
            u8  muRungToCarryOut;
        } mChangeLaneData;
    };
};

class Param
{
public:
    // Retail mxFlags bit values (X360 asm, NOT the Feb-2007 leak numbering):
    //   ClearDying clears 0x02; SetDyingState clears 0x01, masks with 0x8F, sets 0x02;
    //   SetShouldBeRemoved sets 0x10; SetZombie sets 0x20; SetInPurgatory toggles 0x80.
    enum Flags
    {
        E_FLAG_ALIVE            = 0x01,
        E_FLAG_DYING            = 0x02,
        E_FLAG_SHOULD_BE_REMOVED = 0x10,
        E_FLAG_ZOMBIE           = 0x20,
        E_FLAG_IN_PURGATORY     = 0x80,
    };

    // mxEffectAndHistoryState bits used by this TU (asm): SetChangedSection sets 0x08,
    // SetDyingState sets 0x04 (HasDied), and HasDied tests 0x04.
    enum EffectAndHistoryFlags
    {
        E_HISTORY_DIED            = 0x04,
        E_HISTORY_CHANGED_SECTION = 0x08,
    };

    u16 muHullIndex;            // 0x00
    u8  muSectionIndex;         // 0x02
    u8  muCurrentSegment;       // 0x03
    f32 mfParamAlong;           // 0x04
    ParamPlan maPlans[KU_PARAM_NUM_PLANS]; // 0x08 (2 x 6 = 12, ends 0x14)
    f32 mfSpeed;                // 0x14
    u8  muNextStopLineIndex;    // 0x18
    u8  muVehicleType;          // 0x19
    u8  mxEffectAndHistoryState; // 0x1A
    s8  miBehaviour;            // 0x1B
    f32 mfStopDist;             // 0x1C
    f32 mfTargetSpeed;          // 0x20
    f32 mfTimeQueueing;         // 0x24
    f32 mfNextStopLineParam;    // 0x28
    f32 mfRandomVal;            // 0x2C
    f32 mfAcceleration;         // 0x30
    f32 mfLastSpeed;            // 0x34
    f32 mfMaxAcceleration;      // 0x38
    f32 mfFrontDist;            // 0x3C
    u8  mxFlags;                // 0x40
    u8  muCurrentSectionDirection; // 0x41
    u8  mauNeighbourEndRung[2]; // 0x42
    u16 mauNeighbourData[2];    // 0x44
    EntityId mSympCrashTarget;  // 0x48
    u16 mauHistorySegments[KU_PARAM_NUM_SEGMENTS_TO_REMEMBER]; // 0x4C
    u16 mauHistoryHulls[KU_PARAM_NUM_SEGMENTS_TO_REMEMBER];    // 0x58
    u8  muNextHistoryToWrite;   // 0x64
    u8  muStartSectionIndex;    // 0x65
    u16 muStartHullIndex;       // 0x66
    f32 mfBackDist;             // 0x68
    u8  muExtraBehaviourFlags;  // 0x6C

    // --- the 11 functions owned by this TU ---
    void ClearDying(u32 luParam, ParamSoaData& lSoaData);
    void GetHistoryEntry(u32 luHistoryIndex, u32* lpOutSegmentIndex, u32* lpOutHullIndex) const;
    bool IsQueueing() const;
    void PushHistory(u32 luSegmentIndex, u32 luHullIndex);
    void SetChangedSection();
    void SetDyingState(u32 luParam, ParamSoaData& lSoaData);
    void SetInPurgatory(bool lbInPurgatory);
    void SetParamAlong(f32 lfParamAlong);
    void SetShouldBeRemoved();
    void SetZombie(u32 luParam, ParamSoaData& lSoaData);
    bool ShouldBeIndicatingRight() const;

    // Helpers the asm inlines into the above (state predicates).
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsDying() const { return (mxFlags & E_FLAG_DYING) != 0; }
    bool HasDied() const { return (mxEffectAndHistoryState & E_HISTORY_DIED) != 0; }
    bool IsInPurgatory() const { return (mxFlags & E_FLAG_IN_PURGATORY) != 0; }
};

// ---------------------------------------------------------------------------
// BrnTraffic::ParamTransform -- the per-vehicle orientation/position transform block.
// DWARF-AUTHORITATIVE (references/DecFIGS/dwarfdump/.../BrnTrafficParam.h:314): a struct
// with four private 16-byte, 16-aligned SIMD vectors (sizeof == 0x40) and the public method
// set below. Offsets pinned by the store/load displacements in the X360 asm (GetRight +0x20,
// GetLerpedPos/GetSpeed +0x30, Initialise stores mPos@0x00 / mDirAndAccel@0x10 / mRight@0x20 /
// mLerpedPosAndSpeed@0x30) AND corroborated field-for-field by the DWARF member listing:
//   0x00 mPos               (Vector3)      -- deterministic world position
//   0x10 mDirAndAccel       (Vector3Plus)  -- forward dir in xyz, accel scalar in w/plus
//   0x20 mRight             (Vector3)      -- right axis
//   0x30 mLerpedPosAndSpeed (Vector3Plus)  -- render-lerped pos in xyz, speed in w/plus
//
// This batch reconstructs CalcUp/GetLerpedPos/GetRight/GetSpeed/Initialise. The remaining
// DWARF-listed methods are DECLARED here (DWARF shape) for later waves. GetDirection is
// DWARF-real (BrnTrafficParam.h:130) and is CALLED by CalcUp; because its own body is not in
// this batch it is provided here as a minimal inline accessor mirror of the mDirAndAccel slot
// so the TU links -- FLAGGED, to be replaced (not duplicated) by its own wave.
class ParamTransform
{
public:
    // 0x82712500 / 0x827126A0 / 0x82712770 / 0x827128E0 / 0x82712BA8 (this batch)
    Vector3  GetDeterministicPos() const;                      // later wave
    Vector3  GetLerpedPos() const;                             // 0x82712500
    Vector3  GetDirection() const;                             // FLAGGED inline stub (see below)
    Vector3  GetRight() const;                                 // 0x827126A0
    Vector3  CalcUp() const;                                   // 0x82712770
    VecFloat GetSpeed() const;                                 // 0x827128E0
    Vector3  GetLerpedPositionAcross(VecFloat lfAcross) const; // later wave
    void     UpdateLerpedPosition(VecFloat lfBlend);           // later wave
    void     Construct();                                      // later wave
    void     Initialise(Vector3 lPos, Vector3 lDir, Vector3 lRight, VecFloat lfSpeed); // 0x82712BA8
    void     Update(Vector3 lPos, Vector3 lDir, Vector3 lRight, VecFloat lfSpeed, VecFloat lfBlend); // later wave

private:
    Vector3     mPos;                // 0x00
    Vector3Plus mDirAndAccel;        // 0x10 (dir.xyz + accel in .w)
    Vector3     mRight;              // 0x20
    Vector3Plus mLerpedPosAndSpeed;  // 0x30 (lerped pos.xyz + speed in .w)
};
// sizeof(ParamTransform) == 0x40

// FLAGGED stub: GetDirection is a DWARF-real accessor (its own X360 body is a later wave).
// Provided inline so CalcUp compiles/links now; the trivial mDirAndAccel.GetVector3() mirror
// matches GetRight's slot-accessor shape. Its own wave replaces this definition.
inline Vector3 ParamTransform::GetDirection() const
{
    return mDirAndAccel.GetVector3();
}
}

#endif
