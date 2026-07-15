// =====================================================================================
// CLocalHuffmanEncoder -- per-table variable-length-code *encoder* for the RTCMV/WMV video
// path. CAltTablesEncoder owns an array of eleven of these (one per code table); each wraps
// a CLocalHuffman code table and adds the encoder-side scratch buffers.
//
// It derives from CLocalHuffman: the X360 asm destroys one by calling
// CLocalHuffman::~CLocalHuffman on it (0x82A7EE68) and CAltTablesEncoder::clear rebuilds
// its codes with CLocalHuffman::setCodes (0x82A7EA0C) -- both non-virtual base methods
// invoked on the derived pointer, which is what proves the inheritance. The X360 object is
// 0x4C (76) bytes.
//
// This header is reconstructed ONLY as far as its single consumer (class:CAltTablesEncoder,
// BURNOUT_X360_ARTIST.XEX) attests: the constructor / method BODIES and every un-observed
// member are separate, not-yet-reconstructed TUs and are left as declarations / documented
// padding. The X360 addresses members by byte offset (this[N] == byte offset N*4); on this
// PC/x64 target the pointer members widen, so the offsets noted below are documentary only
// and all access is by name.
// =====================================================================================
#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwmovie/CLocalHuffman.h"

class CAltTablesEncoder;   // forward: the ctor takes an owning back-pointer (breaks the cycle)

// Object referenced through CLocalHuffmanEncoder::mpState (+0x48): the owning
// CAltTablesEncoder, reached here through this opaque stand-in so the encoder header does not
// have to include CAltTablesEncoder.h (that would close the include cycle). Only the three
// words the encode/check path touches are named; everything else stays documented padding.
// The named offsets match the fields the X360 asm reaches inside the parent object.
struct SLocalHuffmanEncoderState
{
    u8   mPad00[8];         // +0x00  (unobserved)
    u32  muDirty;           // +0x08  tested by updateHistory / checkFrame / encodeHeader
    u8   mPad0C[0x18];      // +0x0C .. +0x23  (unobserved)
    u32  muMvSearchBits;    // +0x24  MV search range = (128 >> this) + 30; also a mode flag
    u8   mPad28[0x30];      // +0x28 .. +0x57  (unobserved)
    u32* mpuSymbolRecord;   // +0x58  -> the current packed 32-bit symbol record word
};

class CLocalHuffmanEncoder : public CLocalHuffman
{
public:
    // (table index, owning CAltTablesEncoder, error out). Body is a separate X360 TU.
    CLocalHuffmanEncoder(int liTableIndex, CAltTablesEncoder* lpParent, u32* lpuError);

    // --- methods invoked by CAltTablesEncoder (bodies in CLocalHuffmanEncoder.cpp) --------
    int  checkFrame();                     // measure this table's frame; pick the best code set
    int  checkInMV(void* lpContext);       // buffer one non-RTC motion-vector symbol record
    int  checkInRTCMV(void* lpContext);    // buffer one RTCMV motion-vector symbol record
    int  encodeSymbol(void* lpStream);     // emit the current symbol via the VLC table
    u32* WriteSymbol(void* lpStream);      // emit a literal/escape symbol record

    // --- data the X360 reaches into directly from CAltTablesEncoder (hence public) --------
    u32                        muNeedHeader;    // +0x24  set by checkFrame; encodeSymbol emits header then clears
    u32                        muCurrentCode;   // +0x28  selected code set; snapshotted into muHistoryCode
    u32                        muReferenceCode; // +0x2C  = muHistoryCode reference for this frame (0 when dirty)
    u32*                       mpauCodeBuffer;  // +0x30  per-symbol occurrence counts; cleared in clear, freed in dtor
    u32                        muDirtyFlag;     // +0x34  symbols checked-in this frame; zeroed in clear
    u32*                       mpauCodeData;    // +0x38  per-symbol code lengths; freed in dtor
    u32*                       mpauCodes;       // +0x3C  per-symbol code words (generateHuffman_balanced output)
    u32                        muHistoryCode;   // +0x40  = muCurrentCode when the table is dirty
    u32                        muEmittedCode;   // +0x44  muCurrentCode as emitted by encodeHeader
    SLocalHuffmanEncoderState* mpState;         // +0x48  owning CAltTablesEncoder (via the stand-in above)

private:
    // Frame header written ahead of the first symbol; called only by encodeSymbol. Returns the
    // generateHuffman_balanced() result in r3 (a pointer to the rebuilt table), which the
    // caller discards.
    u32* encodeHeader(void* lpStream);
};
