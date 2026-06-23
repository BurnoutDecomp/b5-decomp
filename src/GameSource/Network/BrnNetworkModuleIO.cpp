// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModuleIO.cpp
// ============================================================================
// Bodies for the BrnNetwork::BrnNetworkModuleIO IO-aggregate TU: the three IOBuffer-derived
// payload buffers' lock-guarded Get* accessors and their two partial copy-assignments, plus the
// InGamePlayerStatusInterface / PlayerResultsInterface copy/element helpers.
//
// Each buffer accessor reproduces the X360 lock guard: read accessors assert
// IsBufferLockedForReading() (status bit 0x10 / eStatusLockedForRead), write accessors assert
// IsBufferLockedForWriting() (status bit 0x08 / eStatusLockedForWrite); the X360-baked
// "Not locked for reading/writing\n" StrStream message + d:\p4 path/line are discarded per
// project convention and replaced by CGS_ASSERT on the base predicate. The returned address is
// `this + X360 offset`, modelled as &offset-pinned-storage in BrnNetworkModuleIO.h.
//
// InGamePlayerStatusData::operator= and InGamePlayerStatusData::Clear live in the SharedIO .cpp
// (BrnNetworkModuleInGamePlayerStatusInterface.cpp) alongside the full struct definition.

#include "GameSource/Network/BrnNetworkModuleIO.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // full InGamePlayerStatusData/Interface
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"   // CgsModule::BaseEventQueue<T>::Append (operator= bodies)
#include <cstring>                                             // std::memcpy (OutputBuffer::operator= + the CompletedFburnChallengesData queue append) / std::strncpy (SetGameName)
//
// NOTE: the CompletedFburnChallengesData queue inside PostSimulationInputBuffer::operator= is
// appended with a hand-inlined memcpy (the exact lowering of CgsModule::BaseEventQueue<T>::Append)
// using the X360-proven 264-byte element stride, rather than instantiating
// BaseEventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData>. That element type
// lives in GameSource/GameState/BrnGameStateSharedIO.h, whose file-scope offsetof(CarScoreData,
// <private member>) static_asserts do not compile under MSVC /permissive- (a PRE-EXISTING defect in
// that GameState home -- the already-committed EventQueue_CompletedFburnChallengesData_7.cpp TU fails
// the same gate with the identical C2248). Including it here would inherit that unrelated breakage, so
// this network TU stays decoupled from it. Behaviour is byte-identical to the generic Append (the
// committed BaseEventQueue<CompletedFburnChallengesData>::Append @0x823C46A8 bakes the same 264-byte
// stride). Replace this hand-inlined append with the typed BaseEventQueue<...>::Append call once that
// GameState header's offsetof bug is fixed and the named member lands.

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ========================================================================
    // PreSimulationInputBuffer
    // ========================================================================

    // X360 0x82587AA8 (read-lock; "Not locked for reading", BrnNetworkModuleIO.h:507) -> &mTimerInterface @ this+8.
    const TimerStatusInterface* PreSimulationInputBuffer::GetTimerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const TimerStatusInterface*>(&mTimerInterfaceStorage);
    }

    // ========================================================================
    // PostSimulationInputBuffer -- lock-guarded accessors
    // ========================================================================

    // X360 0x8254EA28 (read, h:566) -> &member @ +54504.
    const u8* PostSimulationInputBuffer::GetVehicleOutputInterfaceRaw_54504() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMember_54504Storage[0];
    }

    // X360 0x823BB8D0 (write, h:574) -> &member @ +40300.
    u8* PostSimulationInputBuffer::GetMember_40300()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<u8*>(&mMember_40300Storage);
    }

    // X360 0x82587BF8 (read, h:603) -> &member @ +38304.
    const u8* PostSimulationInputBuffer::GetMember_38304() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMember_38304Storage[0];
    }

    // X360 0x82587CA0 (read, h:610) -> &member @ +38160.
    const u8* PostSimulationInputBuffer::GetMember_38160() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const u8*>(&mMember_38160Storage);
    }

    // X360 0x82587DF0 (read, h:631) -> &member @ +40240.
    const u8* PostSimulationInputBuffer::GetMember_40240() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const u8*>(&mMember_40240Storage);
    }

    // X360 0x8254EE18 (write, h:752) -> &member @ +180248.
    u8* PostSimulationInputBuffer::GetMember_180248()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMember_180248Storage[0];
    }

    // X360 0x8254ECC8 (read, h:687) -> const NetworkEventQueue* @ +72952 (queue start == storage start).
    const NetworkEventQueue* PostSimulationInputBuffer::GetNetworkEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkEventQueue*>(&mNetworkEventQueueStorage[0]);
    }

    // X360 0x823BBB00 (write, h:701) -> NetworkEventQueue* @ +73284 (queue body == storage +332).
    NetworkEventQueue* PostSimulationInputBuffer::GetNetworkEventQueueForWriting()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<NetworkEventQueue*>(&mNetworkEventQueueStorage[73284 - 72952]);
    }

    // X360 0x8254ED70 (read, h:708) -> const NetworkEventQueue* @ +73284 (const twin of the write accessor).
    const NetworkEventQueue* PostSimulationInputBuffer::GetNetworkEventQueueForWriting() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkEventQueue*>(&mNetworkEventQueueStorage[73284 - 72952]);
    }

    // ========================================================================
    // OutputBuffer -- lock-guarded accessors
    // ========================================================================

    // X360 0x823BC108 (read, h:834) -> const GuiEventQueueSmall* @ +16.
    const GuiEventQueueSmall* OutputBuffer::GetGuiEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const GuiEventQueueSmall*>(&mGuiEventQueueStorage);
    }

    // X360 0x823BC258 (read, h:869) -> const InGamePlayerStatusInterface* @ +162992.
    const InGamePlayerStatusInterface* OutputBuffer::GetInGamePlayerStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const InGamePlayerStatusInterface*>(&mInGamePlayerStatusInterfaceStorage);
    }

    // X360 0x823BC5A0 (read, h:964) -> const NetworkToGameStateInterface* @ +172376.
    const NetworkToGameStateInterface* OutputBuffer::GetNetworkToGameStateInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkToGameStateInterface*>(&mNetworkToGameStateInterfaceStorage);
    }

    // X360 0x823BBC58 (read, h:745) -> const StatsOutputInterface* @ +178692.
    const StatsOutputInterface* OutputBuffer::GetStatsOutputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const StatsOutputInterface*>(&mStatsOutputInterfaceStorage);
    }

    // X360 0x823BC3A8 (read, h:911) -> const NetworkToGuiInterface* @ +180588.
    const NetworkToGuiInterface* OutputBuffer::GetNetworkToGuiInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkToGuiInterface*>(&mNetworkToGuiInterfaceStorage);
    }

    // X360 0x823BC6F0 (read, h:1001) -> const NetworkEventQueue* @ +184080.
    const NetworkEventQueue* OutputBuffer::GetNetworkEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkEventQueue*>(&mNetworkEventQueueStorage);
    }

    // X360 0x8254F160 (write, h:994) -> NetworkEventQueue* @ +184080 (non-const twin of h:1001).
    NetworkEventQueue* OutputBuffer::GetNetworkEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<NetworkEventQueue*>(&mNetworkEventQueueStorage);
    }

    // ========================================================================
    // InGamePlayerStatusInterface helpers
    // ========================================================================

    // X360 0x8230FF60 (ledger stub "In").
    // Returns a writable pointer to the indexed player record. The X360 build asserts the index
    // is in [0, miNumPlayers) (h:256/257 of BrnNetworkModuleInGamePlayerStatusInterface.h), then
    // returns &maInGamePlayerData[liIndex] (element stride 312 == sizeof(InGamePlayerStatusData)).
    // The X360 file/line literals are dropped per project convention.
    InGamePlayerStatusData* InGamePlayerStatusInterface::GetPlayerStatusDataForWriting(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
        CGS_ASSERT(liIndex < miNumPlayers, "liIndex < miNumPlayers");
        return &maInGamePlayerData[liIndex];
    }

    // X360 0x8236B020 (ledger stub "InG").
    // Whole-interface copy assignment: copies the 8 InGamePlayerStatusData records via
    // InGamePlayerStatusData::operator= (0x823628C8, stride 312), then the 36-char game name
    // (+2496), then miNumPlayers (+2532) and mbLocalPlayerIsHost (+2536). The X360 also copies the
    // +2540 trailing pad word (inert alignment with no named member; left as default padding).
    InGamePlayerStatusInterface& InGamePlayerStatusInterface::operator=(const InGamePlayerStatusInterface& lOther)
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            maInGamePlayerData[liIndex] = lOther.maInGamePlayerData[liIndex]; // InGamePlayerStatusData::operator= (stride 312)
        }

        for (s32 liChar = 0; liChar < 36; ++liChar)
        {
            macGameName[liChar] = lOther.macGameName[liChar];   // +2496 (36 bytes)
        }

        miNumPlayers        = lOther.miNumPlayers;          // +2532
        mbLocalPlayerIsHost = lOther.mbLocalPlayerIsHost;   // +2536
        return *this;
    }

    // X360 0x82580F10 (ledger TU class:...::InGamePlayerStatusInterface, SetGameName).
    // Copies the game-name string into the interface's fixed 36-char buffer macGameName (+0x9C0
    // == +2496). The X360 first computes strlen(lpcName) inline and asserts it is < 36 (the
    // "String too long: <name>" assert at the shared CgsStringUtils.h site), then issues
    // strncpy(macGameName, lpcName, 36) -- the Count, Source and Dest are the asm-confirmed
    // r5=0x24(36) / r4=source / r3=this+0x9C0 register setup at 0x82580FF4. The streamed
    // assert message is reduced to CGS_ASSERT per project convention; behaviour (length guard
    // + bounded copy) is identical.
    void InGamePlayerStatusInterface::SetGameName(const char* lpcName)
    {
        CGS_ASSERT(std::strlen(lpcName) < 36, "String too long");
        std::strncpy(macGameName, lpcName, 36);
    }

    // ========================================================================
    // PlayerResultsInterface helper
    // ========================================================================

    // X360 0x823B8F68 (ledger stub "PlayerRe").
    // Whole-interface copy assignment: member-wise copy of the 8 PlayerResultsData records
    // (28-byte stride). The X360 body is the compiler's flat lowering of 8 inlined record copies;
    // PlayerResultsData is modelled opaquely in this slice (no fabricated members), so a faithful
    // no-fabrication reconstruction copies the 224-byte blob as 8 records of 28 bytes. Replace the
    // inner loop with `recordDst[i] = recordSrc[i]` member-wise assignment when PlayerResultsData
    // is homed (identical bytes).
    PlayerResultsInterface& PlayerResultsInterface::operator=(const PlayerResultsInterface& lOther)
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            const s32 liBase = liIndex * 28;
            for (s32 liByte = 0; liByte < 28; ++liByte)
            {
                maPlayerResultsData[liBase + liByte] = lOther.maPlayerResultsData[liBase + liByte];
            }
        }
        return *this;
    }

    // ========================================================================
    // PostSimulationInputBuffer::operator=  (X360 0x82593158, "Net")
    // ========================================================================
    // PARTIAL copy-assign: for each of the five embedded fixed-capacity event queues it (1) zeroes
    // the queue's live count (miLength, queue_base+8) so the tail starts clean, then (2) Appends
    // lOther's live events onto the emptied queue via CgsModule::BaseEventQueue<T>::Append. Finally
    // it copies the single trailing scalar at +9268.
    //
    // X360 queue mapping (zeroed word == queue_base+8 == miLength; pinned by the RoadRulesDownloadEvent
    // Append(this+3712B, other+3712B) whose zeroed word at +3720 == queue_base+8):
    //   RoadRulesRecvData               queue @ +0      (miLength @ +8)
    //   RoadRulesDownloadEvent          queue @ +3712   (miLength @ +3720)
    //   RoadRulesMessageData            queue @ +5968   (miLength @ +5976)
    //   CompletedFburnChallengesData    queue @ +6944   (miLength @ +6952)
    //   DirtyTrickEvent                 queue @ +8808   (miLength @ +8816)
    //   trailing scalar word            @ +9268
    //
    // MINIMAL-SLICE NOTE: none of these queues/fields are named members in the offset-pinned layout
    // (they fall inside the leading opaque padding that precedes mMember_38160Storage), so they are
    // reached by offset reinterpretation onto `this`/`lOther`, mirroring the buffers' raw-storage
    // design. Replace the reinterpret_casts with the real named queue/field members when
    // PostSimulationInputBuffer's full layout is homed (offsets must not move). The miLength=0 reset
    // matches the X360 word-store; Append then merges from the source queue at the same offset.
    PostSimulationInputBuffer& PostSimulationInputBuffer::operator=(const PostSimulationInputBuffer& lOther)
    {
        char*       lpcThis  = reinterpret_cast<char*>(this);
        const char* lpcOther = reinterpret_cast<const char*>(&lOther);

        // --- RoadRulesRecvData queue @ +0 (miLength @ +8) ---
        {
            typedef CgsModule::BaseEventQueue<BrnNetwork::RoadRulesRecvData> Queue;
            Queue&       lDst = *reinterpret_cast<Queue*>(lpcThis + 0);
            const Queue& lSrc = *reinterpret_cast<const Queue*>(lpcOther + 0);
            *reinterpret_cast<s32*>(lpcThis + 8) = 0;   // miLength = 0
            lDst.Append(lSrc);
        }
        // --- RoadRulesDownloadEvent queue @ +3712 (miLength @ +3720) ---
        {
            typedef CgsModule::BaseEventQueue<BrnNetwork::RoadRulesDownloadEvent> Queue;
            Queue&       lDst = *reinterpret_cast<Queue*>(lpcThis + 3712);
            const Queue& lSrc = *reinterpret_cast<const Queue*>(lpcOther + 3712);
            *reinterpret_cast<s32*>(lpcThis + 3720) = 0;
            lDst.Append(lSrc);
        }
        // --- RoadRulesMessageData queue @ +5968 (miLength @ +5976) ---
        {
            typedef CgsModule::BaseEventQueue<BrnNetwork::RoadRulesMessageData> Queue;
            Queue&       lDst = *reinterpret_cast<Queue*>(lpcThis + 5968);
            const Queue& lSrc = *reinterpret_cast<const Queue*>(lpcOther + 5968);
            *reinterpret_cast<s32*>(lpcThis + 5976) = 0;
            lDst.Append(lSrc);
        }
        // --- CompletedFburnChallengesData queue @ +6944 (miLength @ +6952) ---
        // Hand-inlined BaseEventQueue<T>::Append (264-byte element stride) -- see the header note;
        // the BaseEventQueue spine is { T* mpEvents @ +0; s32 miMaxLength @ +4; s32 miLength @ +8 }.
        {
            static const s32 kiCompletedFburnChallengesDataStride = 264; // sizeof(CompletedFburnChallengesData), X360-proven (EventQueue_CompletedFburnChallengesData_7.cpp)
            char*       lpcDstQueue = lpcThis  + 6944;
            const char* lpcSrcQueue = lpcOther + 6944;
            *reinterpret_cast<s32*>(lpcThis + 6952) = 0;   // miLength = 0
            char*       lpDstEvents = *reinterpret_cast<char**>(lpcDstQueue + 0);             // mpEvents
            s32         liDstLength = *reinterpret_cast<s32*>(lpcDstQueue + 8);               // miLength (0 after the reset)
            const char* lpSrcEvents = *reinterpret_cast<char* const*>(lpcSrcQueue + 0);       // src mpEvents
            s32         liSrcLength = *reinterpret_cast<const s32*>(lpcSrcQueue + 8);          // src miLength
            std::memcpy(lpDstEvents + liDstLength * kiCompletedFburnChallengesDataStride,
                        lpSrcEvents,
                        static_cast<size_t>(kiCompletedFburnChallengesDataStride) * liSrcLength);
            *reinterpret_cast<s32*>(lpcDstQueue + 8) = liDstLength + liSrcLength;             // miLength += src
        }
        // --- DirtyTrickEvent queue @ +8808 (miLength @ +8816) ---
        {
            typedef CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent> Queue;
            Queue&       lDst = *reinterpret_cast<Queue*>(lpcThis + 8808);
            const Queue& lSrc = *reinterpret_cast<const Queue*>(lpcOther + 8808);
            *reinterpret_cast<s32*>(lpcThis + 8816) = 0;
            lDst.Append(lSrc);
        }
        // --- trailing scalar word @ +9268 ---
        *reinterpret_cast<s32*>(lpcThis + 9268) = *reinterpret_cast<const s32*>(lpcOther + 9268);

        return *this;
    }

    // ========================================================================
    // OutputBuffer::operator=  (X360 0x825931D0, "Gam")
    // ========================================================================
    // PARTIAL copy-assign: zeroes the embedded DirtyTrickEvent queue's live count (miLength @ +8)
    // and Appends lOther's live DirtyTrickEvents onto it, then copies the trailing scalar field
    // block +460..+537 (the X360 emits this as a run of word + byte stores; reproduced as an
    // exact-byte block copy since those fields are not yet named members).
    //
    // MINIMAL-SLICE NOTE: the DirtyTrickEvent queue (+0) and the +460..+537 fields fall inside the
    // opaque mGuiEventQueueStorage region; reached by offset reinterpretation onto `this`/`lOther`.
    // Replace with the real named members when OutputBuffer is fully homed (offsets must not move).
    // The contiguous touched span is +460..+537 inclusive (78 bytes); the only non-stored bytes in
    // that span (+533..+535) are dead alignment padding, copied harmlessly.
    OutputBuffer& OutputBuffer::operator=(const OutputBuffer& lOther)
    {
        char*       lpcThis  = reinterpret_cast<char*>(this);
        const char* lpcOther = reinterpret_cast<const char*>(&lOther);

        // --- DirtyTrickEvent queue @ +0 (miLength @ +8) ---
        {
            typedef CgsModule::BaseEventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent> Queue;
            Queue&       lDst = *reinterpret_cast<Queue*>(lpcThis + 0);
            const Queue& lSrc = *reinterpret_cast<const Queue*>(lpcOther + 0);
            *reinterpret_cast<s32*>(lpcThis + 8) = 0;   // miLength = 0
            lDst.Append(lSrc);
        }

        // --- trailing scalar field block @ +460 .. +537 (78 bytes) ---
        std::memcpy(lpcThis + 460, lpcOther + 460, 537 - 460 + 1);

        return *this;
    }
}
}
