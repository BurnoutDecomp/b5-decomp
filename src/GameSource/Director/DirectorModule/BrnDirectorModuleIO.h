#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base + read/write lock-state queries
#include "GameShared/GameClasses/Containers/CgsBitArray.h"        // CgsContainers::BitArray<N> (mUsedRaceCars)
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"    // BrnDirector::Camera::VehicleInfo (committed, 1264 bytes)

// BrnDirector::DirectorIO::InputBuffer -- the Director module's per-frame INPUT payload buffer.
// Like every CgsModule IO buffer it derives the shared CgsModule::IOBuffer (status-flag-guarded
// read/write locking; bit 4 = read lock, bit 3 = write lock) at offset +0, then embeds a large
// aggregate of producer-published state the world/game-state/controller bridges fill each frame
// and the director consumes.
//
// LAYOUT PROVENANCE. The member ORDER + member TYPES are the DecFIGS DWARF for
// GameSource/Director/DirectorModule/BrnDirectorModuleIO.h (struct InputBuffer, decl lines
// 86/324-372). The exact BYTE OFFSETS below are recovered directly from the X360 accessor
// bodies in BURNOUT_X360_ARTIST.XEX (the getters `return this + <off>`; the setters store at
// `this + <off>`):
//
//     mUsedRaceCars              @0x0980 (2432)    GetUsedRaceCars / SetRaceCarInfo bit-set
//     mRaceCarInfo[8]            @0x0990 (2448)    SetRaceCarInfo:  VehicleInfo stride 0x4F0 (1264),
//                                                  SetCrashingCentreOfMass: 1264*idx + 0xDF0/0xE75
//     maVehicleInfoArray[8]      @0x3238 (12856)   GetVehicleInfoArray / SetVehicleTeam (4*idx)
//     mControllerInfo            @0x3260 (12896)   SetControllerInfo memcpy 224 bytes
//     mTimerInterface            @0x6750 (26448)   GetTimerStatusInterface
//     mVehicleDriverInputIface   @0x6780 (26496)   GetVehicleInputInterface
//     mContacts                  @0x6AB8 (27320)   GetContacts / AppendContacts (stores word [6830])
//     mHookEnumeration           @0x7910 (30992)   GetHookEnumeration / SetHookEnumeration memcpy 404
//     mePlayerCarIndex           @0x7AA8 (31400)   GetPlayerCarIndex (lwz, s32)
//     mbHasGotHookEnumeration    @0x7AC1 (31425)   SetHookEnumeration sets =1; HasGotHookEnumeration
//     mbGotCrashNavShownEvent    @0x7AC3 (31427)
//     mbGotCrashNavHiddenEvent   @0x7AC4 (31428)
//     mbGotColourCalibrationShownEvent  @0x7AC5 (31429)
//     mbGotColourCalibrationHiddenEvent @0x7AC6 (31430)
//     mbGotShortcutMenuEvent     @0x7ACD (31437)   SetShortcutMenuEvent sets =1; HasGotShortcutMenuEvent
//     mbShortcutMenuState        @0x7ACE (31438)   SetShortcutMenuEvent stores arg; GetShortcutMenuState
//
// VehicleInfo's own internal offsets are independently confirmed by SetCrashingCentreOfMass:
// element base this+1264*idx, mCrashingCentreOfMass at element+0x460 (BrnPlayerInfo.h), the
// mbHasCrashingCenterOfMass flag at element+0x4E5 -- exactly the committed VehicleInfo layout.
//
// HONEST PLACEHOLDERS. Several embedded members are large interface aggregates whose full byte
// layouts are not yet reconstructed (RCEntityGlobalRaceCarOutputInterface, ControllerInfo,
// TimerStatusInterface, BrnDirectorVehicleInputInterface, the world StatusInterface, the
// ContactSpyInterface, TrafficDirectorOutputInterface, PlayerCrashInfo, GuiPFXHookEnumeration,
// CarScoreData, DirectorProfileData, and the GameActionQueue). Rather than fork those homes
// with guessed members, they are modelled here as correctly-SIZED, byte-addressable opaque
// storage members carrying their DWARF names and offsets. This preserves the exact object
// layout the accessors index into (every recovered offset is asserted below) while being honest
// that the interiors are not yet known. Grow each into its real type, additively, when its home
// is reconstructed. The members whose types ARE committed (the IOBuffer base, BitArray<8u>,
// VehicleInfo[8], EActiveRaceCarIndex, and the trailing bool flags) are modelled by their real
// types/names.

