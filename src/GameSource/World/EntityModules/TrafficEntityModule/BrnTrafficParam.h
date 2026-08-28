#ifndef BRN_TRAFFIC_PARAM_H
#define BRN_TRAFFIC_PARAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "BrnCommonTypes.h"

// ---------------------------------------------------------------------------
// BrnTraffic::Param (and the ParamSoaData / ParamPlan it embeds)
//
// The traffic system's per-vehicle parameter block: position-along (mfParamAlong),
// behaviour and speed scalars, a small ring of where-it-has-been history
// (mauHistorySegments / mauHistoryHulls), a packed flag byte (mxFlags) and a separate
// effect/history byte (mxEffectAndHistoryState).
//
// Layout from the X360 XEX member STORE offsets, corroborated by the DWARF type listing.
// Retail is later than the Feb-2007 leak and diverges from it: retail adds
// purgatory/divorce, moves alive/dying/zombie membership into the ParamSoaData bit-sets,
// and renumbers mxFlags. The X360 asm wins wherever they differ.
//
// Byte offsets pinned by name from the store/load instructions (this == u8*):
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
// Console Param record stride is 128, not 0x70: GetParam @0x82707630 returns
// ((luParam + 1291) << 7) + this, UpdateParams @0x82744A80 strides the pool by 128, and
// maParamNeedToSlowData starts at 165248 + 400*128. [MEMBER HOLE 7] RESOLVED: no ARTIST load
// or store reaches a Param base at 0x6D..0x7F, and neither Param::Construct @0x82751B60 nor
// Param::Initialise @0x82755F40 writes there, so the 19-byte tail is record padding, not an
// unmodelled member. The Feb-2007 mfSpeedDiff is gone from the ship record: Initialise seeds
// mfAcceleration (0x30) and mfLastSpeed (0x34) in its place, and both are DWARF-named.
// HOST-NATIVE: this header pins no console size; nothing may assume sizeof(Param) == 128.
// ---------------------------------------------------------------------------

namespace BrnTraffic
{
// Pointer-only in the Param::Initialise signature below (DWARF BrnTrafficParam.h:330).
struct Hull;
struct VehicleTypeData;
struct VehicleTraits;
class  VehicleTypeRuntime;

// KU_PARAM_NUM_SEGMENTS_TO_REMEMBER / KU_PARAM_NUM_PLANS (BrnTrafficConstants.h, leak).
static const u32 KU_PARAM_NUM_SEGMENTS_TO_REMEMBER = 6;
static const u32 KU_PARAM_NUM_PLANS = 2;

// CgsFastBitArray out-of-range assert uses 600 as "max bits"; the SoA sets are
// FastBitArray<601> so a valid traffic param index is in [0, 600).
static const u32 KU_PARAM_MAX_PARAMS = 601;

// ParamSoaData: three FastBitArray<601> membership sets, one quadword-block each.
// Offsets proven by the asm: mAliveParams @0x00, mDyingParams @0x50, mZombieParams @0xA0.
struct ParamSoaData
{
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mAliveParams;  // 0x000
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mDyingParams;  // 0x050
    CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS> mZombieParams; // 0x0A0

