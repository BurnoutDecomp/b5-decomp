#pragma once

#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsWindow.h"

namespace CgsResource
{
    // ARTIST component+0x4DE8..0x4E0B. This window adds no data or
    // Render/Update override; its original vtable retains the base protocol.
    class DebugBundleFilesWindow : public CgsDev::DebugUI::Window
    {
    public:
        void Reset();
    };
}
