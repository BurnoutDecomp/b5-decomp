#include "GameShared/GameClasses/System/Resource/CgsDebugBundleFilesWindow.h"

namespace CgsResource
{
    // ARTIST828E39D0..39F0, inlined in the component's change callback.
    void DebugBundleFilesWindow::Reset()
    {
        CgsDev::DebugUI::Window::Prepare(200.0f, 250.0f, "Bundle Files", KX_FLAGNORMAL);
    }
}
