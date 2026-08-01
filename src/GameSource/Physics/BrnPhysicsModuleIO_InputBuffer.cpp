#include "GameSource/Physics/BrnPhysicsModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memcpy (models the X360 out-of-line operator= / XMemCpy copies)

// BrnPhysics::PhysicsModuleIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 12 X360-emitted InputBuffer accessors:
//
//   GetCameraInput() const              @ 0x8259F7F8 read  (bit 4) -> +16     (DWARF :272)
//   GetVehicleInputInterface() const    @ 0x8259F8A0 read  (bit 4) -> +368    (DWARF :275)
//   GetVehicleInputInterface()          @ 0x8279ED28 write (bit 3) -> +368    (DWARF :276)
//   GetVehicleEffectsInputInterface()   @ 0x8279EE78 write (bit 3) -> +147840 (DWARF :282)
//   GetTimerInterface() const           @ 0x8259FC90 read  (bit 4) -> +327152 (DWARF :295)
//   GetSolverMaxIterations() const      @ 0x8259FD38 read  (bit 4) -> +327200 (DWARF :298)
//   GetPropManagerInputInterface() const@ 0x8259FDE0 read  (bit 4) -> +327216 (DWARF :301)
//   GetPropManagerInputInterface()      @ 0x8279F2F8 write (bit 3) -> +327216 (DWARF :302)
//   SetCameraInput()                    @ 0x827A9D30 write (bit 3) -> +16     (DWARF :273)
//   SetRCEntityOutputInterface()        @ 0x8279EF20 write (bit 3) -> +149632 (DWARF :285)
//   SetTimerInterface()                 @ 0x8279F128 write (bit 3) -> +327152 (DWARF :296)
//   SetSolverMaxIterations()            @ 0x8279F240 write (bit 3) -> +327200 (DWARF :299)
//
// The const (read) handles test the read-lock bit (status>>4 &1); the non-const (write) handles
// + setters test the write-lock bit (status>>3 &1) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). Lock strings carry the trailing \n per
// X360 rodata (aNotLockedForRe / aNotLockedForWr).
//
// The four remaining bare-group InputBuffer accessors (GetVehicleDriverInterface,
// GetVehicleEffectsInputInterface const, GetGameActionQueue const/non-const) live in the
// sibling BrnPhysicsModuleIO_InputBuffer_Accessors.cpp.

namespace BrnPhysics
{
namespace PhysicsModuleIO
{
    void InputBuffer::_AssertLayout()
    {
        // All InputBuffer members are pointer-free opaque byte storages (or a plain u32), so every
        // X360-attested member offset is host-reproducible and assertable.
        static_assert(offsetof(InputBuffer, mCameraInput)                  == 16,     "mCameraInput @16");
        static_assert(offsetof(InputBuffer, mVehicleInputInterface)       == 368,    "mVehicleInputInterface @368");
        static_assert(offsetof(InputBuffer, mVehicleDriverInterface)      == 142544, "mVehicleDriverInterface @142544");
        static_assert(offsetof(InputBuffer, mVehicleEffectsInputInterface)== 147840, "mVehicleEffectsInputInterface @147840");
        static_assert(offsetof(InputBuffer, mRCEntityOutputInterface)     == 149632, "mRCEntityOutputInterface @149632");
        static_assert(offsetof(InputBuffer, mTimerInterface)              == 327152, "mTimerInterface @327152");
        static_assert(offsetof(InputBuffer, mSolverMaxIterations)         == 327200, "mSolverMaxIterations @327200");
        static_assert(offsetof(InputBuffer, mPropManagerInputInterface)   == 327216, "mPropManagerInputInterface @327216");
        static_assert(offsetof(InputBuffer, mGameActionQueue)             == 338496, "mGameActionQueue @338496");
        // The game-action queue member must be the real 13328-byte queue, not a stand-in: it is
        // the LAST member and WorldModule::BridgeActionsToPhysicsModule AddEvents into it.
        static_assert(sizeof(InputBuffer::GameActionQueueStorage) == 13328,
                      "PhysicsModuleIO::InputBuffer::mGameActionQueue must be VariableEventQueue<13312,16> (13328 bytes)");
    }

