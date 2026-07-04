#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// CgsContainers::RingBuffer<int> -- explicit instantiation TU.
//
// The generic RingBuffer<Type> bodies (Push/Pop/operator[]) are committed inline in
// CgsRingBuffer.h; this TU only forces the <int> specialisation the X360 build emits
// out-of-line:
//   RingBuffer<int>::Push(const int*)  @ 0x827B4DF0
//   RingBuffer<int>::Pop(int*)         @ 0x827B4EF0
//   RingBuffer<int>::operator[](u32)   @ 0x827B4F58
// No generic bodies are re-emitted here -- only the explicit member instantiations.

template void CgsContainers::RingBuffer<int>::Push(const int* lpEntry);
template bool CgsContainers::RingBuffer<int>::Pop(int* lpEntry);
template int& CgsContainers::RingBuffer<int>::operator[](u32 luIndex);
