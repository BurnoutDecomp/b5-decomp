#ifndef BRN_PROFILE_H
#define BRN_PROFILE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                               // CgsID (Profile drive-thru / deform / event accessors)
#include "SharedClasses/Progression/BrnTrainingTypes.h"  // BrnProgression::ETrainingType (Profile training-flag accessors)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion::Type (drive-thru discovery accessors)

// Minimal owning slice for BrnProgression::ProfileEvent -- the element type of
// Array<BrnProgression::ProfileEvent,175>. DWARF home is BrnProfile.h:312.
// 8-byte stride is PROVEN by the X360 Array<ProfileEvent,175>::GetItem body
// (`return 8 * index + this`) and the count word landing at +1400 == 175*8:
//   muEventID(4) + muFlags(2) + 2 pad = 8.
// Only the shape needed to give the element a real 8-byte size is sliced here.

namespace BrnProgression
{
// Forward-declared so the CarSelectManager-facing Profile grows below can return/route CarData* by
// pointer (full home: BrnProgressionCarData.h, #included by the dereferencing .cpp).
struct CarData;

struct ProfileEvent
{
    // DWARF BrnProfile.h:296 (Flags enum).
    enum Flags
    {
        E_FLAG_UNDISCOVERED             = 0,
        E_FLAG_DISCOVERED               = 1,
        E_FLAG_FINISHED                 = 2,
        E_FLAG_RANK_WIN                 = 4,
        E_FLAG_NON_RANK_WIN             = 8,
        E_FLAG_WON_SPECIAL_EVENT_BEFORE = 16,
        E_FLAG_WON_EVENT_BEFORE         = 32
    };

    void Construct(u32 luEventID);
    u32  GetID() const;
    u16  GetFlags() const;
    void SetFlags(u16 lu16Flags);
    bool IsFlagSet(Flags leFlag) const;
    void EnableFlags(u16 lu16Flags);
    void ClearFlags(u16 lu16Flags);

    // ADDITIVE GROW (declare-only; bodies in the Profile/ProfileEvent TU).
    // DriveThruManager::UnlockCarChallengeForCar flips the "found" flag on a junction's ProfileEvent.
    // X360: IsFound() reads the muFlags word @+4 bit 0 (E_FLAG_DISCOVERED); SetFound(true) sets it.
    bool IsFound() const;
    void SetFound(bool lbFound);

private:
    u32 muEventID;   // BrnProfile.h:335  (+0)
    u16 muFlags;     // BrnProfile.h:336  (+4)
    // 2 bytes trailing pad -> 8-byte stride (X360 GetItem: 8*index)
};

// Minimal owning slice for BrnProgression::MugshotInfo -- the element type of
// Array<BrnProgression::MugshotInfo, 20>. The DWARF in this batch shows the *PS3*
// instantiation Array<MugshotInfo,30u> (N=30) but the X360 BINARY IS AUTHORITATIVE and
// proves N=20 and a 56-byte stride:
//   * MugshotInfo,20>::GetItem @0x8235ED28 returns `56 * index + this` -> 56-byte stride
//   * the live-count word lands at byte +1120 == 20 * 56 -> capacity N == 20
//   * MugshotInfo,20>::IsFull @0x823679B8 returns `count == 20`
//   * Append/Erase copy 7 qwords (7 * 8 == 56 bytes) per element -> 56-byte stride
// So the X360 game ships Array<MugshotInfo,20> (the DWARF "30u" is PS3 drift; noted).
//
// Only the shape needed to give the element a real 56-byte size is sliced here. The full
// internal layout (incl. the nested MugshotInfo::UniquePlayerID referenced by
// BrnNetworkLiveRevengeRelationship.h:326) is NOT yet reverse-engineered; modelled as an
// opaque 56-byte buffer so the container stride/count offsets are exact without fabricating
// member names. The owning TU of MugshotInfo's real members must replace this pad.
struct MugshotInfo
{
    // Nested unique-player identifier referenced cross-subsystem (DWARF
    // BrnNetworkLiveRevengeRelationship.h:326 "MugshotInfo::UniquePlayerID mUniqueID").
    // Declared-only: its real layout is not recovered here.
    struct UniquePlayerID;

private:
    // Opaque 56-byte body (X360-proven stride). Replace with the real named members
    // (incl. UniquePlayerID) when MugshotInfo's own TU is reconstructed.
    u8 mPad_Body[56];   // sizeof(MugshotInfo) == 56 (X360 Array<MugshotInfo,20>::GetItem stride)
};

// ----------------------------------------------------------------------------
// MINIMAL DECLARE-ONLY slice for BrnProgression::Profile -- the player's persisted
// progression record (embedded by value inside ProgressionManager at +0x170). Its full
// layout (the discovered-event table, training-flag bitfields, takedown/win tallies, etc.)
// is a large, separate TU and is NOT modelled here. Only the methods concrete callers in
// already-reconstructed TUs name are declared; bodies + the real member layout land with
// the Profile/ProgressionManager TU. FLAG: declare-only additive slice.
// ----------------------------------------------------------------------------
class Profile
{
public:
    // X360 BrnProgression::Profile::ClearTrainingFlags -- DEBUG-only reset of the persisted
    // training-flag bitfield. TrainingManager::DEBUG_ClearTrainingFlags (0x82366050) zeroes the
    // four-qword flag region the X360 keeps at Profile+0x1CCC0; that internal layout belongs to
    // the Profile TU. Declare-only here.
    void ClearTrainingFlags();

