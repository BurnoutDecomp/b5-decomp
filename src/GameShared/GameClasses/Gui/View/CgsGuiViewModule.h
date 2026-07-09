#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.h
//
// Canonical home for CgsGui::ViewModule -- the shared (engine-side) GUI *view*
// subsystem module. The X360 asserts cite this exact header path
// ("..\\..\\..\\GameShared\\GameClasses\\Gui/View/CgsGuiViewModule.h"), so this is
// the DWARF-attested home. BrnGui::ViewModule (GameSource/Gui/View/BrnViewModule.cpp)
// derives from this class.
//
// The member order below is the DecFIGS CgsGuiViewModule.h declaration, gated by
// the ARTIST Construct/Prepare/Update/RenderInternal bodies. PC pointer widening
// changes byte offsets, so runtime code addresses these members by name.
//
// Only the four ledger functions in scope are declared:
//   ViewModule()              X360 0x827E2728  (ctor; EXECUTED in the boot trace)
//   GetMovieNameByLevel()     X360 0x824EBCA8
//   SetClearScreenAlpha()     X360 0x82847500
//   SetCustomRendererManager()X360 0x824EBBF8

namespace CgsGui { class CustomRendererManager; }

// Pointer-only parameter types for the module lifecycle/IO virtuals below. These are
// forward-declared (incomplete-type use is sufficient for the declarations, and pulling
// their real homes in would force a large transitive header cascade into this skeleton
// header). Their concrete homes live with the IO/memory/RenderWare TUs that own them.
struct RGBA;                          // GUI clear/tint colour (passed by const ptr to Construct)
namespace CgsModule { struct Event; } // base module event record (ProcessIncomingLoadNotification arg)
namespace CgsMemory { class HeapMalloc; class LinearMalloc; }
namespace rw { class IResourceAllocator; }
namespace CgsGui
{
namespace ViewIO
{
    struct IOBufferStack;   // the per-frame module IO buffer stacks Update walks
    struct InputBuffer;     // the module's input buffer (Update / Render / RenderInternal)
    struct OutputBuffer;    // the module's output buffer (Update)
}
}

namespace CgsGui
{
    struct ImRendererSet
    {
        CgsGui::AptIm2dRenderBuffer* mpIm2dRenderer;
        void* mpReserved04;
        void* mpReserved08;
        void* mpReserved0C;
        void* mp3dRenderer;
    };

    // KI_NUM_MOVIE_LEVELS -- the assert "liLevel>=0 && liLevel < KI_NUM_MOVIE_LEVELS"
    // fires for liLevel > 8 (X360 `cmpwi r31,9; blt` -> valid range [0,8]), so the
    // count is 9. (Grounded: the asm range check, not a fabricated constant.)
    static const int KI_NUM_MOVIE_LEVELS = 9;

