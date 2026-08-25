#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleIO.h"

#include <cstddef>   // offsetof
#include <cstring>   // memcpy (the blind-copy setters model the Xbox XMemCpy intrinsic)

// class:BrnWorld::CrashIO group -- the crash module's input-buffer accessors (plus the
// post-physics output buffer's read view). Reconstructed from BURNOUT_X360_ARTIST.XEX + the
// DecFIGS DWARF. Every accessor tests a lock bit on the IOBuffer status byte:
//   read-lock  (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 == IsBufferLockedForReading())
//              -> on failure streams "Not locked for reading\n";
//   write-lock (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting())
//              -> on failure streams "Not locked for writing\n".
// The streamed-message asserts map to the house CGS_ASSERT (the trailing "\n" is dropped from
// the stringized condition).
//
// InputBuffer_PostPhysics / InputBuffer_PreScene are now the DWARF-authoritative full-member
// buffers (see BrnCrashModuleIO.h). Their read getters return &member (const, read-lock) and
// their setters copy a source view into the matching member (write-lock): embedded interface
// members are assigned via the member type's own operator= (per-instance queue copy, not a raw
// memberwise copy); the blind-copied opaque members are XMemCpy'd. _AssertLayout pins only the
// pointer-free prefix offset of each buffer (the embedded EventQueue-bearing interfaces widen on
// this PC build, so the later members' absolute offsets are host-unstable and documented in the
// header comments instead).

namespace BrnWorld
{
namespace CrashIO
{
    // ====================================================================================
    // InputBuffer_PreScene (DWARF BrnCrashModuleIO.h:63)
    // ====================================================================================
    void InputBuffer_PreScene::_AssertLayout()
    {
        // Pointer-free prefix only: mTimerStatusInterface is 48 bytes of PODs (no host widening),
        // landing at +0x4 after the 1-byte IOBuffer status. The following members embed/precede
        // host-widening EventQueue storage, so their X360 offsets are documented (header) not pinned.
        static_assert(offsetof(InputBuffer_PreScene, mTimerStatusInterface) == 0x4,
                      "InputBuffer_PreScene::mTimerStatusInterface @0x4");
    }

    // 0x827BB288 (DWARF :75) -- read-lock tripwire; returns &mTimerStatusInterface (X360 this+0x4).
    const CgsSystem::TimerStatusInterface*
    InputBuffer_PreScene::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTimerStatusInterface;
    }

