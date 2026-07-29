#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy

// BrnDirector::DirectorIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 27 X360-emitted InputBuffer functions:
//
//   getters @ 0x82206CF8 .. 0x82207628  (return &member; assert IsBufferLockedForReading)
//   mutators @ 0x823B2890 .. 0x823B3248 (write member; assert IsBufferLockedForWriting)
//
// Every accessor first checks the IOBuffer lock-state flag and asserts on violation, exactly as
// the X360 bodies do (the original streams the file/line via CgsDev::Assert; CGS_ASSERT carries
// the stringized condition + __FILE__/__LINE__). The bodies then read/write the member the
// X360 addresses by raw offset -- here named, with the offsets pinned in _AssertLayout().
//
// The "value" payloads of the opaque interface members are not interpreted: AppendContacts /
// SetControllerInfo / SetHookEnumeration copy the producer's bytes into the correctly-sized
// member storage exactly as the X360 memcpy/word-store bodies do.

namespace BrnDirector
{
namespace DirectorIO
{
    // ---- byte-offset pins (X360-recovered) --------------------------------------------------
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mUsedRaceCars)        == 0x0980, "mUsedRaceCars @0x0980");
        static_assert(offsetof(InputBuffer, mRaceCarInfo)         == 0x0990, "mRaceCarInfo @0x0990");
        static_assert(sizeof(BrnDirector::Camera::VehicleInfo)    == 1264,   "VehicleInfo stride 0x4F0");
        static_assert(offsetof(InputBuffer, maVehicleInfoArray)   == 0x3238, "maVehicleInfoArray @0x3238");
        static_assert(offsetof(InputBuffer, mControllerInfo)      == 0x3260, "mControllerInfo @0x3260");
        static_assert(offsetof(InputBuffer, mTimerInterface)      == 0x6750, "mTimerInterface @0x6750");
        static_assert(sizeof(CgsSystem::TimerStatusInterface)     == 0x30,   "TimerStatusInterface fills the @0x6750 span exactly");
        static_assert(offsetof(InputBuffer, mVehicleDriverInputInterface) == 0x6780, "mVehicleDriverInputInterface @0x6780");
        static_assert(offsetof(InputBuffer, mContacts)            == 0x6AB8, "mContacts @0x6AB8");
        static_assert(offsetof(InputBuffer, mHookEnumeration)     == 0x7910, "mHookEnumeration @0x7910");
        static_assert(offsetof(InputBuffer, mePlayerCarIndex)     == 0x7AA8, "mePlayerCarIndex @0x7AA8");
        static_assert(offsetof(InputBuffer, mbHasGotHookEnumeration)           == 0x7AC1, "mbHasGotHookEnumeration @0x7AC1");
        static_assert(offsetof(InputBuffer, mbGotCrashNavShownEvent)           == 0x7AC3, "mbGotCrashNavShownEvent @0x7AC3");
        static_assert(offsetof(InputBuffer, mbGotCrashNavHiddenEvent)          == 0x7AC4, "mbGotCrashNavHiddenEvent @0x7AC4");
        static_assert(offsetof(InputBuffer, mbGotColourCalibrationShownEvent)  == 0x7AC5, "mbGotColourCalibrationShownEvent @0x7AC5");
        static_assert(offsetof(InputBuffer, mbGotColourCalibrationHiddenEvent) == 0x7AC6, "mbGotColourCalibrationHiddenEvent @0x7AC6");
        static_assert(offsetof(InputBuffer, mbGotShortcutMenuEvent)            == 0x7ACD, "mbGotShortcutMenuEvent @0x7ACD");
        static_assert(offsetof(InputBuffer, mbShortcutMenuState)              == 0x7ACE, "mbShortcutMenuState @0x7ACE");
    }

    // ---- getters (read-lock asserted) -------------------------------------------------------

    const void* InputBuffer::GetContacts() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mContacts;
    }

    const CgsContainers::BitArray<8u>* InputBuffer::GetUsedRaceCars() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mUsedRaceCars;
    }

    const u32* InputBuffer::GetVehicleInfoArray() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        // Returns the address of the X360 VehicleInfo* pointer table (pointer-width u32 slots).
        return maVehicleInfoArray;
    }

    const void* InputBuffer::GetControllerInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mControllerInfo;
    }

    // ---- ADDITIVE (MainDirector::UpdateArbitrator @0x82271120) ------------------------------

    // The X360 de-inlined accessor (sub_82207040) whose result UpdateArbitrator indexes as
    // `1264 * playerCarIndex + base` -- i.e. the published VehicleInfo array at @0x0990.
    const BrnDirector::Camera::VehicleInfo* InputBuffer::GetRaceCarInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mRaceCarInfo;
    }

    // ArbStateSharedInfo::mpPlayerCrashInfo == lpInputBuffer + 30944 (@0x78E0), which lands
    // 0xE28 into the honest-opaque contacts span. Addressed off that NAMED member -- this
    // type owns the offset, so no caller has to.
    // FLAG: BrnDirector::PlayerCrashInfo is un-homed; the DWARF slot name is the only
    //   evidence for the role, and the payload is not interpreted here.
    const void* InputBuffer::GetPlayerCrashInfo() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mContacts[0x78E0 - 0x6AB8];
    }

    // @0x7AC8 -- the second byte of the mid-flag block (which spans 0x7AC7..0x7ACD:
    // mbWorldWantsDebugControllerFocus @0x7AC7, mbSimPaused @0x7AC8, ...). Read by
    // MainDirector::UpdateArbitrator (as Arbitrator::Update's lbPaused) and by the gameplay
    // middle of MainDirector::Update.
    bool InputBuffer::IsSimPaused() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mMidFlagBlock[0x7AC8 - 0x7AC7] != 0;
    }

    const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerInterface;
    }

    // X360 @0x823B27E8 -- the write-side overload (BrnGameModule::BridgeTimers takes the write
    // lock, calls this, and copies the game module's 48-byte snapshot over it).
    CgsSystem::TimerStatusInterface* InputBuffer::GetTimerStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mTimerInterface;
    }

    const void* InputBuffer::GetVehicleInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mVehicleDriverInputInterface;
    }

    void* InputBuffer::GetVehicleInputInterface()
    {
        // X360 0x823B2890: the non-const (write-side) handle tests bit 3 (write-lock),
        // unlike the const read-lock overload above.
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return mVehicleDriverInputInterface;
    }

    const void* InputBuffer::GetHookEnumeration() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        CGS_ASSERT(mbHasGotHookEnumeration, "mbHasGotHookEnumeration");
        return mHookEnumeration;
    }

    EActiveRaceCarIndex InputBuffer::GetPlayerCarIndex() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mePlayerCarIndex;
    }

    bool InputBuffer::GetShortcutMenuState() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbShortcutMenuState;
    }

    // ---- has-event queries (read-lock asserted) ---------------------------------------------

    bool InputBuffer::HasGotHookEnumeration() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbHasGotHookEnumeration;
    }

    bool InputBuffer::HasGotShortcutMenuEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotShortcutMenuEvent;
    }

    bool InputBuffer::HasGotCrashNavShownEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotCrashNavShownEvent;
    }

    bool InputBuffer::HasGotCrashNavHiddenEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotCrashNavHiddenEvent;
    }

    bool InputBuffer::HasGotColourCalibrationShownEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotColourCalibrationShownEvent;
    }

    bool InputBuffer::HasGotColourCalibrationHiddenEvent() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return mbGotColourCalibrationHiddenEvent;
    }

    // ---- mutators (write-lock asserted) -----------------------------------------------------

    void InputBuffer::AppendContacts(const void* lpContacts)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360: this[6830] = *(u32*)lpContacts; i.e. publish the first word of the producer's
        // contact-spy payload into the contacts member's leading word.
        *reinterpret_cast<u32*>(mContacts) = *static_cast<const u32*>(lpContacts);
    }

    void InputBuffer::SetControllerInfo(const void* lpControllerInfo)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        std::memcpy(mControllerInfo, lpControllerInfo, sizeof(mControllerInfo)); // 224 bytes
    }

    void InputBuffer::SetHookEnumeration(const void* lpHookEnumeration)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        std::memcpy(mHookEnumeration, lpHookEnumeration, sizeof(mHookEnumeration)); // 404 bytes
        mbHasGotHookEnumeration = true;
    }

    void InputBuffer::SetRaceCarInfo(u32 luIndex, const BrnDirector::Camera::VehicleInfo& lrInfo)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(luIndex < 8, "Index < Number of bits");
        mUsedRaceCars.SetBit(luIndex);
        mRaceCarInfo[luIndex] = lrInfo;
    }

    void InputBuffer::SetCrashingCentreOfMass(u32 luIndex, const Matrix44Affine& lrCentreOfMass)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360 copies the 4-row affine (4 * 16-byte VMX stores) into the indexed VehicleInfo's
        // mCrashingCentreOfMass (element+0x460) then sets mbHasCrashingCenterOfMass (element+0x4E5).
        mRaceCarInfo[luIndex].mCrashingCentreOfMass     = lrCentreOfMass;
        mRaceCarInfo[luIndex].mbHasCrashingCenterOfMass = true;
    }

    void InputBuffer::SetVehicleTeam(EActiveRaceCarIndex leIndex, s32 liTeam)
    {
        CGS_ASSERT(leIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID");
        CGS_ASSERT(leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // X360: *(s32*)&this[4*idx + 12856] = team; the team id is stored into the idx'th slot of
        // the VehicleInfo pointer table (a pointer-width slot reused as the team id).
        maVehicleInfoArray[leIndex] = static_cast<u32>(liTeam);
    }

    // ADDITIVE setters for the two published fields MainDirector::GetLivePlayerCarIndex reads
    // (see the header). Same write-lock assert as every other mutator in this TU.
    void InputBuffer::SetPlayerCarIndex(EActiveRaceCarIndex leIndex)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mePlayerCarIndex = leIndex;
    }

    void InputBuffer::SetRaceCarInUse(u32 luIndex, bool lbInUse)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        CGS_ASSERT(luIndex < 8u, "Index < Number of bits");   // CgsBitArray.h:222
        if (lbInUse)
            mUsedRaceCars.SetBit(luIndex);
        else
            mUsedRaceCars.UnSetBit(luIndex);
    }

    void InputBuffer::SetShortcutMenuEvent(bool lbState)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbShortcutMenuState    = lbState;
        mbGotShortcutMenuEvent = true;
    }

    void InputBuffer::SetGotCrashNavShownEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotCrashNavShownEvent = true;
    }

    void InputBuffer::SetGotCrashNavHiddenEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotCrashNavHiddenEvent = true;
    }

    void InputBuffer::SetGotColourCalibrationShownEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotColourCalibrationShownEvent = true;
    }

    void InputBuffer::SetGotColourCalibrationHiddenEvent()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mbGotColourCalibrationHiddenEvent = true;
    }

    // ---- control-input accessors (read/write) -----------------------------------------------
    // The ControlInput sub-object occupies the head of the committed mMidInterfaceBlock span
    // @0x3340; both overloads `return &mControlInput` (== this+0x3340), differing only in which
    // lock bit they assert.

    // X360 0x82206C50 (BrnDirectorModuleIO.h:583): read-lock; return &mControlInput (this+0x3340).
    const ControlInput* InputBuffer::GetControll() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<const ControlInput*>(mMidInterfaceBlock);
    }

    // X360 0x823B2740 (BrnDirectorModuleIO.h:574): write-lock; return &mControlInput (this+0x3340).
    // Non-const (write-side) overload of GetControll(): tests bit 3 (write-lock), unlike the const
    // read-lock overload above.
    ControlInput* InputBuffer::GetControll()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<ControlInput*>(mMidInterfaceBlock);
    }
}
}
