#include "types.hpp"
#include <new>   // placement new (construct owned sub-objects at their byte offsets)

// BrnDirector::DirectorModule::DirectorModule  — constructor.
//
// NOTE ON GROUPING: this function was ledger-grouped under the catch-all dest
// SDKs/Packages/ICE/ICEPoint.hpp, but it is not an ICEPoint function at all — it is
// the DirectorModule subsystem-module constructor. Its real home is here, alongside
// the sibling Director TUs (BrnDirectorResourceManager, BrnDirectorICEWrapper).
//
// DirectorModule is the top-level director engine module (a CgsModule::ModuleSingleBuffered
// derivative, as declared by BrnGameModule). It is a very large, mostly-opaque object; the
// constructor:
//   * installs the base-most vtable at +0x000, builds two read/write mutexes at +0x010 and
//     +0x118, then re-installs the most-derived vtable at +0x000 and a second vtable pointer
//     at +0x228 (multiple-inheritance / secondary base),
//   * constructs the owned DirectorResourceManager sub-object at +0x248,
//   * zero-/self-initialises three 0x20-byte intrusive list/tree heads at +0x8A8, +0x8C8,
//     +0x8E8 (each: three count/size words cleared, three self-pointing sentinel links, one
//     trailing word cleared),
//   * sets six handle/index fields to -1 (+0x9CC, +0xA70, +0xA9C, +0xAC8, +0xAD0, +0xAFC),
//   * constructs the owned MainDirector sub-object at +0xB00 and the ReplayDirector
//     sub-object at +0x35F50.
//
// Because the surrounding multi-megabyte layout is not yet modelled, every touched field is
// addressed by its byte offset through a char*/u32* view of `this` rather than as a named
// member. Only the members the constructor actually writes are reproduced; everything else
// is not-yet-modelled (see FLAGs in the accompanying report).

namespace BrnDirector
{
    // Owned sub-objects constructed in place. Declaration-only skeletons: the per-TU `cl /c`
    // gate does not link, so the real bodies (their own TUs) resolve at link time.
    //   DirectorResourceManager has a reconstructed home (BrnDirectorResourceManager.h/.cpp).
    //   MainDirector / ReplayDirector do not yet have homes — declared here as externals
    //   (FLAGGED in the report; replace with their real homes when reconstructed).
    struct DirectorResourceManager { DirectorResourceManager(); };
    struct MainDirector            { MainDirector(); };
    struct ReplayDirector          { ReplayDirector(); };
}

namespace EA
{
    namespace Thread
    {
        // Engine threading primitive (external). The two arguments are the
        // (lockRecursively, intraProcess)-style construction flags seen at the call site.
        struct RWMutex
        {
            RWMutex(int liArg0, int liArg1);
        };
    }
}

namespace BrnDirector
{
    // Vtable symbols installed by the constructor (defined elsewhere — external data).
    //   gpDirectorModuleBaseVTable    — installed first at +0x000 (base-most),
    //   gpDirectorModuleVTable        — re-installed at +0x000 (most-derived),
    //   gpDirectorModuleSecondVTable  — installed at +0x228 (secondary base).
    extern void* const gpDirectorModuleBaseVTable;
    extern void* const gpDirectorModuleVTable;
    extern void* const gpDirectorModuleSecondVTable;

    // The DirectorModule object. Declaration-only skeleton: the constructor body addresses
    // its fields by byte offset (the full ~0x36000-byte layout is not yet modelled).
    struct DirectorModule
    {
        DirectorModule();
    };

    // Offsets of the three 0x20-byte intrusive list/tree heads.
    static const int KI_LISTHEAD_0      = 0x8A8;
    static const int KI_LISTHEAD_1      = 0x8C8;
    static const int KI_LISTHEAD_2      = 0x8E8;
    static const int KI_LISTHEAD_STRIDE = 0x20;

    // Initialise one 0x20-byte list/tree head: clear the three leading words and the trailing
    // word, and self-point the three sentinel links at +0x0C/+0x10/+0x14.
    static void InitListHead(char* lpHead)
    {
        u32* lpuWords = reinterpret_cast<u32*>(lpHead);
        lpuWords[0] = 0;                                             // +0x00
        lpuWords[1] = 0;                                             // +0x04
        lpuWords[2] = 0;                                             // +0x08
        lpuWords[0] = 0;                                             // +0x00 (redundant clear, kept for parity)
        *reinterpret_cast<char**>(lpHead + 0x0C) = lpHead;          // +0x0C -> self
        *reinterpret_cast<char**>(lpHead + 0x10) = lpHead;          // +0x10 -> self
        *reinterpret_cast<char**>(lpHead + 0x14) = lpHead;          // +0x14 -> self
        lpuWords[6] = 0;                                             // +0x18
    }

    DirectorModule::DirectorModule()
    {
        char* lpBase = reinterpret_cast<char*>(this);

        // Base-most vtable, then the two read/write mutexes.
        *reinterpret_cast<void**>(lpBase + 0x000) = gpDirectorModuleBaseVTable;
        new (reinterpret_cast<void*>(lpBase + 0x010)) EA::Thread::RWMutex(0, 1);   // +0x010
        new (reinterpret_cast<void*>(lpBase + 0x118)) EA::Thread::RWMutex(0, 1);   // +0x118

        // Most-derived vtable (+0x000) and secondary-base vtable (+0x228).
        *reinterpret_cast<void**>(lpBase + 0x000) = gpDirectorModuleVTable;
        *reinterpret_cast<void**>(lpBase + 0x228) = gpDirectorModuleSecondVTable;

        // Owned resource manager (+0x248).
        new (reinterpret_cast<void*>(lpBase + 0x248)) DirectorResourceManager();

        // Three intrusive list/tree heads.
        InitListHead(lpBase + KI_LISTHEAD_0);
        InitListHead(lpBase + KI_LISTHEAD_1);
        InitListHead(lpBase + KI_LISTHEAD_2);

        // Six handle/index fields preset to -1.
        *reinterpret_cast<s32*>(lpBase + 0x9CC) = -1;   // +0x9CC
        *reinterpret_cast<s32*>(lpBase + 0xA70) = -1;   // +0xA70
        *reinterpret_cast<s32*>(lpBase + 0xA9C) = -1;   // +0xA9C
        *reinterpret_cast<s32*>(lpBase + 0xAC8) = -1;   // +0xAC8
        *reinterpret_cast<s32*>(lpBase + 0xAD0) = -1;   // +0xAD0
        *reinterpret_cast<s32*>(lpBase + 0xAFC) = -1;   // +0xAFC

        // Owned director sub-objects.
        new (reinterpret_cast<void*>(lpBase + 0xB00))   MainDirector();     // +0xB00
        new (reinterpret_cast<void*>(lpBase + 0x35F50)) ReplayDirector();   // +0x35F50
    }
}
