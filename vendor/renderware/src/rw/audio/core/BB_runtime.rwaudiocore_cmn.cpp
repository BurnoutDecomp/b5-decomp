// BB_runtime.rwaudiocore_cmn.cpp: the rw::audio::core common bulk-build translation unit.
// Each member also compiles on its own; this file holds only their includes.
// Evidence:
// - The X360 audio reference MAP (another title's link of rwaudiocore) places rw::audio::core's
//   common classes (System, Voice, Mixer, Collection, DelayLine, DelayFilter, Decoder/DecoderRegistry,
//   Encoder, FastFirEngine, Fir64, Iir2, PlugIn/PlugInRegistry, Profiler, StreamPool, TimerHandle/
//   TimerManager, Unpack0, PackedColumnReader/PackedTableReader, SeekTableParser, the reverb filters,
//   Butterworth, CompressorLimiter1, CpuLoadBalancer, the FFT and the mix kernels) in one object,
//   rwaudiocore:BB_runtime.rwaudiocore_cmn.cpp; the plug-ins, decoders and encoders are other bulk TUs.
// - ARTIST holds those members as one contiguous range, 0x82B64560-0x82B6FC80, in several
//   class-ordered emission passes (AllPassFilter .. Voice from 0x82B64560, again from 0x82B6C248, ...).
// Order: each member at its first function in ARTIST's first pass.

#include "ReverbFilters.cpp"
#include "Butterworth.cpp"
#include "Collection.cpp"
#include "CompressorLimiter1.cpp"
#include "MixKernels.cpp"
#include "CpuLoadBalancer.cpp"
#include "Decoder.cpp"
#include "DecoderRegistry.cpp"
#include "DelayFilter.cpp"
#include "DelayLine.cpp"
#include "Encoder.cpp"
#include "FastFirEngine.cpp"
#include "FFT.cpp"
#include "Fir64.cpp"
#include "Iir2.cpp"
#include "Mixer.cpp"
#include "PlugIn.cpp"
#include "PlugInRegistry.cpp"
#include "Profiler.cpp"
#include "Unpack0.cpp"
#include "PackedColumnReader.cpp"
#include "StreamPool.cpp"
#include "System.cpp"
#include "TimerHandle.cpp"
#include "TimerManager.cpp"
#include "Voice.cpp"
#include "PackedTableReader.cpp"
#include "SeekTableParser.cpp"
