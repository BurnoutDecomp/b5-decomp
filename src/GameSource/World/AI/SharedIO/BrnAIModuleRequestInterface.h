#pragma once

// BrnAI::AIModuleIO -- the reset-on-track REQUEST side of the AI module IO. A consumer
// (e.g. the race-car entity module) pushes ResetOnTrackRequest events here; the AI module
// drains them. Member layout/types recovered from the DecFIGS DWARF
// (BrnAIModuleRequestInterface.h) and confirmed against the X360 element/queue
// constructors:
//   - ResetOnTrackRequest::Construct                  @0x822A0438 (field stores @+0/+4/+8/+0xC)
//   - EventQueue<ResetOnTrackRequest,128>::Construct  @0x822E2870 (maEvents @+0xC: 12-byte base,
//                                                                  element align 4)
//
// This is the type's own ledger home: it now carries the real EventQueue member instead of
// the earlier NOMINAL reserved blob. AIModuleRequestInterface is embedded BY VALUE in
// RaceCarEntityModuleIO::OutputBuffer_PostScene; that buffer only ever returns &member, so
// growing the size here is offset-safe for consumers (semantic-parity target).

#include "types.hpp"                                         // s32/f32
#include "GameShared/GameClasses/Module/CgsEventQueue.h"     // CgsModule::EventQueue<T,N>
#include "GameSource/BurnoutConstants.h"                     // EGlobalRaceCarIndex
#include "GameSource/World/AI/BrnAISharedConstants.h"        // BrnAI::EResetType

namespace BrnAI
{
    namespace AIModuleIO
    {
        // DWARF BrnAIModuleRequestInterface.h:31.
        const s32 KI_AI_DETERMINE_RESET_POS_QUEUE_SIZE = 128;

        // A queued request to reset an AI race car back onto the track. DWARF
        // BrnAIModuleRequestInterface.h:49 declares it `: public Event` (the empty
        // CgsModule event base, EBO -> zero bytes), with fields in this exact order:
        //   meGlobalRaceCarIndex @+0x0, mfResetSpeed @+0x4, mfResetDistance @+0x8,
        //   meResetType @+0xC  (X360 ResetOnTrackRequest::Construct @0x822A0438 stores
        //   the index, two f32 speeds and the reset-type word at those displacements;
        //   the index is asserted 0 <= index < E_GLOBAL_RACE_CAR_INDEX_COUNT). sizeof == 16.
        struct ResetOnTrackRequest
        {
            // X360 @0x822A0438: stw idx@+0, stfs speed@+4, stfs dist@+8, stw type@+0xC.
            void Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex,
                           f32 lfResetSpeed,
                           f32 lfResetDistance,
                           BrnAI::EResetType leResetType);

            f32               GetResetSpeed() const       { return mfResetSpeed; }
            f32               GetResetDistance() const     { return mfResetDistance; }
            EGlobalRaceCarIndex GetGlobalRaceCarIndex() const { return meGlobalRaceCarIndex; }
            BrnAI::EResetType GetResetType() const         { return meResetType; }

        private:
            EGlobalRaceCarIndex meGlobalRaceCarIndex;   // +0x0
            f32                 mfResetSpeed;            // +0x4
            f32                 mfResetDistance;         // +0x8
            BrnAI::EResetType   meResetType;             // +0xC
        };

        // DWARF BrnAIModuleRequestInterface.h:89. Single-member wrapper around the
        // 128-deep reset-on-track request queue.
        struct AIModuleRequestInterface
        {
            typedef CgsModule::EventQueue<ResetOnTrackRequest, KI_AI_DETERMINE_RESET_POS_QUEUE_SIZE>
                    ResetOnTrackRequestQueue;

            // Declared-only: these belong to AIModuleRequestInterface's own behaviour TUs,
            // not this element/queue-instance group. Consumers only call
            // GetResetOnTrackRequestQueue()/&member, so declarations suffice for the gate.
            void Append(const AIModuleRequestInterface*);
            void Clear();
            void RequestResetOnTrack(EGlobalRaceCarIndex, f32, f32, BrnAI::EResetType);
            const ResetOnTrackRequestQueue* GetResetOnTrackRequestQueue() const
            {
                return &mResetOnTrackRequestQueue;
            }

        private:
            ResetOnTrackRequestQueue mResetOnTrackRequestQueue;   // DWARF :109
        };
    }
}
