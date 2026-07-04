#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// CgsContainers::RingBuffer<float> -- explicit instantiation TU.
//
// The generic RingBuffer<Type> bodies (Push/Pop/operator[]) are committed inline in
// CgsRingBuffer.h; this TU only forces the <float> specialisation the X360 build emits
// out-of-line:
//   RingBuffer<float>::Push(const float*)  @ 0x8264DFA0
//   RingBuffer<float>::operator[](u32)     @ 0x8264E0A0
// No generic bodies are re-emitted here -- only the explicit member instantiations
// (mirrors the committed RingBuffer_int.cpp / RingBuffer_Vector3.cpp).

template void  CgsContainers::RingBuffer<float>::Push(const float* lpEntry);
template float& CgsContainers::RingBuffer<float>::operator[](u32 luIndex);
