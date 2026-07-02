#include "GameShared/GameClasses/Gui/CgsGuiModule.h"

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

    // GuiModule's declaration now lives in the shared home CgsGuiModule.h (grown when
    // the BridgeFrom* TU landed); this file keeps the ctor body.

    // X360 byte offsets the ctor writes INSIDE the explicit maHead padding member
    // (authoritative, from the asm). Everything past the head is a named member now
    // (mpViewModule / mLoadNotifications -- see CgsGuiModule.h).
    static const int KI_BASE_VTABLE    = 0x00000;
    static const int KI_BASE_RWMUTEX_A = 0x00010;
    static const int KI_BASE_RWMUTEX_B = 0x00118;
    static const int KI_MODEL_MODULE   = 0x00228;   // addi r3, r31, 0x228

    GuiModule::GuiModule()
    {
        // --- the maHead span: base (CgsModule::ModuleSingleBuffered) sub-object, inlined ---
        // The head's interior (base vtable slot, the two RW locks, the embedded
        // ModelModule) is not yet modelled as named members; the writes anchor on the
        // NAMED explicit-padding member at the attested offsets. Install the base-most
        // vtable, build the two base read/write locks, then install the most-derived
        // (GuiModule) vtable -- the standard ctor vtable hand-off.
        char* lpcHead = reinterpret_cast<char*>(maHead);
        *reinterpret_cast<void**>(lpcHead + KI_BASE_VTABLE) =
            const_cast<void*>(gpModuleBaseVTable);
        new (lpcHead + KI_BASE_RWMUTEX_A) EA::Thread::RWMutex(0, 1);
        new (lpcHead + KI_BASE_RWMUTEX_B) EA::Thread::RWMutex(0, 1);
        *reinterpret_cast<void**>(lpcHead + KI_BASE_VTABLE) =
            const_cast<void*>(gpGuiModuleVTable);

        // --- mModelModule (CgsGui::ModelModule) at head +0x228 ---
        new (lpcHead + KI_MODEL_MODULE) ModelModule();

        // --- the load-notification queue's constructed flag (the X360 stbx of 0 at
        //     +0x1B41C == the queue's leading byte; see BridgeFromModelToOutput) ---
        mLoadNotifications.MarkUnconstructed();
    }
}
