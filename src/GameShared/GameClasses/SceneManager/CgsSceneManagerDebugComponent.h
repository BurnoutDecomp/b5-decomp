#ifndef CGS_SCENE_MANAGER_DEBUG_COMPONENT_H
#define CGS_SCENE_MANAGER_DEBUG_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                          // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // CgsDev::DebugComponent

// CgsSceneManager::SceneManagerDebugComponent - the scene manager's debug component. Its member
// names/types follow the X360-attested DecFIGS declaration; behavior is recovered from ARTIST.
namespace CgsDev { struct Debug3DImmediateRender; }
namespace rw { namespace collision { class Volume; } }
namespace rw { struct RGBA; }
namespace rw { namespace math { namespace vpu { struct MaskScalar; } } }

namespace CgsSceneManager
{
    class SceneManagerModule;

    class SceneManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // @ X360 0x828B6658. Called by SceneManagerModule::Construct before Register.
        void Construct(const SceneManagerModule* lpSceneManagerModule);

        // @ X360 0x828AB500. Clears the query counters exposed by this page each frame.
        void Update() ;

        // @ X360 0x828D04E8. Dispatches the enabled world-space debug draws.
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) ;

    protected:
        const char* GetName() const ;
        void OnActivate() ;

    private:
        // Draw every allocated volume instance's collision volume under its world transform.
        void DrawPrimitives(CgsDev::Debug3DImmediateRender* lpRender);

        // Draw one collision volume (sphere, capsule, box or cylinder) in black, its own relative
        // transform composed with lTransform.
        void DebugRenderCollisionVolume(const rw::collision::Volume* lpVolume,
                                        CgsDev::Debug3DImmediateRender* lpRender,
                                        const Matrix44Affine& lTransform) const;

        // ARTIST callees of RenderWorld. DrawTriangleTest is decoded directly from its raw
        // instruction packet; the other named consumers are reconstructed in this component.
        void DrawCachedObjectTriangles(CgsDev::Debug3DImmediateRender* lpRender);
        void DrawCachedObjectSpheres(CgsDev::Debug3DImmediateRender* lpRender);
        void DrawCachedTriangle(CgsDev::Debug3DImmediateRender* lpRender,
                                Vector3 lVertex0, Vector3 lVertex1, Vector3 lVertex2,
                                rw::RGBA lColour);
        void DrawTriangle(CgsDev::Debug3DImmediateRender* lpRender,
                          Vector3 lTranslation, Vector3 lVertex0, Vector3 lVertex1, Vector3 lVertex2,
                          rw::math::vpu::MaskScalar lHitMask,
                          Vector3 lSphereContactPoint, Vector3 lTriangleContactPoint);
        void DrawTriangleTest(CgsDev::Debug3DImmediateRender* lpRender);

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
