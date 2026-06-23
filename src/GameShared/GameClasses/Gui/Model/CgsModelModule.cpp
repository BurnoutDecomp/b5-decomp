#include "types.hpp"
#include <new>   // placement new (construct owned sub-objects at their byte offsets)

// CgsGui::ModelModule::ModelModule  -- constructor.  Reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x827E4F10.
//
// ModelModule is the GUI model subsystem module: per the DecFIGS DWARF
// (CgsModelModule.h) it is
//   struct CgsGui::ModelModule : public CgsModule::ModuleSingleBuffered {
//       ModelPrepareStage      mePrepareStage;     // :189
//       ModelReleaseStage      meReleaseStage;     // :190
//       EventInterpreterModule mEventInterpreter;  // :193   (at this+0x230 per asm)
//       GuiResourceModule      mGuiResource;        // :194   (at this+0x18040 per asm)
//   };
// Both the base (ModuleSingleBuffered) and the owned mGuiResource sub-object are
// ModuleSingleBuffered-shaped: their construction installs a base-most vtable at +0x000,
// builds two EA::Thread::RWMutex read/write locks at +0x010 and +0x118, then installs
// the most-derived vtable at +0x000. The X360 ctor inlines both (the base ctor and the
// mGuiResource member ctor), so the function performs:
//
//   *(this+0x000)       = gpModuleBaseVTable          ; base-most vtable
//   RWMutex(this+0x010, 0, 1)                          ; base read/write lock A
//   RWMutex(this+0x118, 0, 1)                          ; base read/write lock B
//   *(this+0x000)       = gpModelModuleVTable          ; most-derived (ModelModule) vtable
//   EventInterpreterModule::EventInterpreterModule(this+0x230)   ; mEventInterpreter
//   *(this+0x18040)     = gpModuleBaseVTable           ; mGuiResource base-most vtable
//   RWMutex(this+0x18050, 0, 1)                         ; mGuiResource lock A
//   RWMutex(this+0x18158, 0, 1)                         ; mGuiResource lock B
//   *(this+0x18040)     = gpGuiResourceModuleVTable    ; mGuiResource most-derived vtable
//   return this
//
// (The pseudocode word-indices a1[24592]/a1[24596]/a1[24662] are byte offsets
//  0x18040/0x18050/0x18158 -- matching the asm `addis r30,r31,2; addi r30,r30,-0x7FC0`
//  i.e. this + 0x20000 - 0x7FC0 == this + 0x18040.)
//
// Because the surrounding multi-megabyte ModuleSingleBuffered layout is not yet modelled
// (the committed CgsModuleSingleBuffered.h is a small declarative slice whose sizeof does
// not reproduce the +0x230 / +0x18040 member offsets), every touched location is
// addressed by its X360 byte offset through a char* view of `this` -- the same approach
// the sibling subsystem-module ctor BrnDirector::DirectorModule::DirectorModule uses.
// Only the locations the constructor actually writes are reproduced.

namespace CgsGui
{
    // The owned EventInterpreterModule sub-object: declaration-only here (its real home is
    // CgsEventInterpreterModule.{h,cpp}; the per-TU `cl /c` gate does not link, so the body
    // resolves at link time). Constructed in place at this+0x230.
    struct EventInterpreterModule { EventInterpreterModule(); };

    // The owned GuiResourceModule sub-object (DWARF CgsModelModule.h:194). It is itself a
    // ModuleSingleBuffered derivative; its in-place construction is inlined by the X360
    // ctor as the vtable+RWMutex sequence at this+0x18040 (see above). Declared here only
    // for the named offset documentation; constructed by raw store + RWMutex below because
    // the X360 inlined it (no out-of-line ctor symbol is called).
    struct GuiResourceModule;
}

namespace EA
{
    namespace Thread
    {
        // Engine threading primitive (external; EAThread SDK, not project-owned). The two
        // arguments are the (lockRecursively, intraProcess)-style construction flags the
        // call site passes (r4 = 0, r5 = 1). Member-ctor form: r3 = this.
        struct RWMutex
        {
            RWMutex(int liArg0, int liArg1);
        };
    }
}

namespace CgsGui
{
    // Vtable symbols installed by the constructor (defined elsewhere -- external data).
    //   gpModuleBaseVTable        -- the ModuleSingleBuffered base-most vtable (off_820CE500),
    //                                installed first at +0x000 of both this and mGuiResource.
    //   gpModelModuleVTable       -- ModelModule's most-derived vtable (off_820D11D0).
    //   gpGuiResourceModuleVTable -- GuiResourceModule's most-derived vtable (off_820CF080).
    extern void* const gpModuleBaseVTable;
    extern void* const gpModelModuleVTable;
    extern void* const gpGuiResourceModuleVTable;

    // Declaration-only skeleton: the ctor addresses its fields by byte offset (the full
    // ~0x18000+ layout is not yet modelled). The default ctor is the X360 @0x827E4F10 body.
    struct ModelModule
    {
        ModelModule();
    };

    // X360 byte offsets the ctor writes (authoritative, from the asm / pseudocode).
    static const int KI_BASE_VTABLE          = 0x00000;
    static const int KI_BASE_RWMUTEX_A       = 0x00010;
    static const int KI_BASE_RWMUTEX_B       = 0x00118;
    static const int KI_EVENT_INTERPRETER    = 0x00230;
    static const int KI_GUIRESOURCE          = 0x18040;
    static const int KI_GUIRESOURCE_RWMUTEX_A = 0x18050;  // KI_GUIRESOURCE + 0x10
    static const int KI_GUIRESOURCE_RWMUTEX_B = 0x18158;  // KI_GUIRESOURCE + 0x118

    ModelModule::ModelModule()
    {
        char* lpcThis = reinterpret_cast<char*>(this);

        // --- base (CgsModule::ModuleSingleBuffered) sub-object, inlined ---
        // Install the base-most vtable, build the two base read/write locks, then install
        // the most-derived (ModelModule) vtable -- the standard ctor vtable hand-off.
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpModuleBaseVTable);
        new (lpcThis + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, 1);
        new (lpcThis + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, 1);
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpModelModuleVTable);

        // --- mEventInterpreter (EventInterpreterModule) at +0x230 ---
        new (lpcThis + KI_EVENT_INTERPRETER) EventInterpreterModule();

        // --- mGuiResource (GuiResourceModule, itself ModuleSingleBuffered) at +0x18040 ---
        // The X360 inlined this member's ctor as the same vtable+RWMutex sequence.
        *reinterpret_cast<void**>(lpcThis + KI_GUIRESOURCE) =
            const_cast<void*>(gpModuleBaseVTable);
        new (lpcThis + KI_GUIRESOURCE_RWMUTEX_A) EA::Thread::RWMutex(0, 1);
        new (lpcThis + KI_GUIRESOURCE_RWMUTEX_B) EA::Thread::RWMutex(0, 1);
        *reinterpret_cast<void**>(lpcThis + KI_GUIRESOURCE) =
            const_cast<void*>(gpGuiResourceModuleVTable);
    }
}
