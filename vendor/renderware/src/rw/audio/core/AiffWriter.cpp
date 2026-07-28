// =====================================================================================
// rw::audio::core::AiffWriter bodies -- the audio-core "record the mix to an AIFF file"
// debug plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 PDB entry exist for this type.
//   CreateInstance                @0x82B9D488 -- store-for-store placement init
//   EventEvent                    @0x82BA5448 -- post a start/stop record into the ring
//   GetPlugInDescRunTime          @0x82B968B0 -- returns the registered "AiffWriter" desc
//   GetSize                       @0x82B96838 -- returns 0x50 (80)
//   IntToExtended                 @0x82B96848 -- u32 -> 80-bit big-endian extended float
//   Process                       @0x82BA2168 -- interleave one block to s16 PCM
//   ReleaseEvent                  @0x82BA53F8 -- flush/close then free the buffer
//   StartHandler                  @0x82B9D510 -- open file + placeholder header + timer
//   StopHandler                   @0x82BA1EC0 -- rewrite the AIFF header, close, drop timer
//   `scalar deleting destructor'  @0x82B9D428 -- destruct TimerHandle, reinstall vtable, free
// See AiffWriter.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/AiffWriter.h"
#include "rw/audio/core/PlugIn.h"       // rw::audio::core::System (Alloc/Free/RemoveTimer, ring)
#include "rw/audio/core/TimerManager.h" // rw::audio::core::TimerManager::AddTimer
#include "rw/audio/core/Iir2Filters.h"  // AudioProcessContext / AudioChannelBuffer / AudioFormat

#include <cstdio>  // std::fopen / fwrite / fseek / fclose / FILE
#include <cstring> // std::memcpy / std::memset
#include <cstddef> // offsetof -- the start record's header size on the host

