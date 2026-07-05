// Per-instantiation .cpp for Array<u32,3>. The generic Array<T,N> body (Append / GetItem +
// siblings) is fully inline in CgsArray.h, so this TU is just the explicit class instantiation
// (the X360 emits one out-of-line copy per using-TU):
//   Array<u32,3>::Append  @ 0x8268E4F0  (CgsSound::Playback::Module::UpdateStreamBuffers,
//                                         Array<u32,3>::AppendArray<3>)
//   Array<u32,3>::GetItem @ 0x8268E610  (BrnSound::Module::SoundLogicModule::ProcessStreamFreedQueue,
//                                         Array<u32,3>::AppendArray<3>)
// Layout: maElements[3] (12B) + miCount @ +0xC, matching the X360 result[3]/*(a1+12) count word,
// the Append store (stride 4, ++miCount) and the GetItem `4*index + a1` bounds-checked return.
//
// Element type is unsigned: the X360 caller manglings spell it CgsContainers::Array<I,3> --
// e.g. ??$AppendArray@$02@?$Array@I$02@CgsContainers@@ (MSVC 'I' == unsigned int == u32), and
// BrnRootSoundModule's playback dispatch names it Array<u32,3>. Spelled unqualified to match the
// committed Array<T,N> container convention (CgsArray.h).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<u32, 3>;
