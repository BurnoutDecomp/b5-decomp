// ===========================================================================
// XAUDIO::SRC::Native -- explicit instantiations of the "native" SRC pass.
//
// The X360 image emits one hand-unrolled copy of Process<TSample, CHANNELS>
// per attested <TSample, N> pair. The single generic body lives in the header;
// this file forces the 18 instantiations the image contains (project types map
// 1:1 to the ledger's spellings: f32==float, s16==short, u8==unsigned char).
// ===========================================================================

#include "SDKs/XAudio/XAudioSrcNative.h"

namespace XAUDIO
{
namespace SRC
{

// --- SHORTLE (little-endian 16-bit): N = 1, 2, 4, 6 ------------------------
template void Native::Process<SHORTLE, 1>(XAUDIOSRCHDR*); // @ 0x82977CF8
template void Native::Process<SHORTLE, 2>(XAUDIOSRCHDR*); // @ 0x82977E30
template void Native::Process<SHORTLE, 4>(XAUDIOSRCHDR*); // @ 0x82977F90
template void Native::Process<SHORTLE, 6>(XAUDIOSRCHDR*); // @ 0x82978148

// --- float (already normalised): N = 0, 1, 2, 6 ----------------------------
template void Native::Process<f32, 0>(XAUDIOSRCHDR*);     // @ 0x82976740
template void Native::Process<f32, 1>(XAUDIOSRCHDR*);     // @ 0x82976858
template void Native::Process<f32, 2>(XAUDIOSRCHDR*);     // @ 0x82976968
template void Native::Process<f32, 6>(XAUDIOSRCHDR*);     // @ 0x82976BC0

// --- short (native big-endian 16-bit): N = 0, 1, 2, 4, 6 -------------------
template void Native::Process<s16, 0>(XAUDIOSRCHDR*);     // @ 0x82976D10
template void Native::Process<s16, 1>(XAUDIOSRCHDR*);     // @ 0x82976E50
template void Native::Process<s16, 2>(XAUDIOSRCHDR*);     // @ 0x82976F80
template void Native::Process<s16, 4>(XAUDIOSRCHDR*);     // @ 0x829770D8
template void Native::Process<s16, 6>(XAUDIOSRCHDR*);     // @ 0x82977278

// --- unsigned char (biased 8-bit): N = 0, 1, 2, 4, 6 -----------------------
template void Native::Process<u8, 0>(XAUDIOSRCHDR*);      // @ 0x82977468
template void Native::Process<u8, 1>(XAUDIOSRCHDR*);      // @ 0x82977590
template void Native::Process<u8, 2>(XAUDIOSRCHDR*);      // @ 0x829776C0
template void Native::Process<u8, 4>(XAUDIOSRCHDR*);      // @ 0x82977818
template void Native::Process<u8, 6>(XAUDIOSRCHDR*);      // @ 0x829779B8

// The XAUDIOSRCHDR layout self-check (_AssertLayout) now lives in the shared
// home, SDKs/XAudio/XAudioSrcCommon.h, so the two SRC TUs pin one layout.

} // namespace SRC
} // namespace XAUDIO
