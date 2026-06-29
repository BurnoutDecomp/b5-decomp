#ifndef SDKS_EATECH_SND_SAMPLEDECODER_H
#define SDKS_EATECH_SND_SAMPLEDECODER_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/SampleDecoder.h
//
// EATech "Snd" PCM sample-format decoders. This family converts an incoming
// stream of raw audio samples (in some source encoding) into interleaved-by-
// channel s16 destination buffers. Reconstructed BY NAME from the shared
// CShortDestDecoder layout that every Decode/Feed in the family touches:
//
//   +0x00  vtable
//   +0x04  mpSource       u8*   read cursor into the source sample stream
//   +0x08  mpLastDest     s16*  last destination sample slot written
//   +0x0C  miRemaining    s32   source samples still to decode (consumed by Decode)
//   +0x10  miCapacity     s32   total samples handed in by Feed (set @ +0x10)
//   +0x14  miNumChannels  s16   number of output channels (read as lha, signed)
//
// The offsets are attested by the family's X360 Decode bodies (lwz +0xC remaining,
// lha +0x14 channel count, lwz +4 source cursor, stw +8 last-dest) and by the
// sibling CSign24IntDecS16::Feed @ 0x82B7A7AC (stw r4 -> +4, stw r5 -> +0x10,
// stw r6 -> +0xC). CShortDestDecoder is the shared polymorphic base whose vtable
// (off_821563F0) every derived decoder's destructor resets to during teardown.
//
// The per-format inner conversion differs only in how each source sample is
// widened/byte-ordered into the s16 destination; the surrounding cursor/loop
// bookkeeping is identical across the family. Each Decode is cited by its own
// X360 address in the matching .cpp.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

namespace Snd
{
    // Shared base for the s16-destination sample decoders. Holds the decode-cursor
    // state every derived format walks. The base's own vector-deleting destructor
    // is @ 0x82B73330 (stores off_821563F0, then operator delete on the deleting
    // variant); modelling Decode as virtual here lets each derived format override
    // it while sharing this layout (the X360 reuses the base vtable slot shape).
    class CShortDestDecoder
    {
    public:
        CShortDestDecoder()
            : mpSource(0)
            , mpLastDest(0)
            , miRemaining(0)
            , miCapacity(0)
            , miNumChannels(0)
        {
        }

        // Polymorphic base destructor. Defined out-of-line in CShortDestDecoder.cpp
        // so it owns a real definition home; the body is empty (no owned resources),
        // which is exactly what the X360 vector-deleting thunk @ 0x82B73330 emits
        // (vtable store + conditional operator delete).
        virtual ~CShortDestDecoder();

        // Decode aiCount source samples into apDest's per-channel destination
        // buffers. apDest is an array of s16* (one per channel); sample i of channel
        // c lands at apDest[c][i]. Pure in the base -- only the concrete formats below
        // have an attested X360 Decode body; the base's own Decode slot is never
        // emitted as a standalone function, so it is left abstract (not fabricated).
        virtual s32 Decode(s16* const* apDest, s32 aiCount) = 0;

    protected:
        u8* mpSource;        // +0x04  source read cursor (byte pointer)
        s16* mpLastDest;     // +0x08  last destination slot written
        s32 miRemaining;     // +0x0C  source samples still to decode
        s32 miCapacity;      // +0x10  total samples (set by Feed)
        s16 miNumChannels;   // +0x14  output channel count (read signed)
    };

    // Signed 16-bit source, byte-swapped into the s16 destination.
    // Decode @ 0x82B7A4E0  (stride 2 bytes/sample).
    class CSign16IntDecS16 : public CShortDestDecoder
    {
    public:
        virtual ~CSign16IntDecS16();
        virtual s32 Decode(s16* const* apDest, s32 aiCount);
    };

    // Signed 16-bit source, copied verbatim into the s16 destination.
    // Decode @ 0x82B7A438  (stride 2 bytes/sample).
    class CSign16BigIntDecS16 : public CShortDestDecoder
    {
    public:
        virtual ~CSign16BigIntDecS16();
        virtual s32 Decode(s16* const* apDest, s32 aiCount);
    };

    // Unsigned 8-bit source: bias by -128 then shift into the high byte of the s16.
    // Decode @ 0x82B7A7D8  (stride 1 byte/sample).
    class CUnSign8IntDecS16 : public CShortDestDecoder
    {
    public:
        virtual ~CUnSign8IntDecS16();
        virtual s32 Decode(s16* const* apDest, s32 aiCount);
    };

    // Signed 24-bit source: take the top two bytes (big-endian) into the s16.
    // Decode @ 0x82B7A598  (stride 3 bytes/sample). No standalone destructor TU is
    // recorded for this format, so it inherits the compiler-generated destructor
    // that chains to ~CShortDestDecoder.
    class CSign24bIntDecS16 : public CShortDestDecoder
    {
    public:
        virtual s32 Decode(s16* const* apDest, s32 aiCount);
    };

    // Signed 24-bit source: take the UPPER two bytes of each little-endian 24-bit
    // frame (byte +2 high, byte +1 low) into the s16; the low byte +0 is dropped.
    // Decode @ 0x82B7A648  (stride 3 bytes/sample). Unlike the family's other
    // formats this one also owns a Feed @ 0x82B7A7A0 that primes the source span,
    // and a scalar-deleting destructor @ 0x82B7D188.
    class CSign24IntDecS16 : public CShortDestDecoder
    {
    public:
        virtual ~CSign24IntDecS16();
        virtual s32 Decode(s16* const* apDest, s32 aiCount);

        // Prime the decoder with a source span. Returns 0 on success, -1 if
        // apSource is null or a prior span is still pending (miRemaining != 0).
        s32 Feed(u8* apSource, s32 aiCapacity, s32 aiRemaining);
    };

    // Signed 8-bit source: shift the source byte up into the high byte of the s16
    // (no -128 bias -- the signed counterpart of CUnSign8IntDecS16).
    // Decode @ 0x82B7A6F8  (stride 1 byte/sample). SetState @ 0x82B7A888 sets the
    // channel count (+0x14) from a u16 source word.
    class CSign8IntDecS16 : public CShortDestDecoder
    {
    public:
        virtual ~CSign8IntDecS16();
        virtual s32 Decode(s16* const* apDest, s32 aiCount);
        void SetState(const s16* apNumChannels);
    };
} // namespace Snd

#endif // SDKS_EATECH_SND_SAMPLEDECODER_H
