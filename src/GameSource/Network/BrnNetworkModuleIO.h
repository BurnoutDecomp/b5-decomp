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

#include <cstddef>                                                              // offsetof (PreSimulationInputBuffer::_AssertLayout guards)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer (base; 1B status FlagSet8 @ +0)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                        // CgsSystem::Time (PlayerResultsData::mFinishTime)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (PlayerResultsInterface::GetPlayerResultsData guard)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                     // PlayerName(16B), NetworkPlayerID, EActiveRaceCarIndex, DirtyTrickEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // FULL InGamePlayerStatusData / InGamePlayerStatusInterface
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                     // CgsModule::BaseEventQueue<T> (typed impact/takedown queue accessors)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                 // CgsModule::VariableEventQueue<1536,16> (PostSim AppendVehicleOutputInterface body)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"        // BrnPhysics::Vehicle::ImpactEvent / PhysicalTrafficState
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"       // BrnGameState::TakedownEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"  // GameStateToNetworkInterface (PostSim AppendGameStateToNetworkInterface)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h" // EventQueue<CrashingTrafficUpdateEvent,24> (PostSim SetCrashOutputInterface)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h" // TrafficNetworkInputInterface (PostSim SetTrafficOutputInterface)

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // DWARF BrnNetworkSharedIO.h:458 -- the lifecycle event a freeburn challenge
    // broadcasts over the network. COUNT (6) is the count, not a valid event. The X360
    // build attests its use via BrnNetwork::FreeburnChallengeMessage.
    enum EChallengeEventType
    {
        E_CHALLENGE_EVENT_SELECTED         = 0,
        E_CHALLENGE_EVENT_TRIGGERED        = 1,
        E_CHALLENGE_EVENT_ACTION_SUCCESS   = 2,
        E_CHALLENGE_EVENT_RESET            = 3,
        E_CHALLENGE_EVENT_ENDED            = 4,
        E_CHALLENGE_EVENT_RESULTS_FINISHED = 5,
        E_CHALLENGE_EVENT_COUNT            = 6,
    };

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
    class VehicleDriverInputInterface;   // OutputBuffer @ +5312   (DWARF mVehicleDriverInputInterface, h:440)
    class NetworkInputInterface;         // OutputBuffer @ +147488 (DWARF mCrashNetworkInputInterface, h:442)
    class GameEventQueue;                // OutputBuffer @ +174576 (DWARF mGameEventQueue, h:451)
    struct PostSimulationInputBuffer;    // defined below; PostSim() takes a pointer to it

    // ---- ADDITIVE GROW (BrnNetworkEventScoresManager TU) -------------------------------------
    // BrnNetworkModuleIO::PostSim -- pull the inbound post-sim network-event queue out of a
    // PostSimulationInputBuffer (X360: EventScoresManager::ProcessAfterSimulation @ 0x82565430
    // calls PostSim(lpInput) then walks the returned VariableEventQueue<14000,16> via
    // GetFirstEvent/GetNextEvent). The X360 hands the buffer in r3 and returns the queue start in
    // r3; NetworkEventQueue is CgsModule::VariableEventQueue<14000,16> (see the BuddyManager debug
    // component typedef). Declared-only here; body lands with the BrnNetworkModuleIO TU.
    const NetworkEventQueue* PostSim( const PostSimulationInputBuffer* lpInput );

    // ========================================================================
    // PreSimulationInputBuffer  (DWARF BrnNetworkModuleIO.h:102 : IOBuffer)
    //   members (DWARF order): mbSysMenuOnScreen(+1), mbPadIdle(+2), miControllerPort(+4),
    //   mTimerInterface(+8). All names/offsets/decl-order DWARF-authoritative
    //   (references/DecFIGS/dwarfdump/GameSource/Network/BrnNetworkModuleIO.h:143-148).
    //   Base IOBuffer = 1B status FlagSet8 @ +0. sizeof(CgsModule::IOBuffer)==1.
    //   mTimerInterface is CgsSystem::TimerStatusInterface = 48 bytes (2x 24-byte TimerStatus;
    //   proven by SetTimerStatusInterface's 48-byte store-for-store copy @ 0x823BB670 AND by the
    //   CgsTimerStatusInterface.h DWARF; matches the committed Director sibling mTimerInterface[0x30]).
    //   Modelled as raw offset-pinned storage (forward-declared interface design of this file);
    //   widen to the real typed member on TimerStatusInterface homing (offset +8 must not move).
    // ========================================================================
    struct PreSimulationInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x82587AA8 (read-lock; "Not locked for reading", h:507) -> &mTimerInterface @ +8
        const TimerStatusInterface* GetTimerStatusInterface() const;
        // X360 0x823BB670 (write-lock; h:515) -> memberwise 48-byte copy into mTimerInterface @ +8
        void SetTimerStatusInterface(const TimerStatusInterface* lpTimerStatusInterface);
        // X360 0x823BB520 (write-lock; h:478) -> mbPadIdle @ +2
        void SetPadIdle(bool lbPadIdle);
        // X360 0x82587958 (read-lock;  h:485) -> mbPadIdle @ +2
        bool IsPadIdle() const;
        // X360 0x82587A00 (read-lock;  h:500) -> miControllerPort @ +4
        s32  GetControllerPort() const;
        // X360 0x823BB5C8 (write-lock; h:493) -> miControllerPort @ +4
        void SetControllerPort(s32 liControllerPort);
        // X360 0x823BB780 (write-lock; h:523) -> mbSysMenuOnScreen @ +1
        void SetSysMenuOnScreen(bool lbSysMenuOnScreen);
        // X360 0x82587B50 (read-lock;  h:530) -> mbSysMenuOnScreen @ +1
        bool IsSysMenuOnScreen() const;

        // declared-only API (other TUs): Construct (h:107) / Destruct (h:111).
    private:
        bool mbSysMenuOnScreen;              // +1  (DWARF BrnNetworkModuleIO.h:143)
        bool mbPadIdle;                      // +2  (DWARF BrnNetworkModuleIO.h:144)
        // +3 : implicit alignment pad before the s32 below
        s32  miControllerPort;               // +4  (DWARF BrnNetworkModuleIO.h:145)
        // CgsSystem::TimerStatusInterface mTimerInterface @ +8 (DWARF BrnNetworkModuleIO.h:148);
        // 48 bytes. Raw storage until the type is homed here; the committed GetTimerStatusInterface
        // body returns &mTimerInterfaceStorage and SetTimerStatusInterface memcpys 48B into it.
        u8   mTimerInterfaceStorage[48];     // TimerStatusInterface @ +8 (widen->typed on its TU)

        // Compile-time offset guards (private members -> assert from a member-fn context, mirroring
        // the committed BrnGameStateModuleIO.h PreWorldInputBuffer::_AssertLayout pattern).
        static void _AssertLayout()
        {
            static_assert(offsetof(PreSimulationInputBuffer, mbSysMenuOnScreen)      == 1, "PreSim mbSysMenuOnScreen @ +1");
            static_assert(offsetof(PreSimulationInputBuffer, mbPadIdle)             == 2, "PreSim mbPadIdle @ +2");
            static_assert(offsetof(PreSimulationInputBuffer, miControllerPort)      == 4, "PreSim miControllerPort @ +4");
            static_assert(offsetof(PreSimulationInputBuffer, mTimerInterfaceStorage) == 8, "PreSim mTimerInterface @ +8");
        }
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
        // X360 0x823BBD00 (read,  h:759) -> const u8* @ +180248 (const read twin of the h:752 write accessor)
        const u8* GetMember_180248() const;                        // RENAME / re-home on member-type homing
        // X360 0x823BC300 (read,  h:890) -> const u8* @ +163104
        const u8* GetMember_163104() const;                        // RENAME on member-type homing
        // X360 0x8254EC20 (read,  h:596) -> const u8* @ +27680 (active-race-car interface; RENAME to the real interface-typed accessor on homing)
        const u8* GetActiveRaceCarInterfaceRaw_27680() const;      // RENAME / re-home on member-type homing
        // X360 0x8254ECC8 (read,  h:687) -> const NetworkEventQueue* @ +72952 (queue start)
        const NetworkEventQueue* GetNetworkEventQueue() const;
        // X360 0x823BBB00 (write, h:701) -> NetworkEventQueue* @ +73284 (queue body)
        NetworkEventQueue*       GetNetworkEventQueueForWriting();
        // X360 0x8254ED70 (read,  h:708) -> const NetworkEventQueue* @ +73284 (const twin of the write accessor)
        const NetworkEventQueue* GetNetworkEventQueueForWriting() const;

        // ---- typed input event-queue accessors used by the AggressiveDriving relay (declared-only) ----
        // X360 sub_8254EB78 / sub_8254E980 (Hex-Rays un-named): the inbound vehicle ImpactEvent and
        //   GameState TakedownEvent queues ProcessInputQueue walks each frame (it GetEvent()s every
        //   element and forwards it to AddImpactEvent/AddTakedownEvent). Return the typed
        //   BaseEventQueue so the relay reads GetLength/GetEvent BY NAME. Declared-only (bodies land
        //   with the PostSimulationInputBuffer / VehicleManager IO TUs).
        const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>* GetImpactEventQueue() const;
        const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>*      GetTakedownEventQueue() const;

        // ---- wave4 producer interfaces (bodies in this TU's .cpp) ----
        // X360 0x823BB828 (write, h:558) -> &member @ +54504 (write twin of GetVehicleOutputInterfaceRaw_54504).
        u8*  GetVehicleOutputInterfaceRaw_54504ForWriting();
        // X360 0x823BB978 (write, h:624) -> &member @ +40240 (write twin of GetMember_40240; ledger 'GetP' truncated).
        u8*  GetMember_40240ForWriting();
        // X360 0x823C9DF0 (write, h:680) -> GameStateToNetworkInterface::Append @ +53628 (a2 source; callee void -> return 0).
        int  AppendGameStateToNetworkInterface(const GameStateToNetworkInterface* lpSource);
        // X360 0x823C9D38 (write, h:672) -> BaseEventQueue<BrnGameState::TakedownEvent>::Append @ +54168.
        int  AppendTakedownQueue(const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpSource);
        // X360 0x823C9AE8 (write, h:639) -> splice source vehicle-output interface (a2): 3 queue Appends + +16 field + memcpy body.
        int* AppendVehicleOutputInterface(const void* lpSource);
        // X360 0x823BBA20 (write, h:648) -> assert !=null + write-lock, XMemCpy 10480B active-race-car interface (a2) into &member @ +27680.
        int* SetActiveRaceCarInterface(const void* lpInterface);
        // X360 0x823C9C80 (write, h:664) -> reset miLength (+38312) then EventQueue<CrashingTrafficUpdateEvent,24>::Append @ +38304.
        int  SetCrashOutputInterface(const void* lpSource);
        // X360 0x823C9BD0 (write, h:657) -> copy source traffic-network interface (a2) into &member @ +38160 BEFORE the lock assert.
        void* SetTrafficOutputInterface(const void* lpSource);

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
        u8  maPadToMember_27680[27680 - sizeof(CgsModule::IOBuffer)]; // -> +27680
        u8  mActiveRaceCarInterfaceStorage[38160 - 27680];            // @ +27680 (active-race-car interface; SetActiveRaceCarInterface @0x823BBA20 XMemCpy's 10480B here; h:596 read accessor)
        u8  mMember_38160Storage[38304 - 38160];                      // @ +38160 (h:610 accessor)
        u8  mMember_38304Storage[40240 - 38304];                      // @ +38304 (h:603 accessor)
        u8  mMember_40240Storage[40300 - 40240];                      // @ +40240 (h:631 accessor)
        u8  mMember_40300Storage[54504 - 40300];                      // @ +40300 (h:574 write accessor)
        u8  mMember_54504Storage[72952 - 54504];                      // @ +54504 (h:566 accessor)
        u8  mNetworkEventQueueStorage[163104 - 72952];                // mNetworkEventQueue family @ +72952 (covers +73284); widen/split on VariableEventQueue homing
        u8  mMember_163104Storage[180248 - 163104];                   // @ +163104 (h:890 read accessor)
        u8  mMember_180248Storage[8];                                 // @ +180248 (h:752 write / h:759 read accessors); width is a placeholder
    };

    // ========================================================================
    // OutputBuffer  (DWARF BrnNetworkModuleIO.h:284 : IOBuffer)
    // ========================================================================
    // RE-SLICED for wave4: three of the old opaque spans are subdivided so this batch's new
    // accessors reach their proven X360 byte offsets (mVehicleDriverInputInterface @ +5312,
    // mCrashNetworkInputInterface @ +147488, mGameEventQueue @ +174576, mePlayerActiveRaceCarIndex
    // @ +178688, and the three state bytes @ +180584/+180585/+180586). Every committed accessor's
    // returned start-offset is preserved (mGuiEventQueue@+16, mInGamePlayerStatusInterface@+162992,
    // mNetworkToGameStateInterface@+172376, mStatsOutputInterface@+178692, mNetworkToGuiInterface@
    // +180588, mNetworkEventQueue@+184080). DWARF DECLARATION order != X360 storage order; members
    // are placed at their proven X360 byte offsets.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x823BC060 (read,  h:827) / 0x8254EF68 (write, h:841) -> VehicleDriverInputInterface* @ +5312
        const VehicleDriverInputInterface*  GetVehicleDriverInputInterface() const;
        VehicleDriverInputInterface*        GetVehicleDriverInputInterface();
        // X360 0x823BC1B0 (read,  h:855) / 0x825881F8 (write, h:862) -> NetworkInputInterface* @ +147488
        const NetworkInputInterface*        GetCrashNetworkInputInterface() const;
        NetworkInputInterface*              GetCrashNetworkInputInterface();
        // X360 0x823BBDA8 (read,  h:775) / 0x8254EEC0 (write, h:767) -> GameEventQueue* @ +174576
        // (IDA truncates to GetGuiEv*; DWARF h:304/308 names this pair GetGameEventQueue / member
        //  mGameEventQueue. The committed +16 accessor owns the separate GetGuiEventQueue name.)
        const GameEventQueue*               GetGameEventQueue() const;
        GameEventQueue*                     GetGameEventQueue();
        // X360 0x823BBBA8 (read,  h:719) -> EActiveRaceCarIndex mePlayerActiveRaceCarIndex @ +178688 (lwzx)
        EActiveRaceCarIndex                 GetPlayerActiveRaceCarIndex() const;

        // X360 0x823BC108 (read, h:834) -> const GuiEventQueueSmall* @ +16
        const GuiEventQueueSmall*           GetGuiEventQueue() const;
        // X360 0x823BC258 (read, h:869) -> const InGamePlayerStatusInterface* @ +162992
        const InGamePlayerStatusInterface*  GetInGamePlayerStatusInterface() const;
        // X360 0x823BC5A0 (read, h:964) -> const NetworkToGameStateInterface* @ +172376
        const NetworkToGameStateInterface*  GetNetworkToGameStateInterface() const;
        // X360 0x823BBC58 (read, h:745) -> const StatsOutputInterface* @ +178692
        // X360 0x82587E98 (write, h:737) -> StatsOutputInterface* @ +178692 (non-const twin; DWARF h:390)
        const StatsOutputInterface*         GetStatsOutputInterface() const;
        StatsOutputInterface*               GetStatsOutputInterface();
        // X360 0x823BC3A8 (read, h:911) -> const NetworkToGuiInterface* @ +180588
        const NetworkToGuiInterface*        GetNetworkToGuiInterface() const;
        // X360 0x823BC648 (read, h:987) -> const u8* @ +183408 (GUI-bound sub-interface within NetworkToGuiInterface; RENAME on homing)
        const u8*                           GetMember_183408() const;
        // X360 0x823BC6F0 (read, h:1001) -> const NetworkEventQueue* @ +184080
        const NetworkEventQueue*            GetNetworkEventQueue() const;
        // X360 0x8254F160 (write, h:994)  -> NetworkEventQueue* @ +184080 (non-const twin)
        NetworkEventQueue*                  GetNetworkEventQueue();

        // ---- state flags (X360 stbx/lbzx byte accessors) --------------------
        // X360 0x823BBE50 (read, h:790) / 0x82587F40 (write, h:783) -> mbIsPlaying   @ +180584
        bool IsPlaying() const;    void SetIsPlaying(bool lbPlaying);
        // X360 0x823BBF00 (read, h:805) / 0x82587FF0 (write, h:798) -> mbIsConnected @ +180585
        bool IsConnected() const;  void SetConnected(bool lbConnected);
        // X360 0x823BBFB0 (read, h:820) / 0x825880A0 (write, h:813) -> mbIsInInvite  @ +180586
        bool IsInInvite() const;   void SetIsInInvite(bool lbInInvite);

        // ---- typed event-queue accessors used by the AggressiveDriving relay (declared-only) ----
        // X360 OutputBuffer::GetVehicleManagerOutputInterface (Hex-Rays "GetVeh"): the
        //   per-frame race-car/vehicle output interface; HandleReceivingMessages AddEvent()s the
        //   reconstructed ImpactEvent onto its embedded BaseEventQueue<ImpactEvent> (X360 +141376).
        // X360 OutputBuffer::GetNetworkToGameStateTakedownEventQueue (Hex-Rays "Outp"): the inbound
        //   takedown-event queue the relay drains/feeds. Both return the typed BaseEventQueue so the
        //   relay calls AddEvent/GetEvent/GetLength BY NAME. Declared-only here (bodies + the full
        //   interface layouts land with the OutputBuffer / VehicleManager IO TUs); the storage these
        //   reach lives beyond the offset-pinned region this minimal slice models.
        CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent>* GetVehicleManagerImpactEventQueue();
        CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>*      GetNetworkToGameStateTakedownEventQueue();

        // X360 0x825931D0: OutputBuffer::operator= -- zeroes + Appends the embedded DirtyTrickEvent
        //   queue (@ +0), then copies the trailing scalar field block +460..+537.
        OutputBuffer& operator=(const OutputBuffer& lOther);

        // declared-only: Construct/Destruct + the full DWARF Get*/Set* set.
    private:
        u8  maPadToGuiEventQueue[16 - sizeof(CgsModule::IOBuffer)];        // -> +16
        u8  mGuiEventQueueStorage[5312 - 16];                             // GuiEventQueueSmall @ +16 (h:448; also spans the +460..+537 operator= fields)
        u8  mVehicleDriverInputInterfaceStorage[147488 - 5312];          // VehicleDriverInputInterface @ +5312 (h:440; also spans mVehicleInputInterface @ h:441)
        u8  mCrashNetworkInputInterfaceStorage[162992 - 147488];         // NetworkInputInterface @ +147488 (mCrashNetworkInputInterface, h:442)
        u8  mInGamePlayerStatusInterfaceStorage[172376 - 162992];        // InGamePlayerStatusInterface @ +162992
        u8  mNetworkToGameStateInterfaceStorage[174576 - 172376];        // NetworkToGameStateInterface @ +172376
        u8  mGameEventQueueStorage[178688 - 174576];                     // GameEventQueue @ +174576 (mGameEventQueue, h:451)
        EActiveRaceCarIndex mePlayerActiveRaceCarIndex;                  // @ +178688 (h:450) -- 4-byte enum : s32
        u8  mStatsOutputInterfaceStorage[180584 - 178692];               // StatsOutputInterface @ +178692 (h:447)
        bool mbIsPlaying;                                                // @ +180584 (h:454)
        bool mbIsConnected;                                              // @ +180585 (h:455)
        bool mbIsInInvite;                                               // @ +180586 (h:456)
        u8  maPadToNetworkToGuiInterface[180588 - 180587];               // @ +180587 (mbInvitesOpen, h:457)
        u8  mNetworkToGuiInterfaceStorage[183408 - 180588];              // NetworkToGuiInterface @ +180588 (h:445)
        u8  mMember_183408Storage[184080 - 183408];                      // @ +183408 (h:987 read accessor; GUI-bound sub-interface within NetworkToGuiInterface)
        u8  mNetworkEventQueueStorage[16];                               // mNetworkEventQueue @ +184080 (h:465; width placeholder; widen on VariableEventQueue homing)
    };

    // ========================================================================
    // PlayerResultsData  (DWARF SharedIO/BrnNetworkModulePlayerResultsInterface.h:46)
    // ========================================================================
    // Per-player end-of-round result record. SHAPE is the DWARF member set (h:49-54); the on-disk
    // FIELD ORDER + OFFSETS are X360-AUTHORITATIVE, recovered from the consumer
    // ScoringSystem::UpdateNetworkPlayerResults (0x8231FA90), whose record cursor walks this array
    // at a 28-byte (0x1C) stride and reads each field at a fixed offset:
    //   +0x00 (8) Time mFinishTime    : two words -- miSeconds (stw -0x18(cur)) + mfFraction (stfs -0x14)
    //   +0x08 (4) meActiveRaceCarIndex: the per-record car slot (lwz -0x10(cur); checked != -1, indexes GetCarData)
    //   +0x0C (4) meEliminatorIndex   : eliminator car slot   (lwz -0xC(cur) -> CarScoreData +0x60)
    //   +0x10 (4) mfDistanceToFinish  : captured finish distance (lfs -8(cur) -> CarScoreData +0x48)
    //   +0x14 (4) miEliminations      : eliminations           (lwz -4(cur) -> CarScoreData +0x64)
    //   +0x18 (1) mbValid             : record-present guard    (lbz 0(cur); gates the whole write)
    //   +0x19 (1) mbTimedOut          : timed-out flag          (lbz 1(cur) -> CarScoreData +0x68)
    //   +0x1A (1) mbEliminated        : eliminated flag         (lbz 2(cur) -> CarScoreData +0xD9)
    //   +0x1B (1) padding to the 28-byte stride
    // NOTE on the bool count: the DWARF (h:49) names ONLY one bool (mbTimedOut). The X360 body reads
    // THREE distinct bytes at +0x18/+0x19/+0x1A, so the on-disk record has three bool slots; the two
    // not in the DWARF (mbValid @+0x18, mbEliminated @+0x1A) are named from their X360 use. The DWARF
    // declaration ORDER is NOT the X360 storage order (the X360 packs the 8-byte Time first); members
    // are placed here at their proven on-disk offsets so the 28-byte stride and every consumer read
    // land by name. FLAG: the +0x18 validity byte and the +0x1A eliminated byte are inferred from the
    // ASM (no DWARF name) -- high-confidence from the store targets, but not DWARF-confirmed.
    struct PlayerResultsData
    {
        CgsSystem::Time     mFinishTime;            // +0x00 (8)  (DWARF "Time")
        EActiveRaceCarIndex meActiveRaceCarIndex;   // +0x08
        EActiveRaceCarIndex meEliminatorIndex;      // +0x0C
        f32                 mfDistanceToFinish;     // +0x10  (DWARF float_t -> f32)
        s32                 miEliminations;         // +0x14  (DWARF int32_t)
        bool                mbValid;                // +0x18  (X360 record-present guard; not in DWARF)
        bool                mbTimedOut;             // +0x19  (DWARF h:49)
        bool                mbEliminated;           // +0x1A  (X360 eliminated flag; not in DWARF)
        u8                  maPad1B[1];             // +0x1B  pad to 28-byte stride

        // DWARF h:57: PlayerResultsData::Clear() -- declare-only (own TU body, no standalone export found).
        void Clear();
    };
    static_assert(sizeof(PlayerResultsData) == 28, "PlayerResultsData on-disk stride (0x1C)");

    // ========================================================================
    // PlayerResultsInterface  (DWARF SharedIO/BrnNetworkModulePlayerResultsInterface.h:86)
    //   maPlayerResultsData[8]; operator= @ 0x823B8F68 (8x 28B memberwise copy).
    //   This interface is NOT a committed home elsewhere -- this TU homes it (and PlayerResultsData).
    // ========================================================================
    struct PlayerResultsInterface
    {
        // X360 0x823B8F68: memberwise copy of maPlayerResultsData[8] (28-byte stride).
        PlayerResultsInterface& operator=(const PlayerResultsInterface& lOther);

        // DWARF h:96 -- const element accessor. The X360 consumer (UpdateNetworkPlayerResults)
        // indexes the array linearly; the asserted "liIndex >= 0" guard (BrnNetworkModulePlayer-
        // ResultsInterface.h:141) is reproduced. Inline so the consumer TU needs no out-of-line body.
        const PlayerResultsData* GetPlayerResultsData(s32 liIndex) const
        {
            CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
            return &maPlayerResultsData[liIndex];
        }
        // DWARF h:101 -- mutable element accessor (writer side). Declare-only: no standalone X360
        // export reached from this slice (inlined at its call sites); body lands with this TU.
        PlayerResultsData* GetPlayerResultsDataForWriting(s32 liIndex);

        // DWARF h:91: Clear() -- declare-only (own TU body).
        void Clear();

    private:
        PlayerResultsData maPlayerResultsData[8];   // 8 records, 28-byte stride (== old u8[8*28])
    };
    static_assert(sizeof(PlayerResultsInterface) == 8 * 28, "PlayerResultsInterface layout (8 x 0x1C)");

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
