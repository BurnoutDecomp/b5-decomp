#pragma once

// ===========================================================================
// EATech Apt -- AptInit.h: the Apt runtime BRING-UP entry points.
//
// The free functions CgsGui::AptAux::InitializeApt @0x82848E50 chains to stand up
// the Apt (ActionScript player) runtime, decompiled faithfully from the X360 ARTIST
// disassembly:
//     AptSetSimulationThreadID  @0x82AD8F90   (record the sim-thread id under a lock)
//     AptSetRenderThreadID      @0x82AD8EF0   (record the render-thread id under a lock)
//     AptCommonInitialize       @0x82AE91F0   (the shared once-only static-data init)
//     AptValueInitialize        @0x82B02800   (build the AS value singletons + natives)
//     AptRenderInitialize       @0x82AEDE90   (the render clip-stack + render-item pool)
//     AptUpdateInitialize       @0x82B02D08   (the sim/update state + the AS interpreter)
//
// (AptAllocatorInitialize @0x82ADD118 -- the first entry -- is already homed inside
// BrnAptRuntimeBringUp.cpp; StringPool::Initialize @0x82AE3630, homed in AptInit.cpp,
// is a leaf AptCommonInitialize calls.)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// ---------------------------------------------------------------------------
// The shared 68-byte Apt configuration block (X360 unk_82F733B8). AptUpdate-
// Initialize memcpy's the AptUpdateParams into it; AptCommonInitialize + the AS
// interpreter read their capacity/thread config out of it. Modelled as a raw
// dword array (the console reads it by byte offset); access is by word index.
// The AptInitParmsT reinterpret (AptActionInterpreter::initialize) aliases the
// SAME storage (iStackSize@word[8], iCallStackDepth@word[9], skipTrace@byte[64]).
// ---------------------------------------------------------------------------
extern unsigned int gAptCommonConfig[17];   // unk_82F733B8 (68 bytes + the trailing byte[64])

// AptSetSimulationThreadID @0x82AD8F90 / AptSetRenderThreadID @0x82AD8EF0 -- record
// the calling thread's id into the sim/render thread-id slot (dword_8324E500 /
// dword_8324E504) under the shared thread-id spin lock. `a1` is the X360 pointer arg
// EA::Thread::GetThreadId(a1) reads (0 == the current thread). Returns the id.
int AptSetSimulationThreadID(int a1);
int AptSetRenderThreadID(int a1);

// AptCommonInitialize @0x82AE91F0 -- the once-only shared static-data init (guarded
// by dword_8324E6E0): set up the animation-director static tables, the string pool,
// and the shared deferred-release AptValueVector. `pConfig` is the config block
// (== &gAptCommonConfig); it reads word[6] (max-new-movie-clips), word[10] (the
// deferred-vector capacity) and word[11] (the string-pool size) out of it.
void* AptCommonInitialize(void* pConfig);

// AptValueInitialize @0x82B02800 -- build the process-wide AS value singletons
// (none/undefined, the boolean/lookup/register tables, the empty CIH, the rendering
// context, the Key/Global/Extension objects, the interned String value) and register
// the AS global native functions (setInterval/clearInterval/isNaN/(un)escape/boolean/
// ASSetPropFlags). Returns the last AptValueVector::ReleaseValues result (X360 r3).
int AptValueInitialize();

// AptRenderInitialize @0x82AEDE90 -- bring up the Apt render side: lazy-run
// AptCommonInitialize, allocate the 128-deep render clip stack (AptMath::ClipStackInit),
// record the render-thread id, then allocate the render-item pointer pool
// (off_8324E2C8 = pool->Allocate(4 * 1024)). `a1` is unused (the X360 signature
// carries the AptUpdateInitialize result in r3 but never reads it). Returns the pool.
void* AptRenderInitialize(int a1);

// AptUpdateInitialize @0x82B02D08 -- bring up the Apt sim/update side: record the
// sim-thread id, copy the AptUpdateParams into the config block, lazy-run
// AptCommonInitialize + AptValueInitialize, allocate the update-side deferred vector,
// initialize the AS interpreter (AptActionInterpreter::initialize), build the single-
// list free node, and (when a2 != 0) create + select the initial AptTarget context.
// `a1` is the AptUpdateParams block (null => the built-in defaults). Returns the
// EA::Thread::Mutex::Unlock result (X360 r3).
int AptUpdateInitialize(unsigned int* a1, char a2);