    class ViewModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_MANAGER = 1,
            E_PREPARESTAGE_LANGUAGE = 2,
            E_PREPARESTAGE_MOVIE = 3,
            E_PREPARESTAGE_APT = 4,
            E_PREPARESTAGE_DONE = 5,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_APT = 1,
            E_RELEASESTAGE_MOVIE = 2,
            E_RELEASESTAGE_LANGUAGE = 3,
            E_RELEASESTAGE_MANAGER = 4,
            E_RELEASESTAGE_DONE = 5,
        };

        ViewModule();
        virtual ~ViewModule() {}

        // ----------------------------------------------------------------------------
        // Module lifecycle / IO virtuals (vtable order from the DecFIGS DWARF for
        // CgsGui::ViewModule, gated on the X360 ledger -- every one is an attested X360
        // function and is the override target for BrnGui::ViewModule). The base bodies
        // live in CgsGuiViewModule.cpp (their own ledger TUs); they are declared (not
        // pure) so derived classes can override and so the per-TU `cl /c` gate resolves
        // the override signatures without needing the base bodies at compile time.
        // ----------------------------------------------------------------------------
        virtual void Construct(const char* lpcName, int liArg2, f32 lfArg3,
                               const RGBA* lpColour, int liArg5);
        virtual bool Prepare(CgsMemory::HeapMalloc* lpHeap, rw::IResourceAllocator* lpResAlloc,
                             CgsMemory::HeapMalloc* lpHeap2, CgsMemory::LinearMalloc* lpLinear);
        virtual bool Release();
        virtual void Destruct();
        virtual void Update(ViewIO::IOBufferStack* lpInStack, ViewIO::IOBufferStack* lpOutStack,
                            const ViewIO::InputBuffer* lpInput, ViewIO::OutputBuffer* lpOutput);

        // X360 0x824EBCA8. Returns the name of the movie playing on a level. The guest
        // arithmetic `return 32*(liLevel+1783) + this` is &macCurrentlyPlayingMovies
        // [liLevel] (the array base sits at [c:+57056] == 32*1783, stride 32); the
        // level is asserted in [0, KI_NUM_MOVIE_LEVELS). DWARF return: const char*.
        const char* GetMovieNameByLevel(int liLevel) const;

        // X360 0x82847500. Sets the per-frame clear-screen alpha (asserted in [0,1]).
        void SetClearScreenAlpha(f32 lfAlpha);

        // X360 0x824EBBF8. Installs the custom-renderer manager and wires the module's
        // sub-systems into it (the early sub-object block at +0x3E0, this module's
        // LanguageManager at +0x7C0C, and the supplied lpArg). Stores the manager pointer
        // into mpCustomRendererManager (+0xE008) and mirrors it at +0xE4E4.
        // NOTE: liArg3 (guest r5) is declared but never read by the X360 body -- the
        // asm passes r6 (liArg4) to the third manager call and ignores r5. Kept for
        // call-signature fidelity (Hex-Rays four-arg form == the four guest argregs).
        void SetCustomRendererManager(CustomRendererManager* lpCustomRendererManager,
                                      int liArg3, int liArg4);

        // X360 0x82858988 -- clear the frame to the configured clear-screen colour/alpha
        // before the derived view renders (non-virtual; called first by RenderInternal).
        void RenderBlackScreen();

        // ADDITIVE GROW (GuiModule::BridgeFromInputToView @0x8285B088): build the view
        // state for one frame from an inbound GUI-event queue + the view input buffer.
        // The X360 emits it as a member template parameterised on the source queue
        // (??$AddViewState@$0IAAA@$0BA@@ViewModule@CgsGui@@QAAXPBV?$GuiEventQueueBase@...
        // == AddViewState<32768,16>(const GuiEventQueueBase<32768,16>*,
        // ViewIO::InputBuffer*), returning void). DECLARATION-ONLY (the instantiation
        // body is its own ledger function).
        template <s32 BUFSIZE, s32 ALIGN>
        void AddViewState(const GuiEventQueueBase<BUFSIZE, ALIGN>* lpGuiEvents,
                          ViewIO::InputBuffer* lpInput);

        ImRendererSet* GetImRendererSet()
        { return &mImRenderers; }
        CgsGraphics::TextRenderer* GetTextRenderer()
        { return &mTextRenderer; }
        CgsLanguage::LanguageManager* GetLanguageManager()
        { return &mLanguageManager; }
        const FontCollection* GetFontCollection() const
        { return &mFonts; }
        FontCollection* GetFontCollection()
        { return &mFonts; }
        AptAux* GetAptAux()
        { return &mAptAux; }
        const AptAux* GetAptAux() const
        { return &mAptAux; }
        f32 GetUpdateTimeStep() const
        { return mfUpdateTimeDelta; }

    protected:
        // X360 0x82858AF8 (vtable slot after Update; see DWARF). Renders the shared view
        // content for one frame. Overridden by BrnGui::ViewModule (which chains to this).
        virtual void RenderInternal(const ViewIO::InputBuffer* lpInput);

        // X360 vtable slot after RenderInternal (DWARF). Handles a resource-load
        // notification routed to this module. Overridden by BrnGui::ViewModule.
        virtual void ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent);

    protected:
        bool mbUpdateFlash;
        f32 mfHack_LastValidTimeStep;
        f32 mfCurrentTime;
        f32 mfLastUpdateTime;
        f32 mfLastRenderTime;
        f32 mfUpdateTimeDelta;
        f32 mfRenderTimeDelta;
        ImRendererSet mImRenderers;
        CgsGraphics::TextRenderer mTextRenderer;
        FontCollection mFonts;
        CgsLanguage::LanguageManager mLanguageManager;
        GuiEventQueueBase<256, 16> mOutputEventQueue;

    private:
        EPrepareStage mePrepareStage;
        EReleaseStage meReleaseStage;
        const char* mpcLoadingMovieName;
        s32 miLoadingScreenLevel;
        char macCurrentlyPlayingMovies[KI_NUM_MOVIE_LEVELS][32];
        bool mbClearScreenEnabled;
        f32 mfClearScreenAlpha;
        CustomRendererManager* mpCustomRendererManager;
        AptAux mAptAux;
    };
}
