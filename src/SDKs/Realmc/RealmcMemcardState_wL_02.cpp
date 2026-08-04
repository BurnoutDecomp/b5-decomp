#include "SDKs/Realmc/RealmcMemcardState.h"

// ===========================================================================
// SDKs/Realmc/RealmcMemcardState_wL_02.cpp
//
// Wave-L part-file for RealmcCore::MemcardState -- carries the special members
// that RealmcMemcardState.cpp left declared-only. Reconstructed from the raw
// X360 asm (BURNOUT_X360_ARTIST.XEX); no Feb-2007 leak source and no DWARF
// exists for this TU, so every claim below is asm-measured or explicitly
// labelled inference.
//
// SCOPE OF THIS FILE
//   landed here : RealmcCore::MemcardState::~MemcardState  @ 0x82C46470
//   NOT here    : RealmcCore::MemcardState::MemcardState   @ 0x82C47328 --
//                 DEFINED in RealmcMemcardState.cpp (landed wave N, from the
//                 formerly parked body). Do not add a second definition here.
//
// STATE OF THE FORMER BLOCKERS (fix pass): the `Rea` / `Re` truncated symbols
// are RealmcCore::Allocator64's ctor @ 0x82C45B48 and dtor @ 0x82C45A10 (see
// scratchpad/waveL/MemcardState.spec.md §1). Both are now declared in
// RealmcAllocator64.h with bodies in RealmcAllocator64.cpp, and the stale
// blocker text in RealmcMemcardState.cpp has been rewritten. RealmcMemcardState.h
// still carries its original per-member blocker comments -- including a claim
// that ~MemcardState is "declared only", which THIS FILE contradicts; that
// header was out of this pass's edit scope and is reported for the conductor.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// @ 0x82C46470 -- RealmcCore::MemcardState::~MemcardState
// (sole X360 caller: RealmcIface::MemcardInterfaceImpl::~MemcardInterfaceImpl
//  @ 0x82B52CF0, which embeds a MemcardState at its +0x14C.)
//
// MEASURED, store for store, from the raw asm:
//   0x82C46484  lwz  r3, 0x50(r31)          ; mpMessageFilter
//   0x82C46488  cmplwi r3, 0 / beq ...      ; the null guard reproduced below
//   0x82C46490  lwz r11,0(r3); lwz r11,0(r11); li r4,1; bctrl
//                                           ; vtable slot +0 with the delete
//                                           ; flag == the `vector deleting
//                                           ; destructor' == C++ `delete'
//   0x82C464A4..D8                          ; the inlined deque element-destroy
//                                           ; walk over maStartWaitingQueue: it
//                                           ; only LOADS (front cursor +0x08 and
//                                           ; +0x10, back cursor +0x18, slot
//                                           ; array +0x14, page stride 0x100) and
//                                           ; stores nothing -- the trivial
//                                           ; ~int destructor loop the compiler
//                                           ; emitted and then could not elide.
//                                           ; It has no observable effect, so no
//                                           ; C++ source line corresponds to it.
//   0x82C464DC  bl Re                       ; ~Allocator64(maStartWaitingQueue)
//   0x82C464E0..510                         ; the inlined ~IntVector: when
//                                           ; mpBegin != 0, backend vtable +0xC
//                                           ; Free(mpBegin, (mpCapEnd - mpBegin)
//                                           ; * 4) -- CAPACITY bytes, console
//                                           ; element width 4.
//
// Everything after the filter delete is compiler-emitted member teardown, run in
// reverse declaration order (maStartWaitingQueue, then maTaskStack) -- exactly
// the order the X360 runs it in. So the C++ body is just the filter release.
//
// FORMER CAVEAT, NOW CLOSED: when this body was written neither
// RealmcCore::Allocator64 nor RealmcCore::IntVector declared a destructor, so the
// compiler-emitted member teardown was trivial and the two heap blocks the X360
// frees (the deque pages + page-pointer array, and the task-stack buffer) leaked.
// Both destructors now exist -- `~Allocator64` in RealmcAllocator64.h/.cpp (the
// 0x82C45A10 body) and `~IntVector` inline in RealmcContainers.h (the teardown
// inlined at 0x82C464E0..510) -- so the implicit member teardown emitted here
// releases both blocks, matching the X360. No change to the body below was
// required; that is the point of expressing the teardown as member destructors.
//
// SECOND CAVEAT: `delete` on a bare RealmcCore::MessageFilter currently routes to
// the host global operator delete, while its storage came from the Realmc backend
// (the X360 `vector deleting destructor' @ 0x82C46240 frees 0x10 bytes back
// through the backend). Shared-header request R3 -- a class-scope sized
// `operator delete` on MessageFilter -- fixes that with no call-site change. The
// same latency already exists in the committed SetMessageFilter @ 0x82C44FA8.
// ---------------------------------------------------------------------------
MemcardState::~MemcardState()
{
    if (mpMessageFilter)
        delete mpMessageFilter;   // X360: vtbl[+0](filter, 1) -- deleting dtor
}

} // namespace RealmcCore

// FORMERLY PARKED, NOW LANDED -- RealmcCore::MemcardState::MemcardState
// @ 0x82C47328 was moved from scratchpad/waveL/parked/ into
// RealmcMemcardState.cpp in wave N, once wave L's fix pass had landed its
// prerequisites (`explicit Allocator64(unsigned int)` + `~Allocator64` in
// RealmcAllocator64.{h,cpp}, `IntVector()` / `~IntVector()` in
// RealmcContainers.h, and MessageFilter's class-scope sized `operator delete`
// in RealmcCore.h). All 15 ledger functions of this TU now have bodies.
// ===========================================================================
