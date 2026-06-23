#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"   // CgsInput::InputIO::PadMapping

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// PadMapping event-queue instantiations. The input module enqueues 116-byte
// PadMapping records (the X360 element stride; see CgsInputTypes.h) into two
// fixed-capacity queues: a 4-slot queue (PostWorldInputBuffer's pad-mapping queue
// is the 7-slot variant; the module also carries this 4-slot instance) constructed
// by EventQueue<PadMapping,4>::Construct, and the appends go through the shared
// BaseEventQueue<PadMapping>::AddEvent body.

// CgsInput::InputIO::PadMapping,4>::Construct @ 0x828F2D00
//   Fixed-capacity (4) event-queue instantiation: points the base queue at its inline
//   maEvents buffer (this+0xC), sets miMaxLength = 4, clears miLength.
//   Body inline in CgsEventQueue.h (store-for-store identical to the X360 spine:
//   stw this+0xC@0, li 4@4, li 0@8).
template void CgsModule::EventQueue<CgsInput::InputIO::PadMapping, 4>::Construct();

// CgsInput::InputIO::PadMapping>::AddEvent @ 0x828EA820
//   BaseEventQueue<PadMapping>::AddEvent -- appends UNCONDITIONALLY (the two asserts,
//   "mpEvents != NULL" line 312 and the "Reached Max length" tripwire line 313, are
//   non-gating), block-copies the 116-byte record to mpEvents[miLength] and bumps
//   miLength. Called by PostWorldInputBuffer::PostMappingRequest (0x828EFA10).
//   Body inline in CgsBaseEventQueue.h.
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::PadMapping>::AddEvent(const CgsInput::InputIO::PadMapping&);
