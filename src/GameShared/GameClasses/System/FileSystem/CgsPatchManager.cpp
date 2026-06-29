// CgsFileSystem::PatchManager - the loose-file "patch" overlay for the resource file system.
//
// Reconstructed from the X360 ARTIST asm/pseudocode (Construct 0x828F0980, AddPatch 0x828F8F80,
// DirSearchThreadProc 0x828F0B88, SearchFolder 0x828F0A78, AddFileToCurrentPatch 0x828E9678,
// RemapFilename 0x828E9488, GetPhysicalPath 0x828DCF40). See CgsPatchManager.h for the design.
//
// The inlined "String <s> is too long. Buffer size = N, string length = M" assert blocks the X360
// emitted at CgsStringUtils.h:65 ARE the bounds check inside CgsCore::StrCpy; the de-optimised C++
// expresses them as the StrCpy call they came from.

#include "GameShared/GameClasses/System/FileSystem/CgsPatchManager.h"

#include "GameShared/GameClasses/Containers/CgsHash.h"       // CgsContainers::CgsHash::CalculatePathHash
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SPrintf / StrCpy
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / gxMessageFilterFlags

#include <windows.h>   // FindFirstFileA / FindNextFileA / FindClose (the X360 file enumerator)
#include <cstring>     // strchr / strlen

namespace CgsFileSystem
{
    // The X360 publishes the live PatchManager into a global so the file-system open path can reach
    // RemapFilename without threading a pointer through (X360 dword_830EA3FC). RemapFilename is a
    // member here; the singleton mirrors the guest's global publish in Construct.
    PatchManager* gpPatchManager = 0;

    // CalculatePathHash takes a non-const unsigned char* in the recovered signature; this thin
    // bridge keeps the call sites reading const char* paths.
    static u32 HashPath(const char* lpcPath)
    {
        return CgsContainers::CgsHash::CalculatePathHash(
            reinterpret_cast<unsigned char*>(const_cast<char*>(lpcPath)));
    }

    // @0x828F0980 (boot trace).
    void PatchManager::Construct(rw::IResourceAllocator* lpAllocator)
    {
        mpAllocator         = lpAllocator;
        muCurrentPatchIndex = 0;

        // The X360 carves the map's entry array from the allocator (a 16384-byte, 16-aligned
        // descriptor[0] request) and binds the LinearHashTable<s32,s32> to it with length 0x1000.
        // The length 0x1000 is the grounded immediate (Construct r6 = 0x1000); the entry storage is
        // sized for KU_PATCH_FILE_MAP_SIZE slots.
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size =
            KU_PATCH_FILE_MAP_SIZE * sizeof(CgsContainers::LinearHashEntry<s32, s32>);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        rw::Resource lResource = mpAllocator->DoAllocate(lDescriptor, 0);

        CgsContainers::LinearHashEntry<s32, s32>* lpEntries =
            static_cast<CgsContainers::LinearHashEntry<s32, s32>*>(lResource.m_baseResources[0]);
        mPatchFileMap.Initialize(lpEntries, KU_PATCH_FILE_MAP_SIZE);

        mbPatching     = false;
        gpPatchManager = this;
    }

    // @0x828F8F80.
    bool PatchManager::AddPatch(const char* lpcFolder, s32 liValue)
    {
        CGS_ASSERT(!mbPatching, "Already patching");

        mbPatching = true;

        // Stamp the folder + priority key into the current patch slot. StrCpy carries the X360's
        // inlined "folder too long for the 64-byte buffer" bounds assert.
        PatchEntry& lrPatch = maPatches[muCurrentPatchIndex];
        CgsCore::StrCpy(lrPatch.miacFolder, sizeof(lrPatch.miacFolder), lpcFolder);
        lrPatch.miValue = liValue;

        // Kick the background directory walk that fills the file map (name + priority grounded by
        // the X360 asm: "PatchManDirSearchThread", priority 0x64 == 100).
        EA::Thread::ThreadParameters lParams;
        lParams.mpName     = "PatchManDirSearchThread";
        lParams.mnPriority = 100;
        mDirSearchThread.Begin(DirSearchThreadProc, this, &lParams);
        return true;
    }

    // @0x828F0B88. Background worker; `lpContext` is the PatchManager.
    intptr_t PatchManager::DirSearchThreadProc(void* lpContext)
    {
        PatchManager* lpThis = static_cast<PatchManager*>(lpContext);

        // FLAGGED: the X360 grabs a 26000-byte scratch from the allocator (vtable+16 Alloc), stores
        // the handle into miSearchHandleCount (+0x89C), runs the walk, then frees it (vtable+20). That
        // alloc/free pair is NOT reproduced here (the allocator interface for this manager is not yet
        // modelled, and SearchFolder uses its own fixed stack buffers, so the overlay result is
        // unaffected) -- left as a documented gap rather than fabricating the vtable calls. As a
        // consequence miSearchHandleCount stays unwritten until the allocator is modelled.
        lpThis->SearchFolder(lpThis->maPatches[lpThis->muCurrentPatchIndex].miacFolder, "");

        // Scan finished: advance to the next patch slot and clear the patching flag.
        lpThis->mbPatching = false;
        ++lpThis->muCurrentPatchIndex;
        return 0;
    }

