#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsLinearHashTable.h"   // CgsContainers::LinearHashTable
#include "eathread/eathread_thread.h"                               // EA::Thread::Thread
#include "rw/rwcore_structs.h"                                      // rw::IResourceAllocator (entry storage)

// CgsFileSystem::PatchManager - the dev/loose-file "patch" overlay for the resource file system.
//
// HOME: GameShared/GameClasses/System/FileSystem/CgsPatchManager.{h,cpp} (the baked X360 assert
// strings point at d:\...\gameshared\gameclasses\system\filesystem\CgsPatchManager.cpp).
//
// WHAT IT DOES: a patch is a named folder (e.g. on the dev host's drive) added via AddPatch. Adding
// a patch spawns a background thread (DirSearchThreadProc) that walks the folder tree with the
// Win32 FindFirstFile/FindNextFile API (SearchFolder) and, for every regular file found, hashes its
// path (CgsContainers::CgsHash::CalculatePathHash) and records "this path -> the index of the patch
// that owns it" into mPatchFileMap (AddFileToCurrentPatch). When the resource file system later
// opens a file, RemapFilename looks the requested path's hash up in the map; if a patch claims it,
// the path is rewritten to "<patchFolder>/<originalPath>" so the loose patched file is read instead
// of the shipped bundle. GetPhysicalPath turns a logical path into a host physical path (drive
// letter + back-slashes) for the FindFirstFile scan.
//
// LATER patches do NOT automatically win: when two patches both contain the same file, the map keeps
// whichever patch has the SMALLER miValue (the integer passed to AddPatch as the patch's priority
// key) - see AddFileToCurrentPatch.
//
// SOURCES (X360 ARTIST): Construct 0x828F0980, AddPatch 0x828F8F80, DirSearchThreadProc 0x828F0B88,
// SearchFolder 0x828F0A78, AddFileToCurrentPatch 0x828E9678, RemapFilename 0x828E9488,
// GetPhysicalPath 0x828DCF40. Only Construct ran in the boot-trace milestone.
namespace CgsFileSystem
{
    // A registered patch: the folder path that overlays the file system, plus the integer priority
    // key supplied at AddPatch time. Stride is 68 bytes (0x44) - a 64-char folder name + the s32 key
    // immediately after it - confirmed by the X360 "0x44 * (index + 1)" entry-stride arithmetic.
    struct PatchEntry
    {
        char miacFolder[64];   // +0  the patch folder path (NUL-terminated, < 64 chars)
        s32  miValue;          // +64 the patch's priority key (lower wins on a file clash)
    };

    // CgsPatchManager.h - the patch overlay manager. Instance layout is the faithful X360 member
    // ORDER (members reached BY NAME, never by raw offset); the X360 asm of the methods below is the
    // authority for behaviour. Embedded by value inside the file system; Construct stashes the
    // owning FileSystem so the background search thread can allocate its scratch through it.
    class PatchManager
    {
    public:
        // The X360 RunnableFunction context is the PatchManager itself; KU_MAX_PATCHES patch slots
        // fit before the current-index member (0x884 - 4 == 2176 == 68 * 32).
        static const u32 KU_MAX_PATCHES        = 32;
        // The path-hash -> owning-patch-index map is sized for this many slots (Construct passes
        // 0x1000 as the table length).
        static const u32 KU_PATCH_FILE_MAP_SIZE = 4096;

        // @0x828F0980 (boot trace). Stash the resource allocator, zero the current-patch index, bring
        // up the path-hash map (length 0x1000) over allocator-supplied entry storage, clear the
        // "patching" flag and publish the singleton instance pointer. `lpAllocator` is the object the
        // X360 stores at +0 and whose virtual Alloc carves the map's entry array (and, later, the
        // directory-search thread's scratch).
        void Construct(rw::IResourceAllocator* lpAllocator);

        // @0x828F8F80. Register a new patch folder `lpcFolder` with priority key `liValue`, then
        // kick the background directory-search thread (name "PatchManDirSearchThread", priority 100)
        // that populates the file map. Asserts if a patch search is already running, or if the folder
        // path does not fit a PatchEntry::miacFolder buffer.
        bool AddPatch(const char* lpcFolder, s32 liValue);

        // @0x828E9488. If the requested path `lpcSource` is claimed by a patch, write
        // "<patchFolder>/<sourcePath>" into `lpcDest` (capacity `luDestSize`) and return true.
        // Otherwise copy the source path through unchanged and return false.
        bool RemapFilename(const char* lpcSource, char* lpcDest, u32 luDestSize);

        // @0x828DCF40. Resolve `lpcLogical` to a host physical path in `lpcDest` (capacity
        // `luDestSize`): slash->back-slash, and if the path is "<drive>\..." emit "<drive>:\<rest>",
        // else "<path>:".
        void GetPhysicalPath(char* lpcDest, u32 luDestSize, const char* lpcLogical);

    private:
        // @0x828F0B88. The background thread entry: scan the just-added patch folder, mark the scan
        // finished and advance the current-patch index. `lpContext` is the PatchManager.
        static intptr_t DirSearchThreadProc(void* lpContext);

        // @0x828F0A78. Recursively walk `lpcSubFolder` under the current patch folder via
        // FindFirstFile/FindNextFile: recurse into sub-directories, and add each regular file to the
        // current patch. `luPatchFolderHash`/`luSubFolder` carry the patch root + relative path used
        // to build the search wildcard.
        void SearchFolder(const char* lpcPatchFolder, const char* lpcSubFolder);

        // @0x828E9678. Hash `lpcFile`'s path and record current-patch ownership in mPatchFileMap.
        // On a clash, keep whichever owning patch has the smaller PatchEntry::miValue.
        void AddFileToCurrentPatch(const char* lpcFile);

        // ---- instance layout (faithful X360 member order; X360 members only) ----
        rw::IResourceAllocator*              mpAllocator;                   // +0    (0)    entry storage
        PatchEntry                           maPatches[KU_MAX_PATCHES];     // +4    (4)    32 * 68
        s32                                  muCurrentPatchIndex;           // +0x884 (2180)
        CgsContainers::LinearHashTable<s32, s32> mPatchFileMap;             // +0x888 (2184)
        bool                                 mbPatching;                    // +0x898 (2200)
        s32                                  miSearchHandleCount;           // +0x89C (2204) scan result
        EA::Thread::Thread                   mDirSearchThread;              // +0x8A0 (2208)
    };
}
