#pragma once

// ARTIST AptPseudoData_t constructor @0x82AD9910. The source is an in-place
// 32-bit place command; the snapshot borrows its matrix, colour and handler data.
#include <cstddef>
#include "types.hpp"

struct AptPlaceObjectBody_t
{
    u32 muxFlags;             // +0x00
    s32 mi32Depth;            // +0x04 display depth
    s32 mi32CharacterId;      // +0x08
    u8 maMatrix[24];          // +0x0C six floats
    u8 maColorTransform[8];   // +0x24
    f32 mfRatio;              // +0x2C
    const char* mpName;       // +0x30
    s32 miClipDepth;          // +0x34: LWZ, then truncated to the snapshot halfword
    void* mpClipActions;      // +0x38
};
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, muxFlags) == 0x00), "ARTIST place flags");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, maMatrix) == 0x0C), "ARTIST place matrix");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, maColorTransform) == 0x24), "ARTIST place colour");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mfRatio) == 0x2C), "ARTIST place ratio");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mpName) == 0x30), "ARTIST place name");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, miClipDepth) == 0x34), "ARTIST place clip depth");
CGS_STATIC_ASSERT((offsetof(AptPlaceObjectBody_t, mpClipActions) == 0x38), "ARTIST place handlers");
CGS_STATIC_ASSERT((sizeof(AptPlaceObjectBody_t) == 0x3C), "ARTIST place body");

struct AptPlaceObjectInfo_t
{
    u32 muTag;                // tag 3; body follows at command+4

    const AptPlaceObjectBody_t* GetBody() const
    {
        return reinterpret_cast<const AptPlaceObjectBody_t*>(this + 1);
    }
    AptPlaceObjectBody_t* GetBody()
    {
        return const_cast<AptPlaceObjectBody_t*>(
            static_cast<const AptPlaceObjectInfo_t*>(this)->GetBody());
    }
};
CGS_STATIC_ASSERT((sizeof(AptPlaceObjectInfo_t) == 4), "ARTIST command tag");

struct AptPseudoData_t
{
    void* mpData;             // +0x00
    void* mpMatrix;           // +0x04 borrowed under flag 4
    void* mpColorTransform;   // +0x08 borrowed under flag 8
    void* mpClipActions;      // +0x0C borrowed under flag 0x80
    f32 mfRatio;              // +0x10 captured under flag 0x10
    u32 muxFlags;             // +0x14
    s16 mi16CharacterId;      // +0x18
    s16 mi16Depth;            // +0x1A captured clip depth

    AptPseudoData_t(const AptPlaceObjectInfo_t* lpSource,
                    s16 li16CharacterId, void* lpData);
};
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpData) == 0x00), "ARTIST snapshot data");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpMatrix) == 0x04), "ARTIST snapshot matrix");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpColorTransform) == 0x08), "ARTIST snapshot colour");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mpClipActions) == 0x0C), "ARTIST snapshot handlers");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mfRatio) == 0x10), "ARTIST snapshot ratio");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, muxFlags) == 0x14), "ARTIST snapshot flags");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mi16CharacterId) == 0x18), "ARTIST snapshot character");
CGS_STATIC_ASSERT((offsetof(AptPseudoData_t, mi16Depth) == 0x1A), "ARTIST snapshot clip depth");
CGS_STATIC_ASSERT((sizeof(AptPseudoData_t) == 0x1C), "ARTIST snapshot size");