    // @0x828F0A78.
    void PatchManager::SearchFolder(const char* lpcPatchFolder, const char* lpcSubFolder)
    {
        // Build "<patchFolder><subFolder>" then resolve it to a host physical path, then append the
        // FindFirstFile wildcard "\*".
        char lacLogical[256];
        CgsCore::SPrintf(lacLogical, sizeof(lacLogical), "%s%s", lpcPatchFolder, lpcSubFolder);

        char lacPhysical[256];
        GetPhysicalPath(lacPhysical, sizeof(lacPhysical), lacLogical);

        char lacWildcard[256];
        CgsCore::SPrintf(lacWildcard, sizeof(lacWildcard), "%s\\*", lacPhysical);

        WIN32_FIND_DATAA lFindData;
        HANDLE lFindHandle = FindFirstFileA(lacWildcard, &lFindData);
        if (lFindHandle == INVALID_HANDLE_VALUE)
            return;

        do
        {
            char lacChild[256];
            if ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                // Recurse into sub-directories (carrying the relative path forward).
                CgsCore::SPrintf(lacChild, sizeof(lacChild), "%s\\%s", lpcSubFolder, lFindData.cFileName);
                SearchFolder(lpcPatchFolder, lacChild);
            }
            else if ((lFindData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0)
            {
                // A regular file: record it under the current patch. The asm passes the path with
                // its leading '\' stripped (var_46F = &buf+1) so the recorded key matches lookups.
                CgsCore::SPrintf(lacChild, sizeof(lacChild), "%s\\%s", lpcSubFolder, lFindData.cFileName);
                AddFileToCurrentPatch(lacChild + 1);
            }
        }
        while (FindNextFileA(lFindHandle, &lFindData));

        FindClose(lFindHandle);
    }

    // @0x828E9678.
    void PatchManager::AddFileToCurrentPatch(const char* lpcFile)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Add patch file: "
                                       << (lpcFile ? lpcFile : "<NULLSTRING>")
                                       << "\n";
        }

        const u32 luHash = HashPath(lpcFile);

        s32* lpExisting = mPatchFileMap.FindEntry(static_cast<s32>(luHash));
        if (lpExisting)
        {
            // The path is already claimed. The patch with the LARGER priority key wins the clash
            // (asm @0x828E9758: bge skips the store when existing.miValue >= current.miValue, so
            // it re-points only when the incumbent's key is lower than the current patch's).
            const s32 liCurrent = muCurrentPatchIndex;
            if (maPatches[*lpExisting].miValue < maPatches[liCurrent].miValue)
                *lpExisting = liCurrent;
        }
        else
        {
            mPatchFileMap.AddEntry(static_cast<s32>(luHash), &muCurrentPatchIndex);
        }
    }

    // @0x828E9488.
    bool PatchManager::RemapFilename(const char* lpcSource, char* lpcDest, u32 luDestSize)
    {
        const u32 luHash = HashPath(lpcSource);

        s32* lpEntry = mPatchFileMap.FindEntry(static_cast<s32>(luHash));
        if (lpEntry)
        {
            // Claimed by a patch: rewrite to "<patchFolder>/<sourcePath>".
            CgsCore::SPrintf(lpcDest, luDestSize, "%s/%s", maPatches[*lpEntry].miacFolder, lpcSource);
            return true;
        }

        // Not patched: copy the source through unchanged (StrCpy carries the X360 length assert).
        CgsCore::StrCpy(lpcDest, luDestSize, lpcSource);
        return false;
    }

    // @0x828DCF40.
    void PatchManager::GetPhysicalPath(char* lpcDest, u32 luDestSize, const char* lpcLogical)
    {
        // Normalise into a working buffer: the asm copies from lpcLogical+7 (skipping the fixed
        // mount prefix; addi r29,r5,7) with the 256-byte length assert, then turns every forward
        // slash into a back-slash.
        char lacWork[256];
        CgsCore::StrCpy(lacWork, sizeof(lacWork), lpcLogical + 7);

        for (char* lpc = lacWork; *lpc; ++lpc)
        {
            if (*lpc == '/')
                *lpc = '\\';
        }

        // If the path contains a back-slash, the leading segment is a drive name: split it off and
        // emit "<drive>:\<rest>". Otherwise the whole string is the drive: emit "<path>:".
        char* lpFirstSlash = std::strchr(lacWork, '\\');
        if (lpFirstSlash)
        {
            *lpFirstSlash = '\0';
            const char* lpcRest = lpFirstSlash + 1;
            CgsCore::SPrintf(lpcDest, luDestSize, "%s:\\%s", lacWork, lpcRest);
        }
        else
        {
            CgsCore::SPrintf(lpcDest, luDestSize, "%s:", lacWork);
        }
    }
}
