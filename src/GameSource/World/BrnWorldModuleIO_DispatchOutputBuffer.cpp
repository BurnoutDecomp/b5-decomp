// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.cpp
//
// Out-of-line bodies for the 15 class:BrnWorldIO::DispatchOutputBuffer accessors/mutators the X360
// build emitted for the World module's per-frame graphics-dispatch OUTPUT buffer (the resolved
// lighting/fog aggregates). Each body tests the inherited IOBuffer status flag then acts on the
// named member:
//   - const getters assert the read-lock and return the member BY VALUE (single lvx/stvx per
//     Vector3/Vector4 lane, four lvx/stvx for the Matrix44 sret);
//   - setters assert the write-lock ("Not locked for writing\n") and copy the by-value param in
//     (the incoming vector is spilled to a high lane register across the assert call, then one
//     stvx128 -- or four for a Matrix44 -- stores it store-for-store).
//
// ASSERT-STRING PARITY (verifier rodata notes, matched verbatim): the getter read-lock rodata for
// this buffer carries NO trailing newline ("Not locked for reading") EXCEPT GetWhiteLevel, whose
// rodata carries "Not locked for reading\n"; every setter carries "Not locked for writing\n".
// ============================================================================
#include "GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorldIO
{

// ---- quadric irradiance matrices --------------------------------------------

// X360 0x827BC190 (:462 R) -- const quadric-irradiance-A accessor (Matrix44 @ +16). Four 16-byte
// vector copies (lvx/stvx x4) == the by-value 64-byte Matrix44 return.
Matrix44 DispatchOutputBuffer::GetQuadricIrradianceA() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mmQuadricIrradianceA;
}

// X360 0x827BC268 (:463 W) -- write-lock copy of the 64-byte quadric-irradiance-A Matrix44 into
// mmQuadricIrradianceA (this+0x10). Asm: 4x lvx128/stvx128 pairs copy the by-value source into
// this+0x10; modelled as the Matrix44 struct-assignment (store-for-store).
void DispatchOutputBuffer::SetQuadricIrradianceA(rw::math::vpu::Matrix44 lmQuadricIrradianceA)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mmQuadricIrradianceA = lmQuadricIrradianceA;
}

// X360 0x827BC340 (:464 R) -- const quadric-irradiance-B accessor (Matrix44 @ +80).
Matrix44 DispatchOutputBuffer::GetQuadricIrradianceB() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mmQuadricIrradianceB;
}

// X360 0x827BC418 (:465 W) -- write-lock copy of the 64-byte quadric-irradiance-B Matrix44 into
// mmQuadricIrradianceB (this+0x50). Same 4x lvx128/stvx128 shape; store-for-store.
void DispatchOutputBuffer::SetQuadricIrradianceB(rw::math::vpu::Matrix44 lmQuadricIrradianceB)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mmQuadricIrradianceB = lmQuadricIrradianceB;
}

// ---- fog --------------------------------------------------------------------

// X360 0x827BC5A8 -- write-lock store of the packed fog-colour + white-level Vector4 (this+0x90).
// Incoming Vector4 spilled v1->v127 across the assert, one stvx128 stores it.
void DispatchOutputBuffer::SetFogColourPlusWhiteLevel(rw::math::vpu::Vector4 lvFogColourPlusWhiteLevel)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mvFogColourPlusWhiteLevel = lvFogColourPlusWhiteLevel;
}

// X360 0x827BC668 (:468 R) -- const fog-scattering accessor (member @ +160).
Vector4 DispatchOutputBuffer::GetFogScattering() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mvFogScattering;
}

// X360 0x827BC720 -- write-lock store of the fog-scattering Vector4 (this+0xA0). Incoming Vector4
// spilled v1->v127 across the assert, one stvx128 stores it.
void DispatchOutputBuffer::SetFogScattering(rw::math::vpu::Vector4 lvFogScattering)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mvFogScattering = lvFogScattering;
}

// ---- key light + average irradiance -----------------------------------------

// X360 0x823B54B0 (:470 R) -- const key-light-direction accessor (member @ +176).
Vector3 DispatchOutputBuffer::GetKeyLightDirection() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mvKeyLightDirection;
}

// X360 0x827BC7E0 -- write-lock store of the key-light direction Vector3 (this+0xB0). Incoming
// Vector3 spilled v1->v127 across the assert, one stvx128 stores it.
void DispatchOutputBuffer::SetKeyLightDirection(rw::math::vpu::Vector3 lvKeyLightDirection)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mvKeyLightDirection = lvKeyLightDirection;
}

// X360 0x823B5568 (:472 R) -- const key-light-colour accessor (member @ +192).
Vector3 DispatchOutputBuffer::GetKeyLightColour() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mvKeyLightColour;
}

// X360 0x827BC8A0 -- write-lock store of the key-light colour Vector3 (this+0xC0). Incoming Vector3
// spilled v1->v127 across the assert, one stvx128 stores it.
void DispatchOutputBuffer::SetKeyLightColour(rw::math::vpu::Vector3 lvKeyLightColour)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mvKeyLightColour = lvKeyLightColour;
}

// X360 0x823B5620 (:474 R) -- const average-irradiance-colour accessor (member @ +208). Single
// 16-byte vector copy (lvx/stvx) == the by-value Vector3 return (sret r3).
Vector3 DispatchOutputBuffer::GetAverageIrradianceColour() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
    return mvAverageIrradianceColour;
}

// X360 0x827BC960 -- write-lock store of the average-irradiance colour Vector3 (this+0xD0).
// Incoming Vector3 spilled v1->v127 across the assert, one stvx128 stores it.
void DispatchOutputBuffer::SetAverageIrradianceColour(rw::math::vpu::Vector3 lvAverageIrradianceColour)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mvAverageIrradianceColour = lvAverageIrradianceColour;
}

// ---- white level ------------------------------------------------------------

// X360 0x823B56D8 -- read-lock read of the dispatch white-level scalar (this+0xE0). The asm tail
// `lfs f1,0xE0(r28)` returns the float in f1. (This getter's rodata DOES carry the trailing \n.)
f32 DispatchOutputBuffer::GetWhiteLevel() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return mfWhiteLevel;
}

// X360 0x827BCA20 (:477 W) -- write-lock store of the white-level scalar into mfWhiteLevel
// (this+0xE0). The X360 spills the incoming f1 to f31 across the assert call, then stfs it.
void DispatchOutputBuffer::SetWhiteLevel(f32 lfWhiteLevel)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    mfWhiteLevel = lfWhiteLevel;
}

}   // namespace BrnWorldIO
