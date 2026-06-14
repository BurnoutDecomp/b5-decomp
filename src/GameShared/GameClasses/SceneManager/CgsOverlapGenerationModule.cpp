#include "CgsOverlapGenerationModule.h"

namespace CgsSceneManager
{
OverlapGenerationModule::OverlapGenerationModule()
    : mInputMutex(nullptr, true),
      mOutputMutex(nullptr, true),
      muModuleVTable(0x820CF820),
      muOverlapListVTable(0x820CE200)
{
}
}
