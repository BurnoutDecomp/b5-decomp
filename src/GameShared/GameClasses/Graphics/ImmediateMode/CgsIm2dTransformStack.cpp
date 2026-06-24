// Per-instantiation TU for CgsContainers::Stack<CgsGraphics::Im2dTransform,33>.
//
// The generic Stack<Type,N> bodies are fully inline in CgsStack.h, so this TU is just the
// explicit class instantiation that forces the four functions this TU owns to compile:
//   Push @ 0x8246E438, Grow @ 0x8246E520, Pop @ 0x8246E5D8, Peek @ 0x8246E688.
// The X360 emits one out-of-line copy of the stack methods per using-TU; this is the
// <CgsGraphics::Im2dTransform,33> copy (Push called by BrnFlapt::FlaptRenderer::Construct,
// Grow/Pop by BrnFlapt::MovieClipInstance::Render, Peek by the FlaptRenderer Render* paths).
//
// Im2dTransform is 4 x rw::math::vpu::Vector4 == 64 bytes (0x40), so maData[33] occupies
// +0x000..+0x83F (2112 bytes) and miLength lands at +0x840 -- exactly the `*(this + 0x840)`
// / `result[528]` the X360 pseudocode reads. The X360 Push copies the 64-byte element with
// four lvx128/stvx128 quad-word stores; the generic `maData[miLength] = lrEntry` reproduces
// the same whole-element copy (the default Im2dTransform copy = four Vector4 copies). The
// "Pee" spelling in the dossier is the workflow name-parser truncating "Peek" at the
// template '<'; the body at 0x8246E688 reads CgsStack.h:149/150 and returns &maData[len-1].
#include "GameShared/GameClasses/Containers/CgsStack.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"

// Per-method explicit instantiation of exactly the four functions this TU owns. (A whole-
// class `template struct Stack<...>;` also works but warns C4661 for the declaration-only
// methods Contains/operator[] that belong to other TUs.)
template void CgsContainers::Stack<CgsGraphics::Im2dTransform, 33>::Push(const CgsGraphics::Im2dTransform&);
template CgsGraphics::Im2dTransform* CgsContainers::Stack<CgsGraphics::Im2dTransform, 33>::Grow();
template void CgsContainers::Stack<CgsGraphics::Im2dTransform, 33>::Pop();
template const CgsGraphics::Im2dTransform& CgsContainers::Stack<CgsGraphics::Im2dTransform, 33>::Peek() const;
