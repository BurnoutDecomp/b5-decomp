#ifndef CGS_SCENE_SWEEPER_DEBUG_COMPONENT_H
#define CGS_SCENE_SWEEPER_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"

namespace CgsSceneManager
{
    class SceneSweeper;
}

namespace CgsSceneManager
{

class SceneSweeperDebugComponent : public CgsDev::DebugComponent
{
public:
    void Construct( SceneSweeper* lpSceneSweeper );
    void Destruct();
    virtual void RenderWorld( CgsDev::Debug3DImmediateRender * lpDisplay );

protected:
    virtual void OnActivate();
    inline virtual const char* GetName() const;
    inline virtual const char* GetPath() const;

private:
    void RenderIntervalList( CgsDev::Debug3DImmediateRender * lpDisplay, void* lpIntervalList, rw::RGBA lColour );

    SceneSweeper* mpSceneSweeper;
    float32_t mfDrawDistance;
    bool mbRenderDynamicBoxes;
    bool mbRenderInactiveBoxes;
};

inline const char* SceneSweeperDebugComponent::GetName() const
{
    return "Scene sweeper";
}

inline const char* SceneSweeperDebugComponent::GetPath() const
{
    return "World";
}

}

#endif
