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
        // ⚠ REBASED 2026-08-06 (big-five #2 wave): mVehicleInputInterface is the REAL
        // Vehicle::VehicleInputInterface now (see the header banner), and its host sizeof
        // exceeds the 142176-byte console span, so every member AFTER it drifts by one uniform
        // host constant (the interface is alignas(16), 368 + sizeof is 16-aligned, and the
        // console seat 142544 is 16-aligned too -- the drift preserves every relative spacing).
        // The pre-drift seats stay absolute pins; the post-drift seats are pinned RELATIVE
        // (console offset + KU_DRIFT), which still fails on any wrong pad run -- the gate keeps
        // its teeth.
        static_assert(offsetof(InputBuffer, mCameraInput)                  == 16,     "mCameraInput @16");
        static_assert(offsetof(InputBuffer, mVehicleInputInterface)       == 368,    "mVehicleInputInterface @368");
        {
            constexpr size_t KU_DRIFT =
                (368 + sizeof(VehicleInputInterfaceStorage)) - 142544;   // host growth of the vehicle-input span
            static_assert(sizeof(VehicleInputInterfaceStorage) >= 142176,
                          "vehicle-input span smaller than the console span -- retype regressed");
            static_assert(offsetof(InputBuffer, mVehicleDriverInterface)       == 142544 + KU_DRIFT, "mVehicleDriverInterface @142544+D");
            // ⭐ 2026-08-09 (feed wave): mVehicleDriverInterface is the REAL
            // Vehicle::VehicleDriverInputInterface now. Unlike the vehicle-input span it does NOT
            // grow on the host -- every member is pointer-free -- so it must land EXACTLY on the
            // console span or the members below it would need a second drift constant. Pin it.
            static_assert(sizeof(VehicleDriverInputInterfaceStorage) == 147840 - 142544,
                          "vehicle-driver span must equal the console 5296 (host type is pointer-free)");
            static_assert(offsetof(InputBuffer, mVehicleEffectsInputInterface) == 147840 + KU_DRIFT, "mVehicleEffectsInputInterface @147840+D");
            // ⭐ 2026-08-10 (pre-physics bridge wave): mVehicleEffectsInputInterface is the REAL
            // Vehicle::VehicleEffectsInputInterface now (it was a 1-byte span the pre-physics
            // bridge Appends up to 1792 bytes into -- see the header). Like the driver interface
            // and unlike the vehicle-input span it does NOT grow on the host, so it must land
            // EXACTLY on the console span or every member below would need another drift. Pin it.
            static_assert(sizeof(VehicleEffectsInputInterfaceStorage) == 149632 - 147840,
                          "vehicle-effects span must equal the console 1792 (both queue headers are 16 on both targets)");
            static_assert(offsetof(InputBuffer, mRCEntityOutputInterface)      == 149632 + KU_DRIFT, "mRCEntityOutputInterface @149632+D");
            // ⭐ 2026-08-10 (create-path wave): mRCEntityOutputInterface is the REAL
            // BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface now, and like
            // the vehicle-input and prop-input spans it GROWS on the host (its eight
            // CgsResource::ResourceHandle slots and the WorldMap2D carry x64-widened pointers), so
            // every member below it takes a THIRD uniform drift. Computing it from sizeof keeps the
            // gate's teeth: any wrong pad run still fails.
            // ⚠️ THE OLD 0x28F0 OPAQUE SPAN WAS A LATENT TRUNCATION, not a neutral placeholder --
            // WorldBridgeEntityModulesToPhysics.cpp memcpy's sizeof(RCEntityOutputInterfaceStorage)
            // bytes out of a host source object that is LARGER than that, so the copy stopped
            // short of the tail members. Its own banner said so and said "retype the seat when a
            // consumer needs it"; PhysicsModule::PostSceneUpdate is that consumer as of today, and
            // the three members it reads (mePlayerActiveRaceCarIndex, mbIsPlayerCarActive,
            // maRaceCarStates[player].mEntityId) are only reachable by NAME once the seat is typed.
            constexpr size_t KU_RCENTITY_DRIFT =
                sizeof(RCEntityOutputInterfaceStorage) - (160112 - 149632);  // host growth of the RCEntity span
            static_assert(sizeof(RCEntityOutputInterfaceStorage) >= 160112 - 149632,
                          "RCEntity span smaller than the console span -- retype regressed");
            static_assert(offsetof(InputBuffer, mPotentialContactQueue)        == 160208 + KU_DRIFT + KU_RCENTITY_DRIFT, "mPotentialContactQueue @160208+D+RD (0x8259FB40)");
            static_assert(offsetof(InputBuffer, mOverlapPairsQueue)            == 324064 + KU_DRIFT + KU_RCENTITY_DRIFT, "mOverlapPairsQueue @324064+D+RD (0x8259FBE8)");
            static_assert(offsetof(InputBuffer, mTimerInterface)               == 327152 + KU_DRIFT + KU_RCENTITY_DRIFT, "mTimerInterface @327152+D+RD");
            static_assert(offsetof(InputBuffer, mSolverMaxIterations)          == 327200 + KU_DRIFT + KU_RCENTITY_DRIFT, "mSolverMaxIterations @327200+D+RD");
            static_assert(offsetof(InputBuffer, mPropManagerInputInterface)    == 327216 + KU_DRIFT + KU_RCENTITY_DRIFT, "mPropManagerInputInterface @327216+D+RD");
            // ⭐ 2026-08-10 (root-cause wave): mPropManagerInputInterface is the REAL
            // Props::PropInputInterface now, and -- unlike the driver interface -- it DOES grow
            // on the host (four embedded event queues with an 8-byte mpEvents, plus a
            // two-pointer ResourceHandle), so mGameActionQueue takes a SECOND uniform drift.
            // Computing it from sizeof keeps the gate's teeth: any wrong pad run still fails.
            {
                constexpr size_t KU_PROP_DRIFT =
                    sizeof(PropInputInterfaceStorage) - (338496 - 327216);  // host growth of the prop-input span
                static_assert(sizeof(PropInputInterfaceStorage) >= 338496 - 327216,
                              "prop-input span smaller than the console span -- retype regressed");
                static_assert(offsetof(InputBuffer, mGameActionQueue)
                                  == 338496 + KU_DRIFT + KU_RCENTITY_DRIFT + KU_PROP_DRIFT,
                              "mGameActionQueue @338496+D+RD+PD");
            }
        }
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
    // ⛔⛔ 2026-08-09 (feed wave) -- THE BODY ABOVE WAS A TWO-LINE PARTIAL AND IT COST 913
    // ASSERTS THE MOMENT THE INPUT FEED LANDED.
    // The console body (X360 0x825ABA18) is SIXTY-ONE instructions and constructs FIFTEEN
    // members; the 2026-08-01 wave added only the one member it needed. The first frame
    // WorldModule::BridgeInputToPhysicsModule @0x827AB830 ran, VehicleDriverInputInterface::
    // Append hit an unconstructed VariableEventQueue<5040,16> and fired "Not Constructed"
    // (CgsVariableEventQueue.h:759) every frame for the whole session.
    // Console call order, with each member's console seat:
    //   status = 1
    //   +0x00170  Vehicle::VehicleInputInterface::Construct          -> mVehicleInputInterface
    //   +0x22CD0  Vehicle::VehicleDriverInputInterface::Construct    -> mVehicleDriverInterface
    //   +0x27170  Vehicle::CreateWorldEvent<1>::Construct            -> mCreateWorldEventQueue
    //   +0x271D0  SceneManagerIO::PotentialContact<2048>::Construct  -> mPotentialContactQueue
    //   +0x24180  Vehicle::CreateAirRamEvent<20>::Construct    ) the two queues inside
    //   +0x24690  Vehicle::CreateSpinEvent<10>::Construct      ) mVehicleEffectsInputInterface
    //             + zero stores at +0x24188 / +0x24698
    //   +0x24880  RCEntityActiveRaceCarOutputInterface::Clear        -> mRCEntityOutputInterface
    //   +0x4FDF0  CgsSystem::TimerStatusInterface::Clear             -> mTimerInterface
    //   +0x00010  BrnDirector::Camera::Camera::Construct             -> mCameraInput
    //   +0x4FE30  Props::AddPhysicalPropEvent<50>::Construct   ) the four queues inside
    //   +0x50D90  Props::RemovePhysicalPropEvent<300>::Construct )  mPropManagerInputInterface
    //   +0x526FC  Props::RemovePhysicalPartEvent<100>::Construct )  + five zero stores
    //   +0x50DE0  Props::AddPhysicalPartEvent<50>::Construct    )
    //   +0x52A40  VariableEventQueue<13312,16>::Construct            -> mGameActionQueue
    //   +0x4F1E0  SceneManagerIO::OutOverlapPair<128>::Construct     -> mOverlapPairsQueue
    //   +0x4FE20  mSolverMaxIterations = 0
    //
    // ⚠️ FOUR of those calls CANNOT be emitted yet and are NOT faked: mCreateWorldEventQueue is
    // still a 96-byte pad, and mVehicleEffectsInputInterface / mRCEntityOutputInterface /
    // mCameraInput are still correctly-sized opaque *Storage spans with no members to
    // construct. They are listed above by console seat so the next wave that retypes any of
    // them knows exactly which call to restore -- and any consumer that reaches one of those
    // spans will fire the same loud "Not Constructed" this one did, by design.
    // ⭐ 2026-08-10 (root-cause wave): the PROP leg is off that blocked list. The four console
    // calls at +0x4FE30 / +0x51D90 / +0x526FC / +0x50DE0 (and their five zero stores) are
    // exactly PropInputInterface::Construct, now that the member holds the real type -- see
    // that body for the asm. Three committed bridges Append into this member every frame.
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mVehicleInputInterface.Construct();      // +0x00170
        mVehicleDriverInterface.Construct();     // +0x22CD0
        // ⭐ 2026-08-10 (pre-physics bridge wave): now that the effects seat is the REAL type it
        // can -- and MUST -- be Constructed. It was omitted only because a 1-byte span has no
        // Construct; leaving it out now would hand the pre-physics bridge two event queues with
        // a NULL mpEvents, which is the "mpEvents != NULL" + "Reached Max length" death the
        // race-car pre-physics buffer already suffered once.
        mVehicleEffectsInputInterface.Construct(); // +0x24180
        mPotentialContactQueue.Construct();      // +0x271D0
        mTimerInterface.Clear();                 // +0x4FDF0
        mPropManagerInputInterface.Construct();  // +0x4FE30 (the four prop queues + the flag)
        mGameActionQueue.Construct();            // +0x52A40
        mOverlapPairsQueue.Construct();          // +0x4F1E0
        mSolverMaxIterations = 0;                // +0x4FE20
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
