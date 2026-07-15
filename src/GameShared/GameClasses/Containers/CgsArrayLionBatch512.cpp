// Per-instantiation .cpp for Array<LionBatch, 512>. The generic Array<T,N> body
// (operator[] / GetItem + siblings) is fully inline in CgsArray.h, so this TU is just the
// explicit member instantiation the X360 emitted out-of-line for this instance:
//   Array<LionBatch,512>::GetItem @ 0x8290A250  (cParticleRender::Dispatch)
//
// Layout (X360 ARTIST asm @ 0x8290A250): maElements[512] of the 12-byte LionBatch record
// (startVertex/vertexCount/material -- see LionBatch.h) followed by miCount at guest +0x1800
// (== 512 * 12), matching:
//   - the count word read at *(this + 0x1800) (`lwz r11, 0x1800(r28)`), compared to the -1
//     "Array used before Construct/Clear was called" sentinel (CgsArray.h:538) and then
//     unsigned-compared to the index for the "Array index out of bounds" check
//     (CgsArray.h:539);
//   - the index*12 element addressing that yields the returned element pointer
//     (`slwi r11,r27,1; add r11,r27,r11; slwi r11,r11,2; add r3,r11,r28` == this + 12*index).
// The X360 GetItem streams the dynamic "Array index out of bounds. Index: <i>, length: <n>"
// message (BasePriorityQueue::Clear + StrStream integer append); the committed generic body
// keeps those as static CGS_ASSERT strings (parity note recorded in CgsArrayInt8.cpp).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<LionBatch,512u>.
//
// PER-METHOD (not whole-class) instantiation: the X360 emitted only GetItem for this instance.
// LionBatch is a plain aggregate with no operator== (see LionBatch.h), so the equality-based
// generic members (FindFirstInstanceOf/Contains/CountInstancesOf/EraseInstancesOf) are
// deliberately left un-instantiated -- matching the X360 ledger and avoiding a fabricated
// operator==. This mirrors the per-method instantiation precedent in CgsArrayBankingScore6.cpp.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/LionBatch.h"

template LionBatch& Array<LionBatch, 512>::GetItem(u32);
