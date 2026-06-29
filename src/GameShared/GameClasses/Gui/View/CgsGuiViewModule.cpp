#include "types.hpp"
#include <new>   // placement new (construct owned sub-objects at their byte offsets)

#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// CgsGui::ViewModule -- shared GUI view subsystem module.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ViewModule()               @ 0x827E2728  (ctor; EXECUTED in the boot trace)
//   GetMovieNameByLevel()      @ 0x824EBCA8
//   SetClearScreenAlpha()      @ 0x82847500
//   SetCustomRendererManager() @ 0x824EBBF8
//
// ViewModule is a ~0x28C80-byte subsystem-module object whose layout is dominated by
// large uncommitted embedded sub-objects (LanguageManager, AptRenderHandler, the
// custom-renderer wiring block, ...). Exactly as the sibling module homes do
// (CgsGuiModule.cpp / CgsModelModule.cpp), this is a declaration-only skeleton: each
// field the bodies touch is addressed by its X360 byte offset through a char* view of
// `this`, with named KI_ offset constants. Only the locations the bodies actually read
// or write are reproduced; nothing else of the layout is asserted.

namespace EA
{
    namespace Thread
    {
        // Engine threading primitives (external; EAThread SDK, not project-owned). Member-ctor
        // form: r3 = this; the two flag args are (lockRecursively, intraProcess)-style (r4=0,
        // r5=1) at every call site.
        struct RWMutex { RWMutex(int liArg0, int liArg1); };
        struct Mutex   { Mutex(int liArg0, int liArg1); };
    }
}

namespace CgsLanguage
{
    // Owned localisation manager sub-object (real home CgsLanguageManager.*). Declaration-only
    // here -- the X360 ctor constructs it in place at this+0x7C0C; the per-TU `cl /c` gate does
    // not link, so the real body resolves at link time. (Modelled as a local linkage skeleton
    // for the placement-new, mirroring CgsGuiModule.cpp's ModelModule stub; not a layout fork.)
    struct LanguageManager { LanguageManager(); };
}

namespace CgsGui
{
    // Owned Apt render handler sub-object (real home CgsAptRenderHandler.*). Declaration-only
    // here for the same reason -- constructed in place at this+0xE430.
    struct AptRenderHandler { AptRenderHandler(); };

    // The custom-renderer manager the module installs. On the X360, SetCustomRendererManager
    // dispatches three subsystem-wiring calls through this object's vtable (guest slots +0x38,
    // +0x3C, +0x40), passing the early sub-object block (+0x3E0), this module's LanguageManager
    // (+0x7C0C) and the caller-supplied argument respectively. The committed base
    // CgsGui::CustomRendererManager (a minimal slice homed in GameSource/Gui/BrnCustomRendererManager.h)
    // is intentionally NOT included here -- pulling a GameSource header into a GameShared TU would
    // invert the layering, and its minimal-slice vtable order does not line up with the guest
    // slots. Instead the three wiring calls are modelled as named virtual methods on a local
    // declaration-only interface reflecting the observed argument flow. FLAG: the precise method
    // names/return types of the three guest vtable slots are not independently DWARF-attested;
    // they are reconstructed from the call-site argument flow (subsystem back-pointer wiring).
    struct CustomRendererManagerWiring
    {
        // +0x38: receives the module's early sub-object block (this+0x3E0).
        virtual void   SetParentSubsystem(void* lpSubsystem) = 0;
        // +0x3C: receives this module's LanguageManager (this+0x7C0C).
        virtual void   SetLanguageManager(void* lpLanguageManager) = 0;
        // +0x40: receives the caller-supplied argument; returns a value (guest result in r3).
        virtual int    SetExtraWiring(int liArg) = 0;
    };

    // --- X360 byte offsets the bodies touch (authoritative, from the asm) -----------------
    static const int KI_BASE_VTABLE       = 0x00000;   // base-most ModuleSingleBuffered vtable
    static const int KI_BASE_RWMUTEX_A    = 0x00010;   // base read/write lock A  (addi r3,r31,0x10)
    static const int KI_BASE_RWMUTEX_B    = 0x00118;   // base read/write lock B  (addi r3,r31,0x118)
    static const int KI_SUBSYSTEM_BLOCK   = 0x003E0;   // early sub-object block (manager wiring arg)
    static const int KI_LANGUAGE_MANAGER  = 0x07C0C;   // CgsLanguage::LanguageManager  (addi r3,r31,0x7C0C)
    static const int KI_FLAG_BYTE         = 0x0DDC0;   // single byte cleared to 0 (lis 1; ori 0xDDC0; stbx)
    static const int KI_CLEAR_ALPHA       = 0x0E004;   // f32 mfClearScreenAlpha  (stfsx at 0xE004)
    static const int KI_RENDERER_MANAGER  = 0x0E008;   // CustomRendererManager*  (0x10000-0x1FF8)
    static const int KI_APT_RENDERHANDLER = 0x0E430;   // CgsGui::AptRenderHandler (0x10000-0x1FF0+0x420)
    static const int KI_RENDERER_MIRROR   = 0x0E4E4;   // mirror of the manager ptr (stwx at 0xE4E4)
    static const int KI_TRAILING_MUTEX    = 0x28C78;   // EA::Thread::Mutex (0x30000-0x1FF0-0x5398)

    // Movie-name id-table constants (from GetMovieNameByLevel asm):
    //   guest: addi r11,liLevel,0x6F7 (1783); slwi r11,r11,5 (*32); add r3,r11,this
    static const int KI_MOVIE_ID_BASE     = 0x6F7;     // 1783
    static const int KI_MOVIE_ID_STRIDE   = 32;        // slwi ,5

