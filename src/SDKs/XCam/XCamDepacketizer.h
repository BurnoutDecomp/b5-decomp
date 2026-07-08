#pragma once

// ===========================================================================
// XCAM::CDepacketizer -- the receiver-side FEC (forward-error-correction) video
// depacketizer of the X360 XCAM (Xbox Live Vision camera) streaming SDK in
// BURNOUT_X360_ARTIST.XEX. It is the decode-side sibling of the committed
// CPacketizer (XCamPacketizer.h): it accepts wire packets (data chunks and XOR
// recovery/parity rows), reassembles them into a rolling frame buffer keyed by
// sequence number, recovers dropped chunks from their parity rows, and hands the
// next ready frame's chunks to the decoder.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamDepacketizer.cpp:
//
//     XCAM::CDepacketizer::Initialize                     @ 0x82987E28
//     XCAM::CDepacketizer::Shutdown                       @ 0x82987F18
//     XCAM::CDepacketizer::Reset                          @ 0x829877B0
//     XCAM::CDepacketizer::SubmitPacket                   @ 0x82987C30
//     XCAM::CDepacketizer::SubmitPacket_Init              @ 0x82986DA8
//     XCAM::CDepacketizer::SubmitPacket_DataPacket        @ 0x82987830
//     XCAM::CDepacketizer::SubmitPacket_Correction        @ 0x829878C8
//     XCAM::CDepacketizer::FrameReadyToDecode             @ 0x82987228
//     XCAM::CDepacketizer::GetNextDataChunk               @ 0x82987570
//     XCAM::CDepacketizer::ReleaseLastReadFrame           @ 0x82987730
//     XCAM::CDepacketizer::ReleasePacketsFromPacketBuffer @ 0x82987118
//     XCAM::CDepacketizer::ReleaseOldPacketFromPacketBuffer @ 0x82986D00
//     XCAM::CDepacketizer::AddQOSDataToPacket             @ 0x829866C8
//
// (XCAM::RoundCompare, the circular sequence comparator these use, lives with
// the bodies in XCamDepacketizer.cpp.)
//
// There is no reference source and no DWARF for this TU; the SHAPE below is
// reconstructed store-for-store from the X360 asm (member offsets are the
// load/store displacements off `this`; the X360 offsets are noted in comments,
// but access is by named member -- the PC layout widens the 4-byte console
// pointers, so the console displacements do not carry over). `XCAM` is an X360
// SDK boundary, so its identifiers are preserved verbatim per the convention.
//
// The on-the-wire packets (the `pPacket` byte streams handed to SubmitPacket*)
// and the XOR recovery rows are SERIALISED byte streams whose layout is fixed by
// the format, not a C++ class; their fields are documented raw-offset per the
// serialised-data exception. The wire packet layout (8-byte header + payload)
// mirrors the CPacketizer encoder:
//     +0x00 u8   marker byte
//     +0x01 u8   flags       (bits 0..1 = packet kind: 1 data-request / 2 sync)
//     +0x02 u16  sequence number
//     +0x04 u32  packed metadata bitfield (chunk index / marker / offsets / len)
//     +0x08 ...  chunk payload
//   packed +0x04 metadata (encoder & decoder agree):
//     bits  0..3  chunk-in-row index      bits 12..17 (frameLen-1)/chunkSize
//     bits  4..5  marker & 3              bits 18..21 parity-row span
//     bits  6..11 data-chunk offset       bits 22..31 chunk length - 1
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamOverlappedIO.h"

namespace XCAM
{

// ---- raw Xbox 360 XDK heap + timing API (platform boundary) -----------------
// The asm calls XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) and the
// Win32/X360 QueryPerformance* directly. Supplied by the platform layer; NOT
// reconstructed here. (XMemAlloc/XMemFree already declared by the sibling XCam
// headers; re-declared identically here for a standalone include.)
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);
extern "C" u8 QueryPerformanceCounter(s64* pCount);
extern "C" u8 QueryPerformanceFrequency(s64* pFrequency);

