// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModuleIO.h
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO is a NAMESPACE. The three per-frame IO payload buffers the
// network module exchanges (PreSimulationInputBuffer, PostSimulationInputBuffer, OutputBuffer)
// all derive from CgsModule::IOBuffer (lock state machine; 1-byte status FlagSet8 at offset 0).
// Each lock-guarded Get* accessor first asserts the right lock bit (read = bit 0x10 / "Not locked
// for reading"; write = bit 0x08 / "Not locked for writing") then returns &member-at-X360-offset.
//
// MINIMAL SLICE NOTE (mirrors the committed sibling GameSource/GameState/BrnGameStateModuleIO.h):
// each buffer's real DWARF layout is dozens of large composite interface members from other,
// un-reconstructed modules. Only the members the 24 functions in this TU touch are modelled,
// each pinned to its exact X360 byte offset with explicit u8 storage for the gaps. The interface
// member types are forward-declared incomplete classes; the touched member is given as raw aligned
// storage of the correct width at the correct offset so each buffer's later full reconstruction can
// replace the storage+padding with the real typed members without moving anything.
//
// InGamePlayerStatusData / InGamePlayerStatusInterface are the FULL committed types pulled in from
// SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h (NOT forward-declared here) so the buffer
// accessors and the SharedIO operator= bodies see their named members.
//
// FUNCTION OWNERSHIP (24 funcs):
//   --- OutputBuffer accessors ---
//     0x823BC108 r +16     -> const GuiEventQueueSmall*          GetGuiEventQueue() const
//     0x823BBC58 r +178692 -> const StatsOutputInterface*        GetStatsOutputInterface() const
//     0x823BC258 r +162992 -> const InGamePlayerStatusInterface* GetInGamePlayerStatusInterface() const
//     0x823BC3A8 r +180588 -> const NetworkToGuiInterface*       GetNetworkToGuiInterface() const
//     0x823BC5A0 r +172376 -> const NetworkToGameStateInterface* GetNetworkToGameStateInterface() const
//     0x823BC6F0 r +184080 -> const NetworkEventQueue*           GetNetworkEventQueue() const
//     0x8254F160 w +184080 -> NetworkEventQueue*                 GetNetworkEventQueue()        (non-const twin)
//   --- PostSimulationInputBuffer accessors ---
//     0x8254EA28 r +54504  -> const u8* GetVehicleOutputInterfaceRaw_54504() const  (RENAME on member homing)
//     0x823BB8D0 w +40300  -> u8*       GetMember_40300()                            (RENAME on member homing)
//     0x82587BF8 r +38304  -> const u8* GetMember_38304() const                      (RENAME on member homing)
//     0x82587CA0 r +38160  -> const u8* GetMember_38160() const                      (RENAME on member homing)
//     0x82587DF0 r +40240  -> const u8* GetMember_40240() const                      (RENAME on member homing)
//     0x8254ECC8 r +72952  -> const NetworkEventQueue* GetNetworkEventQueue() const          (queue start)
//     0x823BBB00 w +73284  -> NetworkEventQueue*       GetNetworkEventQueueForWriting()       (queue body)
//     0x8254ED70 r +73284  -> const NetworkEventQueue* GetNetworkEventQueueForWriting() const (const twin of the write accessor)
//     0x8254EE18 w +180248 -> u8*       GetMember_180248()                           (RENAME / re-home on member homing)
//   --- PreSimulationInputBuffer accessor ---
//     0x82587AA8 r +8 -> const TimerStatusInterface* GetTimerStatusInterface() const
//   --- InGamePlayerStatus / PlayerResults helpers (SharedIO homes) ---
//     0x8230FF60 -> InGamePlayerStatusData* InGamePlayerStatusInterface::GetPlayerStatusDataForWriting(s32)
//     0x823628C8 -> InGamePlayerStatusData& InGamePlayerStatusData::operator=(const InGamePlayerStatusData&)
//     0x8236B020 -> InGamePlayerStatusInterface& InGamePlayerStatusInterface::operator=(const InGamePlayerStatusInterface&)
//     0x823B8F68 -> PlayerResultsInterface& PlayerResultsInterface::operator=(const PlayerResultsInterface&)
//   --- big copy-assign on the buffers ---
//     0x825931D0 -> OutputBuffer::operator=               (DirtyTrickEvent queue Append + fields +460..+537)
//     0x82593158 -> PostSimulationInputBuffer::operator=  (5 queue Appends + field +9268)
//   --- FifoQueue<NetworkOutRecvRoadRulesPBEvent,14>::Push 0x8254DFB8 -> thin instantiation TU ---
//
// NOTE: the ledger lists this TU twice (dossiers 02 & 03 byte-identical: X360 emits one instance
// per using-TU; this single header+cpps covers both -- MEMORY dup-ledger gotcha).
//
// The two PostSim NetworkEventQueue accessors at +72952 and +73284 cannot coexist as two const
// overloads with the same name+signature returning different offsets, so the +73284 pair is given
// a distinct name (GetNetworkEventQueueForWriting) -- the truncated ledger collapsed both DWARF
// members to "GetNetworkEventQueue". Both addresses are byte-correct; split into two named queue
// members when the VariableEventQueue family is homed.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer (base; 1B status FlagSet8 @ +0)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                     // PlayerName(16B), NetworkPlayerID, EActiveRaceCarIndex, DirtyTrickEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // FULL InGamePlayerStatusData / InGamePlayerStatusInterface

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // ---- forward declarations of the interface member types (own TUs) --------
    // Full layouts live with their own reconstructions; the accessors return pointers, so
    // incomplete declarations suffice. Replace the matching offset-pinned u8 storage with the
    // real typed member when each interface's own TU lands (offsets must not move).
    class TimerStatusInterface;          // PreSim  @ +8
    class GuiEventQueueSmall;            // OutputBuffer @ +16
    class StatsOutputInterface;          // OutputBuffer @ +178692
    class NetworkToGuiInterface;         // OutputBuffer @ +180588
    class NetworkToGameStateInterface;   // OutputBuffer @ +172376
    class NetworkEventQueue;             // OutputBuffer @ +184080 ; PostSim @ +72952..+73284

    // ========================================================================
    // PreSimulationInputBuffer  (DWARF BrnNetworkModuleIO.h:102 : IOBuffer)
    //   members: mbSysMenuOnScreen, mbPadIdle, miControllerPort, mTimerInterface
    // ========================================================================
    struct PreSimulationInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x82587AA8 (read-lock; "Not locked for reading", h:507) -> &mTimerInterface @ +8
        const TimerStatusInterface* GetTimerStatusInterface() const;

        // declared-only API (other TUs): Construct/Destruct/SetTimerStatusInterface/SetPadIdle/
        // IsPadIdle/GetControllerPort/SetControllerPort/SetSysMenuOnScreen/IsSysMenuOnScreen.
    private:
        // base end (1B) -> +8 : mbSysMenuOnScreen/mbPadIdle/miControllerPort packed in the gap.
        u8 maPadToTimerInterface[8 - sizeof(CgsModule::IOBuffer)];   // -> +8
        u8 mTimerInterfaceStorage[8];                                // TimerStatusInterface @ +8 (widen on its TU)
    };

    // ========================================================================
    // PostSimulationInputBuffer  (DWARF BrnNetworkModuleIO.h:160 : IOBuffer)
    //   Only members touched by this TU's accessors + operator= are pinned.
    // ========================================================================
    struct PostSimulationInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x8254EA28 (read,  h:566) -> &member @ +54504
        const u8* GetVehicleOutputInterfaceRaw_54504() const;       // RENAME on member-type homing
        // X360 0x823BB8D0 (write, h:574) -> &member @ +40300
        u8*       GetMember_40300();                                 // RENAME on member-type homing
        // X360 0x82587BF8 (read,  h:603) -> &member @ +38304
        const u8* GetMember_38304() const;                          // RENAME on member-type homing
        // X360 0x82587CA0 (read,  h:610) -> &member @ +38160
        const u8* GetMember_38160() const;                          // RENAME on member-type homing
        // X360 0x82587DF0 (read,  h:631) -> &member @ +40240
        const u8* GetMember_40240() const;                          // RENAME on member-type homing
        // X360 0x8254EE18 (write, h:752) -> &member @ +180248
        u8*       GetMember_180248();                               // RENAME / re-home on member-type homing
        // X360 0x8254ECC8 (read,  h:687) -> const NetworkEventQueue* @ +72952 (queue start)
        const NetworkEventQueue* GetNetworkEventQueue() const;
        // X360 0x823BBB00 (write, h:701) -> NetworkEventQueue* @ +73284 (queue body)
        NetworkEventQueue*       GetNetworkEventQueueForWriting();
        // X360 0x8254ED70 (read,  h:708) -> const NetworkEventQueue* @ +73284 (const twin of the write accessor)
        const NetworkEventQueue* GetNetworkEventQueueForWriting() const;

        // X360 0x82593158: PostSimulationInputBuffer::operator= -- zeroes the live count of and
        //   Appends lOther's events onto the five embedded fixed-capacity event queues
        //   (RoadRulesRecvData @ +0, RoadRulesDownloadEvent @ +3712, RoadRulesMessageData @ +5968,
        //   GameStateModuleIO::CompletedFburnChallengesData @ +6944, DirtyTrickEvent @ +8808), then
        //   copies the trailing scalar at +9268.
        PostSimulationInputBuffer& operator=(const PostSimulationInputBuffer& lOther);

        // declared-only: Construct/Destruct + the full DWARF Get*/Set*/Append* set.
    private:
        // Offsets are absolute from `this`. Only the pinned anchors below are load-bearing for this TU;
        // intervening storage is opaque padding to be subdivided by sibling member-type TUs.
        u8  maPadToMember_38160[38160 - sizeof(CgsModule::IOBuffer)]; // -> +38160
        u8  mMember_38160Storage[38304 - 38160];                      // @ +38160 (h:610 accessor)
        u8  mMember_38304Storage[40240 - 38304];                      // @ +38304 (h:603 accessor)
        u8  mMember_40240Storage[40300 - 40240];                      // @ +40240 (h:631 accessor)
        u8  mMember_40300Storage[54504 - 40300];                      // @ +40300 (h:574 write accessor)
        u8  mMember_54504Storage[72952 - 54504];                      // @ +54504 (h:566 accessor)
        u8  mNetworkEventQueueStorage[180248 - 72952];                // mNetworkEventQueue family @ +72952 (covers +73284); widen/split on VariableEventQueue homing
        u8  mMember_180248Storage[8];                                 // @ +180248 (h:752 write accessor); width is a placeholder
    };

    // ========================================================================
    // OutputBuffer  (DWARF BrnNetworkModuleIO.h:284 : IOBuffer)
    // ========================================================================
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x823BC108 (read, h:834) -> const GuiEventQueueSmall* @ +16
        const GuiEventQueueSmall*           GetGuiEventQueue() const;
        // X360 0x823BC258 (read, h:869) -> const InGamePlayerStatusInterface* @ +162992
        const InGamePlayerStatusInterface*  GetInGamePlayerStatusInterface() const;
        // X360 0x823BC5A0 (read, h:964) -> const NetworkToGameStateInterface* @ +172376
        const NetworkToGameStateInterface*  GetNetworkToGameStateInterface() const;
        // X360 0x823BBC58 (read, h:745) -> const StatsOutputInterface* @ +178692
        const StatsOutputInterface*         GetStatsOutputInterface() const;
        // X360 0x823BC3A8 (read, h:911) -> const NetworkToGuiInterface* @ +180588
        const NetworkToGuiInterface*        GetNetworkToGuiInterface() const;
        // X360 0x823BC6F0 (read, h:1001) -> const NetworkEventQueue* @ +184080
        const NetworkEventQueue*            GetNetworkEventQueue() const;
        // X360 0x8254F160 (write, h:994)  -> NetworkEventQueue* @ +184080 (non-const twin)
        NetworkEventQueue*                  GetNetworkEventQueue();

        // X360 0x825931D0: OutputBuffer::operator= -- zeroes + Appends the embedded DirtyTrickEvent
        //   queue (@ +0), then copies the trailing scalar field block +460..+537.
        OutputBuffer& operator=(const OutputBuffer& lOther);

        // declared-only: Construct/Destruct + the full DWARF Get*/Set* set.
    private:
        u8  maPadToGuiEventQueue[16 - sizeof(CgsModule::IOBuffer)];        // -> +16
        u8  mGuiEventQueueStorage[162992 - 16];                           // GuiEventQueueSmall @ +16 (also spans the +460..+537 operator= fields)
        u8  mInGamePlayerStatusInterfaceStorage[172376 - 162992];        // InGamePlayerStatusInterface @ +162992
        u8  mNetworkToGameStateInterfaceStorage[178692 - 172376];        // NetworkToGameStateInterface @ +172376
        u8  mStatsOutputInterfaceStorage[180588 - 178692];               // StatsOutputInterface @ +178692
        u8  mNetworkToGuiInterfaceStorage[184080 - 180588];              // NetworkToGuiInterface @ +180588
        u8  mNetworkEventQueueStorage[16];                               // mNetworkEventQueue @ +184080 (width placeholder; widen on VariableEventQueue homing)
    };

    // ========================================================================
    // PlayerResultsInterface  (DWARF SharedIO/BrnNetworkModulePlayerResultsInterface.h:86)
    //   maPlayerResultsData[8]; operator= @ 0x823B8F68 (8x ~28B memberwise copy).
    //   PlayerResultsData is modelled opaquely (no fabricated members); DWARF h:46 gives
    //   {bool mbTimedOut; EActiveRaceCarIndex meActiveRaceCarIndex; EActiveRaceCarIndex meEliminatorIndex;
    //    f32 mfDistanceToFinish; Time mFinishTime; s32 miEliminations;} (~28B). Replace with the typed
    //   struct + member-wise assignment when PlayerResultsData is homed (offsets/stride must not move).
    //   This interface is NOT a committed home elsewhere -- this TU homes it.
    // ========================================================================
    struct PlayerResultsInterface
    {
        // X360 0x823B8F68: memberwise copy of maPlayerResultsData[8] (28-byte stride).
        PlayerResultsInterface& operator=(const PlayerResultsInterface& lOther);

        // declared-only: Clear / GetPlayerResultsData(const) / GetPlayerResultsDataForWriting.
    private:
        u8 maPlayerResultsData[8 * 28];    // 8 PlayerResultsData records (~28B each, copied by operator=)
    };

    // ========================================================================
    // NetworkOutRecvRoadRulesPBEvent  (DWARF BrnNetworkOutEventTypeDefs.h:11)
    //   : public NetworkEvent<33>; { ChallengeHighScoreEntry mPersonalBestScore;
    //     NetworkPlayerID mPersonalBestPlayerID; Road::ChallengeIndex mPersonalBestChallengeIndex;
    //     bool mbWasPBByFriend; } -- X360 FifoQueue element stride == 72 bytes.
    //   NetworkEvent<33> + ChallengeHighScoreEntry NOT yet committed; modelled minimally as 72B so
    //   FifoQueue<...,14>::Push (thin instantiation) compiles. Replace with the real typed layout in
    //   BrnNetworkOutEventTypeDefs.h when those subsystems land.
    // ========================================================================
    struct NetworkOutRecvRoadRulesPBEvent { u8 maOpaque[72]; };
}
}