    // 0x827BB330 (DWARF :78) -- read-lock tripwire; returns &mNetworkInputInterface (X360 this+0x40).
    // Truncated X360 symbol stem 'GetNetw'. Caller CrashModule::HandleNetworkCrashingTraffic.
    const NetworkInputInterface*
    InputBuffer_PreScene::GetNetworkInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mNetworkInputInterface;
    }

    // 0x827BB3D8 (DWARF :81, ex-GetReadInterface) -- read-lock tripwire; returns
    // &mVehicleDriverInterface (X360 this+0x3CD0). Caller CrashModule::ResetCrashedNetworkRaceCars.
    const InputBuffer_PreScene::VehicleDriverInterfaceStorage*
    InputBuffer_PreScene::GetVehicleDriverInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleDriverInterface;
    }

    // 0x827BB480 (DWARF :84) -- read-lock tripwire; returns &mActiveRaceCarInterface
    // (X360 this+0x5180). Callers: CrashModule::PreSceneUpdate @0x827D3A90/@0x827D3AEC,
    // ::TickCrashes @0x827C6648/@0x827C66EC, ::ClearupCrashes.
    // [crash exit 2026-08-25] the return type is the real interface, not the ex-blob: see the
    // member's banner in BrnCrashModuleIO.h.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    InputBuffer_PreScene::GetActiveRaceCarInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mActiveRaceCarInterface;
    }

    // 0x827A20A8 (DWARF :76) -- write-lock tripwire; memberwise-copy the 48-byte timer-status view
    // into mTimerStatusInterface (X360 this+0x4). The X360 inlines the two-record field-by-field
    // copy; reproduced via the authoritative CgsSystem::TimerStatusInterface::operator= (same
    // stores). Caller WorldModule::BridgeInputToCrashModule.
    void InputBuffer_PreScene::SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mTimerStatusInterface = *lpInterface;
    }

    // 0x827ACF88 (DWARF :79) -- write-lock tripwire; assign the source network-input view into
    // mNetworkInputInterface (X360 this+0x40) via NetworkInputInterface::operator= (per-queue copy).
    // Caller WorldModule::BridgeInputToCrashModule.
    void InputBuffer_PreScene::SetNetworkInputInterface(const NetworkInputInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mNetworkInputInterface = *lpInterface;
    }

    // 0x827A21B8 (DWARF :82) -- write-lock tripwire; blind-copy the 0x14B0-byte vehicle-driver view
    // into mVehicleDriverInterface (X360 this+0x3CD0). DWARF VehicleDriverInterface ==
    // VehicleDriverInputInterface (foreign; own home elsewhere). Caller WorldModule::BridgeInputToCrashModule.
    void InputBuffer_PreScene::SetVehicleDriverInterface(const VehicleDriverInterfaceStorage* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        memcpy(&mVehicleDriverInterface, lpInterface, sizeof(mVehicleDriverInterface));
    }

    // 0x827A2270 (DWARF :85) -- write-lock tripwire; blind-copy the 0x28F0-byte active-race-car view
    // into mActiveRaceCarInterface (X360 this+0x5180). DWARF ActiveRaceCarInterface ==
    // RCEntityActiveRaceCarOutputInterface (declared param type kept; the member is opaque storage).
    // Caller WorldModule::BridgeEntityModulesToCrashModule_PreScene @0x827A50D4.
    void InputBuffer_PreScene::SetActiveRaceCarInterface(
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        // [crash exit 2026-08-25] was `memcpy(&member, lpInterface, sizeof(member))` over a
        // 10480-byte console-sized blob. The member is the real type now, so the copy is the
        // interface's own operator= -- which is what the X360 inlines here (the same field-by-field
        // walk BrnRCEntityActiveRaceCarOutputInterface.cpp:105 reconstructs).
        mActiveRaceCarInterface = *lpInterface;
    }

    // 0x827A2328 (DWARF :88) -- write-lock tripwire; blind-copy the 0x3410-byte game-action queue
    // into mGameActionQueue (X360 this+0x7A70). Caller WorldModule::BridgeInputToCrashModule.
    void InputBuffer_PreScene::SetGameActionQueue(const GameActionQueueStorage* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        memcpy(&mGameActionQueue, lpQueue, sizeof(mGameActionQueue));
    }

    // ====================================================================================
    // InputBuffer_PostPhysics (DWARF BrnCrashModuleIO.h:156)
    // ====================================================================================
    void InputBuffer_PostPhysics::_AssertLayout()
    {
        // Pointer-free prefix only: mTrafficInputInterface is align-8 (no vector member) and lands
        // at +0x8 after the 1-byte IOBuffer status. mVehicleOutputInterface (@+0xDA0) and
        // mVehicleManagerOutputInterface (@+0x79B0) sit past host-widening EventQueue storage, so
        // their X360 offsets are documented (header) not pinned.
        static_assert(offsetof(InputBuffer_PostPhysics, mTrafficInputInterface) == 0x8,
                      "InputBuffer_PostPhysics::mTrafficInputInterface @0x8");
    }

    // 0x827BB7C8 (DWARF :168) -- read-lock tripwire; returns &mTrafficInputInterface (X360 this+0x8).
    // Truncated X360 symbol 'GetT'.
    const TrafficInputInterface*
    InputBuffer_PostPhysics::GetTrafficInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mTrafficInputInterface;
    }

    // 0x827BB870 (DWARF :171) -- read-lock tripwire; returns &mVehicleOutputInterface (X360 this+0xDA0).
    // Truncated X360 symbol 'G'.
    const InputBuffer_PostPhysics::VehicleOutputInterface*
    InputBuffer_PostPhysics::GetVehicleOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleOutputInterface;
    }

    // 0x827BB918 (DWARF :174, ex-GetReadInterface) -- read-lock tripwire; returns
    // &mVehicleManagerOutputInterface (X360 this+0x79B0). Callers
    // CrashModule::ProcessCrashedRaceCarEvents / ProcessSlammedTrafficEvents / PostPhysicsUpdate.
    const InputBuffer_PostPhysics::VehicleManagerOutputInterface*
    InputBuffer_PostPhysics::GetVehicleManagerOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleManagerOutputInterface;
    }

    // 0x827AD038 (DWARF :169) -- write-lock tripwire; copies source into mTrafficInputInterface
    // (X360 this+0x8) via TrafficInputInterface::operator= (per-queue copy + bit-mask copy).
    void InputBuffer_PostPhysics::SetTrafficInputInterface(const TrafficInputInterface* lpTrafficInputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mTrafficInputInterface = *lpTrafficInputInterface;
    }

    // 0x827AA388 (DWARF :172) -- write-lock tripwire; copies source into mVehicleOutputInterface
    // (X360 this+0xDA0) via VehicleOutputInterface::operator=.
    void InputBuffer_PostPhysics::SetVehicleOutputInterface(const VehicleOutputInterface* lpVehicleOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mVehicleOutputInterface = *lpVehicleOutputInterface;
    }

    // 0x827AA438 (DWARF :175) -- write-lock tripwire; copies source into mVehicleManagerOutputInterface
    // (X360 this+0x79B0) via VehicleManagerOutputInterface::operator=.
    void InputBuffer_PostPhysics::SetVehicleManagerOutputInterface(const VehicleManagerOutputInterface* lpVehicleManagerOutputInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mVehicleManagerOutputInterface = *lpVehicleManagerOutputInterface;
    }

    // ====================================================================================
    // InputBuffer_PostPhysics::Construct   X360 0x827CEA58
    //
    //   *this = 1                                    -- IOBuffer::Construct (the status bit)
    //   TrafficInputInterface::Construct(this + 8)                       -- mTrafficInputInterface
    //   PhysicalTrafficState_20_::Construct(this + 13248)   \
    //   ImpactEvent_16_::Construct(this + 12464)             |  all four inside
    //   VariableEventQueue<1536,16>::Construct(this + 29584) |  mVehicleOutputInterface (+0xDA0),
    //   the +3488 qword zero and the five +31136..+31140 bytes /  == its own Construct
    //   VehicleManagerOutputInterface::Construct(this + 31152)  -- mVehicleManagerOutputInterface
    //
    // Reproduced BY NAME: both embedded interfaces already own a Construct() that does exactly
    // the console's inlined work over their own members (VehicleOutputInterface::Construct is
    // itself X360-attested as the inlined body of PhysicsModuleIO::OutputBuffer::Construct over
    // the same seat, and VehicleManagerOutputInterface::Construct is the out-of-line 0x822E6790).
    // See the header banner for why this being absent was fatal rather than merely incomplete.
    // ====================================================================================
    void InputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mTrafficInputInterface.Construct();
        mVehicleOutputInterface.Construct();
        mVehicleManagerOutputInterface.Construct();
    }

    // ====================================================================================
    // InputBuffer_HandleGameActions (DWARF BrnCrashModuleIO.h:87) -- single read accessor,
    // still modelled as an opaque read view (no setters/typed members recovered by this slice).
    // ====================================================================================
    void InputBuffer_HandleGameActions::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_HandleGameActions, mReadInterface) == 0x7A70,
                      "InputBuffer_HandleGameActions::mReadInterface @0x7A70");
    }
    const InputBuffer_HandleGameActions::ReadInterfaceStorage*
    InputBuffer_HandleGameActions::GetReadInterface() const   // 0x827BB528
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mReadInterface;
    }

    // ====================================================================================
    // OutputBuffer_PostPhysics_ReadView (DWARF BrnCrashModuleIO.h:210)
    // ====================================================================================
    void OutputBuffer_PostPhysics_ReadView::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics_ReadView, mGameEventQueue) == 0x7A0,
                      "OutputBuffer_PostPhysics_ReadView::mGameEventQueue @0x7A0");
    }
    // PS3 DecFIGS: OutputBuffer_PostPhysics::GetGameEventQueue (BrnCrashModuleIO.h:210).
    const OutputBuffer_PostPhysics_ReadView::GameEventQueueStorage*
    OutputBuffer_PostPhysics_ReadView::GetGameEventQueue() const   // 0x827A2680
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameEventQueue;
    }
}
}