// XMemAlloc/XMemFree attribute word for the depacketizer buffers (lis 0x6090 /
// ori 0x2068) -- the same tag the sibling CPacketizer uses.
static const u32 KU_DEPACKETIZER_XMEM_ATTRIBUTES = 0x60902068u;

// Circular/wraparound comparison of two sequence counters within a modulus
// (default the 0xFF78 sequence space). Returns 0 equal, <0 if uA precedes uB,
// >0 otherwise. Defined in XCamDepacketizer.cpp.  @ 0x829863F0
int RoundCompare(u32 uA, u32 uB, u32 uModulus);

// A serialised XOR recovery/parity packet, one per entry of mpRecoveryTable
// (each miPayloadSize+8 bytes; the header below occupies the first 16 bytes and
// the parity payload follows). External byte stream -- documented by offset.
struct RecoveryPacket
{
    s32 miRemaining;   // +0x00 outstanding-chunk countdown (-1 == frame recovered)
    u32 muMask;        // +0x04 received-chunk bitmask (XOR of 1<<chunkIndex bits)
    u8  muMarker;      // +0x08 marker byte carried from the packet header
    u8  muFlags;       // +0x09 bit0 == in use
    u16 muSeq;         // +0x0A wrapped base sequence of the covered chunk span
    u32 muMeta;        // +0x0C packed metadata bitfield (XOR-accumulated)
    u8  mPayload[1];   // +0x10 XOR parity accumulator (miPayloadSize bytes)
};

// A reorder-list node: 15 are embedded contiguously in the depacketizer (the
// maHeaders array), threaded newest-first through mpNext and consumed in
// sequence order. UpdateFrameBuffer (its own TU) fills mExpected/mReceived and
// stamps mReceiveTime; the members are read here.
struct FrameHeader
{
    u16          muSeq;        // +0x00 sequence number of this frame
    u16          muPad02;      // +0x02
    FrameHeader* mpNext;       // +0x04 next node in the reorder list
    void*        mpData;       // +0x08 non-null once the frame carries chunk data
    s64          mExpected;    // +0x10 expected-chunk mask (== mReceived when done)
    s64          mReceived;    // +0x18 received-chunk mask
    s64          mReceiveTime; // +0x20 QPC stamp of arrival (0 == unfilled/empty)
};

// One per-sequence reassembly slot (300 in the mpFrameSlots table). muType
// selects the active member of the union: type 2 drives an async overlapped read
// (whose overlapped stashes the source packet), type 1 records a synchronously
// known chunk sequence; mpRecovery links the pending XOR correction for the slot.
struct FrameSlot
{
    u32 muType;                  // +0x00 0 empty / 1 sync / 2 async / 3 recovered
    union
    {
        COverlappedIO mAsync;    // +0x04 (type 2) async request driver
        u16           muDataSeq; // +0x04 (type 1) known chunk sequence
    };
    RecoveryPacket* mpRecovery;  // +0x10 pending correction / recovered source
};

class CDepacketizer
{
public:
    // @ 0x82987E28 -- allocate the 300-slot frame table + the 40 XOR recovery
    // packets and reset all state. iPayloadSize (the r5/3rd-arg the asm stores at
    // +0x0C) is the per-chunk payload size; the first arg is unused by the asm.
    // Returns 0 on success, 8 (out of memory).
    int Initialize(int iReserved, int iPayloadSize);

    // @ 0x82987F18 -- Reset() then free the frame table and recovery packets.
    int Shutdown();

    // @ 0x829877B0 -- release every retained packet and zero the frame table,
    // header nodes and recovery packets, leaving the stream idle.
    int Reset();

