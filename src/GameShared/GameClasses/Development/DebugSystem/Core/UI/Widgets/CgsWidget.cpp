#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"

namespace CgsDev
{
    namespace DebugUI
    {
        // ARTIST off_820DC32C uses 0x8284CB38 in the Widget::Update slot; that target is a
        // single blr. Keep the base update as the native no-op.
        void Widget::Update(f32)
        {
        }

        // DecFIGS CgsWidget.cpp identifies the base Render implementation as empty. ARTIST's
        // used WidgetLineGraph vtable replaces this slot with its real Render target, so there
        // is no standalone base Render calltarget in the X360 export.
        void Widget::Render(Debug2DImmediateRender*, f32, f32, f32, f32)
        {
        }

        const Palette& Widget::GetPalette() const
        {
            return DebugManager::GetInstance()->GetUI().GetPalette();
        }

        const Metrics& Widget::GetMetrics() const
        {
            return DebugManager::GetInstance()->GetUI().GetMetrics();
        }

        Debug2DImmediateRender* Widget::Get2DRenderer() const
        {
            return DebugManager::GetInstance()->GetUI().Get2DRenderer();
        }
    }
}
