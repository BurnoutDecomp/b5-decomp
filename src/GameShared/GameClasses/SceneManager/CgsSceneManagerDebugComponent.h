#ifndef CGS_SCENE_MANAGER_DEBUG_COMPONENT_H
#define CGS_SCENE_MANAGER_DEBUG_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                          // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

// CgsSceneManager::SceneManagerDebugComponent - the scene manager's debug component (path "World",
// name "Scene manager"): draws every live collision volume instance, the triangle-test harness and
// the triangle-cache contents. Members in declaration order (the console puts the first one right
// after the base, at +0x0C). Only the methods bodied in CgsSceneManagerDebugComponent.cpp are
// declared.
namespace CgsDev { struct Debug3DImmediateRender; }
namespace rw { namespace collision { class Volume; } }

namespace CgsSceneManager
{
    class SceneManagerModule;

    class SceneManagerDebugComponent : public CgsDev::DebugComponent
    {
    protected:
        const char* GetName() const override;

    private:
        // Draw every allocated volume instance's collision volume under its world transform.
        void DrawPrimitives(CgsDev::Debug3DImmediateRender* lpRender);

        // Draw one collision volume (sphere, capsule, box or cylinder) in black, its own relative
        // transform composed with lTransform.
        void DebugRenderCollisionVolume(const rw::collision::Volume* lpVolume,
                                        CgsDev::Debug3DImmediateRender* lpRender,
                                        const Matrix44Affine& lTransform) const;

        const SceneManagerModule* mpSceneManagerModule;
        bool    mbDrawCollisionVolumes;
        bool    mbDrawTriangleTest;
        bool    mbDrawPadding;
        Vector3 mInitialCameraPosition;
        Vector3 mTriangleOffset;
        f32     mfPadding;
        f32     mfTriangleScale;
        s32     miTestsToExecute;
        s32     miTotalCycles;
        s32     miCyclesPerCall;
        s32     miCyclesPerTriangle;
        bool    mbDrawCachedTriangles;
        bool    mbDrawCacheSpheres;
        bool    mbDrawNonCachedTriangles;
        bool    mbDrawNonCachedSpheres;
    };
}

#endif