    void Construct();
};

// ParamPlan: a queued lane / section change (6 bytes, verified by the maPlans stride).
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
        // SetDivorced sets 0x40 (sibling StaticTrafficParam::SetDivorced @0x82706CE8
        // `ori r11, r11, 0x40`); KillParam @0x82721FB8 reads the same bit.
        E_FLAG_DIVORCED         = 0x40,
        E_FLAG_IN_PURGATORY     = 0x80,
    };

    // mxEffectAndHistoryState bits used by this TU (asm): SetChangedSection sets 0x08,
    // SetDyingState sets 0x04 (HasDied), and HasDied tests 0x04.
    // 0x02 / 0x10: Initialise @0x82755F40 seeds the byte with 0x12, and UpdateParams
    // @0x82744A80 tests 0x02 (skip a param born this frame) and 0x10 (needs a new plan).
    enum EffectAndHistoryFlags
    {
        E_HISTORY_BORN            = 0x02,
        E_HISTORY_DIED            = 0x04,
        E_HISTORY_CHANGED_SECTION = 0x08,
        E_HISTORY_NEEDS_NEW_PLAN  = 0x10,
    };

    // UpdateParams_UpdateBehaviour @0x82716C90 asserts miBehaviour in [0, 7).
    //
    // [crash-surface wave 2026-08-28] FOUR OF THE SIX PREVIOUSLY-"NOT RECOVERABLE" ENUMERATORS
    // ARE ATTESTED -- by the console's own baked assert STRINGS, which are the original source
    // expressions verbatim. UpdateParams_UpdateBehaviour @0x82716C90 fires
    //   "lpParamNeedToSlowData->miBehaviour != Param::E_BEHAVIOUR_SLOWING_FOR_CRASH"        vs 0
    //   "...!= Param::E_BEHAVIOUR_STOPPING_FOR_OBSTRUCTION"                                 vs 1
    //   "...!= Param::E_BEHAVIOUR_DRIVING_AROUND_OBSTRUCTION"                               vs 2
    //   "...!= Param::E_BEHAVIOUR_FOLLOWING_RACE_CAR"                                       vs 3
    // and _wT2_03.cpp already compares those literals beside those exact strings. The two
    // remaining ship enumerators (4 and 5, written by UpdateParams_PrecalcBehaviourParams)
    // carry no assert string and stay unnamed -- do NOT invent them.
    // The KI_ spelling (not E_) matches this header's existing constants; the console's E_
    // names live in the assert strings above.
    static const s8 KI_BEHAVIOUR_SLOWING_FOR_CRASH            = 0;
    static const s8 KI_BEHAVIOUR_STOPPING_FOR_OBSTRUCTION     = 1;
    static const s8 KI_BEHAVIOUR_DRIVING_AROUND_OBSTRUCTION   = 2;
    static const s8 KI_BEHAVIOUR_FOLLOWING_RACE_CAR           = 3;
    static const s8 KI_BEHAVIOUR_NORMAL   = 6;
    static const s8 KI_BEHAVIOUR_INVALID  = -1;   // Construct @0x82751B60 seed
    static const s32 KI_BEHAVIOURS_COUNT  = 7;

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
    void Construct();                                            // @ 0x82751B60
    // @ 0x82755F40. Parameter list is DWARF BrnTrafficParam.h:330, confirmed slot for slot
    // against the prologue (r4/r5, f1/f2 with r6/r7 skipped, r8/r9/r10, then four 8-byte
    // right-justified stack slots).
    void Initialise(u32 luHullIndex,
                    u32 luSectionIndex,
                    f32 lfParamAlong,
                    f32 lfRandomVal,
                    u32 luVehicleType,
                    const Hull* lpHull,
                    const VehicleTypeData* lpVehicleTypeData,
                    const VehicleTypeRuntime* lpVehicleTypeRuntime,
                    const VehicleTraits* lpVehicleTraits,
                    u32 luParam,
                    ParamSoaData& lParamSoaData);
    void ClearDying(u32 luParam, ParamSoaData& lSoaData);
    void GetHistoryEntry(u32 luHistoryIndex, u32* lpOutSegmentIndex, u32* lpOutHullIndex) const;
    bool IsQueueing() const;
    void PushHistory(u32 luSegmentIndex, u32 luHullIndex);
    void SetChangedSection();
    void SetDyingState(u32 luParam, ParamSoaData& lSoaData);
    void SetInPurgatory(bool lbInPurgatory);
    void SetParamAlong(f32 lfParamAlong);
    void SetShouldBeRemoved();
    // @ 0x82736918 (ARTIST EXPORT HOLE -- no per-function JSON; GenerateNewVehicle
    // @0x82736528 calls it by name). Shape from the sibling StaticTrafficParam::SetDivorced
    // @0x82706CE8, whose flag byte carries the same bit values.
    void SetDivorced();
    void SetZombie(u32 luParam, ParamSoaData& lSoaData);
    bool ShouldBeIndicatingRight() const;

    // Helpers the asm inlines into the above (state predicates).
    bool IsAlive() const { return (mxFlags & E_FLAG_ALIVE) != 0; }
    bool IsDying() const { return (mxFlags & E_FLAG_DYING) != 0; }
    bool HasDied() const { return (mxEffectAndHistoryState & E_HISTORY_DIED) != 0; }
    bool IsInPurgatory() const { return (mxFlags & E_FLAG_IN_PURGATORY) != 0; }
    // Attested by TrafficEntityModule::IsVehiclesParamAZombie @0x82715D70, whose
    // STANDARD-species arm is `(*(GetParam(luVehicle) + 64) >> 5) & 1`: offset 0x40 is
    // mxFlags and bit 5 is E_FLAG_ZOMBIE.
    bool IsZombie() const { return (mxFlags & E_FLAG_ZOMBIE) != 0; }
};

