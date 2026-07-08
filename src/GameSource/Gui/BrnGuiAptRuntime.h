#ifndef BRN_GUI_APT_RUNTIME_H
#define BRN_GUI_APT_RUNTIME_H

#include "types.hpp"

namespace CgsGraphics { struct Im2d; }
namespace CgsGui { class ViewModule; }

namespace BrnGui
{
    // Gui-owned Apt movie host.
    //
    // This is the public ownership boundary used by GuiModule and the renderer. The
    // remaining Apt load/tick/render implementation is private to BrnGuiAptRuntime.cpp
    // until AptDataHandler/ViewModule/resource ownership can be split into the final
    // reconstructed homes.
    class AptRuntimeHost
    {
    public:
        bool Prepare(CgsGui::ViewModule* lpViewModule);
        bool Prepare();
        void PlayMovie(const char* lpacMovieName, s32 liLevelNum);
        void Update();
        void Render(CgsGraphics::Im2d* lpIm2d);
        void StopMovie();

        bool IsReady() const;
        bool IsMovieLive() const;
        bool IsMovieComposed() const;

        bool SetComponentViewState(const char* lpacInstName, const char* lpacViewState);
        bool SetComponentKeyValue(const char* lpacInstName, const char* lpacKey,
                                  const char* lpacValue);

    private:
        CgsGui::ViewModule* mpViewModule = nullptr;
    };

    // Interim renderer bridge, matching gpActiveMovieManager: GuiModule publishes
    // its owned AptRuntimeHost while prepared; BrnRendererModule renders through it.
    extern AptRuntimeHost* gpActiveAptRuntimeHost;
}

#endif