    // ⛔ 2026-08-01 (BridgeGameStateToWorld wave). The X360 CreateIOBuffer<T> stack template runs
    // T::Construct after the alloc; the PC template placement-news only, so WorldModule::Update
    // calls Construct explicitly -- and until now that resolved to the base
    // CgsModule::IOBuffer::Construct, which raises the status byte and nothing else. The embedded
    // game-action queue was therefore never Constructed, and the first
    // BridgeActionsToPhysicsModule AddEvent fired "Not Constructed"
    // (CgsVariableEventQueue.h:454 / :728). Measured live the moment BridgeGameStateToWorld
    // started delivering game actions to the world.
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mGameActionQueue.Construct();
    }

    // ---- getters (read-lock: status bit 4) ------------------------------------------

    // X360 0x8259F7F8 (DWARF :272): read-lock; return &mCameraInput (this+16).
    const InputBuffer::CameraStorage* InputBuffer::GetCameraInput() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mCameraInput;
    }

    // X360 0x8259F8A0 (DWARF :275): read-lock; return &mVehicleInputInterface (this+368).
    const InputBuffer::VehicleInputInterfaceStorage* InputBuffer::GetVehicleInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mVehicleInputInterface;
    }

    // X360 0x8259FC90 (DWARF :295): read-lock; return &mTimerInterface (this+327152).
    const InputBuffer::TimerStatusInterfaceStorage* InputBuffer::GetTimerInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mTimerInterface;
    }

    // X360 0x8259FD38 (DWARF :298): read-lock; return &mSolverMaxIterations (this+327200).
    const u32* InputBuffer::GetSolverMaxIterations() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSolverMaxIterations;
    }

    // X360 0x8259FDE0 (DWARF :301): read-lock; return &mPropManagerInputInterface (this+327216).
    const InputBuffer::PropInputInterfaceStorage* InputBuffer::GetPropManagerInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPropManagerInputInterface;
    }

    // ---- getters (write-lock: status bit 3 -- mutable overloads test the WRITE bit) --

    // X360 0x8279ED28 (DWARF :276): write-lock; return &mVehicleInputInterface (this+368).
    InputBuffer::VehicleInputInterfaceStorage* InputBuffer::GetVehicleInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleInputInterface;
    }

    // X360 0x8279EE78 (DWARF :282): write-lock; return &mVehicleEffectsInputInterface (this+147840).
    InputBuffer::VehicleEffectsInputInterfaceStorage* InputBuffer::GetVehicleEffectsInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mVehicleEffectsInputInterface;
    }

    // X360 0x8279F2F8 (DWARF :302): write-lock; return &mPropManagerInputInterface (this+327216).
    InputBuffer::PropInputInterfaceStorage* InputBuffer::GetPropManagerInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropManagerInputInterface;
    }

    // ---- setters (write-lock: status bit 3) -----------------------------------------

    // X360 0x827A9D30 (DWARF :273): write-lock; mCameraInput = *lpCamera. The X360 build calls
    // Camera::operator= out-of-line copying the 0x160 Camera extent; modelled as an image copy.
    void InputBuffer::SetCameraInput(const CameraStorage* lpCamera)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mCameraInput, lpCamera, sizeof(CameraStorage));
    }

    // X360 0x8279EF20 (DWARF :285): write-lock; XMemCpy 0x28F0 (10480) bytes into
    // mRCEntityOutputInterface (this+149632).
    void InputBuffer::SetRCEntityOutputInterface(const RCEntityOutputInterfaceStorage* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mRCEntityOutputInterface, lpInterface, sizeof(RCEntityOutputInterfaceStorage));
    }

    // X360 0x8279F128 (DWARF :296): write-lock; mTimerInterface = *lpTimer. The X360 build expands
    // operator= as two contiguous 24-byte field runs (+0x00, +0x18); modelled as a single 0x30-byte
    // struct-image copy.
    void InputBuffer::SetTimerInterface(const TimerStatusInterfaceStorage* lpTimer)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mTimerInterface, lpTimer, sizeof(TimerStatusInterfaceStorage));
    }

    // X360 0x8279F240 (DWARF :299): write-lock; store one word into mSolverMaxIterations (this+327200).
    void InputBuffer::SetSolverMaxIterations(const u32* lpValue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mSolverMaxIterations = *lpValue;
    }
}
}
