// =====================================================================================
// rw::audio::core::XasDec bodies.
//
// EARenderWare "rwaudio" XAS0 (block-ADPCM) stream decoder plug-in, "version 0" sibling
// of Xas1Dec. Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative
// for every offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist
// for this TU. See XasDec.h for the appended layout and Decoder.h for the shared base.
//   CreateInstanceEvent @0x82B91EA0
//   GetSize             @0x82B91E70
//   GetDecoderDesc      @0x82B91E80
//   DecodeEvent         @0x82B94118  -- BLOCKED (indexes un-recovered XAS coefficient
//                                       rodata flt_8215A7F0/flt_8215A7F4/flt_8215A810),
//                                       defined in its own TU; only declared in XasDec.h.
// =====================================================================================

#include "rw/audio/core/XasDec.h"

namespace rw
{
namespace audio
{
namespace core
{

// This codec's static registration descriptor (off_82F8A528 in the X360 rodata). Its
// bytes (name / GUID / factory callbacks) are not recovered here; GetDecoderDesc only
// returns its address, and DecoderDesc is an incomplete type, so no contents are needed
// (nor fabricated). Same pattern as the foreign statics reached from Xas1Dec.cpp.
extern "C" DecoderDesc off_82F8A528;

// -------------------------------------------------------------------------------------
// CreateInstanceEvent @0x82B91EA0
// Plug-in "create instance" event: reset the streaming state for a fresh XAS stream by
// clearing the encoded cursor and the remaining-sample counter, then report success.
// The asm operates on the instance passed in r3 and always returns 1.
// -------------------------------------------------------------------------------------
s32 XasDec::CreateInstanceEvent(XasDec *pDecoder)
{
    pDecoder->miRemainingSamples = 0;   // +0x38
    pDecoder->mpEncodedCursor = 0;      // +0x34
    return 1;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B91E70
// Report the codec's per-instance allocation footprint: the required alignment (4) is
// written through the out-param and the fixed X360 instance size (60 bytes / 0x3C) is
// returned. The descriptor framework passes the instance pointer in r3, but the asm
// overwrites r3 with the return value without reading it, so it is unused here.
// -------------------------------------------------------------------------------------
s32 XasDec::GetSize(XasDec *pDecoder, u32 *puAlignment)
{
    (void)pDecoder;
    *puAlignment = 4;
    return 60;
}

// -------------------------------------------------------------------------------------
// GetDecoderDesc @0x82B91E80
// Hand back the address of this codec's static registration descriptor. Callers
// (DecoderRegistry::RegisterStandardRunTimeDecoders, the AEMS sample factories) use it to
// register / instantiate the XAS0 decoder.
// -------------------------------------------------------------------------------------
DecoderDesc *XasDec::GetDecoderDesc()
{
    return &off_82F8A528;
}

// DecodeEvent @0x82B94118 -- BLOCKED. Left to its own TU: its inlined per-channel decode
// indexes the un-recovered XAS ADPCM coefficient rodata (flt_8215A7F0 / flt_8215A7F4 /
// flt_8215A810), whose bytes are not in the dossier, so a faithful body cannot be written
// without fabricating those tables. Declared in XasDec.h; not defined here.

} // namespace core
} // namespace audio
} // namespace rw