    // X360 BrnProgression::Profile::HasPlayerSeenTrainingType -- true when the player has already
    // seen training tip leTrainingType (read from the persisted seen-bitfield). Named by
    // TrainingManager::RequestTraining (0x82365B20) and several Progression callers. Declare-only.
    bool HasPlayerSeenTrainingType(ETrainingType leTrainingType) const;

    // X360 BrnProgression::Profile::SetTrainingAlreadySeen -- mark training tip leTrainingType as
    // seen. Named by TrainingManager::SendTrainingTickerMessage. Declare-only.
    void SetTrainingAlreadySeen(ETrainingType leTrainingType);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the DriveThruManager TU. The drive-thru discovery /
    // body-shop-repair / event-unlock flow reads + writes the persisted Profile. Signatures +
    // semantics are X360-asm-attested; the real member layout (the discovered-drive-thru table,
    // the deform tally, the race-event id table at +29168) is owned by the Profile TU. Declare-only.
    // ------------------------------------------------------------------------

    // X360 0x8236ABA8. True iff the drive-thru region lId of kind leType has already been discovered.
    bool IsDriveThruDiscoverd(CgsID lId, BrnTrigger::GenericRegion::Type leType) const;

    // X360 0x823619F8. Count of drive-thrus of kind leType the player has discovered so far.
    s32 GetNumDriveThrusDiscovered(BrnTrigger::GenericRegion::Type leType) const;

    // X360 0x8230FBA8. The car lCarId's base (un-repaired) deformation amount (> 0 == damaged).
    f32 GetPlayerBaseDeformAmount(CgsID lCarId) const;

    // ++ the discovered-event counter (X360 Profile+580). UnlockCarChallengeForCar calls it on a
    // newly-found event junction.
    void IncrementNumDiscoveredEvents();

    // X360 linear search of the persisted race-event id table (count word +1000, id table base
    // +29168) -> the matching ProfileEvent (or NULL). Used by UnlockCarChallengeForCar.
    ProfileEvent* FindProfileEventByRaceEventId(CgsID lEventId);

    // ------------------------------------------------------------------------
    // ADDITIVE GROW (declare-only) for the BrnGameState::CarSelectManager (junkyard car-select) TUs.
    // The junkyard FSM walks the owned-car array and reads/writes the chosen-livery + start-of-game
    // deform state. Signatures + semantics are X360-asm-attested; the real owned-car array layout
    // (base Profile+0x280/640, stride 0x18, count word Profile+0x26C/620) is owned by the Profile TU.
    // Bodies land with the Profile TU; declare-only suffices for `cl /c`.
    // ------------------------------------------------------------------------

    // X360 reads the car-count word at Profile+0x26C/620 -- the number of owned-car records.
    s32 GetCarCount() const;

    // X360 0x82354950. The liIndex'th owned-car record (car-array base Profile+0x280/640, stride 0x18,
    // id at +0x00). Asserts liIndex in range (BrnProfile.h:1923 'liCarIndex >= 0 && liCarIndex < miCarCount').
    const CarData* GetCarData(s32 liIndex) const;
          CarData* GetCarData(s32 liIndex);

    // X360 0x82359BF8. Linear lookup of the owned-car record whose id == lCarId, or NULL on miss.
    CarData* FindCar(CgsID lCarId);

    // X360 0x82359C90. Pin the chosen base-car livery on the profile (called from EnterJunkyard with the
    // active player car id).
    void SetChosenLiveryIdForBaseCar(CgsID lBaseCarId);

    // X360 0x82359D70. The player's saved livery-variant id for base car lBaseCarId.
    CgsID GetChosenLiveryIdForBaseCar(CgsID lBaseCarId) const;

    // FLAG: de-inlined byte read at Profile+118401 in ReallyEnterJunkyardAtStartOfGame -- gates whether
    // the start-of-game car gets the 0.85 deform. Exact member name unconfirmed.
    bool IsStartOfGameDeformActive() const;
};
}

#endif // BRN_PROFILE_H
