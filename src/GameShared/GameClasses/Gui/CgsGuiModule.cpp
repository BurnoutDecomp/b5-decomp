#include "types.hpp"
#include <new>   // placement new (construct the owned ModelModule sub-object at its byte offset)

// CgsGui::GuiModule::GuiModule  -- constructor.  Reconstructed from
// BURNOUT_X360_ARTIST.XEX @ 0x827E54B0.
//
// GuiModule is the top-level GUI subsystem module: a CgsModule::ModuleSingleBuffered
// derivative that owns the GUI ModelModule (the model-side subsystem, +0x228) plus a small
// trailing byte flag. The X360 ctor is the standard subsystem-module shape (the same one the
// sibling ctors CgsGui::ModelModule::ModelModule @0x827E4F10 and
// BrnDirector::DirectorModule::DirectorModule use):
//
//   *(this+0x000)   = gpModuleBaseVTable               ; base-most (ModuleSingleBuffered) vtable
//   RWMutex(this+0x010, 0, 1)                            ; base read/write lock A
//   RWMutex(this+0x118, 0, 1)                            ; base read/write lock B
//   *(this+0x000)   = gpGuiModuleVTable                 ; most-derived (GuiModule) vtable
//   CgsGui::ModelModule::ModelModule(this+0x228)         ; mModelModule (owned sub-object)
//   *(this+0x1B41C) = (u8)0                              ; trailing byte flag (stbx of 0)
//   return this
//
// The byte store offset 0x1B41C is the X360 `lis r11,1; ori r11,r11,0xB41C; stbx r10,r31,r11`
// (0x10000 | 0xB41C == 0x1B41C) -- a single-byte field past the embedded ModelModule.
//
// Because the surrounding ModuleSingleBuffered layout (and the ~0x18000-byte embedded
// ModelModule) are not yet modelled as named members, every touched location is addressed by
// its X360 byte offset through a char* view of `this` -- the same approach CgsModelModule.cpp
// uses. Only the locations the constructor actually writes are reproduced.

namespace CgsGui
{
    // The owned ModelModule sub-object: declaration-only here (its real home is
    // CgsModelModule.cpp; the per-TU `cl /c` gate does not link, so the body resolves at link
    // time). Constructed in place at this+0x228.
    struct ModelModule { ModelModule(); };
}

namespace EA
{
    namespace Thread
    {
        // Engine threading primitive (external; EAThread SDK, not project-owned). The two
        // arguments are the (lockRecursively, intraProcess)-style construction flags the call
        // site passes (r4 = 0, r5 = 1). Member-ctor form: r3 = this.
        struct RWMutex
        {
            RWMutex(int liArg0, int liArg1);
        };
    }
}

namespace CgsGui
{
    // Vtable symbols installed by the constructor (defined elsewhere -- external data).
    //   gpModuleBaseVTable -- the ModuleSingleBuffered base-most vtable (off_820CE500),
    //                         installed first at +0x000 (shared with ModelModule's ctor).
    //   gpGuiModuleVTable  -- GuiModule's most-derived vtable (off_820D1330).
    extern void* const gpModuleBaseVTable;
    extern void* const gpGuiModuleVTable;

    // Declaration-only skeleton: the ctor addresses its fields by byte offset (the full
    // ~0x1B400+ layout, dominated by the embedded ModelModule, is not yet modelled). The
    // default ctor is the X360 @0x827E54B0 body.
    struct GuiModule
    {
        GuiModule();
    };

    // X360 byte offsets the ctor writes (authoritative, from the asm).
    static const int KI_BASE_VTABLE    = 0x00000;
    static const int KI_BASE_RWMUTEX_A = 0x00010;
    static const int KI_BASE_RWMUTEX_B = 0x00118;
    static const int KI_MODEL_MODULE   = 0x00228;   // addi r3, r31, 0x228
    static const int KI_TRAILING_FLAG  = 0x1B41C;   // (0x10000 | 0xB41C) stbx of 0

    GuiModule::GuiModule()
    {
        char* lpcThis = reinterpret_cast<char*>(this);

        // --- base (CgsModule::ModuleSingleBuffered) sub-object, inlined ---
        // Install the base-most vtable, build the two base read/write locks, then install the
        // most-derived (GuiModule) vtable -- the standard ctor vtable hand-off.
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpModuleBaseVTable);
        new (lpcThis + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, 1);
        new (lpcThis + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, 1);
        *reinterpret_cast<void**>(lpcThis + KI_BASE_VTABLE) =
            const_cast<void*>(gpGuiModuleVTable);

        // --- mModelModule (CgsGui::ModelModule) at +0x228 ---
        new (lpcThis + KI_MODEL_MODULE) ModelModule();

        // --- trailing byte flag at +0x1B41C ---
        *reinterpret_cast<u8*>(lpcThis + KI_TRAILING_FLAG) = 0;
    }
}
