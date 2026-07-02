#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEventQueueBase (AddViewState source)

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.h
//
// Canonical home for CgsGui::ViewModule -- the shared (engine-side) GUI *view*
// subsystem module. The X360 asserts cite this exact header path
// ("..\\..\\..\\GameShared\\GameClasses\\Gui/View/CgsGuiViewModule.h"), so this is
// the DWARF-attested home. BrnGui::ViewModule (GameSource/Gui/View/BrnViewModule.cpp)
// derives from this class.
//
// LAYOUT NOTE (authoritative byte offsets, pinned by the X360 bodies in
// CgsGuiViewModule.cpp): ViewModule is a very large subsystem-module object
// (~0x28C80 bytes) built in the standard CgsModule::ModuleSingleBuffered shape --
// base-most vtable + two read/write locks, then the most-derived vtable, then a
// long run of embedded sub-objects (a CgsLanguage::LanguageManager at +0x7C0C, a
// CgsGui::AptRenderHandler at +0xE430, a trailing EA::Thread::Mutex at +0x28C78,
// etc.). The full layout is dominated by uncommitted sub-objects, so -- exactly as
// the sibling module homes do (CgsGuiModule.cpp / CgsModelModule.cpp) -- the body
// addresses each field it writes by its X360 byte offset through a char* view of
// `this`, with named KI_ offset constants. This is a declaration-only skeleton; the
// constructor body and the three accessor bodies live in CgsGuiViewModule.cpp.
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
namespace CgsGui { class ImRendererSet; class FontCollection; }
namespace CgsGraphics { class TextRenderer; }
namespace CgsLanguage { class LanguageManager; }
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
    // KI_NUM_MOVIE_LEVELS -- the assert "liLevel>=0 && liLevel < KI_NUM_MOVIE_LEVELS"
    // fires for liLevel > 8 (X360 `cmpwi r31,9; blt` -> valid range [0,8]), so the
    // count is 9. (Grounded: the asm range check, not a fabricated constant.)
    static const int KI_NUM_MOVIE_LEVELS = 9;

    class ViewModule
    {
    public:
        // X360 0x827E2728. Standard subsystem-module bring-up (see .cpp).
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

        // X360 0x824EBCA8. Returns the movie-name id for a level: a fixed string-id
        // base offset by (liLevel * 32) plus this module's id-table base. The level is
        // asserted to be in [0, KI_NUM_MOVIE_LEVELS). Pure arithmetic; no member writes.
        //   guest: return 32 * (liLevel + 1783) + this    (the `this`-relative base id table)
        int GetMovieNameByLevel(int liLevel) const;

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

        // -------------------------------------------------------------------------------
        // Named accessors for the owned sub-objects a DERIVED module needs to wire up
        // (BrnGui::ViewModule hands these to its FlaptManager and reads the per-frame
        // time-step from the base each Update). The base is a declaration-only skeleton
        // whose interior sub-objects are not modelled as named C++ members (their full
        // x64 layouts are uncommitted and their X360 byte offsets cannot be reproduced
        // with named members on the wider PC ABI -- see LAYOUT NOTE). These accessors are
        // the BY-NAME interface to those sub-objects: each returns the X360-attested
        // sub-object the derived code uses, encapsulating the offset inside the base (the
        // same house style the base bodies use for their own fields), so derived TUs never
        // do offset arithmetic themselves. The offsets are the ones the X360
        // BrnGui::ViewModule::Construct/Update bodies pass/read.
        ImRendererSet*            GetImRendererSet()
        { return reinterpret_cast<ImRendererSet*>(_BaseField(KI_OFF_IMRENDERERS)); }
        CgsGraphics::TextRenderer* GetTextRenderer()
        { return reinterpret_cast<CgsGraphics::TextRenderer*>(_BaseField(KI_OFF_TEXTRENDERER)); }
        CgsLanguage::LanguageManager* GetLanguageManager()
        { return reinterpret_cast<CgsLanguage::LanguageManager*>(_BaseField(KI_OFF_LANGUAGEMANAGER)); }
        const FontCollection*     GetFontCollection() const
        { return reinterpret_cast<const FontCollection*>(_BaseFieldC(KI_OFF_FONTCOLLECTION)); }
        // The per-frame update time-step the base maintains (X360 Update reads lfs at +0x23C).
        f32 GetUpdateTimeStep() const
        { return *reinterpret_cast<const f32*>(_BaseFieldC(KI_OFF_UPDATE_TIMESTEP)); }

    protected:
        // X360 0x82858AF8 (vtable slot after Update; see DWARF). Renders the shared view
        // content for one frame. Overridden by BrnGui::ViewModule (which chains to this).
        virtual void RenderInternal(const ViewIO::InputBuffer* lpInput);

        // X360 vtable slot after RenderInternal (DWARF). Handles a resource-load
        // notification routed to this module. Overridden by BrnGui::ViewModule.
        virtual void ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent);

    private:
        // X360 byte offsets of the owned sub-objects the accessors above expose (from the
        // BrnGui::ViewModule::Construct/Update asm; the base file's own constants cover the
        // fields its own bodies touch). Kept private -- the offset never leaves this class.
        static const u32 KI_OFF_UPDATE_TIMESTEP  = 0x23C;   // f32, fed to the flapt manager
        static const u32 KI_OFF_IMRENDERERS      = 0x250;   // ImRendererSet
        static const u32 KI_OFF_TEXTRENDERER     = 0x3E0;   // TextRenderer (early sub-object block)
        static const u32 KI_OFF_FONTCOLLECTION   = 0x7BF4;  // FontCollection
        static const u32 KI_OFF_LANGUAGEMANAGER  = 0x7C0C;  // CgsLanguage::LanguageManager
        char*       _BaseField(u32 luOffset)        { return reinterpret_cast<char*>(this) + luOffset; }
        const char* _BaseFieldC(u32 luOffset) const { return reinterpret_cast<const char*>(this) + luOffset; }
    };
}
