#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsDataStructure.h"
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"

// The game's legacy DataStructure pair, owned by ModuleSingleBufferedTemplate.
// ARTIST factories @0x827E3090/30C8 and hooks @0x827E3100..322C; DecFIGS BrnGameModuleIO.h.
// These are distinct from the IOBufferStack payloads used by the staged Prepare owner.
namespace BrnGame
{
namespace BrnGameModuleIO
{
    struct InputBuffer : public CgsModule::DataStructure
    {
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
    };

    struct OutputBuffer : public CgsModule::DataStructure
    {
        typedef BrnResource::GameDataIO::RequestInterface<1024> ResourceRequestInterface;

        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();

    private:
        ResourceRequestInterface mResourceRequestInterface;
    };
}
}