namespace BrnDirector
{
namespace DirectorIO
{
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // --- queries (getters): all const, all read-lock-asserted ---
        // (GetContacts/GetHookEnumeration/GetControllerInfo/GetTimerStatusInterface/
        //  GetVehicleInputInterface return the addresses of opaque-but-correctly-sized members,
        //  typed as void* until those interface homes are reconstructed.)
        const void*                                        GetContacts() const;
        const CgsContainers::BitArray<8u>*                 GetUsedRaceCars() const;
        // Address of the X360 VehicleInfo* pointer table (stored as pointer-width u32 slots).
        const u32*                                         GetVehicleInfoArray() const;
        const void*                                        GetControllerInfo() const;
        const void*                                        GetTimerStatusInterface() const;
        const void*                                        GetVehicleInputInterface() const;
        void*                                              GetVehicleInputInterface();
        const void*                                        GetHookEnumeration() const;
        EActiveRaceCarIndex                                GetPlayerCarIndex() const;
        bool                                               GetShortcutMenuState() const;

        bool HasGotHookEnumeration() const;
        bool HasGotShortcutMenuEvent() const;
        bool HasGotCrashNavShownEvent() const;
        bool HasGotCrashNavHiddenEvent() const;
        bool HasGotColourCalibrationShownEvent() const;
        bool HasGotColourCalibrationHiddenEvent() const;

        // --- mutators: all write-lock-asserted ---
        void AppendContacts(const void* lpContacts);
        void SetControllerInfo(const void* lpControllerInfo);
        void SetHookEnumeration(const void* lpHookEnumeration);
        void SetRaceCarInfo(u32 luIndex, const BrnDirector::Camera::VehicleInfo& lrInfo);
        void SetCrashingCentreOfMass(u32 luIndex, const Matrix44Affine& lrCentreOfMass);
        void SetVehicleTeam(EActiveRaceCarIndex leIndex, s32 liTeam);

        void SetShortcutMenuEvent(bool lbState);
        void SetGotCrashNavShownEvent();
        void SetGotCrashNavHiddenEvent();
        void SetGotColourCalibrationShownEvent();
        void SetGotColourCalibrationHiddenEvent();

    private:
        // Recovered byte offsets are pinned by _AssertLayout() below.

        // @0x0001 .. : the IOBuffer base is 1 byte (FlagSet8). The first published member,
        // mGlobalRaceCarInterface (RCEntityGlobalRaceCarOutputInterface), spans from just past the
        // base up to mUsedRaceCars @0x0980. HONEST opaque storage (type home not yet reconstructed).
        u8  mGlobalRaceCarInterface[0x0980 - 0x0001];   // RCEntityGlobalRaceCarOutputInterface @ ~0x0001

        // @0x0980 (2432): the active-race-car bitmask. BitArray<8u> is one u64 field (8 bytes);
        // VehicleInfo is alignas(16) so the array slot is 16-byte aligned -> 8 bytes trailing pad.
        CgsContainers::BitArray<8u> mUsedRaceCars;       // @0x0980
        u8  mUsedRaceCarsPad[0x0990 - 0x0980 - sizeof(CgsContainers::BitArray<8u>)];

        // @0x0990 (2448): per-active-car published vehicle state. Stride 0x4F0 (1264) confirmed.
        BrnDirector::Camera::VehicleInfo mRaceCarInfo[8]; // @0x0990 .. 0x3110

        // @0x3110 .. : mPlayerScoreData (CarScoreData) + mbPlayerComboWarningActive +
        // mfPlayerBoostPercentage, ending at the vehicle-info pointer array @0x3238. HONEST opaque.
        u8  mScoreAndBoostBlock[0x3238 - (0x0990 + 8 * 1264)]; // CarScoreData + combo flag + boost f32

