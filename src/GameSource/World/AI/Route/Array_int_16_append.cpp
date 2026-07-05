// GameSource/World/AI/Route/Array_int_16_append.cpp
//
// CgsContainers::Array<int, 16>::Append (non-const) @ 0x82769830
//   (caller BrnAI::RouteRequestManager::GenerateStandardRouteRequest -- the block-section-id
//    append loop that reverses to RaceRouteRequest::AddBlockSectionId).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Thin explicit instantiation of the generic
// Array<T,N>::Append committed inline in CgsArray.h (do NOT re-define). The stride-4 store and
// the count word at +0x40 (== 16 * 4) fix sizeof(T)==4 (int) and N==16. Mirrors the committed
// Array_LandmarkIndex_16.cpp explicit-instantiation form; primitive element needs no
// element_home include (CgsArray.h already pulls types.hpp).

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append (inline generic)

template void Array<int, 16>::Append(const int&);
