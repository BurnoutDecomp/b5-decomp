#pragma once

// Replay phase IO buffers and their native accessors. ARTIST allocates the phase
// buffers separately; OutputBuffer_PostSim is 0x614 bytes (823AE66C).

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEventQueueSmall (GuiEventQueueBase<4096,16>)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::PadOutputInformation (InputPadInformation)
#include "GameSource/Replays/BrnReplayStatusInterface.h"          // BrnReplays::ReplayIO::StatusInterface
#include "GameSource/Replays/BrnReplayRequestInterface.h"         // BrnReplays::ReplayIO::RequestInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h" // BrnResource::GameDataIO::RequestInterface<1024>

namespace BrnReplays
{
namespace ReplayIO
{
    // ---- post-sim output buffer ---------------------------------------------------
    // ARTIST 823AE66C allocates 0x614 bytes: IOBuffer followed by the queue.
    struct OutputBuffer_PostSim : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

        // ARTIST 82653010 initializes IOBuffer then the queue.
        void Construct();
        // Destroy factory 823AE744 calls the IOBuffer-only destructor.
        void Destruct();

        // X360 0x823BB478, callsite 0x823E93D0 (ICF-shared +4 read accessor):
        // asserts the buffer is locked for reading, then returns &mGameEventQueue.
        const GameEventQueue* GetGameEventQueue() const;

        // X360 0x8265A798: write-lock (bit 3); asserts the buffer is locked for writing
        // ("Not locked for writing", BrnReplayModuleIO.h:176), then bulk-appends the
        // source game-event queue into mGameEventQueue (Append<1536,16>). Returns the
        // Append result.
        int AppendGameEventQueue(const GameEventQueue* lpSourceQueue);

        static void _AssertLayout();

    private:
        u8             maStatusPad[3];     // +0x001..+0x003 (force the queue to +4)
        GameEventQueue mGameEventQueue;    // +0x004 (queue.miBufferWritePos @ +1540)
    };

    // ---- pre-sim output buffer ----------------------------------------------------
    // DWARF home: BrnReplayModuleIO.h:96 (struct OutputBuffer_PreSim : public IOBuffer).
    // The pre-sim output buffer the replay module fills each frame and publishes to the
    // world/sound/effects/gui consumers. Layout authoritative from the X360 getter return
    // offsets (each accessor reads the IOBuffer lock byte @+0 then returns &member):
    //   +0x000  CgsModule::IOBuffer base                                  (status byte @+0)
    //   +0x004  StatusInterface                       mStatusInterface        (sizeof 0x618)
    //   +0x61C  BrnResource::GameDataIO::RequestInterface<1024> mGameDataRequestInterface (sizeof 0x410 = 1024+16)
    //   +0xA2C  GuiEventQueue (GuiEventQueueSmall)    mGuiEventQueue          (VariableEventQueue<4096,16>)
    // The five owned accessors (this TU): GetStatusInterface const/non-const (this+4),
    // GetGameDataRequestInterface const/non-const (this+0x61C), GetGuiEventQueue const (this+0xA2C).
    // GROWN from a prior opaque-storage placeholder once this type's accessor TU landed.
    struct OutputBuffer_PreSim : public CgsModule::IOBuffer
    {
        typedef CgsGui::GuiEventQueueSmall GuiEventQueue;   // == CgsModule::VariableEventQueue<4096,16>
        typedef BrnResource::GameDataIO::RequestInterface<1024> GameDataRequestInterface;

        // ARTIST 82652F48: selective status reset and both real queue constructors.
        // Reel names and queue payload bytes are retained in reused IO-stack storage.
        void Construct();
        // ARTIST destroy factory 823AE4F8 uses the shared IOBuffer-only destructor.
        void Destruct();

        // X360 0x823BB080: read-lock (bit 4, "Not locked for reading", :103); returns
        // &mStatusInterface (this+4).
        const StatusInterface* GetStatusInterface() const;
        // X360 0x8264E1D0: write-lock (bit 3, "Not locked for writing", :104); returns
        // &mStatusInterface (this+4).
        StatusInterface* GetStatusInterface();
        // X360 0x823BB128: read-lock; ICF-shared +0x61C accessor, called at 0x823F1F3C.
        const GameDataRequestInterface* GetGameDataRequestInterface() const;
        // X360 0x8264E278: write-lock (bit 3, "Not locked for writing", :108); returns
        // &mGameDataRequestInterface (this+0x61C).
        GameDataRequestInterface* GetGameDataRequestInterface();
        // X360 0x823BB1D0: read-lock (bit 4, "Not locked for reading", :110); returns
        // &mGuiEventQueue (this+0xA2C).
        const GuiEventQueue* GetGuiEventQueue() const;

        static void _AssertLayout();

    private:
        u8                       maStatusPad[3];            // +0x001..+0x003 (force +4 placement)
        StatusInterface          mStatusInterface;          // +0x004
        GameDataRequestInterface mGameDataRequestInterface; // +0x61C
        GuiEventQueue            mGuiEventQueue;             // +0xA2C
    };

