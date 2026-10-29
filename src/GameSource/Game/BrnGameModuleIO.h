#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsDataStructure.h"
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"

namespace BrnGame
{
namespace BrnGameModuleIO
{
    // ARTIST's ModuleSingleBufferedTemplate owns these DataStructures by value.
    // They are not the separately allocated per-frame IOBuffer family.
    struct InputBuffer : public CgsModule::DataStructure
    {
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void Clear();
    };

    struct OutputBuffer : public CgsModule::DataStructure
    {
        typedef BrnResource::GameDataIO::RequestInterface<1024> ResourceRequestInterface;
        OutputBuffer();
        void Construct();
        bool Prepare();
        bool Release();
        void Destruct();
        void Clear();
        const ResourceRequestInterface* GetResourceRequestInterface() const
        {
            return &mResourceRequestInterface;
        }

    private:
        ResourceRequestInterface mResourceRequestInterface;
    };
}
}
