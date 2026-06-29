// Per-instantiation .cpp for Array<s32,20>. The generic Array<T,N> body (Append / Erase /
// EraseInstancesOf / GetItem / InsertBefore + siblings) is fully inline in CgsArray.h, so this
// TU is just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,20>::Append           @ 0x821FBCD0  (BrnDirector::DebugLog::ActualAppend)
//   Array<int,20>::Erase            @ 0x821FBDF0  (DebugLog::Update/ActualAppend,
//                                                  Array<int,20>::EraseInstancesOf)
//   Array<int,20>::EraseInstancesOf @ 0x821FBEA0  (ICEMoviePlaylist::DebugMenuRemoveMovie)
//   Array<int,20>::GetItem          @ 0x821FF680  (DebugLog::Update/Print/ActualAppend,
//                                                  ICEMoviePlaylist::Serialise<...>)
//   Array<int,20>::InsertBefore     @ 0x822111B0  (ICEMoviePlaylist::InsertMovieBefore)
// Layout: maElements[20] (80B) + miCount @ +0x50, matching the X360 result[20]/*(a1+80) count word
// and the `4*index + a1` (== &maElements[index]) bounds-checked GetItem return.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,20u>. int == s32 (types.hpp). A primitive
// element needs no element_home include. InsertBefore/EraseInstancesOf were GROWN into the shared
// CgsArray.h generic body (grounded on the X360 0x822111B0 / 0x821FBEA0 pseudocode) so every
// instantiation re-verifies them; they are not specialized here.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 20>;
