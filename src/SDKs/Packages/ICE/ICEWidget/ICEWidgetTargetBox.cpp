// ============================================================================
// SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.cpp
//
// ICE::ICEWidgetTargetBox::Render @0x8252D2B8 -- draw the camera-take editor's 3D
// "target box": a small fixed-size cube (a reticle / bounding box) drawn in the
// widget's world transform (i.e. at the take's look-at point), through the CgsDev
// debug system's 3D renderer.
//
// X360 shape (@0x8252D2B8): two fixed corner vectors are built on the stack
// (min = {-0.05,-0.05,-0.05,0}, max = {+0.05,+0.05,+0.05,0} -- a +/-0.05 cube), a
// stack DebugInterface is constructed, its 3D DebugRender is acquired (GetRender,
// NOT Get2dRender), DrawBox draws the cube in the `this+0x10` world transform (the
// caller's 4-row matrix; look-at translation at +0x40) in white (colour -1 ==
// 0xFFFFFFFF), then the manager is thread-safe-released. The
// release mirrors ICERender's idiom (the DebugInterface holds the acquired manager;
// the X360 release is gated on the interface's automatic-class flag, which a
// default-constructed acquiring interface sets).
// ============================================================================

#include "SDKs/Packages/ICE/ICEWidget/ICEWidgetTargetBox.hpp"

#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"         // CgsDev::DebugRender::DrawBox (3D) / RGBA
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"          // CgsDev::DebugManager::ThreadSafeRelease

namespace ICE
{
    // The fixed half-extent of the target cube (the X360 inline literal
    // flt_82004E3C = -0.05 / flt_820047C8 = +0.05, splatted across x/y/z with a
    // zero w lane).
    static const f32 KF_TARGET_BOX_HALF_EXTENT = 0.05f;

    void ICEWidgetTargetBox::Render()
    {
        // The two world-space corner offsets relative to the box centre: a +/-0.05
        // cube (w lane zero). The X360 builds these as two stack vectors and passes
        // them in the SIMD arg registers (v1 = min, v2 = max).
        Vector4 lv4MinCorner;
        lv4MinCorner.x = -KF_TARGET_BOX_HALF_EXTENT;
        lv4MinCorner.y = -KF_TARGET_BOX_HALF_EXTENT;
        lv4MinCorner.z = -KF_TARGET_BOX_HALF_EXTENT;
        lv4MinCorner.w = 0.0f;

        Vector4 lv4MaxCorner;
        lv4MaxCorner.x = KF_TARGET_BOX_HALF_EXTENT;
        lv4MaxCorner.y = KF_TARGET_BOX_HALF_EXTENT;
        lv4MaxCorner.z = KF_TARGET_BOX_HALF_EXTENT;
        lv4MaxCorner.w = 0.0f;

        // Acquire the debug system through a stack interface and draw the box in the
        // widget's world transform through the 3D renderer in white. The X360 passes
        // `this+0x10` (the transform matrix base) directly as the DrawBox transform.
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.GetRender().DrawBox(reinterpret_cast<const f32*>(&mTransform),
                                            (CgsDev::RGBA)0xFFFFFFFFu,
                                            lv4MinCorner, lv4MaxCorner);
        CgsDev::DebugManager::ThreadSafeRelease(&lDebugInterface.GetDebugManager());
    }
}
