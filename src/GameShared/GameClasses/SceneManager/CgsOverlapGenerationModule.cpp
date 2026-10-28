#include "CgsOverlapGenerationModule.h"

namespace CgsSceneManager
{
OverlapGenerationModule::OverlapGenerationModule()
    : mInputMutex(0, true),
      mOutputMutex(0, true),
      muModuleVTable(0x820CF820),
      muOverlapListVTable(0x820CE200)
{
}
}
