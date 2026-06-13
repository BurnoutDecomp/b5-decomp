#ifndef CGS_SCENE_MANAGER_DEBUG_COMPONENT_H
#define CGS_SCENE_MANAGER_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"
#include "DebugSystem/Core/UI/CgsTypes.h"
#include "SceneManager/CgsEntityManager.h"
#include "SceneManager/CgsVolumeInstanceId.h"

namespace CgsSceneManager
{
class SceneManagerModule;

static const int32_t KI_MAX_NUM_COLLISION_POLY_CALLBACKS = 4;

typedef void DrawCollisionPolyCallback(
                CgsDev::Debug3DImmediateRender * lpRender,
                const rw::collision::TriangleVolume * lpTriangleVolume,
                const rw::math::Vector3 * lpWorldSpaceTriPoints,
                const VolumeInstanceId lVolumeInstanceId,
                void * lpUserData );

class DrawCollisionPolyCallbackData
{
public:
    inline void Construct( DrawCollisionPolyCallback * lpCallbackFunc, void * lpUserData );
    inline DrawCollisionPolyCallback * GetCallback() ;
    inline void * GetUserData();

private:
    DrawCollisionPolyCallback * mpCallbackFunc;
    void * mpUserData;
};

class SceneManagerDebugComponent : public CgsDev::DebugComponent
{
public:
    void Construct( const SceneManagerModule * lpSceneManagerModule );
    void Destruct();
    virtual void RenderWorld( CgsDev::Debug3DImmediateRender * lpDisplay );
    static bool RegisterDrawCollisionPolyCallback( DrawCollisionPolyCallback * lpHandlerFunc, void * lpUserData );

protected:
    virtual void OnActivate();
    virtual const char* GetName() const { return "Scene manager"; }
    virtual const char* GetPath() const { return "World"; }

private:
    void UpdateViewFrustum( CgsDev::Debug3DImmediateRender * lpDisplay );
    void DebugRenderInstances( CgsDev::Debug3DImmediateRender * lpDisplay );
    void DebugRenderInstance( const VolumeInstance * lpVolumeInstance, rw::math::vpu::Matrix44Affine::InParam lWorldSpaceTransform, CgsDev::Debug3DImmediateRender * lpDisplay );
    void DebugRenderAggregate( const VolumeInstance * lpVolumeInstance, rw::math::vpu::Matrix44Affine::InParam lWorldSpaceTransform, CgsDev::Debug3DImmediateRender * lpDisplay );
    bool IsTriVisible( rw::math::Vector3 * laTriPoints, CgsDev::Debug3DImmediateRender * lpDisplay );
    void DrawCollisionPoly( CgsDev::Debug3DImmediateRender * lpRender, const rw::collision::TriangleVolume * lpTriangleVolume, const rw::math::Vector3 * lpWorldSpaceTriPoints, const VolumeInstanceId lVolumeInstanceId );
    const VolumeManager * GetVolumeManager();
    const EntityManager * GetEntityManager();
    static inline DrawCollisionPolyCallbackData * GetDrawPolyCallbackData( int32_t liIndex );

    static DrawCollisionPolyCallbackData saDrawPolyCallbacks[KI_MAX_NUM_COLLISION_POLY_CALLBACKS];
    static int32_t siNumDrawPolyCallbacks;

    const SceneManagerModule * mpSceneManagerModule;

    uint8_t mau8VolumeBBoxQueryBuffer[ 128 * 1024 ] __attribute__((aligned(16)));

    rw::collision::Frustum mViewFrustum;
    rw::collision::VolumeBBoxQuery * mpVolumeBBoxQuery;
    float32_t mrColDrawDistance;

    bool mbDrawCollision;
    bool mbUpdateViewFrustum;
    bool mbPrintCollisionMetrics;
};

inline void DrawCollisionPolyCallbackData::Construct( DrawCollisionPolyCallback * lpCallbackFunc, void * lpUserData )
{
    mpCallbackFunc = lpCallbackFunc;
    mpUserData = lpUserData;
}

inline DrawCollisionPolyCallback * DrawCollisionPolyCallbackData::GetCallback()
{
    return mpCallbackFunc;
}

inline void * DrawCollisionPolyCallbackData::GetUserData()
{
    return mpUserData;
}

inline DrawCollisionPolyCallbackData * SceneManagerDebugComponent::GetDrawPolyCallbackData( int32_t liIndex )
{
    return &saDrawPolyCallbacks[liIndex];
}

}

#endif
