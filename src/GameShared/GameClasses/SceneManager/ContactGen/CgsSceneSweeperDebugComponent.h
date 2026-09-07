#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "rw/rwcore_structs.h"

namespace CgsDev { struct Debug3DImmediateRender; }

namespace CgsSceneManager
{
    class IntervalList;
    class SceneSweeper;

    // DecFIGS declaration shape, with all behavior below verified against ARTIST.
    class SceneSweeperDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(SceneSweeper* lpSceneSweeper);
        void Destruct();
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay) override;

    protected:
        void OnActivate() override;
        const char* GetName() const override { return "Scene sweeper"; }
        const char* GetPath() const override { return "World"; }

    private:
        void RenderIntervalList(CgsDev::Debug3DImmediateRender* lpDisplay,
                                const IntervalList* lpIntervalList,
                                rw::RGBA lColour);

        SceneSweeper* mpSceneSweeper;
        f32 mfDrawDistance;
        bool mbRenderDynamicBoxes;
        bool mbRenderInactiveBoxes;
    };
}
