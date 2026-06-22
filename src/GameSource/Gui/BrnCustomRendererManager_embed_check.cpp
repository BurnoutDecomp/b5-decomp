// Compile-only embedder check for the BrnGui::CustomRendererManager TU.
// Not part of the shipping build: it embeds a CustomRendererManager by value and exercises
// the reconstructed setter by name.
#include "GameSource/Gui/BrnCustomRendererManager.h"

namespace BrnGui
{

struct CustomRendererManagerEmbedderCheck
{
    CustomRendererManager mManager;
};

CustomRendererManager* CustomRendererManagerEmbedderCheck_Exercise(
    CustomRendererManagerEmbedderCheck& lrEmbedder, void* lpSerialiser )
{
    return lrEmbedder.mManager.SetReplaySerialiser( lpSerialiser );
}

} // namespace BrnGui