// BrnTraffic::ParamTransform -- the per-vehicle orientation/position transform block.
// DWARF BrnTrafficParam.h:314: four private 16-byte, 16-aligned SIMD vectors (sizeof ==
// 0x40) plus the method set below. Offsets pinned by the X360 store/load displacements
// (GetRight +0x20, GetLerpedPos/GetSpeed +0x30, Initialise stores all four) and corroborated
// field for field by the DWARF member listing:
//   0x00 mPos               (Vector3)      -- deterministic world position
//   0x10 mDirAndAccel       (Vector3Plus)  -- forward dir in xyz, accel scalar in w/plus
//   0x20 mRight             (Vector3)      -- right axis
//   0x30 mLerpedPosAndSpeed (Vector3Plus)  -- render-lerped pos in xyz, speed in w/plus
// Methods without an address below are declared in DWARF shape for later waves.
class ParamTransform
{
public:
    Vector3  GetDeterministicPos() const;                      // 0x82712430 (export hole)
    Vector3  GetLerpedPos() const;                             // 0x82712500
    Vector3  GetDirection() const;                             // 0x827125D0
    Vector3  GetRight() const;                                 // 0x827126A0
    Vector3  CalcUp() const;                                   // 0x82712770
    VecFloat GetSpeed() const;                                 // 0x827128E0
    // PARK: no ARTIST symbol and no caller in the traffic function set, so there is no
    // attested body. Declaration only.
    Vector3  GetLerpedPositionAcross(VecFloat lfAcross) const;
    void     UpdateLerpedPosition(VecFloat lfSimTimeStep);     // 0x82712968
    void     Construct();                                      // 0x82751AF8
    void     Initialise(Vector3 lPos, Vector3 lDir, Vector3 lRight, VecFloat lfSpeed); // 0x82712BA8
    // 0x82712E28. The fifth argument is lfAcceleration, not a blend factor: the console's
    // own assert string at BrnTrafficParam.h:697 names it.
    void     Update(Vector3 lPos, Vector3 lDir, Vector3 lRight, VecFloat lfSpeed,
                    VecFloat lfAcceleration);

private:
    Vector3     mPos;                // 0x00
    Vector3Plus mDirAndAccel;        // 0x10 (dir.xyz + accel in .w)
    Vector3     mRight;              // 0x20
    Vector3Plus mLerpedPosAndSpeed;  // 0x30 (lerped pos.xyz + speed in .w)
};
// sizeof(ParamTransform) == 0x40

// BrnTraffic::ParamListNode -- DWARF BrnTrafficParam.h:416. One node of the ordered
// "params on this section" doubly-linked list the UpdateParams_UpdateLinkedList pipeline
// walks; the module owns maParamListNodes[KU_MAX_PARAMS]. Record is 8 bytes on the console
// (Reset @0x8272CDA0 Constructs 400 of them from +222848 with an 8-byte stride, and
// 222848 + 400*8 == 226048 == the maParamTransforms base UpdateVehicles @0x82744F58 passes).
struct ParamListNode
{
    u16 muNextParam;   // :418
    u16 muPrevParam;   // :419
    f32 mfParamAlong;  // :421

    void Construct();  // @ 0x82751C38
};

// BrnTraffic::ParamNeedToSlowData -- DWARF BrnTrafficParam.h:439. The per-param
// "who is in front of me and how hard must I brake" record UpdateParams_PrecalcBehaviourParams
// writes and UpdateParams_UpdateBehaviour @0x82716C90 copies into the Param. Record is 16 bytes
// on the console (GetParamNeedToSlowData @0x827077D0 returns 16 * (luParam + 13528) + this, and
// 16*13528 == 216448 == 165248 + 400*128). Member offsets confirmed by Reset @0x8272CDA0's
// inlined per-param seed (+0 u16, +2 s8, +4/+8/+12 f32) and by UpdateParams_UpdateBehaviour
// copying +12 into Param::mfStopDist and +8 into Param::mfTargetSpeed.
struct ParamNeedToSlowData
{
    u16 muParamInFront;  // :442  +0x00
    s8  miBehaviour;     // :443  +0x02
    f32 mfNextParamDist; // :445  +0x04
    f32 mfTargetSpeed;   // :446  +0x08
    f32 mfStopDist;      // :447  +0x0C

    void Construct();    // :452
    void Clear();        // :456
};
}

#endif
