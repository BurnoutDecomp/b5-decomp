#ifndef BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
#define BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H

#include <cstddef>   // offsetof (buffer layout asserts)
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                 // ::EActiveRaceCarIndex (mePlayerActiveRaceCarIndex)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (base; lock state machine)
#include "GameShared/GameClasses/Module/CgsEventQueue.h" // CgsModule::EventQueue<T,N> (AudioCarLoadedDataQueue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<N,16> (RootOutputBuffer::Construct)
#include <cstring>                                      // memset (RootOutputBuffer::Construct trailing-state clear)
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h" // AudioEffectsMessageQueue (adopted real type)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // AudioCarDataLoadedEvent (queue element)

// =============================================================================
// BrnSound::Module::Io buffer accessors
//   GameSource/Sound/Module/BrnRootSoundModuleIo.h (DWARF home) +
//   GameSource/Sound/Module/BrnRootSoundModuleIO.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The BrnSound::Module::Io buffers
// (LogicInputBuffer, RootInputBuffer, ...) derive from CgsModule::IOBuffer: the
// status flag byte sits at offset 0, and the lock-guarded Get* accessors assert
// the right lock (read-lock bit 4 for const getters, write-lock bit 3 for the
// mutable getters) then return &member-at-X360-offset. Same shape as the
// committed BrnGameState::GameStateModuleIO buffers.
//
// This group lands exactly two of those accessors (the two TU functions):
//   * LogicInputBuffer::GetVehicleInterface() const  (X360 0x82694D30, truncated
//     to "BrnSound::Module::I" in the IDA export) -- read-locked, returns the
//     embedded vehicle (active-race-car output) interface at this+0x620. Proven
//     by VehicleStateManager::OnAssetLoaded: it calls this on GetBrnInputStructure()
//     (a LogicInputBuffer*) and feeds the result to
//     RCEntityActiveRaceCarOutputInterface::GetPlay(...).
//   * RootInputBuffer::GetPropUpdateNotificationQueue()  (X360 0x823B8518,
//     truncated to "BrnSound::Module::Io") -- write-locked, returns the
//     prop-update notification queue at this+0x10520. Proven by
//     BrnGameModule::BridgeWorldToSound: the result is passed straight to
//     PropUpdateNotification_::Append(...).
//
// MINIMAL SLICE: each buffer models only its touched member, pinned to its exact
// X360 byte offset with explicit u8 storage for the gap (the BrnGameStateModuleIO
// precedent). The member types are forward-declared incomplete classes; the
// touched member is named opaque storage of the correct width at the correct
// offset, so the buffer's later full reconstruction can replace the storage with
// the real typed member without moving anything. Offsets ARE asserted here: only
// the IOBuffer base + u8 storage precede each touched member (no host-width
// pointers), so offsetof is byte-faithful on the 64-bit gate.
// FLAG: the IDA-truncated method names (GetVehicleInterface / GetPropUpdate-
// NotificationQueue) are inferred from the call-site usage + the DWARF method list
// (BrnRootSoundModuleIo.h lines 108 / 226); the X360 bodies (lock assert + return
// &member-at-offset) are exact.
// =============================================================================

