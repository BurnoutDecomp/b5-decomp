#pragma once

// BrnAI::AIModuleIO -- the RESULT side of the AI module IO. The AI module publishes
// reset-on-track results (where it decided to put each car) and place-on-track requests
// here; consumers read them. Member layout/types recovered from the DecFIGS DWARF
// (BrnAIModuleResultInterface.h) and confirmed against the X360 queue constructors:
//   - EventQueue<ResetOnTrackResult,128>::Construct  @0x822E28E0 (maEvents @+0x10: 12-byte
//                                                                 base + 4 pad, element align 16)
//   - EventQueue<PlaceOnTrackRequest,128>::Construct @0x822E2950 (maEvents @+0x10, same shape)
// Both element structs embed two Vector3s (16-byte SIMD) so they are 16-byte aligned, which
// forces the queue's inline maEvents to +0x10 after the 12-byte base -- exactly what those
// constructors' `addi rN, r31, 0x10` shows.
//
// This is the type's own ledger home: it now carries the real EventQueue members instead of
// the earlier NOMINAL reserved blob. AIModuleResultInterface is embedded BY VALUE in
// RaceCarEntityModuleIO::InputBuffer_PrePhysics; that buffer only ever returns &member, so
// growing the size here is offset-safe for consumers (semantic-parity target).

#include "types.hpp"                                         // s32/f32
#include "BrnCommonTypes.h"                                  // Vector3 (rw::math::vpu::Vector3)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"     // CgsModule::EventQueue<T,N>
#include "GameSource/BurnoutConstants.h"                     // EGlobalRaceCarIndex

namespace BrnAI
{
    namespace AIModuleIO
    {
        // DWARF BrnAIModuleResultInterface.h:31.
        const s32 KI_RESET_POS_RESULT_QUEUE_SIZE = 128;

        // The result of resolving a reset-on-track request: the chosen position/direction,
        // success/failure, the car it applies to and the reset speed. DWARF
        // BrnAIModuleResultInterface.h:38 declares it `: public Event` (empty CgsModule event
        // base, EBO -> zero bytes). Field order/offsets:
        //   mResetPosition @+0x0 (V3), mResetDirection @+0x10 (V3), meState @+0x20,
        //   meGlobalRaceCarIndex @+0x24, mfResetSpeed @+0x28. 16-byte aligned; sizeof == 48.
        struct alignas(16) ResetOnTrackResult
        {
            // DWARF BrnAIModuleResultInterface.h:41.
            enum State
            {
                E_STATE_SUCCESS = 0,
                E_STATE_FAILURE = 1,
            };

            void Construct(State leState,
                           EGlobalRaceCarIndex leGlobalRaceCarIndex,
                           f32 lfResetSpeed,
                           Vector3 lResetPosition,
                           Vector3 lResetDirection);

            State               GetState() const               { return meState; }
            EGlobalRaceCarIndex GetGlobalRaceCarIndex() const   { return meGlobalRaceCarIndex; }
            f32                 GetResetSpeed() const           { return mfResetSpeed; }
            Vector3             GetResetPosition() const        { return mResetPosition; }
            Vector3             GetResetDirection() const       { return mResetDirection; }

        private:
            Vector3             mResetPosition;        // +0x00
            Vector3             mResetDirection;       // +0x10
            State               meState;               // +0x20
            EGlobalRaceCarIndex meGlobalRaceCarIndex;  // +0x24
            f32                 mfResetSpeed;           // +0x28
        };

        // A queued request to place a car at a specific on-track position/direction. DWARF
        // BrnAIModuleResultInterface.h:81 (`: public Event`, EBO). Field order/offsets:
        //   mResetPosition @+0x0 (V3), mResetDirection @+0x10 (V3), meGlobalRaceCarIndex @+0x20,
        //   mfResetSpeed @+0x24. 16-byte aligned; sizeof == 48.
        struct alignas(16) PlaceOnTrackRequest
        {
            void Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                           f32 lfResetSpeed,
                           Vector3 lResetPosition,
                           Vector3 lResetDirection);

            EGlobalRaceCarIndex GetGlobalRaceCarIndex() const   { return meGlobalRaceCarIndex; }
            f32                 GetResetSpeed() const           { return mfResetSpeed; }
            Vector3             GetResetPosition() const        { return mResetPosition; }
            Vector3             GetResetDirection() const       { return mResetDirection; }

        private:
            Vector3             mResetPosition;        // +0x00
            Vector3             mResetDirection;       // +0x10
            EGlobalRaceCarIndex meGlobalRaceCarIndex;  // +0x20
            f32                 mfResetSpeed;           // +0x24
        };

        // DWARF BrnAIModuleResultInterface.h:121. Holds the reset-on-track result queue and
        // the place-on-track request queue, in that member order (DWARF :148, :149).
        struct AIModuleResultInterface
        {
            typedef CgsModule::EventQueue<ResetOnTrackResult,  KI_RESET_POS_RESULT_QUEUE_SIZE>
                    ResetOnTrackResultQueue;
            typedef CgsModule::EventQueue<PlaceOnTrackRequest, KI_RESET_POS_RESULT_QUEUE_SIZE>
                    PlaceOnTrackRequestQueue;

            // Declared-only: behaviour belongs to AIModuleResultInterface's own TUs. Consumers
            // only call the Get*Queue accessors / &member, so declarations suffice for the gate.
            void Construct();
            void AddResetOnTrackResult(ResetOnTrackResult::State, EGlobalRaceCarIndex, f32,
                                       Vector3, Vector3);
            const ResetOnTrackResultQueue* GetResetOnTrackResultQueue() const
            {
                return &mResetPosResultEventQueue;
            }
            void AddPlaceOnTrackRequest(EGlobalRaceCarIndex, f32, Vector3, Vector3);
            const PlaceOnTrackRequestQueue* GetPlaceOnTrackRequestQueue() const
            {
                return &mPlaceOnTrackRequestQueue;
            }

        private:
            ResetOnTrackResultQueue  mResetPosResultEventQueue;   // DWARF :148
            PlaceOnTrackRequestQueue mPlaceOnTrackRequestQueue;   // DWARF :149
        };
    }
}
