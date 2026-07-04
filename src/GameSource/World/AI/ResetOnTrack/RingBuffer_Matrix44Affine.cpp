#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"   // CgsContainers::RingBuffer<T>::Push (inline generic)
#include "rw/math/vpu/types.h"                                    // rw::math::vpu::Matrix44Affine (64-byte element)

// Explicit instantiation of
//   CgsContainers::RingBuffer<rw::math::vpu::Matrix44Affine>::Push.
// X360 @0x822AD1A0 (the only RingBuffer member this instantiation emits out-of-line for
// Matrix44Affine; called by BrnWorld::ActiveRaceCar::UpdateResetTransform -- a recent
// car-transform history ring used for on-track reset).
//
// The shared generic body lives in CgsRingBuffer.h: it copies the 64-byte Matrix44Affine
// into mpData[miWritePos] (the asm's `slwi r11,r11,6` == * sizeof 64, `lwz r10,0(r3)` mpData,
// `add r11,r11,r10`, then four 16-byte lvx128/stvx128 of v0 at +0/+0x10/+0x20/+0x30 ==
// the four 16-aligned Vector3 axes xAxis/yAxis/zAxis/wAxis). It then advances and wraps
// miWritePos, grows miLength, and -- when the buffer is already full -- clamps miLength to
// miMaxLength, advances miReadPos modulo miMaxLength (`divw`/`mullw`/`subf`), and asserts
// read==write ("Read pos should equal write pos if buffer is full"; the X360 folds in the
// StrStream message + `tw`-trap divide guards which CGS_ASSERT / the language operators
// subsume). Member offsets are exactly RingBuffer<Type>: mpData@+0, miMaxLength@+4,
// miReadPos@+8, miWritePos@+0xC, miLength@+0x10. Mirrors RingBuffer_Vector3.cpp /
// RingBuffer_TrafficHashEntry_Push.cpp / the ResetOnTrack RingBuffer_*.cpp siblings; only
// the element type/stride (64) differ.

template void
CgsContainers::RingBuffer<rw::math::vpu::Matrix44Affine>::Push(
    const rw::math::vpu::Matrix44Affine* lpEntry);
