#pragma once

// ===========================================================================
// XCAM::CPacketizer -- the FEC (forward-error-correction) video packetizer of
// the X360 XCAM (Xbox Live Vision camera) streaming SDK in
// BURNOUT_X360_ARTIST.XEX. It is the encoder-side sibling of the committed
// CDepacketizer (XCamDepacketizer.cpp): it chops a captured frame into
// fixed-size chunks, packs each chunk into a wire packet (8-byte header +
// payload), and maintains a rolling set of XOR parity ("recovery") rows so a
// dropped chunk can be reconstructed by the receiver.
//
// This header is the canonical OWNING home for the reconstructed bodies in
// XCamPacketizer.cpp:
//
//     XCAM::CPacketizer::Initialize                    @ 0x82986440
//     XCAM::CPacketizer::Shutdown                      @ 0x82986628
//     XCAM::CPacketizer::GetBufferForWriting           @ 0x82986680
//     XCAM::CPacketizer::GetNewestPacketSequenceNumber @ 0x829866B8
//     XCAM::CPacketizer::SetBitrate                    @ 0x829866C0
//     XCAM::CPacketizer::BufferWriteCompleted          @ 0x82986748
//     XCAM::CPacketizer::GetEncodedPacket              @ 0x82986A88
//
// There is no reference source and no DWARF for this TU; the SHAPE below is
// reconstructed store-for-store from the X360 asm (member offsets are the
// load/store displacements off `this`). `XCAM` is an X360 SDK boundary, so its
// identifiers (CPacketizer, GetEncodedPacket, ...) are preserved verbatim per
// the naming convention.
//
// The on-the-wire packet buffers and the parity rows are SERIALISED byte
// streams (external data whose layout is fixed by the format, not a C++ class),
// so their fields are accessed by documented raw offset per the serialised-data
// exception; only `this` uses named members.
//
//   PACKET BUFFER (one of mpBuffers[0..2]; iBufferSize = chunkSize*chunks+0x11):
//     +0x00 u32  sequence number stamped at completion (== miNewestSeq)
//     +0x04 u32  parity-row index snapshot (miCurrentRow)
//     +0x08 u32  chunk-in-row snapshot     (miChunkInRow)
//     +0x0C u32  frame length              (the a2 passed to BufferWriteCompleted)
//     +0x10 u8   marker byte               (*pMarker)
//     +0x11 ...  chunk payload data
//
//   PARITY ROW (row stride = chunkSize+12; mpRowTable is a table of muRowCount
//   pointers followed by the row bodies):
//     +0x00 u32  max chunk length XORed into this row
//     +0x04 u8   packet header byte 0
//     +0x05 u8   packet header byte 1
//     +0x06 u16  wrapped sequence (mod 0xFF78)
//     +0x08 u32  packed metadata bitfield (chunk index / marker / offsets / len)
//     +0x0C ...  XOR parity accumulator (payload)
//
//   Packed +0x08 / packet-header +4 metadata bitfield layout (encoder & decoder
//   agree):
//     bits  0..3  chunk-in-row index
//     bits  4..5  marker & 3
//     bits  6..11 data-chunk offset  / chunkSize
//     bits 12..17 (frameLen-1)       / chunkSize
//     bits 18..21 parity-row span
//     bits 22..31 chunk length - 1
// ===========================================================================

#include "types.hpp"

namespace XCAM
{

// ---- raw Xbox 360 XDK heap API (platform externs) -----------------------------------
// The asm calls XMemAlloc(SIZE_T,DWORD)->LPVOID / XMemFree(LPVOID,DWORD) directly
// (r3=size/pAddress, r4=attributes), matching the sibling rwmovie codec TUs.
extern void* XMemAlloc(u32 uSize, u32 uAttributes);
extern void  XMemFree(void* pAddress, u32 uAttributes);

// Win32/X360 millisecond tick used to timestamp the stream (lis/ori attr aside).
extern "C" u32 GetTickCount();

// XMemAlloc/XMemFree attribute word for the packetizer buffers (lis 0x6090 / ori 0x2068).
static const u32 KU_PACKETIZER_XMEM_ATTRIBUTES = 0x60902068u;

class CPacketizer
{
public:
    // @ 0x82986440 -- allocate the 3 assembly buffers + the parity-row table and
    // reset all counters. iPayloadSize is the frame size to packetize;
    // uMaxPacketSize is the requested wire packet size (clamped to [0x10,0x408]).
    // Returns 0 on success, 8 (out of memory / too small) or 87 (too many chunks).
    int Initialize(int iPayloadSize, u32 uMaxPacketSize);

    // @ 0x82986628 -- free the two XMemAlloc'd blocks.
    void Shutdown();

    // @ 0x82986680 -- return the payload write pointer of the current buffer
    // (buffer + 0x11) and latch the recycled buffer's sequence base into miSeqBase.
    u8* GetBufferForWriting();

    // @ 0x829866B8 -- newest emitted packet sequence number (miNewestSeq).
    int GetNewestPacketSequenceNumber();

    // @ 0x829866C0 -- set the bitrate budget field (gates the FEC path).
    void SetBitrate(int iBitrate);

    // @ 0x82986748 -- finalise the chunk just written into the current buffer:
    // stamp its header, then (when the bitrate budget > 32) fold it into the
    // rolling XOR parity rows, advancing sequence/row/buffer counters. uLength is
    // the chunk byte count (0xFFFFFFFF or a null marker is a no-op).
    void BufferWriteCompleted(u32 uLength, const u8* pMarker);

    // @ 0x82986A88 -- reconstruct the packet for sequence iSeq into pOut, writing
    // its byte length to *piOutLen. Walks the buffers/parity rows to recover a
    // missing data chunk from its parity row. Returns 0, or 87 if iSeq is outside
    // the retained window.
    int GetEncodedPacket(int iSeq, u8* pOut, int* piOutLen);

    // --- layout (member ORDER is store-for-store with the X360 asm; offsets are
    //     the PC x64 widened form -- pointers are 8 bytes here vs 4 on X360, so
    //     the console displacements do not carry over. Parity is by named member,
    //     not byte offset, per the x64-widening convention.) ---
    u8*  mpBuffers[3];   // X360 +0x00 the three round-robin packet-assembly buffers
    int  miWriteBuffer;  // +0x0C index (0..2) of the buffer being written
    u32  muRowCount;     // +0x10 number of parity rows in mpRowTable
    u8*  mpRowTable;     // +0x14 table of muRowCount row pointers, then row bodies
    int  miCurrentRow;   // +0x18 index of the parity row currently accumulating
    int  miChunkInRow;   // +0x1C chunks folded into the current parity row so far
    int  miChunkTarget;  // +0x20 chunks-per-row flush threshold (8)
    int  miBitrate;      // +0x24 bitrate budget (FEC enabled when > 32)
    u32  muPacketSize;   // +0x28 wire packet size (chunkSize + 8)
    u32  muChunkSize;    // +0x2C payload chunk size
    u32  muWriteCount;   // +0x30 chunks written since the last parity flush
    int  miSeqBase;      // +0x34 sequence base of the oldest retained buffer
    int  miNewestSeq;    // +0x38 newest emitted sequence number
    u32  muStartTick;    // X360 +0x3C GetTickCount() at Initialize
    u32  muReserved40;   // X360 +0x40 written 0 by Initialize; unused by these methods
};

} // namespace XCAM
