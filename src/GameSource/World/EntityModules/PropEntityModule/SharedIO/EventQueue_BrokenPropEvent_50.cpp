#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

// Explicit-instantiation TU for the BrokenPropEvent-element fixed-capacity queue the
// X360 build emitted out-of-line. The generic body is committed inline in
// CgsModule::EventQueue<T,N>; this TU only forces the per-instantiation emission and
// pins the layout the X360 Construct attests.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsModule::EventQueue<BrnWorld::PropEntityIO::BrokenPropEvent, 50>::Construct  @ 0x822E5060
//     points the base queue at its inline maEvents (this + 0xC -- mpEvents@0,
//     miMaxLength@4, miLength@8, maEvents@12, no alignment pad because the element is
//     one byte), sets miMaxLength = 50 (0x32), clears miLength. The "lpEventBuffer !=
//     NULL" assert (CgsBaseEventQueue.h:160) lives in BaseEventQueue::Construct.
//     Called by BrnWorld::PropEntityIO::OutputBuffer_PostPhysics::Construct (which
//     constructs this queue at +0x820 within the post-physics output buffer).
//
// The matching BaseEventQueue<BrokenPropEvent>::AddEvent @ 0x822C9838 (a single
// `lbz/stbx` byte copy at stride 1, its own ledger TU) confirms sizeof == 1.

static_assert(sizeof(BrnWorld::PropEntityIO::BrokenPropEvent) == 1, "BrokenPropEvent stride drift (asm lbz/stbx, stride 1)");

template void
CgsModule::EventQueue<BrnWorld::PropEntityIO::BrokenPropEvent, 50>::Construct();
