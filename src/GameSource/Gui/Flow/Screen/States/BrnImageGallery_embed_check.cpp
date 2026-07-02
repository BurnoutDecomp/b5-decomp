#include "GameSource/Gui/Flow/Screen/States/BrnImageGallery.h"

// BrnImageGallery.h embed check -- anchors the header-TU (the four inline
// ImageGalleryState bodies) and DEFINES its static tables. MOVE NOTE: when the
// BrnImageGallery.cpp TU lands the state's out-of-line surface, migrate these
// definitions there and retire this anchor.

namespace BrnGui
{

// The per-category invisible frame labels (XEX .data @0x82F25350 -> .rodata
// strings; indexed by GetCurrentCategory()).
const char* const ImageGalleryState::KAPC_CATEGORY_INVISIBLE_FRAMES[4] =
{
    "takedownInvisible", "mugshotInvisible", "rulebreakerInvisible", "finishInvisible",
};

// The gallery's one-APT resource list (XEX .rodata @0x8205E608: {0xA4,
// E_GUI_RESOURCETYPE_APT}).
const CgsGui::sResourceTuple ImageGalleryState::KA_RESOURCES_TO_LOAD[1] =
{
    { 0xA4u, CgsGui::E_GUI_RESOURCETYPE_APT },
};

// Anchor: force the header-inline bodies to be semantically checked by the gate.
void ImageGalleryState_EmbedCheck(ImageGalleryState& lrState,
                                  const CgsGui::sResourceTuple** lppTuples, u32* lpuCount)
{
    lrState.GetResourcesToLoad(lppTuples, lpuCount);
    lrState.SetExpectedAptComponentList();
    if (lrState.UpdateWFInit())
    {
        lrState.HideMiddleImage();
    }
}

}
