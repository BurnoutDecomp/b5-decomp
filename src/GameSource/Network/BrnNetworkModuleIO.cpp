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
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<1536,16>::Append (AppendVehicleOutputInterface)
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"     // GameStateToNetworkInterface::Append (AppendGameStateToNetworkInterface)
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h" // EventQueue<CrashingTrafficUpdateEvent,24> (SetCrashOutputInterface)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h" // TrafficNetworkInputInterface (SetTrafficOutputInterface)
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

    // X360 0x823BB670 (write-lock; "Not locked for writing", h:515) ->
    //   memberwise copy of the 48-byte CgsSystem::TimerStatusInterface (2x 24-byte TimerStatus) from
    //   the source into mTimerInterface @ this+8. The asm is the compiler's flat lowering of the
    //   inlined POD TimerStatusInterface::operator=; TimerStatusInterface is modelled here as raw
    //   48-byte storage, so the memberwise copy is reproduced as a byte-identical block copy. Replace
    //   with `mTimerInterface = *lpTimerStatusInterface;` when the type is homed here (same bytes/offset).
    void PreSimulationInputBuffer::SetTimerStatusInterface(const TimerStatusInterface* lpTimerStatusInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(&mTimerInterfaceStorage[0], lpTimerStatusInterface, sizeof(mTimerInterfaceStorage));
    }

    // X360 0x823BB520 (write-lock; "Not locked for writing", h:478) -> stb r27,2(this) : mbPadIdle @ +2.
    void PreSimulationInputBuffer::SetPadIdle(bool lbPadIdle)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbPadIdle = lbPadIdle;
    }

    // X360 0x82587958 (read-lock; "Not locked for reading", h:485) -> lbz r3,2(this) : mbPadIdle @ +2.
    bool PreSimulationInputBuffer::IsPadIdle() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbPadIdle;
    }

    // X360 0x82587A00 (read-lock; "Not locked for reading", h:500) -> lwz r3,4(this) : miControllerPort @ +4.
    s32 PreSimulationInputBuffer::GetControllerPort() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return miControllerPort;
    }

    // X360 0x823BB5C8 (write-lock; "Not locked for writing", h:493) -> stw r27,4(this) : miControllerPort @ +4.
    void PreSimulationInputBuffer::SetControllerPort(s32 liControllerPort)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        miControllerPort = liControllerPort;
    }

    // X360 0x823BB780 (write-lock; "Not locked for writing", h:523) -> stb r27,1(this) : mbSysMenuOnScreen @ +1.
    void PreSimulationInputBuffer::SetSysMenuOnScreen(bool lbSysMenuOnScreen)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbSysMenuOnScreen = lbSysMenuOnScreen;
    }

    // X360 0x82587B50 (read-lock; "Not locked for reading", h:530) -> lbz r3,1(this) : mbSysMenuOnScreen @ +1.
    bool PreSimulationInputBuffer::IsSysMenuOnScreen() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbSysMenuOnScreen;
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

    // X360 0x823BBD00 (read, h:759) -> &member @ +180248 (const read twin of the h:752 write accessor).
    const u8* PostSimulationInputBuffer::GetMember_180248() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMember_180248Storage[0];
    }

    // X360 0x823BC300 (read, h:890) -> &member @ +163104.
    const u8* PostSimulationInputBuffer::GetMember_163104() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMember_163104Storage[0];
    }

    // X360 0x8254EC20 (read-lock; "Not locked for reading", h:596) -> &member @ +27680.
    // Read accessor for the embedded active-race-car interface (SetActiveRaceCarInterface
    // @0x823BBA20 XMemCpy's 10480 bytes into this same +27680 offset). RENAME to the real
    // interface-typed accessor when that interface's layout is homed (offset must not move).
    const u8* PostSimulationInputBuffer::GetActiveRaceCarInterfaceRaw_27680() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mActiveRaceCarInterfaceStorage[0];
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
    // PostSimulationInputBuffer -- wave4 producer interfaces
    // ========================================================================
    // Each sub-object below lives inside this buffer's opaque leading padding and is reached by
    // offset reinterpretation onto `this`/a2, exactly like PostSimulationInputBuffer::operator=.
    // Replace the reinterpret_casts with the real named members when the buffer's full layout is
    // homed (offsets must not move).

    // X360 0x823BB828 (write-lock; "Not locked for writing", h:558) -> &member @ +54504.
    // Write twin of the committed read accessor GetVehicleOutputInterfaceRaw_54504() const (0x8254EA28).
    u8* PostSimulationInputBuffer::GetVehicleOutputInterfaceRaw_54504ForWriting()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMember_54504Storage[0];
    }

    // X360 0x823BB978 (write-lock; "Not locked for writing", h:624) -> &member @ +40240.
    // Write twin of the committed read accessor GetMember_40240() const (0x82587DF0); ledger 'GetP'
    // is a DWARF truncation.
    u8* PostSimulationInputBuffer::GetMember_40240ForWriting()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<u8*>(&mMember_40240Storage);
    }

    // X360 0x823C9DF0: assert write-lock, then forward to the embedded GameStateToNetworkInterface's
    // Append at this+53628 (addis+addi = +0x10000-0x2E84). The callee
    // GameStateToNetworkInterface::Append(const GameStateToNetworkInterface*) (0x823C9360) is void;
    // the X360 wrapper's r3 return is a dead tail-call passthrough -> return 0.
    int PostSimulationInputBuffer::AppendGameStateToNetworkInterface(const GameStateToNetworkInterface* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        reinterpret_cast<GameStateToNetworkInterface*>(reinterpret_cast<char*>(this) + 53628)->Append(lpSource);
        return 0;
    }

    // X360 0x823C9D38: assert write-lock, then Append the source queue onto the embedded
    // BaseEventQueue<BrnGameState::TakedownEvent> at this+54168 (+0x10000-0x2C68). No miLength reset
    // in the asm.
    int PostSimulationInputBuffer::AppendTakedownQueue(
            const CgsModule::BaseEventQueue<BrnGameState::TakedownEvent>* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        typedef CgsModule::BaseEventQueue<BrnGameState::TakedownEvent> Queue;
        Queue* lpDst = reinterpret_cast<Queue*>(reinterpret_cast<char*>(this) + 54168);
        return lpDst->Append(*lpSource) ? 1 : 0;
    }

    // X360 0x823C9AE8: assert write-lock, then splice the source vehicle-output interface (a2) into
    // this buffer's embedded copy. Offsets are the asm-attested byte offsets (Hex-Rays _QWORD scaling
    // ignored). Returns the memcpy destination (r3).
    int* PostSimulationInputBuffer::AppendVehicleOutputInterface(const void* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        char*       lpcThis = reinterpret_cast<char*>(this);
        const char* lpcSrc  = reinterpret_cast<const char*>(lpSource);

        // PhysicalTrafficState queue: dst this+9776, src a2+9760.
        {
            typedef CgsModule::BaseEventQueue<BrnPhysics::Vehicle::PhysicalTrafficState> Queue;
            reinterpret_cast<Queue*>(lpcThis + 9776)->Append(
                *reinterpret_cast<const Queue*>(lpcSrc + 9760));
        }
        // ImpactEvent queue: dst this+8992, src a2+8976.
        {
            typedef CgsModule::BaseEventQueue<BrnPhysics::Vehicle::ImpactEvent> Queue;
            reinterpret_cast<Queue*>(lpcThis + 8992)->Append(
                *reinterpret_cast<const Queue*>(lpcSrc + 8976));
        }
        // Game-event queue: VariableEventQueue<1536,16>::Append<1536,16>: dst this+26112, src a2+26096.
        {
            typedef CgsModule::VariableEventQueue<1536, 16> Queue;
            reinterpret_cast<Queue*>(lpcThis + 26112)->Append(
                *reinterpret_cast<const Queue*>(lpcSrc + 26096));
        }

        // std r11,0(r31): copy the leading 8-byte field (this+16 <- *(u64*)a2).
        *reinterpret_cast<u64*>(lpcThis + 16) = *reinterpret_cast<const u64*>(lpcSrc);

        // memcpy(this+32, a2+16, 8960) -- the bulk interface body copy (returns dst in r3).
        return reinterpret_cast<int*>(std::memcpy(lpcThis + 32, lpcSrc + 16, 8960));
    }

    // X360 0x823BBA20: assert lpInterface != null (direct FireAssert, no stream) and write-lock, then
    // XMemCpy the 10480-byte active-race-car interface (a2) into this buffer's embedded copy at
    // this+27680 (pDest=this+0x6C20). XMemCpy modelled as std::memcpy; returns the destination (r3).
    int* PostSimulationInputBuffer::SetActiveRaceCarInterface(const void* lpInterface)
    {
        CGS_ASSERT(lpInterface != nullptr, "lpInterface");
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<int*>(
            std::memcpy(reinterpret_cast<char*>(this) + 27680, lpInterface, 10480));
    }

    // X360 0x823C9C80: assert write-lock, then reset the embedded CrashingTrafficUpdateEvent queue's
    // live count (miLength @ this+38312 == queue_base+8) and Append the source queue (a2) onto it.
    // Queue base is this+38304 (+0x10000-0x6A60); it is the NetworkOutputInterface's lone
    // EventQueue<CrashingTrafficUpdateEvent,24> (offset 0).
    int PostSimulationInputBuffer::SetCrashOutputInterface(const void* lpSource)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        typedef CgsModule::EventQueue<BrnWorld::CrashIO::CrashingTrafficUpdateEvent,
                                      BrnWorld::CrashIO::KI_MAX_NETWORK_TRAFFIC_UPDATES_PER_PLAYER> Queue;
        char*       lpcThis = reinterpret_cast<char*>(this);
        const char* lpcSrc  = reinterpret_cast<const char*>(lpSource);
        *reinterpret_cast<s32*>(lpcThis + 38312) = 0;   // queue miLength = 0 (queue_base+8)
        Queue*       lpDst = reinterpret_cast<Queue*>(lpcThis + 38304);
        const Queue& lSrc  = *reinterpret_cast<const Queue*>(lpcSrc + 0);
        return lpDst->Append(lSrc) ? 1 : 0;
    }

    // X360 0x823C9BD0: FIRST copy the source traffic-network interface (a2) into this buffer's
    // embedded copy at this+38160 (+0x10000-0x6AF0), THEN assert write-lock (the X360 runs the copy
    // BEFORE the lock guard -- order preserved). FLAG (LOW CONFIDENCE): the exact callee symbol is
    // truncated in the ledger; the copy is modelled as a byte-faithful whole-object copy of
    // TrafficNetworkInputInterface (no fabricated member layout, no wrong named call). The +38160
    // target offset and the copy-before-assert ordering are the load-bearing asm-certain facts.
    // Re-home to the real typed copy / correct Input-vs-Output type when the symbol resolves.
    void* PostSimulationInputBuffer::SetTrafficOutputInterface(const void* lpSource)
    {
        using BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface;
        TrafficNetworkInputInterface*       lpDst = reinterpret_cast<TrafficNetworkInputInterface*>(
            reinterpret_cast<char*>(this) + 38160);
        const TrafficNetworkInputInterface* lpSrc = reinterpret_cast<const TrafficNetworkInputInterface*>(lpSource);
        *lpDst = *lpSrc;   // whole-object copy (byte-faithful; truncated callee symbol not resolvable in scope)
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return lpDst;
    }

    // ========================================================================
    // OutputBuffer -- lock-guarded accessors
    // ========================================================================

    // X360 0x823BC060 (read, h:827) -> const VehicleDriverInputInterface* @ +5312.
    const VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const VehicleDriverInputInterface*>(&mVehicleDriverInputInterfaceStorage);
    }

    // X360 0x8254EF68 (write, h:841) -> VehicleDriverInputInterface* @ +5312 (non-const twin; DWARF h:357).
    VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<VehicleDriverInputInterface*>(&mVehicleDriverInputInterfaceStorage);
    }

    // X360 0x823BC1B0 (read, h:855) -> const NetworkInputInterface* @ +147488 (DWARF h:360).
    const NetworkInputInterface* OutputBuffer::GetCrashNetworkInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const NetworkInputInterface*>(&mCrashNetworkInputInterfaceStorage);
    }

    // X360 0x825881F8 (write, h:862) -> NetworkInputInterface* @ +147488 (non-const twin; DWARF h:363).
    NetworkInputInterface* OutputBuffer::GetCrashNetworkInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<NetworkInputInterface*>(&mCrashNetworkInputInterfaceStorage);
    }

    // X360 0x823BBDA8 (read, h:775) -> const GameEventQueue* @ +174576.
    // IDA truncates the name to 'GetGuiEv', but the committed +16 accessor already owns
    // GetGuiEventQueue()const; DWARF h:308 names this offset's read accessor GetGameEventQueue
    // (member mGameEventQueue @ h:451).
    const GameEventQueue* OutputBuffer::GetGameEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const GameEventQueue*>(&mGameEventQueueStorage);
    }

    // X360 0x8254EEC0 (write, h:767) -> GameEventQueue* @ +174576 (non-const twin of h:775; DWARF h:304).
    GameEventQueue* OutputBuffer::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<GameEventQueue*>(&mGameEventQueueStorage);
    }

    // X360 0x823BBBA8 (read, h:719) -> EActiveRaceCarIndex mePlayerActiveRaceCarIndex @ +178688 (lwzx, 4-byte).
    EActiveRaceCarIndex OutputBuffer::GetPlayerActiveRaceCarIndex() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mePlayerActiveRaceCarIndex;
    }

    // X360 0x82587E98 (write, h:737) -> StatsOutputInterface* @ +178692 (non-const twin of h:745; DWARF h:390).
    StatsOutputInterface* OutputBuffer::GetStatsOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return reinterpret_cast<StatsOutputInterface*>(&mStatsOutputInterfaceStorage);
    }

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

    // X360 0x823BC648 (read-lock; "Not locked for reading", h:987) -> &member @ +183408.
    // OutputBuffer read accessor for a GUI-bound sub-interface (BridgeNetworkToGui consumes it;
    // lies within the NetworkToGuiInterface span). RENAME to the real interface-typed accessor
    // when that interface's layout is homed (offset must not move).
    const u8* OutputBuffer::GetMember_183408() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMember_183408Storage[0];
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

    // ---- OutputBuffer state flags (X360 stbx/lbzx byte accessors) --------------------

    // X360 0x823BBE50 (read, h:790) -> bool mbIsPlaying @ +180584 (lbzx).
    bool OutputBuffer::IsPlaying() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsPlaying;
    }

    // X360 0x82587F40 (write, h:783) -> mbIsPlaying @ +180584 = arg (stbx).
    void OutputBuffer::SetIsPlaying(bool lbPlaying)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsPlaying = lbPlaying;
    }

    // X360 0x823BBF00 (read, h:805) -> bool mbIsConnected @ +180585 (lbzx).
    bool OutputBuffer::IsConnected() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsConnected;
    }

    // X360 0x82587FF0 (write, h:798) -> mbIsConnected @ +180585 = arg (stbx).
    void OutputBuffer::SetConnected(bool lbConnected)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsConnected = lbConnected;
    }

    // X360 0x823BBFB0 (read, h:820) -> bool mbIsInInvite @ +180586 (lbzx).
    bool OutputBuffer::IsInInvite() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mbIsInInvite;
    }

    // X360 0x825880A0 (write, h:813) -> mbIsInInvite @ +180586 = arg (stbx).
    void OutputBuffer::SetIsInInvite(bool lbInInvite)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mbIsInInvite = lbInInvite;
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
        // PlayerResultsData is now homed (28-byte POD), so the X360 "8x 28B memberwise copy"
        // (0x823B8F68) is the per-record member-wise assignment -- NOT a byte loop over the old
        // u8[8*28] view (which would index the typed[8] array out of bounds).
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            maPlayerResultsData[liIndex] = lOther.maPlayerResultsData[liIndex];
        }
        return *this;
    }

    // X360 0x82355B10 (ledger stub "PlayerRe").
    // Resets all 8 per-player end-of-round result records to their empty defaults. The X360 body is
    // the compiler's flat lowering of PlayerResultsData::Clear() inlined into an 8-iteration loop
    // (28-byte record stride). Per record: mFinishTime is zeroed via the out-of-line
    // CgsSystem::Time::SetFloatVal(0.0f), the two race-car slots are set to "none" (-1), the finish
    // distance is -1.0f, eliminations is 0, and the three trailing bool bytes are cleared.
    //
    // NOTE: the two EActiveRaceCarIndex fields resolve to BrnNetwork::EActiveRaceCarIndex (from
    // BrnNetworkSharedIO.h; BurnoutConstants.h is NOT included by BrnNetworkModuleIO.h), whose only
    // enumerator is E_ACTIVE_RACE_CAR_NONE == -1 -- exactly the X360 store. Using the
    // ::E_ACTIVE_RACE_CAR_INDEX_INVALID spelling here would not compile (out of scope). This mirrors
    // the committed sibling InGamePlayerStatusData::Clear.
    void PlayerResultsInterface::Clear()
    {
        for (s32 liIndex = 0; liIndex < 8; ++liIndex)
        {
            PlayerResultsData& lRecord = maPlayerResultsData[liIndex];

            lRecord.mFinishTime.SetFloatVal(0.0f);                 // +0x00 (out-of-line SetFloatVal)
            lRecord.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_NONE; // +0x08 (stw -1)
            lRecord.meEliminatorIndex    = E_ACTIVE_RACE_CAR_NONE; // +0x0C (stw -1)
            lRecord.mfDistanceToFinish   = -1.0f;                  // +0x10 (stfs flt_820037C8)
            lRecord.miEliminations       = 0;                      // +0x14 (stw 0)
            lRecord.mbValid              = false;                  // +0x18 (stb 0)
            lRecord.mbTimedOut           = false;                  // +0x19 (stb 0)
            lRecord.mbEliminated         = false;                  // +0x1A (stb 0)
        }
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