    // ---- pre-sim INPUT buffer ------------------------------------------------------
    // DWARF home: BrnReplayModuleIO.h:60 (struct InputBuffer_PreSim : public IOBuffer).
    // The per-frame input the replay module records pre-simulation. Layout: the game-action
    // queue at +4, the timer-status snapshot at +0x3414, plus the per-pad input record.
    //   +0x000   CgsModule::IOBuffer base                            (status byte @+0)
    //   +0x004   GameActionQueue (VariableEventQueue<13312,16>) mGameActionQueue (sizeof 0x3410 = 13312+16)
    //   +0x3414  CgsSystem::TimerStatusInterface mTimerStatusInterface (sizeof 48)
    // FLAG (X360-vs-DWARF order divergence; asm authoritative): the DWARF declares the member
    // order { mGameActionQueue, mPadInput, mTimerStatusInterface }, but the X360 accessor
    // offsets pin mGameActionQueue @+4 (13328 bytes) and mTimerStatusInterface @+0x3414 --
    // those two are byte-adjacent, leaving no room for the 932-byte mPadInput between them.
    // mPadInput (CgsInput::InputIO::PadOutputInformation) is therefore placed after the
    // timer-status block here so the two asm-attested offsets (queue +4, timer +0x3414) are
    // exact. GetTimerStatusI / SetTimerStatusInterface (this TU) reach the timer block by
    // name; AppendGameActionQueue bulk-appends into the queue base at +4.
    struct InputBuffer_PreSim : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<13312, 16>     GameActionQueue;  // BaseGameActionQueue<13312> base
        typedef CgsInput::InputIO::PadOutputInformation      InputPadInformation;

        // ARTIST 0x82652F08: construct base and game-action queue, clear the timer
        // snapshot. The pad-input payload is deliberately not touched.
        void Construct();

        // X360 0x823C9A38: write-lock (bit 3, "Not locked for writing", :72); bulk-appends the
        // source game-action queue into mGameActionQueue (VariableEventQueue<13312,16>::
        // Append<13312,16>). Returns the Append result.
        int AppendGameActionQueue(const GameActionQueue* lpSourceQueue);
        // X360 0x8264E128: read-lock (bit 4, "Not locked for reading", :74); returns
        // &mTimerStatusInterface (this+0x3414).
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const;
        // X360 0x823BAF70: write-lock (bit 3, "Not locked for writing", :75); copies the source
        // TimerStatusInterface (48 bytes: game + sim TimerStatus blocks) into
        // mTimerStatusInterface (this+0x3414).
        void SetTimerStatusInterface(const CgsSystem::TimerStatusInterface* lpSource);

        static void _AssertLayout();

    private:
        u8                              maStatusPad[3];        // +0x001..+0x003 (force +4 placement)
        GameActionQueue                 mGameActionQueue;      // +0x004
        CgsSystem::TimerStatusInterface mTimerStatusInterface; // +0x3414
        InputPadInformation             mPadInput;             // after timer (see FLAG above)
    };

    // ---- post-sim INPUT buffer -----------------------------------------------------
    // DWARF home: BrnReplayModuleIO.h:134 (struct InputBuffer_PostSim : public IOBuffer).
    //   +0x000  CgsModule::IOBuffer base                          (status byte @+0)
    //   +0x004  RequestInterface       mRequestInterface          (sizeof 0x2C = 44)
    //   +0x030  GuiEventQueue          mGuiEventQueue             (GuiEventQueueSmall)
    // The four owned accessors (this TU): GetRequestInterface non-const (this+4),
    // AppendRequestInterface (merges into this+4), GetGuiEvent const + GetGuiEventQueue
    // non-const (this+0x30).
    struct InputBuffer_PostSim : public CgsModule::IOBuffer
    {
        typedef OutputBuffer_PreSim::GuiEventQueue GuiEventQueue;  // same GuiEventQueueSmall typedef

        // ARTIST 0x82652FE0: base flag, eleven named serialiser slots, then GUI queue.
        void Construct();

        // X360 0x823BB278: write-lock (bit 3, "Not locked for writing", :142); returns
        // &mRequestInterface (this+4).
        RequestInterface* GetRequestInterface();
        // ARTIST 8264E320: read-lock bit4, then return the same named interface.
        const RequestInterface* GetRequestInterface() const;
        // X360 0x823BB320: write-lock (bit 3, "Not locked for writing", :143); merges the source
        // request interface's serialiser slots into mRequestInterface (this+4). Returns the
        // Append result.
        int AppendRequestInterface(const RequestInterface* lpSource);
        // X360 0x8264E3C8: read-lock (bit 4, "Not locked for reading", :145); returns
        // &mGuiEventQueue (this+0x30).
        const GuiEventQueue* GetGuiEvent() const;
        // X360 0x823BB3D0: write-lock (bit 3, "Not locked for writing", :146); returns
        // &mGuiEventQueue (this+0x30).
        GuiEventQueue* GetGuiEventQueue();

        static void _AssertLayout();

    private:
        u8               maStatusPad[3];      // +0x001..+0x003 (force +4 placement)
        RequestInterface mRequestInterface;   // +0x004
        GuiEventQueue    mGuiEventQueue;       // +0x030
    };

}
}
