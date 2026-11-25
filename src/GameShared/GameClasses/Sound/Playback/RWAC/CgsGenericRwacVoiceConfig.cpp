#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"

namespace CgsSound
{
namespace Playback
{
    GenericRwacVoiceConfig::GenericRwacVoiceConfig(Environment& arEnvironment)
        : mScratchPad(), mu32PluginCount(0), mu32FirstSendPlugin(0),
          miProcessingStage(0), meVoiceType(E_PLAYER_VOICE),
          mEnvironment(arEnvironment)
    {
    }

    void* GenericRwacVoiceConfig::operator new(size_t auSize, Environment& arEnvironment)
    {
        return arEnvironment.Allocate(auSize, 4, "GenericRwacVoiceConfig");
    }

    void GenericRwacVoiceConfig::operator delete(void* /*apMemory*/,
                                                  Environment& /*arEnvironment*/)
    {
    }

    void GenericRwacVoiceConfig::Release()
    {
        rw::IResourceAllocator* lpAllocator = mEnvironment.GetAllocator();
        rw::Resource lResource;
        lResource.m_baseResources[0] = this;
        lResource.m_baseResources[1] = 0;
        lResource.m_baseResources[2] = 0;
        lResource.m_baseResources[3] = 0;
        this->~GenericRwacVoiceConfig();
        lpAllocator->DoFree(lResource);
    }
}
}
