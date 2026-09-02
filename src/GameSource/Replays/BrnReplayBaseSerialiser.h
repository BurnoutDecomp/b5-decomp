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
//   +0x1C .. +0x24 : see the X360-extension note below
//   +0x24 miStaticBufferSize int32_t
//   +0x28 meId               ESerialiserId       (RegisterSerialiser reads this)
//   +0x2C meContext          ESerialiserContext
//   +0x30 macName[32]        char   (.. +0x50)
//   +0x50 mbIsKeyFrame       bool   (asm Read/Write select the key-frame path off lbz 0x50)
//   +0x54 mfTime             f32
//   +0x58 mbDataReady        bool
//   +0x59 mbDataRestored     bool
//   +0x5A mbAllowStreaming   bool
//   +0x5B mbSkipModuleSerialise  bool   -- X360-only, NOT in the DWARF; see its declaration
//
// [2026-08-18 CORRECTED, wave Q round 2 -- the tail offsets above are now MEASURED, not
// inferred.] BaseSerialiser::Construct @0x8264C280 has NO per-address JSON export, so earlier
// passes could only guess this half. I disassembled it headless (idat.exe over a scratch copy of
// the .i64; the function extent is 0x8264C280..0x8264C470, 124 insns) and its prologue writes
// EVERY field, which pins the whole tail:
//     8264C29C  stw r4,  0x28(this)   meId              <- Construct arg 1
//     8264C2A0  stw r5,  0x2C(this)   meContext         <- arg 2
//     8264C2A4  stw r30, 8(this)      mpBuffer     = 0
//     8264C2A8  stw r6,  0xC(this)    miBufferSize      <- arg 3
//     8264C2AC  stw r30, 0x10(this)   miBufferUsed = 0
//     8264C2B0  stw r30, 0x20(this)   <X360 extension word> = 0
//     8264C2B4  stw r7,  0x24(this)   miStaticBufferSize <- arg 4
//     8264C2B8  stw r30, 0(this)      meMode       = 0
//     8264C2BC  stb r30, 4(this)      mbLocked     = false
//     8264C2C0  stw r30, 0x14(this)   miBufferRead = 0
//     8264C2C4  stb r30, 0x50(this)   mbIsKeyFrame = false
//     8264C2C8  stb r9,  0x5B(this)   mbSkipModuleSerialise <- arg 6   (see below)
//     8264C2CC  stw r30, 0x18(this)   mpStaticBuffer = 0
//     8264C2D0  stw r30, 0x1C(this)   <X360 extension word> = 0
//     8264C41C  <macName copy loop into this+0x30>
// So the X360 SKU's two extra words sit at +0x1C/+0x20, i.e. BETWEEN mpStaticBuffer and
// miStaticBufferSize -- not after miStaticBufferSize where wave 1's placeholder put them. The
// old comment already flagged the contradiction ("PreUpdateRecord reads the static-buffer-size
// at live+0x24, 8 bytes past the DWARF +0x1C placement") and left it unresolved; Construct
// resolves it. Nothing behavioural changes -- every access is by name -- but a wrong layout
// comment is a real defect, so the placeholder moved to where the asm puts it.
// meId at +0x28 (not the DWARF's +0x20) is the same X360-extension shift, and is corroborated
// twice: RegisterSerialiser @0x821F34A0 reads `lwz 0x28(r31)`, and Construct stores arg 1 there.

