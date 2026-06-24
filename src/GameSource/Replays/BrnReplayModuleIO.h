#pragma once

// Canonical home for BrnReplays::ReplayIO and its nested per-phase IO buffers
// (InputBuffer_PreSim / InputBuffer_PostSim / OutputBuffer_PreSim / OutputBuffer_PostSim,
// plus the ReplayIO_Buffer payload that embeds the two output buffers).
// DWARF/assert home path: GameSource/Replays/BrnReplayModuleIO.h (the X360 bodies'
// lock asserts cite this file's line numbers, recorded per accessor below).
//
// ReplayIO is the per-frame IO payload the replay module exchanges with the rest of
// the game. It derives from CgsModule::IOBuffer (status byte at +0) and embeds two
// phase output buffers: a pre-sim buffer (returned by GetOutputBuffer_PreSim) and a
// post-sim buffer (returned by GetOutputBuffer_PostSim). The phase accessors take the
// ReplayIO's READ lock (bit 4) and hand back the embedded sub-buffer pointer; the
// sub-buffer is then WRITE-locked (bit 3) by its producer while it is filled.
//
// LAYOUT (X360 asm authoritative; getter return-offsets pin the two sub-buffers):
//   base CgsModule::IOBuffer                          (status byte @ +0; +1..+3 pad)
//   +0x004   OutputBuffer_PostSim mPostSimBuffer      (GetOutputBuffer_PostSim @0x823BB478 -> this+4)
//   +0x61C   OutputBuffer_PreSim  mPreSimBuffer       (GetOutputBuffer_PreSim  @0x823BB128 -> this+1564)
//
// OutputBuffer_PostSim itself derives from CgsModule::IOBuffer and embeds a single
// game-event queue: VariableEventQueue<1536,16> at +4 (AppendGameEventQueue @0x8265A798
// write-locks, then Append<1536,16>'s a source <1536,16> queue into this+4). The
// post-sim sub-buffer occupies +4..+1564 of ReplayIO (1560 bytes).

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
    // Derives from IOBuffer (its own status byte at +0). The game-event queue lives at
    // +4 (AppendGameEventQueue appends into this+4). VariableEventQueue<1536,16> has
    // miBufferWritePos at queue-offset +1540 (X360 *(a1+1540)) -> sizeof 1552; with the
    // 4-byte IOBuffer header that puts the buffer at 1556 bytes. The ReplayIO getter
    // spacing (this+4 .. this+1564) gives the embedded sub-buffer a 1560-byte slot, so
    // a 4-byte trailing reserve rounds it to the X360-observed stride.
    struct OutputBuffer_PostSim : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<1536, 16> GameEventQueue;

        // X360 0x8265A798: write-lock (bit 3); asserts the buffer is locked for writing
        // ("Not locked for writing", BrnReplayModuleIO.h:176), then bulk-appends the
        // source game-event queue into mGameEventQueue (Append<1536,16>). Returns the
        // Append result.
        int AppendGameEventQueue(const GameEventQueue* lpSourceQueue);

        static void _AssertLayout();

    private:
        u8             maStatusPad[3];     // +0x001..+0x003 (force the queue to +4)
        GameEventQueue mGameEventQueue;    // +0x004 (queue.miBufferWritePos @ +1540)
        u8             maTailReserve[4];   // round the sub-buffer to the +1560 stride
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
    // The four owned accessors (this TU): GetStatusInterface const/non-const (this+4),
    // GetGameDataRequestInterface non-const (this+0x61C), GetGuiEventQueue const (this+0xA2C).
    // GROWN from a prior opaque-storage placeholder once this type's accessor TU landed.
    struct OutputBuffer_PreSim : public CgsModule::IOBuffer
    {
        typedef CgsGui::GuiEventQueueSmall GuiEventQueue;   // == CgsModule::VariableEventQueue<4096,16>
        typedef BrnResource::GameDataIO::RequestInterface<1024> GameDataRequestInterface;

        // X360 0x823BB080: read-lock (bit 4, "Not locked for reading", :103); returns
        // &mStatusInterface (this+4).
        const StatusInterface* GetStatusInterface() const;
        // X360 0x8264E1D0: write-lock (bit 3, "Not locked for writing", :104); returns
        // &mStatusInterface (this+4).
        StatusInterface* GetStatusInterface();
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

        // X360 0x823BB278: write-lock (bit 3, "Not locked for writing", :142); returns
        // &mRequestInterface (this+4).
        RequestInterface* GetRequestInterface();
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

    // ---- the replay module IO payload ---------------------------------------------
    struct ReplayIO_Buffer : public CgsModule::IOBuffer
    {
        // X360 0x823BB478: read-lock (bit 4); asserts locked for reading
        // ("Not locked for reading", BrnReplayModuleIO.h:175); returns &mPostSimBuffer
        // (this+4).
        OutputBuffer_PostSim* GetOutputBuffer_PostSim();
        // X360 0x823BB128: read-lock (bit 4); asserts locked for reading
        // ("Not locked for reading", BrnReplayModuleIO.h:107); returns &mPreSimBuffer
        // (this+1564).
        OutputBuffer_PreSim* GetOutputBuffer_PreSim();

        static void _AssertLayout();

    private:
        u8                   maStatusPad[3];   // +0x001..+0x003 (force +4 placement)
        OutputBuffer_PostSim mPostSimBuffer;   // +0x004
        OutputBuffer_PreSim  mPreSimBuffer;    // +0x61C (this+1564)
    };
}
}
