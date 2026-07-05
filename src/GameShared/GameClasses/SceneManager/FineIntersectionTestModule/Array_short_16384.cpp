#include "GameShared/GameClasses/Containers/CgsArray.h"

// Explicit instantiation of the generic Array<T,N>::GetItem (inline in CgsArray.h) for the
// Array<u16, 16384> leaf instantiation -- the committed Array_/EventQueue_ explicit-
// instantiation pattern (mirrors the sibling Array_short_256.cpp:
// `template u16& Array<u16, 256>::GetItem(u32);`). Driven by
// CgsSceneManager::FineIntersectionTestModule::ComputeVolumeTestFine.
//
//   CgsContainers::Array<unsigned short, 16384u>::GetItem(u32)   @ 0x828AE6E0
//
// Element type unsigned short: 2-byte slot stride and the count word at this+0x8000
// (== 16384 * sizeof(u16)) together fix element = u16 and extent N = 16384. Non-const GetItem
// overload returning T& = u16&. A primitive element needs no element_home include.
template u16& Array<u16, 16384>::GetItem(u32);
