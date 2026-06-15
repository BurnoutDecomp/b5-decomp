#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"

// CgsGraphics::Im2d - the concrete screen-space immediate-mode renderer. Im2dBase<V>
// adds the transform stack on top of ImRenderer<V>; Im2d specialises it for the
// coloured+textured vertex and adds the (out-of-scope) mask state. Hierarchy from the
// DecFIGS DWARF (CgsIm2d.h). The mask/device internals are out of scope; in-scope
// callers only hold an Im2d* and drive it through the inherited render API.
namespace CgsGraphics
{
    template <typename V>
    struct Im2dBase : public ImRenderer<V>
    {
        void SetTransform(const Im2dTransform& lTransform);

        Im2dTransform mCurrentTransform;
    };

    struct Im2d : public Im2dBase<Basic2dColouredTexturedVertex>
    {
    };
}
