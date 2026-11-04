#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/Internal/CgsDebugInternal.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"

// CgsDev::DebugUI::Widget -- the empty debug-UI base. ARTIST installs the WidgetLineGraph
// vtable at 0x820DC32C from MenuItemVariableLineGraph::Render: slot 0 is the one-instruction
// no-op at 0x8284CB38; slot 1 is WidgetLineGraph::Render at 0x82824558. This confirms the
// Update/Render slot order and that Widget has no virtual destructor slot. The base Render
// body itself is not emitted as a separate ARTIST target because the used derived widget
// overrides it; DecFIGS records its default body as empty.
namespace CgsDev
{
    struct Debug2DImmediateRender;

    namespace DebugUI
    {
        struct Widget : public Internal::DebugInternal
        {
            virtual void Update(f32 lfTimeStep);
            virtual void Render(Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY,
                                f32 lfWidth, f32 lfHeight);

        protected:
            const Palette&          GetPalette() const;
            const Metrics&          GetMetrics() const;
            Debug2DImmediateRender* Get2DRenderer() const;
        };
    }
}
