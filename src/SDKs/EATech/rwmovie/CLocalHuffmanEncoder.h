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

// Small object referenced through CLocalHuffmanEncoder::mpState (+0x48). Only its +0x08 word
// is observed -- CAltTablesEncoder::updateHistory tests it -- so the remainder is opaque and
// recovered here as leading padding.
struct SLocalHuffmanEncoderState
{
    u8  mPad00[8];   // +0x00  (unobserved)
    u32 muDirty;     // +0x08  tested by CAltTablesEncoder::updateHistory
};

class CLocalHuffmanEncoder : public CLocalHuffman
{
public:
    // (table index, owning CAltTablesEncoder, error out). Body is a separate X360 TU.
    CLocalHuffmanEncoder(int liTableIndex, CAltTablesEncoder* lpParent, u32* lpuError);

    // --- methods invoked by CAltTablesEncoder; bodies are separate, not-yet-done TUs -----
    int  checkFrame();                     // symbols this table contributed this frame
    int  checkInRTCMV(void* lpContext);    // emit one RTCMV symbol record
    int  encodeSymbol(void* lpStream);     // emit the current symbol via the VLC table
    u32* WriteSymbol(void* lpStream);      // emit a literal/escape symbol record

    // --- data the X360 reaches into directly from CAltTablesEncoder (hence public) --------
    u32                        muPad24;         // +0x24  (unobserved)
    u32                        muCurrentCode;   // +0x28  snapshotted into muHistoryCode
    u32                        muPad2C;         // +0x2C  (unobserved)
    u32*                       mpauCodeBuffer;  // +0x30  4*numSymbols bytes; cleared in clear, freed in dtor
    u32                        muDirtyFlag;     // +0x34  zeroed in clear; tested in updateHistory
    u32*                       mpauCodeData;    // +0x38  freed in dtor
    u32                        muPad3C;         // +0x3C  (unobserved)
    u32                        muHistoryCode;   // +0x40  = muCurrentCode when the table is dirty
    u32                        muPad44;         // +0x44  (unobserved)
    SLocalHuffmanEncoderState* mpState;         // +0x48  (only its +0x08 word is observed)
};
