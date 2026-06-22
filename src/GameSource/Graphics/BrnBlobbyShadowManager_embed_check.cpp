// Embed-check / compile tripwire for BrnBlobbyShadowManager (1 ledger func: AddShadow).
// Pins the X360 record layout (64-byte ShadowStruct slot stride, count @ buffer offset 0,
// bound 0x40) and forces the AddShadow body to instantiate. Not a unit test -- a compile/link
// guard for this TU.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"
#include <cstddef>

namespace
{
    typedef BrnBlobbyShadowManager::BrnBlobbyShadowBuffer Buffer;
    typedef Buffer::ShadowStruct                          ShadowStruct;

    // Layout pins (AddShadow asm: slwi count,6 => 0x40 slot stride; lwz/stw 0(this) count; cmpwi 0x40).
    static_assert(sizeof(ShadowStruct) == 0x40, "ShadowStruct == 64 bytes (slwi count, 6)");
    static_assert(offsetof(ShadowStruct, mvPos) == 0x00, "mvPos @0x00");
    static_assert(offsetof(ShadowStruct, mvAt) == 0x10, "mvAt @0x10");
    static_assert(offsetof(ShadowStruct, mvScaledRight_HeightOffGround) == 0x20, "scaledRight @0x20");
    static_assert(offsetof(ShadowStruct, mvFront_Rear_BackAxle_FrontAxle) == 0x30, "front/rear @0x30");
    static_assert(offsetof(Buffer, miNumShadows) == 0x00, "count @ buffer offset 0 (lwz/stw 0(this))");
    static_assert(KI_MAX_SHADOWS == 64, "KI_MAX_SHADOWS == 0x40 (cmpwi bound)");

    bool ExerciseAddShadow(Buffer* lpBuffer,
                           const Matrix44Affine& lTransform,
                           const Vector4& lExtents,
                           VecFloat lfWidth,
                           VecFloat lfHeight)
    {
        return lpBuffer->AddShadow(lTransform, lExtents, lfWidth, lfHeight);
    }
}

bool (*g_BrnBlobbyShadowManagerEmbedCheck)(
    BrnBlobbyShadowManager::BrnBlobbyShadowBuffer*,
    const Matrix44Affine&, const Vector4&, VecFloat, VecFloat) = &ExerciseAddShadow;
