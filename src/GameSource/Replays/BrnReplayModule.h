#pragma once

// BrnReplays::ReplayModule -- the replay record/playback module that owns the
// write/read streams, the per-serialiser book-keeping, and the action-replay
// control flags. DWARF/assert home path: GameSource/Replays/BrnReplayModuleIO.h
// (the bodies live in the ReplayModule TU; the IO buffers are already homed in
// BrnReplayModuleIO.h).
//
// MINIMAL ACCESSOR SLICE. The full ReplayModule is a multi-kilobyte class whose
// complete member layout belongs to the ReplayModule TU. This header carries only
// the surface the *debug overlay* (BrnReplays::DebugComponent) reads/writes, so the
// debug TU can compile against named members instead of raw offset pokes. Every
// field the debug component touches is exposed as a named accessor; the gaps are
// honest explicit padding sized to the X360 byte offsets observed in the ARTIST asm.
// GROW this home additively when the full ReplayModule TU lands; do NOT fork it.
//
// X360 offsets the debug overlay touches (from BrnReplayDebugComponent.cpp asm):
//   +0x230 (560)   meState         -- EStreamState (RenderStatus/RenderMainWindow)
//   +0x234 (564)   meStreamStage   -- EStreamStage
//   +0x84C..+0x874 mapSerialisers[11] -- BaseSerialiser* per ESerialiserId
//                   (PreUpdateRecord loops 0x84C..<0x878 step 4 == 11 entries)
//   +0x89C (2204)  mpWriteStream   -- WriteStream* (RenderWriteStreamBlocks)
//   +0x8C0 (2240)  miWriteIndex    -- s32 write cursor / block-start
//   +0x8C4 (2244)  miWriteStalls   -- s32 stall count
//   +0x8E0 (2272)  mpReadStream    -- ReadStream* (RenderReadStreamBlocks)
//   +0x8E4 (2276)  miReadBlockStart-- s32 read block-start
//   +0x5C3F..+0x5C45 (23615..23621) action-replay control flags (bool)

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"

namespace BrnReplays
{
    // The live per-serialiser book-keeping objects the overlay snapshots each frame
    // are BaseSerialiser instances (PreUpdateRecord @0x8264BD08 reads them at the
    // BaseSerialiser field offsets: mode@0x00, bufferSize@0x0C, bufferUsed@0x10,
    // bufferRead@0x14, staticSize@0x24, id@0x28, context@0x2C, name@0x30). The
    // overlay copies those named fields into its own DebugSerialiserInfo record.
    class BaseSerialiser;

    // DWARF home: BrnReplayModuleIO.h:96 et al. The write-side replay stream.
    // Reached only by pointer from the debug overlay's block view; its block table
    // layout is owned by the ReplayModule TU.
    class WriteStream;
    class ReadStream;

    // DWARF: BrnReplayShared.h -- the replay stream-state machine the overlay
    // labels in RenderStatus (off_82F2A56C "IDLE"/... lookup, indexed by meState).
    enum EStreamState
    {
        E_STREAM_STATE_IDLE      = 0,
        E_STREAM_STATE_RECORDING = 1,
        E_STREAM_STATE_PLAYING   = 2,
        E_STREAM_STATE_COUNT     = 3,
    };

    // The per-state stream stage the overlay labels (off_82F2A57C "CLOSED"/...,
    // indexed by meStreamStage). Five named stages (KAC_STREAM_STAGE_NAMES has 5).
    enum EStreamStage
    {
        E_STREAM_STAGE_CLOSED  = 0,
        E_STREAM_STAGE_OPENING = 1,
        E_STREAM_STAGE_OPEN    = 2,
        E_STREAM_STAGE_CLOSING = 3,
        E_STREAM_STAGE_ERROR   = 4,
        E_STREAM_STAGE_COUNT   = 5,
    };

    class ReplayModule
    {
    public:
        // Number of serialiser slots the overlay snapshots == ESerialiserId count.
        static const s32 KI_NUM_SERIALISERS = E_ID_COUNT; // 11

        // --- accessors the debug overlay reads (RenderStatus/RenderMainWindow) ---
        EStreamState GetState() const          { return meState; }
        EStreamStage GetStreamStage() const    { return meStreamStage; }

        // The live per-serialiser record for a given id, or null if that slot is
        // empty. PreUpdateRecord walks all KI_NUM_SERIALISERS slots. The live object
        // is a BaseSerialiser (mapSerialisers @0x84C..0x874, 11 pointers).
        BaseSerialiser* GetSerialiser(s32 liId) const { return mapSerialisers[liId]; }

        // --- write/read stream surface (RenderWriteStreamBlocks/...Status) ---
        WriteStream* GetWriteStream() const    { return mpWriteStream; }
        ReadStream*  GetReadStream() const     { return mpReadStream; }
        s32          GetWriteIndex() const      { return miWriteIndex; }
        s32          GetWriteStalls() const     { return miWriteStalls; }
        s32          GetReadBlockStart() const  { return miReadBlockStart; }

        // --- action-replay control flags driven by the overlay's menu callbacks ---
        // The overlay's *CB callbacks set these; the module consumes them next sim.
        void RequestStartPlaying()    { mbStartPlaying = true; }
        void RequestStopPlaying()     { mbStopPlaying = true; }
        void RequestStartRecording()  { mbStartRecording = true; }
        void RequestStopRecording()   { mbStopRecording = true; }
        void RequestMarkActionReplay(){ mbMarkActionReplay = true; }
        void RequestStartActionReplay(){ mbStartActionReplay = true; }

        // OnActivate registers this flag with the debug menu (Enable auto-start).
        bool* GetAutoStartFlagPtr()   { return &mbAutoStart; }

    private:
        // Honest explicit padding to the X360-attested offsets. All access is BY
        // NAME; on the 64-bit host pointer widths differ, so these byte offsets are
        // the X360 sequence markers, not host offsets (same rule as BaseSerialiser).
        u8                   maPad0[0x230];           // @0x000
        EStreamState         meState;                 // @0x230
        EStreamStage         meStreamStage;           // @0x234
        u8                   maPad238[0x84C - 0x238]; // @0x238
        BaseSerialiser*      mapSerialisers[KI_NUM_SERIALISERS]; // @0x84C..@0x874 (11 ptrs)
        u8                   maPad878[0x89C - 0x878]; // @0x878
        WriteStream*         mpWriteStream;           // @0x89C
        u8                   maPad8A0[0x8C0 - 0x8A0]; // @0x8A0
        s32                  miWriteIndex;            // @0x8C0
        s32                  miWriteStalls;           // @0x8C4
        u8                   maPad8C8[0x8E0 - 0x8C8]; // @0x8C8
        ReadStream*          mpReadStream;            // @0x8E0
        s32                  miReadBlockStart;        // @0x8E4
        u8                   maPad8E8[0x5C3F - 0x8E8];// @0x8E8
        bool                 mbStartPlaying;          // @0x5C3F (23615)
        bool                 mbStopPlaying;           // @0x5C40 (23616)
        bool                 mbStartRecording;        // @0x5C41 (23617)
        bool                 mbStopRecording;         // @0x5C42 (23618)
        bool                 mbMarkActionReplay;      // @0x5C43 (23619)
        bool                 mbStartActionReplay;     // @0x5C44 (23620)
        bool                 mbAutoStart;             // @0x5C45 (23621) -- Enable auto-start
    };
}
