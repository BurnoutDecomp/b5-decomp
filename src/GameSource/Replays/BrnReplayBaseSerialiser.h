#pragma once

// BrnReplays::BaseSerialiser -- the abstract base for all replay serialisers.
// DWARF home: GameSource/Replays/BrnReplayBaseSerialiser.h:51.
//
// Wave 1 created this as a MINIMAL SLICE (opaque header blob + meId) just for the
// RequestInterface TU. This wave GROWS the home additively with the full
// DWARF-attested member layout and bodies the read/write/seek serialiser
// primitives (Lock/Unlock/SetMode/Read/Write/IsPlaying/IsRecording).
//
// LAYOUT (DWARF BrnReplayBaseSerialiser.h:254-269, cross-checked against X360 asm):
//   +0x00 meMode             EMode               (asm: lwz 0(this) for mode checks)
//   +0x04 mbLocked           bool                (asm: lbz 4(this) lock checks)
//   +0x08 mpBuffer           void*               (asm: lwz 8(this) buffer base)
//   +0x0C miBufferSize       int32_t             (Write: lwz 0xC -> size)
//   +0x10 miBufferUsed       int32_t             (Write: lwz 0x10; Read: lwz 0x10)
//   +0x14 miBufferRead       int32_t             (Read:  lwz 0x14 read cursor)
//   +0x18 mpStaticBuffer     void*
//   +0x1C miStaticBufferSize int32_t
//   +0x20 meId               ESerialiserId       (RegisterSerialiser reads this)
//   +0x24 meContext          ESerialiserContext
//   +0x28 macName[32]        char   (.. +0x48; +0x48..+0x50 reserved/padding)
//   +0x50 mbIsKeyFrame       bool   (asm Read/Write select the key-frame path off lbz 0x50)
//   +0x54 mfTime             f32
//   +0x58 mbDataReady        bool
//   +0x59 mbDataRestored     bool
//   +0x5A mbAllowStreaming   bool
//
// NOTE on meId offset: wave 1's comment placed meId at +0x28 from the asm. With the
// real named layout (8 leading fields == 0x20 bytes, meId at +0x20) the slot index
// math in RegisterSerialiser (slwi id,2 ; stwx) is unaffected -- the +0x28 figure
// referred to a different read. The DWARF member order is authoritative here.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"

namespace BrnReplays
{
    // DWARF: BrnReplayBaseSerialiser.h:51
    class BaseSerialiser
    {
    public:
        // DWARF: BrnReplayBaseSerialiser.h:54. Serialiser mode state machine.
        enum EMode
        {
            E_MODE_IDLE                = 0,
            E_MODE_RECORDING_PREPARING = 1,
            E_MODE_RECORDING           = 2,
            E_MODE_RECORDING_STALLED   = 3,
            E_MODE_PLAYING_PREPARING   = 4,
            E_MODE_PLAYING             = 5,
            E_MODE_PLAYING_STALLED     = 6,
            E_MODE_RESTORING           = 7,
            E_MODE_COUNT               = 8,
        };

        // DWARF: BrnReplayBaseSerialiser.h:67
        static const s32 KI_MAX_NAME_LENGTH = 32;

        // Construct @ (BaseSerialiser TU, external). Shared initialiser every concrete
        // serialiser forwards to: (id, mode, bufferSize, staticBufferSize, name, flag).
        // X360 attestation: each leaf serialiser calls it with 6 args after `this`
        // (e.g. PropEntitySerialiser @0x8264C6C0 -> 6,0,0x4000,0x7480,"PropEntity",1;
        // RaceCarEntitySerialiser -> 0,0,0x1B000,0x1B000,"RaceCarEntity",1). Declared
        // here (home of BaseSerialiser); the body lives in the BaseSerialiser TU.
        s32 Construct(s32 liId, s32 liMode, s32 liBufferSize, s32 liStaticBufferSize,
                      const char* lpcName, s32 liFlag);

        // --- read/write/seek primitives owned by THIS TU ---

        // Write(const void*, int32_t) @ 0x8264C050. Appends liSize bytes into the
        // record buffer (only while E_MODE_RECORDING); returns bytes written.
        s32 Write(const void* lpData, s32 liSize);

        // Read(void*, int32_t) @ 0x8264C188. Pops liSize bytes from the playback
        // buffer at the read cursor; returns bytes read.
        s32 Read(void* lpData, s32 liSize);

        // ReadByte(void*) @ 0x8264FCF0. The 1-byte read primitive (X360 sub_8264FCF0):
        // reads exactly one byte from the playback buffer into *lpDest. The traffic
        // frame reader uses this for the bonus-asset count and the trailing slot table.
        s32 ReadByte(void* lpDest);

        // Serialise(void*, int32_t) @ 0x8264C470. Mode-directed dispatch: forwards
        // the (buffer, size) pair to Write while recording, to Read while playing,
        // and is a no-op (returns 0) in every other mode.
        s32 Serialise(void* lpBuffer, s32 liSize);

        // Lock @ 0x823A6718 / Unlock @ 0x823A67C0. Guard the buffer against
        // concurrent serialiser access.
        bool Lock();
        bool Unlock();