        // @0x3238 (12856): the parallel array of VehicleInfo pointers GetVehicleInfoArray() returns;
        // SetVehicleTeam writes the team id (a VehicleInfo*-typed slot, stored as s32) per index.
        // NOTE: the X360 is a 32-bit target (4-byte pointers), but the compile gate builds for the
        // 64-bit host. To preserve the exact 32-byte X360 layout (4 bytes/slot) the pointer table is
        // stored as 8 X360-pointer-width u32 slots; the accessors reinterpret them as needed.
        u32 maVehicleInfoArray[8];                       // @0x3238 (32 bytes -> 0x3258)
        u8  maVehicleInfoArrayPad[0x3260 - (0x3238 + 8 * sizeof(u32))]; // 8-byte gap to controller

        // @0x3260 (12896): controller snapshot, 224 (0xE0) bytes (SetControllerInfo memcpy). HONEST.
        u8  mControllerInfo[224];                        // @0x3260 .. 0x3340

        // @0x3340 .. 0x6750: the world status interface, vehicle-driver input interface tail, timer,
        // etc. live across this span; only the addressed anchors below are recovered. The first
        // recovered anchor after the controller block is mTimerInterface @0x6750. HONEST opaque span.
        u8  mMidInterfaceBlock[0x6750 - 0x3340];         // StatusInterface / driver-input / score / etc.

        // @0x6750 (26448): timer status interface (GetTimerStatusInterface). HONEST opaque.
        u8  mTimerInterface[0x6780 - 0x6750];            // @0x6750

        // @0x6780 (26496): the vehicle driver-input interface (Get/SetVehicleInputInterface).
        // HONEST opaque, spanning to the contact-spy interface @0x6AB8.
        u8  mVehicleDriverInputInterface[0x6AB8 - 0x6780]; // @0x6780

        // @0x6AB8 (27320): contacts. AppendContacts stores one 32-bit word at member word [0]
        // (this[6830] == 0x6AB8). GetContacts returns its address. HONEST opaque storage spanning
        // to the hook-enumeration block @0x7910.
        u8  mContacts[0x7910 - 0x6AB8];                  // @0x6AB8

        // @0x7910 (30992): the GUI PFX hook enumeration, 404 (0x194) bytes (SetHookEnumeration
        // memcpy). HONEST opaque, padded out to the scalar/flag tail @0x7AA8.
        u8  mHookEnumeration[404];                       // @0x7910 .. 0x7AA4
        u8  mHookTailBlock[0x7AA8 - (0x7910 + 404)];     // DirectorProfileData etc. up to the index

        // @0x7AA8 (31400): the active player car index (GetPlayerCarIndex, read as a 32-bit word).
        EActiveRaceCarIndex mePlayerCarIndex;            // @0x7AA8
        // @0x7AAC .. 0x7AC1: mePlayerKillerCarIndex + miRankUpNewRank + early bool flags
        // (mbRankUpThisFrame, mbStartNewProfileIntro, ...) up to mbHasGotHookEnumeration. HONEST.
        u8  mIndexTailBlock[0x7AC1 - (0x7AA8 + sizeof(EActiveRaceCarIndex))];

        // @0x7AC1 (31425): the recovered trailing event flags. Each is a 1-byte bool addressed by
        // the X360 bodies at the exact offsets pinned in _AssertLayout().
        bool mbHasGotHookEnumeration;                    // @0x7AC1
        bool mbEndOfCarSelect;                           // @0x7AC2 (DWARF order; not addressed here)
        bool mbGotCrashNavShownEvent;                    // @0x7AC3
        bool mbGotCrashNavHiddenEvent;                   // @0x7AC4
        bool mbGotColourCalibrationShownEvent;           // @0x7AC5
        bool mbGotColourCalibrationHiddenEvent;          // @0x7AC6
        // @0x7AC7 .. 0x7ACD: mbWorldWantsDebugControllerFocus, mbSimPaused,
        // mbHasNewDirectorProfileData, mbPlayerCrashbreakerFired, mbCarSelectionChangedThisFrame,
        // mbCarSelectTickerClosedThisFrame (DWARF order), up to mbGotShortcutMenuEvent.
        u8  mMidFlagBlock[0x7ACD - 0x7AC7];
        bool mbGotShortcutMenuEvent;                     // @0x7ACD
        bool mbShortcutMenuState;                        // @0x7ACE

        // Compile-time pin of every recovered offset (private members -> assert from a member fn).
        static void _AssertLayout();
    };
}
}