    // @ 0x82987C30 -- accept one wire packet: advance the reorder list, drop it
    // if it is older than the current window or a duplicate, else route it to the
    // data / correction handlers and complete its overlapped. Returns 0, or 13
    // (duplicate). pTimestamp/a3 is the packet byte length; pOverlapped is the
    // caller's async request.
    int SubmitPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped);

    // @ 0x82986DA8 -- lazily prime the reorder list and window from the first
    // packet of a stream (seeds all header nodes + recovery packets with the
    // starting sequence). Returns QueryPerformanceCounter's BOOL.
    u8 SubmitPacket_Init(const u8* pPacket);

    // @ 0x82987830 -- record a data chunk into its frame slot: either as an async
    // overlapped read (kind 1) or a synchronously known sequence (otherwise).
    // Returns 0 or the overlapped Setup error.
    int SubmitPacket_DataPacket(const u8* pPacket, int iLength, XOVERLAPPED* pOverlapped);

    // @ 0x829878C8 -- fold a chunk into its XOR recovery packet; when the row's
    // last missing chunk is filled, reconstruct it and post it back as a
    // recovered frame slot. Always returns 0.
    int SubmitPacket_Correction(const u8* pPacket, int iLength);

    // @ 0x82987228 -- decide whether the next frame is ready to hand to the
    // decoder (bounded by the latency budget / decode-time average), advancing
    // the playback sequence and updating timing. *piCodecType receives the
    // frame's 2-bit codec/type field. Returns 1 when a frame is ready, else 0.
    int FrameReadyToDecode(int* piCodecType);

    // @ 0x82987570 -- fetch the next chunk of the frame currently being decoded:
    // *ppData = payload pointer, *piLength = chunk byte length, *pbLast = whether
    // this is the final chunk. Returns `this` (r3 passthrough).
    CDepacketizer* GetNextDataChunk(void** ppData, int* piLength, int* pbLast);

    // @ 0x82987730 -- release the packets of the frame just decoded and rewind
    // the read cursor to the frame boundary.
    int ReleaseLastReadFrame();

    // @ 0x82987118 -- release every retained packet whose sequence lies in the
    // [iFirst, iLast] window (inclusive, wrapping), signalling each async read
    // complete and clearing its slot.
    FrameSlot* ReleasePacketsFromPacketBuffer(int iFirst, int iLast);

    // @ 0x82986D00 -- release the single retained packet at sequence iSeq: signal
    // an async read complete (recovered), then clear the slot. Returns the slot.
    FrameSlot* ReleaseOldPacketFromPacketBuffer(int iSeq);

    // @ 0x829866C8 -- fold the pending QOS feedback counters into an outgoing
    // packet's flag byte (loss / NACK / key-frame-request bits). Returns `this`.
    CDepacketizer* AddQOSDataToPacket(u8* pPacket);

    // Reassemble a data/recovered chunk into the frame buffer. Its own TU (not in
    // this ledger); declared so the handlers above compile against it.  @ external
    FrameHeader* UpdateFrameBuffer(const u8* pPacket, int iLength);

    // --- layout (member ORDER is store-for-store with the X360 asm; the X360
    //     displacement is noted, but access is by named member) ---
    int              miRequestKeyFrame;    // X360 +0x00 QOS: request a key frame
    int              miResendRequestCount; // +0x04 QOS: pending resend/NACK count
    int              miLostPacketCount;    // +0x08 QOS: observed lost-packet count
    int              miPayloadSize;        // +0x0C per-chunk payload size
    FrameSlot*       mpFrameSlots;         // +0x10 300-entry per-sequence table
    u32              muRecoveryCount;      // +0x14 recovery-packet count (== 40)
    RecoveryPacket** mpRecoveryTable;      // +0x18 table of muRecoveryCount packets
    FrameHeader      maHeaders[15];        // +0x20 embedded reorder-list nodes
    FrameHeader*     mpCurrentHeader;      // +0x278 head of the reorder list
    s64              mPerfFrequency;       // +0x280 QueryPerformanceFrequency
    s64              mLastDecodeTime;      // +0x288 QPC stamp of last decoded frame
    int              miMaxLatencyMs;       // +0x290 latency budget (200 ms)
    int              miAvgDecodeMs;        // +0x294 running decode-interval avg
    u16              muCurrentSeq;         // +0x298 current playback sequence
    u16              muPad29A;             // +0x29A
    int              miReadOffset;         // +0x29C chunk cursor within the frame
};

} // namespace XCAM
