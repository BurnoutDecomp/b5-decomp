// Per-instantiation .cpp for CgsContainers::Array<int,20>. Every member is the generic inline
// body of CgsArray.h; this TU only emits the instantiation, whose out-of-line ARTIST copies are
//   Array<int,20>::Append           @ 0x821FBCD0  (BrnDirector::DebugLog::ActualAppend)
//   Array<int,20>::Erase            @ 0x821FBDF0  (DebugLog::Update/ActualAppend,
//                                                  Array<int,20>::EraseInstancesOf)
//   Array<int,20>::EraseInstancesOf @ 0x821FBEA0  (ICEMoviePlaylist::DebugMenuRemoveMovie)
//   Array<int,20>::GetItem          @ 0x821FF680  (CgsArray.h:556/557)
//   Array<int,20>::GetItem const    @ 0x821FF788  (CgsArray.h:538/539)
//   Array<int,20>::InsertBefore     @ 0x822111B0  (ICEMoviePlaylist::InsertMovieBefore)
// Layout: maElements[20] (80B) + the count @ +0x50. int == s32 (types.hpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class CgsContainers::Array<s32, 20>;