// The vehicle interface returned by LogicInputBuffer::GetVehicleInterface is the
// race-car active-output interface; only a pointer/reference is handed out, so an
// incomplete forward declaration suffices (its full layout lives in its own TU).
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnSound
{
namespace Module
{
namespace Io
{
    // The prop-update notification queue handed out (by pointer) from
    // RootInputBuffer::GetPropUpdateNotificationQueue. Promoted from a forward `class`
    // decl to a complete opaque struct so it can be an inline by-value RootInputBuffer
    // member (the caller PropUpdateNotification_::Append supplies the queue operations in
    // its own TU). FLAG: 0x40-byte width is nominal (only the +0x10520 offset is X360-
    // attested; the following member's offset is absorbed by an explicit pad).
    struct PropUpdateNotificationQueue { u8 mData[0x40]; };

    // The prop-became-physical event queue handed out (by pointer) from
    // RootInputBuffer::GetPropBecamePhysicalEventQueue @ +0x103D0. Opaque, correctly
    // sized to its X360-attested span (+0x103D0 .. +0x10520 == 0x150 bytes) so the
    // following PropUpdateNotificationQueue keeps its +0x10520 offset.
    struct PropBecamePhysicalEventQueue { u8 mData[0x10520 - 0x103D0]; }; // 0x150 attested span

    // =========================================================================
    // BrnSound::Module::Io::PreUpdateOutput -- MINIMAL SLICE (canonical home is
    // GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h:149; reproduced here
    // only as far as RootPreUpdateOutputBuffer::SetPreUpdateOutput touches it).
    //
    // DWARF member order (BrnPreUpdateSharedIo.h:212-214):
    //   +0x000  GuiOutEventQueue (GuiEventQueueBase<256,16>)  mGuiOutEventQueue
    //   +0x110  EventQueue<AudioCarDataLoadedEvent,16>        mAudioCarDataLoadedQueue
    //   +0x2A0  AudioEffectsMessageQueue (VariableEventQueue<128,16>) mAudioEffectsMessageQueue
    //   sizeof == 0x330 (816)
    //
    // The X360 SetPreUpdateOutput (0x826E0C10) copies this struct in three steps,
    // which fixes the offsets:
    //   memcpy(dst+0x000, src+0x000, 0x110)   -> the GuiOut queue region
    //   dst.mAudioCarDataLoadedQueue.miLength = 0; Append(src.mAudioCarDataLoadedQueue)
    //   memcpy(dst+0x2A0, src+0x2A0, 0x090)   -> the AudioEffects queue region
    // The middle queue is copied via Clear()+Append (NOT memcpy) because it owns a
    // self-referential mpEvents pointer into its own inline buffer.
    //
    // The GuiOut and AudioEffects queues are NOT touched member-wise by this group,
    // so they are modelled as named opaque byte storage of their exact X360 width;
    // the real typed members land with the BrnPreUpdateSharedIo.h TU without moving
    // anything. The AudioCarDataLoadedQueue IS modelled as its real committed type
    // (EventQueue<AudioCarDataLoadedEvent,16>) so the Clear()+Append the X360 body
    // performs are reconstructed by name against the committed queue generics.
    // FLAG: minimal slice -- the omitted GuiOut/AudioEffects queue internals are
    // reconstructed by the BrnPreUpdateSharedIo.h TU; do not duplicate them here.
    // =========================================================================
    typedef CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>
        AudioCarLoadedDataQueue;   // BrnWorldModuleIO.h:1505 (DWARF typedef)

    struct PreUpdateOutput
    {
        // GetCarDataLoadedQueue() const (BrnPreUpdateSharedIo.h:193); referenced by
        // SetPreUpdateOutput to merge the source's audio-car-loaded events.
        const AudioCarLoadedDataQueue& GetCarDataLoadedQueue() const { return mAudioCarDataLoadedQueue; }
        AudioCarLoadedDataQueue&       GetCarDataLoadedQueue()       { return mAudioCarDataLoadedQueue; }

        // GetAudioEffectsMessageQueue (BrnPreUpdateSharedIo.h:200/:206) -- X360
        // header-inlines (BrnGame::BrnGameModule::BridgeSoundToTraining @0x823C63C0
        // folds the const one to GetPreUpdateOutput()+0x2A0).
        const AudioEffectsMessageQueue& GetAudioEffectsMessageQueue() const { return mAudioEffectsMessageQueue; }
        AudioEffectsMessageQueue&       GetAudioEffectsMessageQueue()       { return mAudioEffectsMessageQueue; }

        u8                      maGuiOutEventQueueStorage[0x110];     // @ +0x000 mGuiOutEventQueue
        AudioCarLoadedDataQueue mAudioCarDataLoadedQueue;            // @ +0x110 (0x190 wide)
        // Adopted real type (was named opaque u8[0x90] storage): the committed
        // AudioEffectsMessageQueue (SharedIO/BrnPreUpdateSharedIo.h) is pointer-free
        // (inline buffer + s32 cursors), so its host sizeof stays the X360 0x90 and
        // SetPreUpdateOutput's whole-region memcpy remains valid.
        AudioEffectsMessageQueue mAudioEffectsMessageQueue;          // @ +0x2A0 mAudioEffectsMessageQueue

        static void _AssertLayout()
        {
            // X360 byte-faithful up to the audio-car-loaded queue: the leading GuiOut
            // region is pure POD storage (no host-wider members), so the queue's start
            // is asserted at its exact X360 offset (load-bearing for the memcpy split).
            static_assert(offsetof(PreUpdateOutput, mAudioCarDataLoadedQueue) == 0x110,
                          "PreUpdateOutput.mAudioCarDataLoadedQueue @ +0x110");
            // X360-32bit vs host-64bit divergence: AudioCarDataLoadedEvent embeds a
            // host-wider pointer (mpVehicleListEntry: 4B on X360, 8B on host) and the
            // queue's mpEvents pointer is likewise host-wider, so the queue is larger on
            // the 64-bit gate than the X360 0x190. The trailing AudioEffects region and
            // total size therefore CANNOT be asserted at the X360 offsets 0x2A0 / 0x330;
            // members are pinned BY NAME and SEQUENCE only past the queue. The three-step
            // SetPreUpdateOutput copy stays correct because it copies each region by its
            // named bounds (sizeof storage / Clear()+Append), never by absolute offset.
        }
    };

    // BrnSound::Module::Io::LogicInputBuffer -- the per-frame logic input payload the
    // sound logic module reads. Derives from CgsModule::IOBuffer.
    struct LogicInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x82694D30 (read-lock; "Not locked for reading", BrnRootSoundModuleIo.h:405)
        // -- the embedded vehicle (active-race-car output) interface at this+0x620.
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
            GetVehicleInterface() const;

    private:
        u8 maPadToVehicleInterface[0x620 - sizeof(CgsModule::IOBuffer)]; // base end -> +0x0620
        // RCEntityActiveRaceCarOutputInterface @ +0x620; named opaque storage (full
        // layout lives in its own TU). GetVehicleInterface returns &this member.
        u8 mVehicleInterfaceStorage[0x40];                               // @ +0x0620

        static void _AssertLayout()
        {
            static_assert(offsetof(LogicInputBuffer, mVehicleInterfaceStorage) == 0x620,
                          "RCEntityActiveRaceCarOutputInterface @ +0x620");
        }
    };

    // =========================================================================
    // BrnSound::Module::Io::RootInputBuffer -- the root sound module input payload
    // the BridgeWorldToSound bridge fills. Derives from CgsModule::IOBuffer.
    //
    // Grown from the minimal slice to the full byte-exact DWARF layout
    // (BrnRootSoundModuleIo.h member decls) this wave's 19 RootInputBuffer accessors +
    // the 6 folded bare-class accessors run. Each member is pinned to its X360-attested
    // byte offset; members whose own offset is not attested by an accessor are placed by
    // DWARF name+sequence and absorbed by named pad spans. Member-interface types are
    // DWARF-named, correctly-sized opaque storage (sizes from the attested memcpy widths
    // where available, else nominal + absorbed by the following pad).
    //
    // FLAG(sizes): where a setter copies via operator= (no asm-attested width) the
    // interface struct is a nominal opaque size and the NEXT member's exact X360 offset
    // is preserved by the explicit pad -- the member's own start offset is always exact.
    // The queue setters (SetPhysicalTrafficStates/SetWorldLoadInterface) model a
    // reset-count-then-Append as a wholesale memcpy over pointer-free inline queue
    // storage; stride/capacity not attested (start offset exact).
    // =========================================================================

    struct RootInputBuffer : public CgsModule::IOBuffer
    {
        // DWARF-named member-interface types (nested so accessor bodies name them
        // RootInputBuffer::X). Sizes are the X360-attested copy widths where available,
        // else nominal (absorbed by pads).
        struct ReplayStatusInterface         { u8 mData[4]; };
        struct DirectorCamera                { u8 mData[4]; };
        struct InputContactSpyQueueInterface { u32 mData; };
        struct GameEventQueue                { u8 mData[4]; };
        struct TrafficSoundOutputInterface   { u8 mData[4]; };
        struct PhysicalTrafficStateQueue     { u8 mData[16]; };
        struct DeformationInterface          { u8 mData[4]; };
        struct ScoringOutputInterface        { u8 mData[0xAB0]; };   // XMemCpy 0xAB0 (SetScoringInterface)
        struct OnlineScoringOutputInterface  { u8 mData[0xA4]; };    // memcpy 0xA4  (SetOnlineScoringInterface)
        struct SoundWorldLoadInterface       { u8 mData[16]; };
        struct GuiEventQueue                 { u8 mData[4]; };
        struct GameModeOutputInterface       { u8 mData[0x10]; };    // 4-word copy (SetGameModeInterface)
        struct UpdateInfo                    { u8 mData[1]; };       // single-byte copy (SetUpdateInfo)
        struct AICarOutputInterface          { u8 mData[0x14E8]; };  // memcpy 0x14E8 (SetAICarOutputInterface)
        struct GuiAudioEventResults          { u8 mData[4]; };

        // InputBuffer::GameActionQueue typedef (BrnRootSoundModuleIo.h:49). Modelled as
        // correctly-sized opaque storage: its span is +0x3084 .. +0x6494 (internal layout
        // unattested here). Returned by reference from GetGameActionQueue().
        struct GameActionQueue               { u8 mData[0x6494 - 0x3084]; }; // @ +0x03084 (0x3410 wide)

        // ---- read-lock const getters (this wave) ------------------------------------
        // X360 0x82694940 / 0x823B83C8 -- the game event queue @ +0x6494.
        const GameEventQueue* GetGameEventQueue() const;
        GameEventQueue*       GetGameEventQueue();
        // X360 0x82695128 -- the gui-audio event results @ +0x13730.
        const GuiAudioEventResults* GetGuiAudioEventResults() const;
        // X360 0x82695078 -- the gui event queue pointer @ +0xEBA8.
        const GuiEventQueue* GetGuiEventQueue() const;
        // X360 0x82694F28 -- the player's active-race-car slot index @ +0x2F10.
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const;
        // X360 0x82694550 -- the per-frame update info @ +0xEBBC.
        const UpdateInfo* GetUpdateInfo() const;
        // X360 0x826947F0 -- the online-scoring output interface @ +0xEA30 (folded bare class).
        const OnlineScoringOutputInterface* GetOnlineScoringInterface() const;
        // X360 0x826945F8 -- the game-mode output interface @ +0xEBAC (folded bare class).
        const GameModeOutputInterface* GetGameModeInterface() const;
        // X360 0x82694748 -- the scoring output interface @ +0xDF80 (folded bare class).
        const ScoringOutputInterface* GetScoringInterface() const;
        // X360 0x82694E80 -- the deformation output interface @ +0xB490 (by reference, folded bare class).
        const DeformationInterface& GetDeformationInterface() const;
        // X360 0x82694C88 (read) / 0x823B8668 (write) -- the game-action queue @ +0x3084 (by reference).
        const GameActionQueue& GetGameActionQueue() const;
        GameActionQueue&       GetGameActionQueue();
        // X360 0x823B8518 (write-lock) -- the prop-update notification queue @ +0x10520.
        PropUpdateNotificationQueue* GetPropUpdateNotificationQueue();
        // X360 0x826949E8 (read-lock) -- CONST overload of the prop-update notification queue @ +0x10520.
        const PropUpdateNotificationQueue* GetPropUpdateNotificationQueue() const;
        // X360 0x823B8470 (write-lock) -- the prop-became-physical event queue @ +0x103D0.
        PropBecamePhysicalEventQueue* GetPropBecamePhysicalEventQueue();

        // ---- write-lock setters (this wave) -----------------------------------------
        void SetReplayStatusInterface(const ReplayStatusInterface* lpInterface);            // X360 0x823B7EC0 @ +0x0004
        void SetCameraInput(const DirectorCamera* lpCamera);                                // X360 0x823C9140 @ +0x2F20
        void SetContactSpyQueueInterface(const InputContactSpyQueueInterface* lpInterface); // X360 0x823B8908 @ +0x3080
        void SetTrafficOutputInterface(const TrafficSoundOutputInterface* lpInterface);     // X360 0x823B8710 @ +0x6AB0
        void SetPhysicalTrafficStates(const PhysicalTrafficStateQueue* lpStates);           // X360 0x823C9088 @ +0x74C0
        void SetDeformationInterface(const DeformationInterface* lpInterface);              // X360 0x823C91F0 @ +0xB490
        void SetScoringInterface(const ScoringOutputInterface* lpInterface);                // X360 0x823B81A0 @ +0xDF80
        void SetOnlineScoringInterface(const OnlineScoringOutputInterface* lpInterface);    // X360 0x823B8258 @ +0xEA30
        void SetWorldLoadInterface(const SoundWorldLoadInterface* lpInterface);             // X360 0x823C92A8 @ +0xEAD4
        void SetGuiEventQueue(const GuiEventQueue* lpQueue);                                // X360 0x823B89B8 @ +0xEBA8
        void SetGameModeInterface(const GameModeOutputInterface* lpInterface);              // X360 0x823B8028 @ +0xEBAC
        void SetUpdateInfo(const UpdateInfo* lpInfo);                                       // X360 0x823B7F70 @ +0xEBBC
        void SetAICarOutputInterface(const AICarOutputInterface* lpInterface);              // X360 0x823B8310 @ +0xEEE0

    private:
        u8 maPad0[0x004 - sizeof(CgsModule::IOBuffer)];                               // base end -> +0x00004
        ReplayStatusInterface         mReplayStatusInterface;      // @ +0x00004
        u8 maPad1[0x2F10 - (0x004 + sizeof(ReplayStatusInterface))];
        EActiveRaceCarIndex           mePlayerActiveRaceCarIndex;  // @ +0x02F10
        u8 maPad2[0x2F20 - (0x2F10 + sizeof(EActiveRaceCarIndex))];
        DirectorCamera                mDirectorCamera;             // @ +0x02F20
        u8 maPad3[0x3080 - (0x2F20 + sizeof(DirectorCamera))];
        InputContactSpyQueueInterface mContactSpyQueueInterface;   // @ +0x03080
        GameActionQueue               mGameActionQueue;            // @ +0x03084 (carved from the old pad)
        // (no pad: GameActionQueue is sized 0x3084..0x6494 exactly, so mGameEventQueue @ +0x06494)
        GameEventQueue                mGameEventQueue;             // @ +0x06494
        u8 maPad5[0x6AB0 - (0x6494 + sizeof(GameEventQueue))];
        TrafficSoundOutputInterface   mTrafficOutputInterface;     // @ +0x06AB0
        u8 maPad6[0x74C0 - (0x6AB0 + sizeof(TrafficSoundOutputInterface))];
        PhysicalTrafficStateQueue     mPhysicalTrafficStates;      // @ +0x074C0
        u8 maPad7[0xB490 - (0x74C0 + sizeof(PhysicalTrafficStateQueue))];
        DeformationInterface          mDeformationInterface;       // @ +0x0B490
        u8 maPad8[0xDF80 - (0xB490 + sizeof(DeformationInterface))];
        ScoringOutputInterface        mScoringInterface;           // @ +0x0DF80
        // (no pad: ScoringOutputInterface is 0xAB0, filling 0xDF80..0xEA30 exactly)
        OnlineScoringOutputInterface  mOnlineScoringInterface;     // @ +0x0EA30
        // (no pad: OnlineScoringOutputInterface is 0xA4, filling 0xEA30..0xEAD4 exactly)
        SoundWorldLoadInterface       mWorldLoadInterface;         // @ +0x0EAD4
        u8 maPad11[0xEBA8 - (0xEAD4 + sizeof(SoundWorldLoadInterface))];
        const GuiEventQueue*          mpGuiEventQueue;             // @ +0x0EBA8 (host-widens past here)
        GameModeOutputInterface       mGameModeInterface;          // @ +0x0EBAC
        UpdateInfo                    mUpdateInfo;                 // @ +0x0EBBC
        u8 maPad12[0xEEE0 - (0xEBBC + sizeof(UpdateInfo))];
        AICarOutputInterface          mAICarOutputInterface;       // @ +0x0EEE0
        u8 maPad13[0x103D0 - (0xEEE0 + sizeof(AICarOutputInterface))];
        PropBecamePhysicalEventQueue  mPropBecamePhysicalEventQueue; // @ +0x103D0 (0x150)
        PropUpdateNotificationQueue   mPropUpdateNotificationQueue;// @ +0x10520
        u8 maPad14[0x13730 - (0x10520 + sizeof(PropUpdateNotificationQueue))];
        GuiAudioEventResults          mGuiAudioEventResults;       // @ +0x13730

        static void _AssertLayout()
        {
            // Byte-faithful up to mpGuiEventQueue: only the 1-byte IOBuffer base + POD
            // opaque storage precede these members (no host-wider pointers), so offsetof
            // matches the X360 offsets exactly. Members past the host-widening
            // mpGuiEventQueue pointer are pinned by DWARF name + sequence only.
            static_assert(offsetof(RootInputBuffer, mReplayStatusInterface)  == 0x00004, "mReplayStatusInterface @ +0x004");
            static_assert(offsetof(RootInputBuffer, mePlayerActiveRaceCarIndex) == 0x02F10, "mePlayerActiveRaceCarIndex @ +0x2F10");
            static_assert(offsetof(RootInputBuffer, mDirectorCamera)         == 0x02F20, "mDirectorCamera @ +0x2F20");
            static_assert(offsetof(RootInputBuffer, mContactSpyQueueInterface) == 0x03080, "mContactSpyQueueInterface @ +0x3080");
            static_assert(offsetof(RootInputBuffer, mGameActionQueue)        == 0x03084, "mGameActionQueue @ +0x3084");
            static_assert(offsetof(RootInputBuffer, mGameEventQueue)         == 0x06494, "mGameEventQueue @ +0x6494");
            static_assert(offsetof(RootInputBuffer, mTrafficOutputInterface) == 0x06AB0, "mTrafficOutputInterface @ +0x6AB0");
            static_assert(offsetof(RootInputBuffer, mPhysicalTrafficStates)  == 0x074C0, "mPhysicalTrafficStates @ +0x74C0");
            static_assert(offsetof(RootInputBuffer, mDeformationInterface)   == 0x0B490, "mDeformationInterface @ +0xB490");
            static_assert(offsetof(RootInputBuffer, mScoringInterface)       == 0x0DF80, "mScoringInterface @ +0xDF80");
            static_assert(offsetof(RootInputBuffer, mOnlineScoringInterface) == 0x0EA30, "mOnlineScoringInterface @ +0xEA30");
            static_assert(offsetof(RootInputBuffer, mWorldLoadInterface)     == 0x0EAD4, "mWorldLoadInterface @ +0xEAD4");
        }
    };

    // BrnSound::Module::Io::RootPreUpdateOutputBuffer (DWARF BrnRootSoundModuleIo.h:322)
    // -- the root sound module's pre-update output payload. Derives from CgsModule::
    // IOBuffer and holds a single PreUpdateOutput. The X360 places mPreUpdateOutput at
    // this+0x08: the 1-byte IOBuffer status base pads up to the PreUpdateOutput's
    // 8-byte alignment (its leading queue member is 8-aligned). Confirmed by the two
    // accessor bodies returning/operating at this+0x08:
    //   * GetPreUpdateOutput() const  (X360 0x823B8BB8, read-lock "Not locked for
    //     reading", BrnRootSoundModuleIo.h:338) -- returns &mPreUpdateOutput.
    //   * SetPreUpdateOutput(const PreUpdateOutput&)  (X360 0x826E0C10, write-lock
    //     "Not locked for writing", BrnRootSoundModuleIo.h:346) -- copies the source
    //     PreUpdateOutput into mPreUpdateOutput (POD spans by memcpy, the
    //     audio-car-loaded queue by Clear()+Append).
    struct RootPreUpdateOutputBuffer : public CgsModule::IOBuffer
    {
        // BrnRootSoundModuleIo.h:331 (DWARF; own TU, declared-only here).
        void Construct();

        // BrnRootSoundModuleIo.h:338 (DWARF). X360 0x823B8BB8.
        const PreUpdateOutput& GetPreUpdateOutput() const;

        // BrnRootSoundModuleIo.h:346 (DWARF). X360 0x826E0C10.
        void SetPreUpdateOutput(const PreUpdateOutput& lPreUpdateOutput);

    private:
        PreUpdateOutput mPreUpdateOutput;   // @ +0x08 (X360)

        static void _AssertLayout()
        {
            static_assert(offsetof(RootPreUpdateOutputBuffer, mPreUpdateOutput) == 0x08,
                          "RootPreUpdateOutputBuffer.mPreUpdateOutput @ +0x08");
        }
    };

    // =========================================================================
    // BrnSound::Module::Io::RootOutputBuffer (DWARF BrnRootSoundModuleIo.h:270) -- the root
    // sound module's output payload the load path forwards into the GameData input queue.
    // Grown from the empty minimal slice to the DWARF shape this wave's request-interface
    // accessors (funcs 00-03, 12, 13) run.
    //
    // X360-ATTESTED OFFSETS (this wave's asm return tails):
    //   ::G          0x823B8A68 -> this+0x04    => mResourceRequestInterface @ +0x0004 (read-lock)
    //   ::GetReso    0x826951D0 -> this+0x04    (write-lock, same member)
    //   ::RootOutp   0x823B8B10 -> this+0x1014  => mAttribSysRequestInterface @ +0x1014 (read-lock)
    //   ::RootOutputBuff 0x82695278 -> this+0x1014 (write-lock, same member)
    //   ::GetReplayR 0x823B85C0 -> this+0x1824  => mReplayRequestInterface  @ +0x1824 (read-lock)
    //   ::GetReplayRequest 0x82694A90 -> this+0x1824  (write-lock, same member)
    // Spans: mResourceRequestInterface = 0x1014-0x04 = 0x1010 (RequestInterface<4096>);
    //        mAttribSysRequestInterface = 0x1824-0x1014 = 0x0810 (AttribSysRequestInterface<2048>).
    //
    // MINIMAL SLICE: the request-interface members are modelled as named opaque byte storage
    // of their exact X360 widths, pinned to their X360 offsets, rather than pulling in the
    // RequestInterface<>/AttribSysRequestInterface<> template layouts (not needed to satisfy
    // these accessors, which only assert a lock bit and return &member). When
    // RootSoundModule::Prepare consumes the real interface operations, replace each *Storage
    // with its committed typed member without moving anything. The forward-declared typedefs
    // give the accessor return types their DWARF names.
    // FLAG(medium): mReplayRequestInterfaceStorage trailing width is NOT X360-attested (last
    // member, nothing follows). Its START offset (+0x1824) IS attested by both replay
    // accessors; the width 0x1010 mirrors sizeof RequestInterface<4096> (DWARF typedefs
    // ReplayRequestInterface = RequestInterface, same default cap) but is provisional.
    //
    // The nested typedefs are PUBLIC because LogicOutputBuffer's members reference them
    // (RootOutputBuffer::AttribSysRequestInterface / ::SoundResourceRequestInterface).
    // =========================================================================

    // Return-type name tags for the accessors (DWARF typedefs; full template layouts land
    // with the request-interface TUs). Incomplete forward declarations suffice: the accessors
    // only hand out a pointer to embedded storage.
    // ⭐ DEFINED 2026-08-17 (boot audit F-P6-17 / F-P5-10). These were forward declarations
    // only -- "full template layouts land with the request-interface TUs" -- which meant the
    // accessors below returned pointers to INCOMPLETE types and LoadSoundModule's forwarding
    // arm could not be written at all: the sound module's initial-load resource requests had
    // nowhere to go.
    //
    // The layout is derived, not guessed, and three sources agree:
    //   1. the sibling interfaces (GameDataIO::RequestInterface<N>,
    //      AttribSysIO::AttribSysRequestInterface<N>) each hold their queue -- a
    //      VariableEventQueue<N,16> -- at offset 0, and nothing else;
    //   2. the X360 spans measured from this buffer's own accessor return tails:
    //      RequestInterface<4096> = 0x1014-0x04 = 0x1010, AttribSysRequestInterface<2048>
    //      = 0x1824-0x1014 = 0x810. VariableEventQueue<BUFSIZE,16> is BUFSIZE+16 bytes by
    //      its documented layout (+0 bool, +1 macData[BUFSIZE], +BUFSIZE+4/+8/+12 the three
    //      s32s), so 4096+16 == 0x1010 and 2048+16 == 0x810 exactly;
    //   3. RootOutputBuffer::Construct in this very header already reinterpret_casts the two
    //      storage blocks to VariableEventQueue<4096,16> and VariableEventQueue<2048,16>.
    //
    // The opaque *Storage members stay exactly where they are, so no offset moves; these
    // types are the typed view the accessors hand out.
    template <int N> struct RequestInterface
    {
        CgsModule::VariableEventQueue<N, 16> mRequestQueue;   // offset 0, per the siblings
    };
    template <int N> struct AttribSysRequestInterface
    {
        CgsModule::VariableEventQueue<N, 16> mRequestQueue;   // offset 0, per the siblings
    };

    struct RootOutputBuffer : public CgsModule::IOBuffer
    {
        typedef RequestInterface<4096>          SoundResourceRequestInterface; // BrnSoundCommonSharedIO.h:37
        typedef AttribSysRequestInterface<2048> AttribSysRequestInterface;     // BrnSoundLogicSharedIO.h:47
        typedef RequestInterface<4096>          ReplayRequestInterface;        // BrnRootSoundModuleIo.h:56 (RequestInterface, default cap)

        // BrnRootSoundModuleIo.h:284 (DWARF). X360 body @0x826AF448, reached from the
        // CreateIOBuffer<RootOutputBuffer> instantiation @0x823AD458 (Alloc size 6224):
        //     *a1 = 1;                                              -- IOBuffer::Construct
        //     VariableEventQueue<4096,16>::Construct(a1 + 4);    Clear(a1 + 4);
        //     VariableEventQueue<2048,16>::Construct(a1 + 4116); Clear(a1 + 4116);
        //     for (11 words at a1 + 6180) *w++ = 0;
        // 4 == mResourceRequestInterface, 4116 == 0x1014 == mAttribSysRequestInterface,
        // 6180 == 0x1824 == mReplayRequestInterface. The two request interfaces derive from
        // those queues, so the Construct+Clear pair is the queue's own -- reached through the
        // members' named storage (same discipline as the accessors below, which the opaque
        // request-interface storage forces).
        // FLAG PC: homed inline here rather than in BrnRootSoundModuleIO.cpp (the console emits
        // it out-of-line) because that TU is not on the exe build list yet and the template now
        // REFERENCES this symbol from every CreateIOBuffer<RootOutputBuffer> site.
        // FLAG(medium), pre-existing and unchanged by this edit: the console object is 6224
        // bytes and the 11-word (44-byte) run at +0x1824 is all the trailing state it has, so
        // mReplayRequestInterfaceStorage's provisional 0x1010 width over-states sizeof by 4068
        // bytes. Only the attested 44 bytes are cleared here; the width is left alone (a layout
        // change is out of this change's scope -- see the report).
        void Construct()
        {
            CgsModule::IOBuffer::Construct();

            CgsModule::VariableEventQueue<4096, 16>* lpResourceQueue =
                reinterpret_cast<CgsModule::VariableEventQueue<4096, 16>*>(mResourceRequestInterfaceStorage);
            lpResourceQueue->Construct();
            lpResourceQueue->Clear();

            CgsModule::VariableEventQueue<2048, 16>* lpAttribSysQueue =
                reinterpret_cast<CgsModule::VariableEventQueue<2048, 16>*>(mAttribSysRequestInterfaceStorage);
            lpAttribSysQueue->Construct();
            lpAttribSysQueue->Clear();

            memset(mReplayRequestInterfaceStorage, 0, KI_ReplayStateWordsCleared * sizeof(u32));
        }

        // X360 ::G 0x823B8A68 (read-lock, DWARF :289) / ::GetReso 0x826951D0 (write-lock, DWARF :292)
        // -> &mResourceRequestInterface (+0x04).
        const SoundResourceRequestInterface* GetResourceRequestInterface() const;
        SoundResourceRequestInterface*       GetResourceRequestInterface();

        // X360 ::RootOutp 0x823B8B10 (read-lock const, DWARF :295) / ::RootOutputBuff 0x82695278
        // (write-lock non-const, DWARF :298) -> &mAttribSysRequestInterface (+0x1014).
        const AttribSysRequestInterface* GetAttribSysRequestInterface() const;
        AttribSysRequestInterface*       GetAttribSysRequestInterface();

        // X360 ::GetReplayR 0x823B85C0 (read-lock const, DWARF :300) / ::GetReplayRequest 0x82694A90
        // (write-lock non-const, DWARF false-negative) -> &mReplayRequestInterface (+0x1824).
        const ReplayRequestInterface* GetReplayRequestInterface() const;
        ReplayRequestInterface*       GetReplayRequestInterface();

        // BrnRootSoundModuleIo.h:301 (DWARF; own TU, declared-only here).
        void SetReplayRequestInterface(const ReplayRequestInterface* lpReplayRequestInterface);

    private:
        // Byte widths of each opaque interface span (derived from the attested X360 offsets).
        static const int KI_ResourceInterfaceBytes  = 0x1010; // +0x0004 .. +0x1014 (RequestInterface<4096>)
        static const int KI_AttribSysInterfaceBytes = 0x0810; // +0x1014 .. +0x1824 (AttribSysRequestInterface<2048>)
        static const int KI_ReplayInterfaceBytes    = 0x1010; // +0x1824 ..        (width provisional; start attested)

        // X360 @0x826AF448 zeroes exactly 11 words (44 bytes) from +0x1824 -- `v5 = 11; do
        // *v4++ = 0; while (--v5);`. That run, not KI_ReplayInterfaceBytes, is the console's
        // whole trailing-state clear (6224 - 0x1824 == 44).
        static const int KI_ReplayStateWordsCleared = 11;

        // IOBuffer base is a single status byte; u8 storage is 1-byte aligned, so the first
        // member sits at +0x04 with an explicit 3-byte gap.
        u8 maStatusPad[0x04 - sizeof(CgsModule::IOBuffer)];               // base end -> +0x0004
        u8 mResourceRequestInterfaceStorage[KI_ResourceInterfaceBytes];  // @ +0x0004 (0x1010 wide)
        u8 mAttribSysRequestInterfaceStorage[KI_AttribSysInterfaceBytes];// @ +0x1014 (0x0810 wide)
        u8 mReplayRequestInterfaceStorage[KI_ReplayInterfaceBytes];      // @ +0x1824 (width provisional)

        static void _AssertLayout()
        {
            // Only the 1-byte IOBuffer base + POD u8 storage precede each member (no host-wider
            // pointers), so these X360 offsets are byte-faithful on the 64-bit gate.
            static_assert(offsetof(RootOutputBuffer, mResourceRequestInterfaceStorage) == 0x0004,
                          "RootOutputBuffer.mResourceRequestInterface @ +0x04");
            static_assert(offsetof(RootOutputBuffer, mAttribSysRequestInterfaceStorage) == 0x1014,
                          "RootOutputBuffer.mAttribSysRequestInterface @ +0x1014");
            static_assert(offsetof(RootOutputBuffer, mReplayRequestInterfaceStorage) == 0x1824,
                          "RootOutputBuffer.mReplayRequestInterface @ +0x1824");
        }
    };

} // namespace Io
} // namespace Module
} // namespace BrnSound

#endif // BRN_SOUND_MODULE_BRN_ROOT_SOUND_MODULE_IO_H