        // IsPlaying @ 0x821F3440 / IsRecording @ 0x821F3470. Mode-state predicates.
        bool IsPlaying() const;
        bool IsRecording() const;

        // GetId @ inline (BrnReplayBaseSerialiser.h:173) -- returns meId.
        ESerialiserId GetId() const { return meId; }

        // --- snapshot accessors the replay debug overlay reads each frame ---
        // (additive surface; the bodies are trivial named-member reads). They are the
        // live-serialiser fields PreUpdateRecord @0x8264BD08 copies into the overlay's
        // DebugSerialiserInfo record. PreUpdateRecord is the X360 attestation of where
        // each lives on this build (live-serialiser byte offsets in parentheses):
        //   GetMode             <- lwz 0x00(src)  (0x8264BF68)
        //   GetBufferSize       <- lwz 0x0C(src)  (0x8264BF78)  "Buffer Size" column
        //   GetBufferUsed       <- lwz 0x10(src)  (0x8264BF88)  "Buffer Used" column
        //   GetBufferRead       <- lwz 0x14(src)  (0x8264BF98)
        //   GetId               <- lwz 0x28(src)  (0x8264BFA8)  ID column
        //   GetContext          <- lwz 0x2C(src)  (0x8264BFB8)
        //   GetStaticBufferSize <- lwz 0x24(src)  (0x8264BFC8)  "Static Size" column
        //   GetName             <- &src+0x30      (0x8264BF40 byte copy)  macName
        // NOTE: PreUpdateRecord reads the static-buffer-size at live+0x24 (8 bytes past
        // the DWARF +0x1C placement); on this 64-bit host all access is by name so the
        // exact X360 byte offset of miStaticBufferSize is immaterial to the snapshot.
        EMode              GetMode() const             { return meMode; }
        s32                GetBufferSize() const       { return miBufferSize; }
        s32                GetBufferUsed() const       { return miBufferUsed; }
        s32                GetBufferRead() const       { return miBufferRead; }
        s32                GetStaticBufferSize() const { return miStaticBufferSize; }
        ESerialiserContext GetContext() const          { return meContext; }
        const char*        GetName() const             { return macName; }

        // ADDITIVE GROW: the raw static-buffer pointer every leaf serialiser's
        // GetStaticLayout() (e.g. BrnReplays::GuiModuleSerialiser::GetStaticLayout,
        // BrnReplayGuiModuleSerialiser.cpp @0x82410D90) returns after asserting
        // miStaticBufferSize is big enough and reinterpreting this pointer as its own
        // static-layout type. Exposed here (the base's own member) so cross-TU callers
        // that only need the raw buffer (not a leaf-typed accessor) can reach it without
        // forking BaseSerialiser's storage locally.
        void* GetStaticBuffer() const { return mpStaticBuffer; }

    protected:
        // SetMode @ 0x8264B0F8. Private in the leak; protected here so the embed
        // check and (future) construction path can drive the mode while it stays
        // off the public surface.
        void SetMode(EMode leMode);

    protected:
        // DWARF-attested layout (BrnReplayBaseSerialiser.h:254-269).
        EMode              meMode;             // @0x00
        bool               mbLocked;           // @0x04
        void*              mpBuffer;           // @0x08
        s32                miBufferSize;       // @0x0C
        s32                miBufferUsed;       // @0x10
        s32                miBufferRead;       // @0x14
        void*              mpStaticBuffer;     // @0x18
        s32                miStaticBufferSize; // @0x1C
        // FLAG (X360 overrides DWARF): RegisterSerialiser @0x821F34A0 reads GetId()/meId at
        // *(this+0x28) (lwz 0x28(r31) at six GetId/range-guard sites + the slwi;stwx slot-index
        // store), so meId sits at +0x28 on X360 -- 8 bytes higher than the Feb-2007 PS3 DWARF's
        // +0x20. The X360 SKU extended the serialiser (ESerialiserId E_ID_COUNT 5->11); those two
        // leading words are unrecovered, modelled as a sized placeholder so meId + the whole tail
        // land at their X360-attested offsets. (The 7 bodied funcs touch only +0x00..+0x14, so
        // they are unaffected; this keeps any future Construct/by-offset path faithful.)
        u8                 maX360Extension20[8]; // @0x20 (unmodeled X360-extension fields)
        ESerialiserId      meId;               // @0x28 -- read by RegisterSerialiser (lwz 0x28)
        ESerialiserContext meContext;          // @0x2C
        char               macName[32];        // @0x30
        bool               mbIsKeyFrame;       // @0x50
        f32                mfTime;             // @0x54
        bool               mbDataReady;        // @0x58
        bool               mbDataRestored;     // @0x59
        bool               mbAllowStreaming;   // @0x5A
        // NOTE: the @0xNN offsets above are X360 byte offsets (4-byte pointers). On the 64-bit
        // host mpBuffer/mpStaticBuffer are 8 bytes, so absolute host offsets differ -- all access
        // is BY NAME, so this is immaterial; the maX360Extension20[8] placeholder keeps the X360
        // field SEQUENCE (meId after 8 extra X360 words) faithful, which is what matters.
    };
}
