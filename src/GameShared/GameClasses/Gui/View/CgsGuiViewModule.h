#pragma once

#include "types.hpp"

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
    };
}