    // Vtable symbols installed by the constructor (defined elsewhere -- external data).
    //   gpModuleBaseVTable -- ModuleSingleBuffered base-most vtable (off_820CE500), installed first.
    //   gpViewModuleVTable -- ViewModule's most-derived vtable (off_820D0B68).
    extern void* const gpModuleBaseVTable;
    extern void* const gpViewModuleVTable;

    // -------------------------------------------------------------------------------------
    // Constructor @ 0x827E2728.  Standard subsystem-module bring-up:
    //   install base vtable -> build the two base read/write locks -> install the derived
    //   vtable -> construct the owned LanguageManager -> clear a flag byte -> construct the
    //   owned AptRenderHandler -> build the trailing Mutex.
    // -------------------------------------------------------------------------------------
    ViewModule::ViewModule()
    {
        char* lpcThis = reinterpret_cast<char*>(this);

        // --- base (CgsModule::ModuleSingleBuffered) sub-object, inlined ---
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpModuleBaseVTable);
        new (lpcThis + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, 1);
        new (lpcThis + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, 1);
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpViewModuleVTable);

        // --- owned CgsLanguage::LanguageManager at +0x7C0C ---
        new (lpcThis + KI_LANGUAGE_MANAGER) CgsLanguage::LanguageManager();

        // --- single flag byte at +0xDDC0 cleared ---
        *reinterpret_cast<u8*>(lpcThis + KI_FLAG_BYTE) = 0;

        // --- owned CgsGui::AptRenderHandler at +0xE430 ---
        new (lpcThis + KI_APT_RENDERHANDLER) AptRenderHandler();

        // --- trailing EA::Thread::Mutex at +0x28C78 ---
        new (lpcThis + KI_TRAILING_MUTEX) EA::Thread::Mutex(0, 1);
    }

    // -------------------------------------------------------------------------------------
    // GetMovieNameByLevel @ 0x824EBCA8.  Returns a movie-name string id for a level.
    //   guest: return KI_MOVIE_ID_STRIDE * (liLevel + KI_MOVIE_ID_BASE) + (int)this
    // The `this`-relative base is the per-module id-table address; the result is just the
    // arithmetic the asm performs (no member access). Level asserted in [0, KI_NUM_MOVIE_LEVELS).
    // -------------------------------------------------------------------------------------
    int ViewModule::GetMovieNameByLevel(int liLevel) const
    {
        CGS_ASSERT(liLevel >= 0 && liLevel < KI_NUM_MOVIE_LEVELS,
                   "liLevel out of range in GetMovieNameByLevel");

        return KI_MOVIE_ID_STRIDE * (liLevel + KI_MOVIE_ID_BASE)
             + static_cast<int>(reinterpret_cast<intptr_t>(this));
    }

    // -------------------------------------------------------------------------------------
    // SetClearScreenAlpha @ 0x82847500.  Stores the per-frame clear-screen alpha (in [0,1])
    // into mfClearScreenAlpha at +0xE004.
    // -------------------------------------------------------------------------------------
    void ViewModule::SetClearScreenAlpha(f32 lfAlpha)
    {
        CGS_ASSERT(lfAlpha >= 0.0f && lfAlpha <= 1.0f,
                   "lfAlpha must be in [0,1] in SetClearScreenAlpha");

        char* lpcThis = reinterpret_cast<char*>(this);
        *reinterpret_cast<f32*>(lpcThis + KI_CLEAR_ALPHA) = lfAlpha;
    }

    // -------------------------------------------------------------------------------------
    // SetCustomRendererManager @ 0x824EBBF8.  Installs the custom-renderer manager pointer
    // (+0xE008), wires three of the module's sub-systems into it through its vtable, then
    // mirrors the pointer at +0xE4E4.
    //   guest:
    //     mpCustomRendererManager      = lpManager
    //     lpManager->SetParentSubsystem(this + 0x3E0)
    //     lpManager->SetLanguageManager(this + 0x7C0C)
    //     result = lpManager->SetExtraWiring(liArg4)
    //     mpRendererManagerMirror      = mpCustomRendererManager
    // liArg3 (guest r5) is unread by the asm.
    // -------------------------------------------------------------------------------------
    void ViewModule::SetCustomRendererManager(CustomRendererManager* lpCustomRendererManager,
                                              int liArg3, int liArg4)
    {
        (void)liArg3;   // guest r5: never read by the X360 body

        CGS_ASSERT(lpCustomRendererManager != 0,
                   "lpCustomRendererManager must not be null");

        char* lpcThis = reinterpret_cast<char*>(this);

        // Store the manager pointer (+0xE008).
        *reinterpret_cast<CustomRendererManager**>(lpcThis + KI_RENDERER_MANAGER) =
            lpCustomRendererManager;

        // Wire the module sub-systems into the manager (three vtable dispatches). The manager
        // pointer is re-read from the member slot before each call, matching the asm
        // (`lwz r3,0(r31)`); modelled through the wiring interface above.
        CustomRendererManagerWiring* lpWiring =
            reinterpret_cast<CustomRendererManagerWiring*>(
                *reinterpret_cast<CustomRendererManager**>(lpcThis + KI_RENDERER_MANAGER));

        lpWiring->SetParentSubsystem(lpcThis + KI_SUBSYSTEM_BLOCK);
        lpWiring->SetLanguageManager(lpcThis + KI_LANGUAGE_MANAGER);
        lpWiring->SetExtraWiring(liArg4);

        // Mirror the manager pointer at +0xE4E4.
        *reinterpret_cast<CustomRendererManager**>(lpcThis + KI_RENDERER_MIRROR) =
            *reinterpret_cast<CustomRendererManager**>(lpcThis + KI_RENDERER_MANAGER);
    }
}
