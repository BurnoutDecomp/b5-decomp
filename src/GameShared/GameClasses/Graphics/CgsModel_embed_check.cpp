// Compile-only embedder check for CgsGraphics::Model. Not part of the shipping build:
// it embeds a Model by value and drives the three accessors bodied in CgsModel.cpp
// (GetRenderable / DoesStateExist / GetLodDistance) the way the world/prop/traffic
// render paths do (BrnWorld::PropEntityModule::RenderModel et al.).
#include "GameShared/GameClasses/Graphics/CgsModel.h"

namespace CgsGraphics
{

struct ModelEmbedderCheck
{
    Model mModel;
};

void ModelEmbedderCheck_Exercise(ModelEmbedderCheck& lrEmbedder, Model::State leState, u32 luLod)
{
    const Model& lrModel = lrEmbedder.mModel;

    if (lrModel.DoesStateExist(leState))
    {
        const Renderable* lpRenderable = lrModel.GetRenderable(leState);
        (void)lpRenderable;
    }

    const f32 lfDistance = lrModel.GetLodDistance(luLod);
    (void)lfDistance;
}

}
