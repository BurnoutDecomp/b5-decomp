// Per-instantiation .cpp for Array<u32,8>. The generic Array<T,N> body (Append / operator[] /
// AppendArray + siblings) is fully inline in CgsArray.h, so this TU is just the explicit class
// instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<u32,8>::AppendArray<8> @ 0x8278A108  (BrnAI::AIModule::OnModeStart)
//   Array<u32,8>::Append         @ (int_8___Append, emitted out-of-line for this instantiation)
//   Array<u32,8>::operator[]     @ 0x8276AB10  (source GetItem inside AppendArray<8>)
// Layout: maElements[8] (32B) + miCount @ +0x20, matching the X360 result[8]/*(a2+0x20) count
// word (lwz r11,0x20(r29); -1 sentinel), the Append store (stride 4, ++miCount), and the
// operator[] `4*index + base` bounds-checked return.
//
// AppendArray<8> asserts both arrays were Construct/Clear'd (this @ CgsArray.h:245, source
// GetLength @ :336) and that the combined live count fits N=8 ("Array container out of space
// appending an array" @ :246), then Appends source[0..GetLength()-1] in order -- exactly the
// generic AppendArray body in CgsArray.h.
//
// Element type is unsigned: the X360 caller mangling spells it CgsContainers::Array<I,8>
// (MSVC 'I' == unsigned int == u32); the Hex-Rays `int_8_::Append` label is just the
// disassembler collapsing unsigned int to int -- same instantiation/layout. Spelled unqualified
// to match the committed Array<T,N> container convention (CgsArray.h). A primitive element needs
// no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<u32, 8>;