namespace rw
{
namespace audio
{
namespace core
{

// XMemCpy is the Xbox platform block copy (declared here, defined in the platform TU);
// used to flush the interleaved PCM staging buffer to mpBuf. @0x82BA2168 tail-calls it.
extern "C" void *XMemCpy(void *pDest, const void *pSrc, unsigned int uiCount);

// -------------------------------------------------------------------------------------
// Opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
//   off_8217F308 -- the AiffWriter v-table installed by CreateInstance.
//   off_820AA810 -- the base v-table the scalar deleting destructor reinstalls (shared with
//                   the Snd9Service / PlugIn family's deleting destructors).
//   off_82F8C228 -- the registered run-time descriptor record (its +0x00 label is the
//                   string "AiffWriter").
// -------------------------------------------------------------------------------------
static void *const KAIFF_WriterVTable = nullptr; // off_8217F308
static void *const KAIFF_BaseVTable   = nullptr; // off_820AA810
static char       *g_AiffWriterDesc   = nullptr; // off_82F8C228 (the "AiffWriter" record)

// The clamp / scale constants Process uses (flt_82001C98 == 1.0, flt_820037C8 == -1.0,
// flt_820AD310 == 32767.0 -- the full-scale s16 magnitude).
static const f32 KF_ONE       = 1.0f;
static const f32 KF_MINUS_ONE = -1.0f;
static const f32 KF_S16_SCALE = 32767.0f;

// The Process staging reservation: 3072 bytes == 256 f32 sample frames * 6 channels * 2
// bytes (the interleaved s16 span), carved off the System object table's scratch cursor.
enum { KI_AIFF_SCRATCH_BYTES = 3072, KI_AIFF_BLOCK_SAMPLES = 256 };

// The embedded TimerHandle sub-object's destructor, invoked on (self+0x24) by the scalar
// deleting destructor. IDA renders the callee as an unresolved STUB and it is un-exported
// with no disassembly, so it is declared here and left to link rather than fabricated.
void AiffWriter_DestructTimerHandle(TimerHandle *handle);

// The deferred-command ring records EventEvent posts. The "start" record is variable length
// (a header + the NUL-terminated path, rounded up so the next record stays aligned); the
// "stop" record is a fixed header-only record. Grounded in EventEvent @0x82BA5448 and read
// back by StartHandler / StopHandler.
//
// RECORD STRIDE (X360-literal trap): every cursor advance and handler return below is the
// record's OWN size. The console immediates (the start path's `(strlen + 16) & ~3` and the
// stop path's `addi r10,r10,8` @0x82BA54E4 / `li r3,8` @0x82BA215C) are the 32-bit sizes of
// these same structs; on the host the handler/writer pointers widen to 8 bytes, so the sizes
// are taken from sizeof/offsetof and the console values are kept only as comments.
//
// Start record (X360 12-byte header + path; host offsetof(maPath) == 20):
//   +0x00  mpfnHandler   (StartHandler)
//   +0x04  mpWriter      (the AiffWriter this)
//   +0x08  muRecordSize  (the rounded record byte size, returned by StartHandler)
//   +0x0C  maPath[]      (the NUL-terminated output path)
struct AiffCommandRecord
{
    int (*mpfnHandler)(void *);
    AiffWriter *mpWriter;
    u32         muRecordSize;
    char        maPath[1];
};

// Stop record (X360 sizeof 8; host 16) -- header only. StopHandler reads just mpWriter and
// returns this record's size as the ring advance.
//   +0x00  mpfnHandler   (StopHandler)
//   +0x04  mpWriter      (the AiffWriter this)
struct AiffStopCommandRecord
{
    int (*mpfnHandler)(void *);
    AiffWriter *mpWriter;
};

// -------------------------------------------------------------------------------------
// Store a 32-bit value big-endian (MSB first) into `p` -- the byte order every AIFF size /
// count field uses. The X360 asm produces this naturally by storing the word and copying
// its (big-endian) bytes; on the little-endian PC target it is written out explicitly.
// -------------------------------------------------------------------------------------
static inline void StoreBE32(u8 *p, u32 v)
{
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B96838 -- the plug-in instance footprint.
// -------------------------------------------------------------------------------------
int AiffWriter::GetSize()
{
    return 0x50; // li r3, 0x50
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B968B0 -- return the address of the registered descriptor
// record (its label is the string "AiffWriter").
// -------------------------------------------------------------------------------------
char **AiffWriter::GetPlugInDescRunTime()
{
    return &g_AiffWriterDesc; // &off_82F8C228
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82B9D488 -- placement-init an AiffWriter over `self`.
//   if (self) { self->mpVTable = off_8217F308; TimerHandle_ctor(&self->mTimerHandle); }
//   self->mpFile = 0;  self->mbDataWritten = 0;  self->mbTimerCreated = 0;
//   buf = System::Alloc(self->mpSystem, muBufferChannels<<9, "...mpBuf", 16, 0);
//   self->mpBuf = buf;  return buf != 0;
// -------------------------------------------------------------------------------------
int AiffWriter::CreateInstance(AiffWriter *self)
{
    if (self)
    {
        self->mpVTable = KAIFF_WriterVTable;                // off_8217F308 -> +0x00
        TimerHandle::TimerHandle_ctor(&self->mTimerHandle); // ctor(self+0x24)
    }

    // Buffer size = muBufferChannels * 512 bytes (rotlwi r4, r10, 9 == <<9).
    const u32 uBufferBytes = static_cast<u32>(self->muBufferChannels) << 9;

    self->mpFile        = nullptr; // stw 0 -> +0x3C
    self->mbDataWritten = 0;       // stb 0 -> +0x4C
    self->mbTimerCreated = 0;      // stb 0 -> +0x4D

    void *buf = System::Alloc(self->mpSystem, uBufferBytes,
                              "rw::audio::core::AiffWriter::mpBuf", 16, 0);
    self->mpBuf = buf; // +0x40
    return buf != nullptr;
}

// -------------------------------------------------------------------------------------
// EventEvent @0x82BA5448 -- append a start / stop record to the System deferred-command
// ring for the audio thread to dispatch.
//   sys = self->mpSystem;
//   if (a2) {  // stop: fixed 8-byte record
//     rec = sys->mpDeferredRingBase + sys->muDeferredRingCursor;
//     sys->muDeferredRingCursor += 8;
//     rec->mpfnHandler = &StopHandler;  rec->mpWriter = self;
//   } else {   // start: 12-byte header + path, rounded up to 4 bytes
//     name = *pName;  size = (strlen(name) + 16) & ~3;
//     rec = sys->mpDeferredRingBase + sys->muDeferredRingCursor;
//     sys->muDeferredRingCursor += size;
//     rec->mpfnHandler = &StartHandler;  rec->mpWriter = self;  rec->muRecordSize = size;
//     strcpy(rec->maPath, name);
//   }
//   return self;
// -------------------------------------------------------------------------------------
AiffWriter *AiffWriter::EventEvent(AiffWriter *self, int a2, char **pName)
{
    System *sys = self->mpSystem; // v3 = *(result+4)

    if (a2)
    {
        // Stop record: header only (handler + writer).
        u32 cursor = sys->muDeferredRingCursor;                 // *(v3+4280)
        AiffStopCommandRecord *rec =
            reinterpret_cast<AiffStopCommandRecord *>(sys->mpDeferredRingBase + cursor);
        sys->muDeferredRingCursor =
            cursor + static_cast<u32>(sizeof(AiffStopCommandRecord)); // X360: +8 @0x82BA54E4
        rec->mpfnHandler = &AiffWriter::StopHandler;
        rec->mpWriter    = self;
    }
    else
    {
        // Start record: header + the NUL-terminated path, rounded up so the next record's
        // leading handler pointer stays aligned.
        const char *name = *pName; // v4 = *a3

        u32 nameLen = 0;
        while (name[nameLen])
            ++nameLen; // strlen via the asm's byte walk

        // X360: `(strlen + 16) & ~3` (addi 0x10 @0x82BA5488 / clrrwi 2 @0x82BA548C) == a
        // 12-byte header + NUL, 4-aligned for the console's 4-byte pointers. On the host the
        // header is offsetof(maPath) and the align is 8. muRecordSize therefore carries the
        // HOST size, which StartHandler returns unchanged as the ring advance.
        const u32 recordSize =
            (static_cast<u32>(offsetof(AiffCommandRecord, maPath)) + nameLen + 1 + 7) & ~7u;

        u32 cursor = sys->muDeferredRingCursor;                 // *(v3+4280)
        AiffCommandRecord *rec =
            reinterpret_cast<AiffCommandRecord *>(sys->mpDeferredRingBase + cursor);
        sys->muDeferredRingCursor = cursor + recordSize;

        rec->mpfnHandler  = &AiffWriter::StartHandler;
        rec->mpWriter     = self;
        rec->muRecordSize = recordSize;

        // Copy the path (including its NUL terminator) into the record body.
        char *dst = rec->maPath;
        const char *src = name;
        char c;
        do
        {
            c = *src++;
            *dst++ = c;
        } while (c);
    }

    return self;
}

// -------------------------------------------------------------------------------------
// IntToExtended @0x82B96848 -- convert `uValue` to its 80-bit IEEE-754 extended-precision
// big-endian representation (the AIFF COMM chunk's sample-rate encoding).
//   iBits  = bit position (1-based) of the most-significant set bit;
//   exp    = iBits + 16382 (= 16383 bias + (iBits - 1));   16-bit big-endian at [0..1];
//   mant   = uValue << (32 - iBits)  (leading 1 normalised to the top);  the 64-bit
//            mantissa's top 32 bits are `mant` big-endian at [2..5], the low 32 bits 0.
// -------------------------------------------------------------------------------------
u8 *AiffWriter::IntToExtended(u8 *pExtended, u32 uValue)
{
    u32 uShifted = uValue >> 1;
    int iBits = 1;
    while (uShifted)
    {
        uShifted >>= 1;
        ++iBits;
    }

    const u32 uExponent = static_cast<u32>(iBits + 16382); // 16383 bias + (iBits - 1)
    const u32 uMantissa = uValue << (32 - iBits);          // normalised, leading 1 at top

    pExtended[0] = static_cast<u8>(uExponent >> 8); // exponent high byte
    pExtended[1] = static_cast<u8>(uExponent);      // exponent low byte
    pExtended[2] = static_cast<u8>(uMantissa >> 24);
    pExtended[3] = static_cast<u8>(uMantissa >> 16);
    pExtended[4] = static_cast<u8>(uMantissa >> 8);
    pExtended[5] = static_cast<u8>(uMantissa);
    pExtended[6] = 0; // low 32 mantissa bits are 0
    pExtended[7] = 0;
    pExtended[8] = 0;
    pExtended[9] = 0;
    return pExtended;
}

// -------------------------------------------------------------------------------------
// Process @0x82BA2168 -- when recording is live, interleave one 256-sample block of the
// source buffer's channels to signed-16-bit PCM in mpBuf.
//   if (!mpFile) return 1;                       // not recording
//   if (!muSampleRate) muSampleRate = (u32)ctx->mpFormat->mfSampleRate;
//   reserve 3072 bytes off the System object-table scratch cursor (word[3]);
//   for each channel c in [0, muChannelCount):
//     src = ctx->mpSrcBuffer->mpSamples + ctx->mpSrcBuffer->muStride * c;
//     for each sample s in [0, 256):
//       v = clamp(src[s], -1, 1) * 32767;
//       scratch[muChannelCount * s + c] = (s16)v;   // interleaved
//   XMemCpy(mpBuf, scratch, muChannelCount << 9);
//   restore the scratch cursor;  mbDataWritten = 1;  return 1;
// (Note: the block's frame count is metered into muSampleFrameCount by RwacTimerClient, not
// here -- Process only flushes the interleaved PCM.)
// -------------------------------------------------------------------------------------
int AiffWriter::Process(AiffWriter *self, AudioProcessContext *ctx)
{
    if (!self->mpFile) // *(a1+60)
        return 1;

    if (!self->muSampleRate) // *(a1+72)
        self->muSampleRate = static_cast<u32>(ctx->mpFormat->mfSampleRate);

    // Reserve a 3072-byte interleave region off the System object table's scratch cursor
    // (word[3]); the value is restored at the tail. (Same idiom as Delay::Process.)
    System *pSystem = self->mpSystem;
    u8 **ppScratchTable = reinterpret_cast<u8 **>(pSystem->mpObjectTable); // **(a1+4)
    u8 *pScratchTop = ppScratchTable[3];                                   // objectTable[3]
    u8 *pScratch = pScratchTop - KI_AIFF_SCRATCH_BYTES;                    // top - 3072
    ppScratchTable[3] = pScratch;

    AudioChannelBuffer *pSrc = ctx->mpSrcBuffer; // *(a2+0x3000C)
    s16 *pInterleaved = reinterpret_cast<s16 *>(pScratch);
    const u32 uChannels = self->muChannelCount;

    for (u32 c = 0; c < uChannels; ++c)
    {
        const f32 *pSamples = pSrc->mpSamples + pSrc->muStride * c; // base + 4*stride*c bytes
        for (int s = 0; s < KI_AIFF_BLOCK_SAMPLES; ++s)
        {
            f32 sample = pSamples[s];
            if (sample <= KF_ONE)
            {
                if (sample < KF_MINUS_ONE)
                    sample = KF_MINUS_ONE;
            }
            else
            {
                sample = KF_ONE;
            }
            pInterleaved[uChannels * static_cast<u32>(s) + c] =
                static_cast<s16>(sample * KF_S16_SCALE);
        }
    }

    XMemCpy(self->mpBuf, pScratch, uChannels << 9); // memcpy(mpBuf, scratch, channels*512)

    ppScratchTable[3] = pScratchTop; // restore the scratch cursor
    self->mbDataWritten = 1;         // *(a1+76) = 1
    return 1;
}

// -------------------------------------------------------------------------------------
// StartHandler @0x82B9D510 -- deferred "start recording" ring handler. Runs on the audio
// thread with `pCommandRecord` -> the start record EventEvent posted.
//   self = record->mpWriter;
//   build an 82-byte header placeholder (12-byte tag + 70 zero bytes);
//   if (!self->mpFile) {
//     self->mbTimerCreated = 0; self->muSampleFrameCount = 0; self->muSampleRate = 0;
//     self->mpFile = fopen(record->maPath, "wb");
//     if (self->mpFile) {
//       fwrite(header, 1, 82, self->mpFile);
//       if (!TimerManager::AddTimer(&mpSystem->mTimerManager, &mTimerHandle,
//                                   RwacTimerClient, self, "AiffWriter", 1, 1))
//         self->mbTimerCreated = 1;
//     }
//   }
//   return record->muRecordSize;
// -------------------------------------------------------------------------------------
int AiffWriter::StartHandler(void *pCommandRecord)
{
    AiffCommandRecord *record = static_cast<AiffCommandRecord *>(pCommandRecord);
    AiffWriter *self = record->mpWriter; // v1 = *(a1+4)

    // The 82-byte header placeholder: a 12-byte "PlaceHolder\0" tag then 70 zero bytes.
    char header[82];
    std::memcpy(header, "PlaceHolder", 12); // strcpy -> memcpy(Size=0xC)
    std::memset(&header[12], 0, 70);

    if (!self->mpFile) // *(v1+60)
    {
        self->mbTimerCreated     = 0; // *(v1+77) = 0
        self->muSampleFrameCount = 0; // *(v1+68) = 0
        self->muSampleRate       = 0; // *(v1+72) = 0

        self->mpFile = std::fopen(record->maPath, "wb");
        if (self->mpFile)
        {
            std::fwrite(header, 1, 82, self->mpFile);

            // Register the pump timer in the System TimerManager (collection 1, visible).
            // AddTimer returns 0 on success; the asm tests only its low byte.
            if (!(TimerManager::AddTimer(
                      &self->mpSystem->mTimerManager, &self->mTimerHandle,
                      reinterpret_cast<TimerManager::TimerCallback>(&AiffWriter::RwacTimerClient),
                      self, "AiffWriter", 1, 1) &
                  0xFF))
            {
                self->mbTimerCreated = 1; // *(v1+77) = 1
            }
        }
    }

    return static_cast<int>(record->muRecordSize); // result = *(a1+8)
}

// -------------------------------------------------------------------------------------
// StopHandler @0x82BA1EC0 -- deferred "stop recording" ring handler. Seeks back to the file
// start and rewrites the AIFF header now that the final sizes are known, then closes the
// file and unregisters the pump timer. Returns the stop record's byte size (the ring
// advance) -- host sizeof(AiffStopCommandRecord); X360: li r3, 8 @0x82BA215C.
//
// The 82-byte header is FORM(8) + "AIFF"(4) + COMM(8+18) + INST(8+20) + SSND(8+8). All size
// / count fields are big-endian (the asm builds them MSB-first).
// -------------------------------------------------------------------------------------
int AiffWriter::StopHandler(void *pCommandRecord)
{
    AiffStopCommandRecord *record = static_cast<AiffStopCommandRecord *>(pCommandRecord);
    AiffWriter *self = record->mpWriter; // v1 = *(a1+4)

    std::FILE *file = self->mpFile; // v2 = *(v1+60)
    if (file)
    {
        std::fseek(file, 0, SEEK_SET);

        // Sample-data byte count = 2 bytes/sample * channels * frame count.
        const u32 dataBytes = 2u * self->muChannelCount * self->muSampleFrameCount; // v3

        u8 chunkHeader[8];

        // FORM chunk header: "FORM" + (dataBytes + 74) -- everything after the size field.
        std::memcpy(chunkHeader, "FORM", 4);
        StoreBE32(&chunkHeader[4], dataBytes + 74);
        std::fwrite(chunkHeader, 1, 8, file);

        // Form type.
        std::fwrite("AIFF", 1, 4, file);

        // COMM chunk header: "COMM" + 18.
        std::memcpy(chunkHeader, "COMM", 4);
        StoreBE32(&chunkHeader[4], 18);
        std::fwrite(chunkHeader, 1, 8, file);

        // COMM body (18 bytes): numChannels(u16) + numSampleFrames(u32) + sampleSize(u16) +
        // sampleRate(80-bit extended). All big-endian.
        u8 comm[18];
        // numChannels is a u8 channel count stored into the 16-bit COMM field (the asm's
        // `sth` of a `lbz` byte -> high byte 0); widen before the split to keep it a u16.
        const u32 uNumChannels = self->muChannelCount;
        comm[0] = static_cast<u8>(uNumChannels >> 8); // numChannels (BE u16), high byte
        comm[1] = static_cast<u8>(uNumChannels);
        StoreBE32(&comm[2], self->muSampleFrameCount);        // numSampleFrames (BE u32)
        comm[6] = 0;                                          // sampleSize (BE u16) = 16
        comm[7] = 16;
        AiffWriter::IntToExtended(&comm[8], self->muSampleRate); // sampleRate (10 bytes)
        std::fwrite(comm, 1, 18, file);

        // INST chunk header: "INST" + 20.
        std::memcpy(chunkHeader, "INST", 4);
        StoreBE32(&chunkHeader[4], 20);
        std::fwrite(chunkHeader, 1, 8, file);

        // INST body (20 bytes): baseNote 60, detune 0, lowNote 0, highNote 127,
        // lowVelocity 0, highVelocity 127, then 14 zero bytes (gain + the two loops).
        u8 inst[20];
        std::memset(inst, 0, sizeof(inst));
        inst[0] = 0x3C; // baseNote == 60 (middle C); "<" in the asm's strcpy
        inst[3] = 127;  // highNote
        inst[5] = 127;  // highVelocity
        std::fwrite(inst, 1, 20, file);

        // SSND chunk header: "SSND" + (dataBytes + 8) -- the 8-byte offset/blockSize plus the
        // already-written sample data.
        std::memcpy(chunkHeader, "SSND", 4);
        StoreBE32(&chunkHeader[4], dataBytes + 8);
        std::fwrite(chunkHeader, 1, 8, file);

        // SSND body prefix: offset(u32) + blockSize(u32), both 0. The sample data itself was
        // streamed in during recording and already follows on disk.
        u8 ssndPrefix[8];
        std::memset(ssndPrefix, 0, sizeof(ssndPrefix));
        std::fwrite(ssndPrefix, 1, 8, file);

        std::fclose(self->mpFile);

        const u8 timerCreated = self->mbTimerCreated; // *(v1+77)
        self->mpFile = nullptr;                       // *(v1+60) = 0
        if (timerCreated)
        {
            System::RemoveTimer(self->mpSystem, &self->mTimerHandle);
            self->mbTimerCreated = 0; // *(v1+77) = 0
        }
    }

    return static_cast<int>(sizeof(AiffStopCommandRecord)); // X360: li r3, 8 @0x82BA215C
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82BA53F8 -- finalise + tear down: run StopHandler over a throwaway record
// pointing at this writer (to flush + close the file), then free the PCM buffer.
//   char rec[8]; ((AiffWriter**)rec)[1] = self;
//   result = StopHandler(rec);
//   if (self->mpBuf) return System::Free(self->mpSystem, self->mpBuf, 0);
//   return result;
// (StopHandler reads only the record's mpWriter slot, so a header-only stop record on the
// stack suffices -- the X360 reserves exactly its 8 bytes, the host struct is 16.)
// -------------------------------------------------------------------------------------
int AiffWriter::ReleaseEvent(AiffWriter *self)
{
    AiffStopCommandRecord rec;
    rec.mpfnHandler = nullptr;
    rec.mpWriter    = self;    // v5 = a1  (StopHandler reads *(rec+4))

    int result = AiffWriter::StopHandler(&rec);

    void *buf = self->mpBuf; // v3 = *(a1+64)
    if (buf)
    {
        System::Free(self->mpSystem, buf, nullptr);
        return 0; // System::Free is void; the asm's returned r3 is that void register
    }
    return result;
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82B9D428
//   AiffWriter_DestructTimerHandle(&self->mTimerHandle); // STUB(self+0x24)
//   self->mpVTable = off_820AA810;   // reinstall the base v-table
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *AiffWriter::ScalarDeletingDestructor(AiffWriter *self, char flags)
{
    AiffWriter_DestructTimerHandle(&self->mTimerHandle); // STUB(self+0x24)
    self->mpVTable = KAIFF_BaseVTable;                   // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