#include "types.hpp"
#include "GameSource/Replays/BrnReplayShared.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN> + Event (Read/WriteVariableQueue)

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

        // Construct @0x8264C280 (124 insns). Shared initialiser every concrete serialiser
        // forwards to: (id, context, bufferSize, staticBufferSize, name, skipModuleSerialise).
        // X360 attestation: each leaf serialiser calls it with 6 args after `this`
        // (e.g. PropEntitySerialiser @0x8264C6C0 -> 6,0,0x4000,0x7480,"PropEntity",1;
        // RaceCarEntitySerialiser -> 0,0,0x1B000,0x1B000,"RaceCarEntity",1). Declared
        // here (home of BaseSerialiser); the body lives in the BaseSerialiser TU.
        // The 6th argument (r9) is the one that lands in mbSkipModuleSerialise -- named for it
        // now that `stb r9, 0x5B(r31)` is measured (see the layout note at the top of this file).
        s32 Construct(s32 liId, s32 liContext, s32 liBufferSize, s32 liStaticBufferSize,
                      const char* lpcName, s32 liSkipModuleSerialise);

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

        // --- additive primitives the BrnReplayArray / SoundSerialiser delta paths drive
        //     (declaration-only here; bodies live in the BaseSerialiser TU). Each mirrors a
        //     distinct X360 sub_* entry point. ---

        // WriteByte(const void*) @ 0x8264FBC8 (sub_8264FBC8). The 1-byte write primitive,
        // the record mirror of ReadByte.
        s32 WriteByte(const void* lpSrc);

        // ReadFloat(void*) @ 0x82650130 (sub_82650130). Read one 32-bit float into *lpDest.
        s32 ReadFloat(void* lpDest);

        // WriteFloat(const void*) @ 0x8264FEE8 (sub_8264FEE8). Write one 32-bit float from
        // *lpSrc (the record mirror of ReadFloat).
        s32 WriteFloat(const void* lpSrc);

        // Read/WriteVariableQueue -- pop / push a packed VariableEventQueue<BUFSIZE,ALIGN> off /
        // onto the stream (e.g. the sound frame's embedded <512,16> event queue, the director
        // bridge's <13312,16> game-action queue). X360-attested as templates on the queue's
        // <BUFSIZE,ALIGN> (ReadVariableQueue<13312,16> @0x82653A60, <512,16> @0x82656CF0;
        // WriteVariableQueue<13312,16> @0x826539B8). The generic bodies are out-of-line below.
        template <s32 BUFSIZE, s32 ALIGN>
        s32 ReadVariableQueue(CgsModule::VariableEventQueue<BUFSIZE, ALIGN>* lpQueue);
        template <s32 BUFSIZE, s32 ALIGN>
        s32 WriteVariableQueue(CgsModule::VariableEventQueue<BUFSIZE, ALIGN>* lpQueue);

        // IsKeyFrame() -- expose the protected mbIsKeyFrame flag (asm reads lbz 0x50) so the
        // delta serialisers (BrnReplayArray<T,N>, SoundSerialiser) can select the key-frame
        // vs delta path without forking the flag.
        bool IsKeyFrame() const { return mbIsKeyFrame; }

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

        // ================================================================================
        // ADDITIVE GROW (wave Q round 2) -- the three tail flags the prop/race-car/traffic
        // entity modules drive from their own update, plus the byte the DWARF stops short of.
        // ================================================================================

        // DWARF BrnReplayBaseSerialiser.h:229 / :232 (source lines; in the dumpfile
        // references/DecFIGS/dwarfdump/GameSource/Replays/BrnReplayBaseSerialiser.h they are
        // lines 207/210 -- an earlier request cited the dumpfile numbers as if they were the
        // source ones). Parameter names lbDataReady / lbDataRestored are the DWARF's, from
        // _compile/BrnEntityModuleUnity.cpp:12935 and :23876.
        //
        // Header-inline on the console: no out-of-line body exists for either, and every writer
        // is a bare byte store folded into its caller. PropEntityModule::PostPhysicsUpdate
        // @0x823031D8 does exactly four of them -- `stbx r17, r28, 0xD31D8` (mbDataReady) and
        // `stbx r17, r28, 0xD31D9` (mbDataRestored) at 0x82303548 / 0x823034B4 / 0x82303578 /
        // 0x823035C4, where mPropEntitySerialiser sits at module +0xD3180, so +0x58 / +0x59.
        // No lock test is inlined at any of the four sites, so neither setter carries one here.
        void SetDataReady(bool lbDataReady)         { mbDataReady = lbDataReady; }
        void SetDataRestored(bool lbDataRestored)   { mbDataRestored = lbDataRestored; }

        // AllowStreaming() -- DWARF :212, the mbAllowStreaming reader.
        bool AllowStreaming() const { return mbAllowStreaming; }

        // The X360-only byte at +0x5B. See mbSkipModuleSerialise's declaration below for the
        // full measurement; short version: it is Construct's 6th argument, it is 1 for this
        // build's PropEntity / RaceCarEntity / TrafficEntity / Sound serialisers, and when it
        // is set the owning module SKIPS its own inline serialiser Read/Write.
        bool SkipModuleSerialise() const { return mbSkipModuleSerialise; }

        // ReplayModule::StoreSerialisers @0x8264B600 writes both of these from OUTSIDE the
        // serialiser (`*(*v5 + 8) = Malloc(...)`, `*(*v5 + 32) = Malloc(...)`): the module owns
        // the linear region every serialiser's buffers are carved from. Named accessors rather
        // than a friend declaration, so the store is spelled the same way every other member is.
        void  SetBuffer(void* lpBuffer)             { mpBuffer = lpBuffer; }
        void  SetStaticBuffer(void* lpBuffer)       { mpStaticBuffer = lpBuffer; }
        void* GetBuffer() const                     { return mpBuffer; }
        void* GetStaticBufferPtr() const            { return mpStaticBuffer; }

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
        // CORRECTED 2026-09-02 (tyre-mark wave) -- mpStaticBuffer IS AT +0x20, NOT +0x18.
        // The old model put the pointer at +0x18 and the two unnamed X360-extension words at
        // +0x1C/+0x20, on the strength of a label in the Construct listing ("stw r30, 0x18(this)
        // mpStaticBuffer = 0"). Two functions read the field directly and both say +0x20:
        //   EffectsSerialiser::GetStaticLayout @0x82278698
        //       lwz  r11, 0x24(r28)      ; miStaticBufferSize
        //       cmpwi cr6, r11, 0x12B0   ; >= 4784
        //       ...  return *(a1 + 32)   ; <-- THE STATIC BUFFER, at +0x20
        //   ReplayModule::StoreSerialisers @0x8264B600
        //       v14 = *(serialiser + 36) ; miStaticBufferSize (+0x24)
        //       *(serialiser + 32) = LinearMalloc::Malloc(module linear, v14)
        // -- pointer immediately followed by its size, exactly like mpBuffer(+0x08) /
        // miBufferSize(+0x0C). So the two X360-extension words are at +0x18/+0x1C and the
        // Construct listing's label was on the wrong store. The pad moves; nothing else does.
        u8                 maX360Extension18[8]; // @0x18 (unmodeled X360-extension fields)
        void*              mpStaticBuffer;     // @0x20 -- GetStaticLayout's `*(a1 + 32)`
        s32                miStaticBufferSize; // @0x24 -- Construct `stw r7, 0x24(r31)`
        ESerialiserId      meId;               // @0x28 -- read by RegisterSerialiser (lwz 0x28)
        ESerialiserContext meContext;          // @0x2C
        char               macName[32];        // @0x30
        bool               mbIsKeyFrame;       // @0x50
        f32                mfTime;             // @0x54
        bool               mbDataReady;        // @0x58
        bool               mbDataRestored;     // @0x59
        bool               mbAllowStreaming;   // @0x5A

        // ================================================================================
        // @0x5B -- X360-ONLY, NOT IN THE DWARF (whose member list ends at mbAllowStreaming
        // @0x5A, and PropEntitySerialiser::mbPreviousFrameInitialized is already at +0x5C, so
        // this is a fourth bool squeezed into the base's tail: a merge-window delta).
        //
        // ⚠️ THE NAME IS DESCRIPTIVE ONLY. No symbol, assert string, DWARF entry or Feb-2007
        // source attests it. It is named for the ONE effect both of its readers have.
        //
        // WHAT IS MEASURED (all of it by me, wave Q round 2, from the ARTIST image):
        //   * ONE WRITER, and it is not "none". BaseSerialiser::Construct @0x8264C280 does
        //     `stb r9, 0x5B(r31)` at 0x8264C2C8 -- r9 being its 6th post-`this` argument, moved
        //     by no instruction between the call site and the store. Construct has NO
        //     per-address JSON export (that is why an earlier export-set scan concluded "zero
        //     writers image-wide" and a spec built on that conclusion was wrong); it must be
        //     disassembled from the .i64.
        //   * PER-SERIALISER VALUE, read off `li r9, <n>` at all nine Construct call sites:
        //         PropEntity 1   RaceCarEntity 1   TrafficEntity 1   Sound 1
        //         DirectorBridge 0   GameModule 0   Effects 0   GuiModule 0   DirectorModule 0
        //     So on the shipped image the prop serialiser's byte is 1 and is never cleared.
        //   * SCAN FOR OTHER WRITERS: a full-image opcode scan of both executable segments for
        //     `stb rX, 0x5B(rY)` returns 38 sites; the only one whose base is a replay serialiser
        //     is Construct's. The rest are stack slots or unrelated classes (BrnSound::Logic::
        //     MusicEffect/HUDEffect/MusicStream, BrnGame::BrnGameModule, SHA1/md5, CEMBMVQ).
        //     A scan for the indexed form -- the module-relative constant 0xD31DB that
        //     PropEntityModule uses -- finds exactly TWO materialisations image-wide, and both
        //     are `lbzx` READS. HONEST LIMIT: those two scans do not prove a universal negative
        //     for every possible indexed store through every serialiser; what they prove is that
        //     no displacement-form store and no PropEntityModule-relative store other than
        //     Construct's touches this byte. Do not upgrade that to "nothing ever writes it".
        //   * THE TWO READERS, and what the flag DOES:
        //         PropEntityModule::ReplayPreSceneUpdate @0x822EF878 --
        //             0x822EF91C lis r11,0xD ; ori r11,r11,0x31DB ; lbzx r11, r31, r11
        //             0x822EF928 cmplwi r11,0 ; bne -> 0x822EF944
        //           i.e. NON-ZERO SKIPS the `GetStaticLayout(); Read()` pair and jumps straight
        //           to ReplayUpdatePropsInScene / ReplayUpdatePartsInScene (which are ungated).
        //         PropEntityModule::PostPhysicsUpdate @0x823031D8 --
        //             0x823034A8 the same constant, `lbzx r11, r28, r11`
        //             0x823034B8 cmplwi r11,0 ; bne -> 0x823035C8
        //           same polarity, skipping `GetStaticLayout(); Write()`.
        //     mPropEntitySerialiser is at module +0xD3180, so 0xD31DB is exactly +0x5B.
        //
        // CONSEQUENCE FOR ANYONE LANDING THOSE TWO BODIES: with the flag at 1, the shipped image
        // NEVER runs PropEntitySerialiser::Read there and NEVER runs ::Write there. Landing
        // either call unconditionally is not "behaviour-identical" -- BaseSerialiser::Read
        // @0x8264C188 pops bytes off the playback stream, so an extra Read desyncs every
        // subsequent replay read. Gate both on !SkipModuleSerialise(), or omit them.
        //
        // WHAT IS *NOT* MEASURED: why the flag exists. The four serialisers that set it are the
        // ones whose frames their owning module drives itself; the four that clear it are driven
        // through the generic path. That is a pattern, not an attestation -- treat the name as a
        // label for the observed effect, not as a recovered concept.
        // ================================================================================
        bool               mbSkipModuleSerialise; // @0x5B
        // NOTE: the @0xNN offsets above are X360 byte offsets (4-byte pointers). On the 64-bit
        // host mpBuffer/mpStaticBuffer are 8 bytes, so absolute host offsets differ -- all access
        // is BY NAME, so this is immaterial; the maX360Extension1C[8] placeholder keeps the X360
        // field SEQUENCE faithful, which is what matters. [round-3 correction: this note still
        // named the placeholder maX360Extension20 and put it AFTER miStaticBufferSize. It was
        // renamed and MOVED -- it sits at +0x1C, BETWEEN mpStaticBuffer and miStaticBufferSize,
        // per Construct's `stw r30,0x1C` / `stw r30,0x20` / `stw r7,0x24` -- so it is two
        // unnamed X360 words BEFORE miStaticBufferSize, not eight words before meId.]
    };

    // -------- ReadVariableQueue<BUFSIZE,ALIGN> @ X360 0x82653A60 (<13312,16>) / 0x82656CF0 (<512,16>) --------
    // Pop a packed variable-event queue off the playback stream and rebuild it in lpQueue:
    //   read the event count, then for each event read {type, size}, allocate the record in the
    //   destination queue (VariableEventQueue::AllocateEvent) and read the payload into it.
    // Per-event copy loop (NOT a bulk memcpy) -- each record is streamed individually because the
    // records are variable-length.
    template <s32 BUFSIZE, s32 ALIGN>
    s32 BaseSerialiser::ReadVariableQueue(CgsModule::VariableEventQueue<BUFSIZE, ALIGN>* lpQueue)
    {
        s32 liNumEvents;
        s32 liResult = Read(&liNumEvents, static_cast<s32>(sizeof(s32)));
        for (s32 liEvent = liNumEvents; liEvent > 0; --liEvent)
        {
            s32 liEventType;
            s32 liEventSize;
            Read(&liEventType, static_cast<s32>(sizeof(s32)));
            Read(&liEventSize, static_cast<s32>(sizeof(s32)));
            void* lpEventData = lpQueue->AllocateEvent(liEventType, liEventSize);
            liResult = Read(lpEventData, liEventSize);
        }
        return liResult;
    }

    // -------- WriteVariableQueue<BUFSIZE,ALIGN> @ X360 0x826539B8 (<13312,16>) --------
    // Push a packed variable-event queue onto the record stream:
    //   write the event count, then walk the queue with GetFirstEvent/GetNextEvent, writing
    //   {type, size, payload} for each event. Per-event copy loop (variable-length records).
    // FAITHFUL X360 QUIRK: the event TYPE id is captured ONCE from GetFirstEvent and re-written
    // for every event -- the type returned by GetNextEvent is discarded (the binary never
    // re-stores it into the write slot; only its next-event pointer + size outputs are used).
    template <s32 BUFSIZE, s32 ALIGN>
    s32 BaseSerialiser::WriteVariableQueue(CgsModule::VariableEventQueue<BUFSIZE, ALIGN>* lpQueue)
    {
        s32 liLength = lpQueue->GetLength();
        Write(&liLength, static_cast<s32>(sizeof(s32)));

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpQueue->GetFirstEvent(&lpEvent, &liEventSize);
        s32 liResult = liEventType;
        while (lpEvent != 0)
        {
            Write(&liEventType, static_cast<s32>(sizeof(s32)));  // NOTE: liEventType is NOT refreshed in-loop (see banner)
            Write(&liEventSize, static_cast<s32>(sizeof(s32)));
            Write(lpEvent, liEventSize);
            liResult = lpQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
        return liResult;
    }
}
