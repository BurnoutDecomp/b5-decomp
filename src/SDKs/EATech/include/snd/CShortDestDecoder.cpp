// ============================================================================
// SDKs/EATech/include/snd/CShortDestDecoder.cpp
//
// Out-of-line definition for the shared sample-decoder base Snd::CShortDestDecoder.
//
//   - Snd::CShortDestDecoder::~CShortDestDecoder  (base polymorphic destructor)
//       reconstructed from the X360 vector-deleting destructor @ 0x82B73330
//
// The X360 thunk stores the CShortDestDecoder vtable (off_821563F0) into the
// object then, for the deleting variant, calls operator delete. The base owns no
// resources, so the destructor body is empty: defining it out-of-line emits
// exactly that vtable store + conditional operator-delete tail.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/EATech/include/snd/SampleDecoder.h"

namespace Snd
{
    // 0x82B73330. Base vector-deleting destructor: vtable store + operator delete.
    CShortDestDecoder::~CShortDestDecoder()
    {
    }
} // namespace Snd
