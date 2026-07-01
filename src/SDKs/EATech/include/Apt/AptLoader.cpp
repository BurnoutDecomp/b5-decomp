// ===========================================================================
// EATech Apt -- AptLoader request layer.   DECOMPILED from the PS3 EXTERNAL ELF
// (cross-checked vs X360 ARTIST):
//   FileNameCompare        @0x7E3E94   findFile     @0x7FBAA0
//   AptLoader::IsLoaded    @0x80D2C0   Load         @0x80CFF4
//   AptLoader::Invalidate  @0x7F2AE4
//
// The loader holds a singly-linked list of weak {AptFile*, next} nodes (head at
// mpHead). Load dedups by file name and, on a miss, registers a new AptFile in
// the "requested" state. The AptSharedPtr handed back owns the file's only
// counted reference; the node is weak and is unlinked by ~AptFile -> Invalidate
// when that last reference dies.
//
// All list access is bracketed by MutexAptLoader. IsLoaded/Load lock and then
// call findFile (which locks again) -- the EA mutex is recursive, matching the
// console's nested lock/unlock.
//
// POOL: Load/Invalidate allocate the AptFile + nodes from gpNonGCPoolManager,
// which is non-null by the time the Apt system runs Load (after AptInit), so no
// bring-up fallback is needed here (unlike EAStringC, which is used pre-AptInit).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptLoader.h"
#include "SDKs/EATech/include/Apt/AptConstFile.h"          // the serialised .apt header
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h" // Resolve (the movie root)
#include "SDKs/EATech/include/Apt/AptFile.h"               // AptMovieData (AllImportsAvailable)
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptTarget.h"    // gpAptTarget->mpLinker (off_8324E574+0x20)
#include "SDKs/EATech/Apt/DogmaAllocator.h"        // DOGMA_PoolManager::Allocate/Deallocate

#include "eathread/eathread_mutex.h"               // EA::Thread::Mutex

#include <new>       // placement new (init the AptFile's EAStringC member in pool memory)
#include <cstring>   // strlen

// The one global lock for the loader's list. (EA::Thread::Mutex is recursive,
// which IsLoaded/Load rely on.)
EA::Thread::Mutex MutexAptLoader;

// ---------------------------------------------------------------------------
// FLAG (un-homed engine hooks, owned by their own deferred TUs). The X360 loader
// reaches the async stream subsystem + the linker through fn-ptr slots / the
// AptLinker; routed here through named externs (not the literal console offsets)
// so the x64 layout stays correct. Each lands when its owning TU is reconstructed.
//
//   AptLoader_StartAsyncLoad  (dword_8324E838) -- kick off the async .apt stream
//        for a freshly-requested file: takes the file name buffer + the AptFile
//        handle. Called from Update on the state 1 -> 2 transition.
//   AptLoader_CancelAsyncLoad (dword_8324E83C) -- abort an in-flight stream
//        (state 2 with a pending data block). Called from CancelPreloadedAnimation.
//   AptLinker_isFileImported  (AptLinker__isFileImported) -- true when the
//        candidate AptFile is still imported by any movie the linker tracks;
//        CONSUMES the candidate handle (disposes + nulls *ppCandidate), matching
//        the AptFile::isFileImported it wraps.
// ---------------------------------------------------------------------------
extern void AptLoader_StartAsyncLoad(const char* pFileName, AptFilePtr* pFile);   // dword_8324E838
extern void AptLoader_CancelAsyncLoad(void* pDataBlock);                          // dword_8324E83C
extern bool AptLinker_isFileImported(AptLinker* pLinker, AptFilePtr* ppCandidate); // AptLinker__isFileImported

// ---------------------------------------------------------------------------
// FileNameCompare @0x7E3E94 -- case-insensitive, slash-normalised ('\' == '/')
// path equality. Returns 1 if equal, 0 otherwise. (De-tangled from the asm's
// hand-optimised loop; semantics are identical: equal length + each char equal
// after lower-casing A-Z and folding backslash to forward slash.)
// ---------------------------------------------------------------------------
static inline char FileNameNormChar(char c)
{
    if (c >= 'A' && c <= 'Z')
        c = static_cast<char>(c + 32);   // to lower
    if (c == '\\')
        c = '/';                          // path separators are equivalent
    return c;
}

static int FileNameCompare(const char* a1, const char* a2)
{
    if (!a1 || !a2)
        return 0;
    const size_t len = strlen(a1);
    if (strlen(a2) != len)
        return 0;
    for (size_t i = 0; i < len; ++i)
    {
        if (a1[i] == a2[i])
            continue;
        if (FileNameNormChar(a1[i]) != FileNameNormChar(a2[i]))
            return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// findFile @0x7FBAA0
// ---------------------------------------------------------------------------
AptFilePtr AptLoader::findFile(const EAStringC& fileName)
{
    AptFilePtr result;
    result.pData = nullptr;

    MutexAptLoader.Lock();

    AptLoaderNode* node = mpHead;
    if (!node)
    {
        MutexAptLoader.Unlock();
        return result;
    }

    while (!FileNameCompare(node->mpFile->mFileName.c_str(), fileName.c_str()))
    {
        node = node->mpNext;
        if (!node)
        {
            MutexAptLoader.Unlock();
            return result;
        }
    }

    MutexAptLoader.Unlock();
    result.pData = node->mpFile;
    if (node->mpFile)
        AptSharedPtrIncRef(node->mpFile);
    return result;
}

// ---------------------------------------------------------------------------
// IsLoaded @0x80D2C0 -- findFile + accept only the fully-loaded states (4/5).
// ---------------------------------------------------------------------------
AptFilePtr AptLoader::IsLoaded(const EAStringC& fileName)
{
    AptFilePtr result;
    result.pData = nullptr;

    MutexAptLoader.Lock();
    AptFilePtr found = findFile(fileName);

    int state;
    if (found.pData && ((state = found.pData->mnState) == 4 || state == 5))
    {
        MutexAptLoader.Unlock();
        result.pData = found.pData;
        AptSharedPtrIncRef(found.pData);
    }
    else
    {
        MutexAptLoader.Unlock();
    }

    // Release the local `found` reference taken by findFile.
    AptFile* tmp = found.pData;
    found.pData = nullptr;
    if (tmp && AptSharedPtrDecRef(tmp) == 0)
        AptSharedPtrDelete(tmp);
    return result;
}

// ---------------------------------------------------------------------------
// Load @0x80CFF4 -- return the shared handle for fileName, registering a new
// requested AptFile on a miss.
// ---------------------------------------------------------------------------
AptFilePtr AptLoader::Load(const EAStringC& fileName)
{
    AptFilePtr result;
    result.pData = nullptr;

    MutexAptLoader.Lock();
    AptFilePtr found = findFile(fileName);

    if (found.pData)
    {
        // Already registered -- hand back a counted reference.
        MutexAptLoader.Unlock();
        result.pData = found.pData;
        AptSharedPtrIncRef(found.pData);
    }
    else
    {
        // Register a fresh AptFile in the "requested" state.
        AptFile* f = static_cast<AptFile*>(gpNonGCPoolManager->Allocate(sizeof(AptFile)));
        f->mnRefCount = 0;
        new (&f->mFileName) EAStringC(fileName);   // copy ctor: share buffer + IncreaseInternalRefCount
        f->mpResolveContext = nullptr;
        f->mnField12        = 1;
        f->mnState          = 1;                    // requested
        f->mpData           = nullptr;
        f->mpDataBlock      = nullptr;
        AptSharedPtrIncRef(f);                       // refcount 0 -> 1

        // Push a weak node onto the list head.
        AptLoaderNode* n = static_cast<AptLoaderNode*>(gpNonGCPoolManager->Allocate(sizeof(AptLoaderNode)));
        n->mpFile = f;
        n->mpNext = mpHead;
        mpHead    = n;

        MutexAptLoader.Unlock();

        // Hand back the file's one counted reference (the node is weak).
        result.pData = f;
        AptSharedPtrIncRef(f);                       // 1 -> 2 (returned ref)
        if (AptSharedPtrDecRef(f) == 0)              // 2 -> 1
            AptSharedPtrDelete(f);
        // `found` is null in this branch -> the cleanup below is a no-op.
    }

    AptFile* tmp = found.pData;
    found.pData = nullptr;
    if (tmp && AptSharedPtrDecRef(tmp) == 0)
        AptSharedPtrDelete(tmp);
    return result;
}

// ---------------------------------------------------------------------------
// Invalidate @0x7F2AE4 -- unlink the (weak) node owning pFile from the list.
// ---------------------------------------------------------------------------
void AptLoader::Invalidate(AptFile* pFile)
{
    MutexAptLoader.Lock();

    if (mpHead && mpHead->mpFile == pFile)
    {
        AptLoaderNode* next = mpHead->mpNext;
        gpNonGCPoolManager->Deallocate(mpHead, sizeof(AptLoaderNode));
        mpHead = next;
        MutexAptLoader.Unlock();
        return;
    }

    if (!mpHead)
    {
        MutexAptLoader.Unlock();
        return;
    }

    AptLoaderNode* prev = mpHead;
    AptLoaderNode* node = mpHead->mpNext;
    if (!node)
    {
        MutexAptLoader.Unlock();
        return;
    }

    while (node->mpFile != pFile)
    {
        prev = node;
        node = node->mpNext;
        if (!node)
        {
            MutexAptLoader.Unlock();
            return;
        }
    }

    prev->mpNext = node->mpNext;
    gpNonGCPoolManager->Deallocate(node, sizeof(AptLoaderNode));
    MutexAptLoader.Unlock();
}

// CompleteLoad @0x82AFF9E8 (PS3 @0x80EF2C) -- a streamed .apt has arrived; resolve +
// publish it. DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX. The X360 args are
// (a1=this loader, a2=&handle, a3=pBase, a4=pConstFile, a5=pAptDataHeader):
//
//   if (!a3) { dispose(*a2); return; }                    // pBase null guard
//   v15 = a4->mnDataRootOffset; if (v15) v15 += a3;       // relocate the root in place
//   a4->mnDataRootOffset = v15;
//   Resolve(v15 + 16, a3, a4, a5);                        // Resolve(defBase, pBase, pConstFile, a5)
//   f = *a2;
//   f[4] = a3;   // mpResolveContext = pBase   (+0x10)
//   f[6] = a5;   // mpDataBlock      = a5       (+0x18)  <-- THE AptDataHeader (not the raw block)
//   f[5] = a4->mnDataRootOffset;   // mpData = pRoot      (+0x14)
//   f->mnState = 3; f->mnField12 = prevState;
//   a4->mnDataRootOffset -= a3;                           // un-relocate the offset back
//   dword_8324E840(a4);                                   // the load-complete hook (FLAG)
//   dispose(*a2);
//
// pBlock (a5) is the AptDataHeader: it is what f->mpDataBlock is set to AND what flows to
// Resolve->Fixup so the case-1 pfnLoadRenderingUnit reads AptData+12 (the geometry object).
void AptLoader::CompleteLoad(AptFilePtr filePtr, void* pBase, AptConstFile* pConstFile,
                             void* pBlock, void* pPreResolvedRoot)
{
    if (!pBase)
    {
        // The by-value handle is owned by the callee; release it on this exit too
        // (asm early-exit @0x82AFFA10: *a2=0 + DecRef + delete-if-zero).
        AptSharedPtr<AptFile>::Dispose(filePtr.pData);
        filePtr.pData = nullptr;
        return;
    }

    // The data root (the movie root CHARACTER HEADER), and the movie's embedded
    // AptCharacterAnimation def base at root + the pointer-size header.
    // FLAG (x64 fork): the console relocates a4->mnDataRootOffset in place in the 32-bit
    // file slot (v15 += a3, stored back, then subtracted off again after Resolve); x64
    // computes the absolute address without the 32-bit write-back. The +pBase relocation is
    // GUARDED on a nonzero offset (asm @0x82AFFA44 beq): a zero offset leaves the root null.
    //
    // FLAG (x64 converted 8-byte bundle): our converted .apt's mnDataRootOffset does not
    // locate the type-9 movie root (its console layout diverged), so the host pre-locates the
    // root character header (signature scan) and passes it in pPreResolvedRoot; the def base is
    // root + the native-8 header (0x20). The console 4-byte formula (pBase + dataRootOffset,
    // def base at root+16) is used when no pre-resolved root is supplied.
    void* pRoot;
    unsigned int luHdrSize;
    if (pPreResolvedRoot != nullptr)
    {
        pRoot     = pPreResolvedRoot;                         // FLAG (x64): host-located root header
        luHdrSize = (pConstFile->GetPointerSizeBytes() == 8) ? 0x20u : 0x10u;
    }
    else
    {
        pRoot     = pConstFile->mnDataRootOffset
                        ? static_cast<char*>(pBase) + pConstFile->mnDataRootOffset
                        : nullptr;
        luHdrSize = 16u;                                      // console 4-byte header
    }
    AptCharacterAnimation* pCharAnim =
        reinterpret_cast<AptCharacterAnimation*>(static_cast<char*>(pRoot) + luHdrSize);

    // Resolve the (serialised) movie root against the load base. a5 (pBlock == the
    // AptDataHeader) is threaded through Resolve -> Fixup faithfully.
    pCharAnim->Resolve(pBase, pConstFile, pBlock);

    AptFile* f = filePtr.pData;
    f->mpResolveContext = pBase;    // f[4]  (+0x10)  = pBase
    f->mpDataBlock      = pBlock;   // f[6]  (+0x18)  = a5 (the AptDataHeader)  <-- item 2 fix
    f->mpData           = pRoot;    // f[5]  (+0x14)  = the movie root
    f->mnField12        = f->mnState;   // record the previous state
    f->mnState          = 3;            // loaded / resolved

    // FLAG: the console then notifies/frees through the load-complete hook (dword_8324E840(a4)).

    // Release the by-value handle the callee owns (asm normal-exit @0x82AFFAC0:
    // *a2=0 + DecRef + AptSharedPtrDelete), mirroring AllImportsAvailable.
    AptSharedPtr<AptFile>::Dispose(filePtr.pData);
    filePtr.pData = nullptr;
}

// ---------------------------------------------------------------------------
// AptCompleteAnimationAsyncLoad @0x82AFFD38 -- the async-completion glue between the
// AptCallbackFile::LoadAnimation host callback and AptLoader::CompleteLoad.
// DECOMPILED FAITHFULLY from BURNOUT_X360_ARTIST.XEX.
//
//   The X360 (a1=&handle, a2=pBase, a3=pConstFile, a4=pAptDataHeader):
//     if (*a1) IncRef(*a1);                                 // pin the handle
//     AptLoader::CompleteLoad(gpAptTarget->mpLoader, a1, a2, a3, a4);
//     r = *a1; *a1 = 0; if (r && --r.count == 0) AptSharedPtrDelete(r);   // drop the pin
//     return r;
//
// The loader is gpAptTarget->mpLoader (X360 *(off_8324E574 + 7) == gpAptTarget[+0x1C]).
// pPreResolvedRoot is threaded through for the x64 converted-bundle root location (FLAG;
// see CompleteLoad) -- it is not a console argument (defaulted null on the console path).
// ---------------------------------------------------------------------------
void AptCompleteAnimationAsyncLoad(AptFilePtr* pHandle, void* pBase, AptConstFile* pConstFile,
                                   void* pAptDataHeader, void* pPreResolvedRoot)
{
    // Pin the handle for the duration of the completion (asm's leading lwarx/stwcx. IncRef).
    if (pHandle->pData)
        AptSharedPtrIncRef(pHandle->pData);

    // Forward to the loader's CompleteLoad. The by-value AptFilePtr the callee consumes is a
    // copy of *pHandle (the asm passes v15 = a copy of *a1).
    AptFilePtr laHandle;
    laHandle.pData = pHandle->pData;
    gpAptTarget->mpLoader->CompleteLoad(laHandle, pBase, pConstFile, pAptDataHeader, pPreResolvedRoot);

    // Drop the pin (asm's trailing lwarx/stwcx. DecRef + delete-if-zero), and null the handle.
    AptFile* pConsumed = pHandle->pData;
    pHandle->pData = nullptr;
    if (pConsumed && AptSharedPtrDecRef(pConsumed) == 0)
        AptSharedPtrDelete(pConsumed);
}

// AllImportsAvailable @0x82AEB270 -- true when every import referenced by `file`'s
// movie has finished loading (AptLoader::IsLoaded accepts it). Consumes the passed
// handle (the by-value AptFilePtr argument's teardown). FLAG: AptMovieData is the
// homed-but-file-local view of the loaded .apt root (AptFile.cpp).
bool AptLoader::AllImportsAvailable(AptFilePtr file)
{
    bool bAllAvailable = true;

    const AptMovieData* pMovie = static_cast<const AptMovieData*>(file.pData->mpData);
    if (pMovie->mnImportCount > 0)
    {
        for (int32_t iImport = 0; iImport < pMovie->mnImportCount; ++iImport)
        {
            // Temporary EAStringC around the import file name (ctor InitFromBuffer /
            // dtor DecreaseInternalRefCount = the asm's per-iteration bracket).
            EAStringC importName(pMovie->mpImportTable[iImport].mpImportFileName);
            AptFilePtr loaded = IsLoaded(importName);
            const bool bLoaded = (loaded.pData != nullptr);
            AptSharedPtr<AptFile>::Dispose(loaded.pData);   // drop IsLoaded's returned ref

            if (!bLoaded)
            {
                bAllAvailable = false;
                break;
            }
        }
    }

    // Consume the passed handle (by-value AptFilePtr param teardown; AptFilePtr has
    // no RAII dtor, so the release is explicit -- matching the asm).
    AptSharedPtr<AptFile>::Dispose(file.pData);
    file.pData = nullptr;
    return bAllAvailable;
}

// ---------------------------------------------------------------------------
// FLAG (un-homed engine hook, owned by class:AptCharacterAnimation's link TU):
// Link @... binds the loaded movie root into the scene -- it resolves every import
// (pulling characters from the imported movies' export tables) and finalises the
// character list. Routed through a named extern (root+16 is the embedded
// AptCharacterAnimation; the console passes the raw mpData/mpDataBlock) so the x64
// layout stays correct. Lands with the AptCharacterAnimation link follow-on.
// ---------------------------------------------------------------------------
void AptCharacterAnimation_Link(AptCharacterAnimation* pCharAnim, void* pData, void* pDataBlock);

// ---------------------------------------------------------------------------
// notify @0x82B02030 -- publish a just-linked AptFile through the global Apt
// notification hook, then consume the passed handle.
//
// The asm takes a local copy of the file (IncRef), hands that copy to the
// notification function (which takes ownership of it -- note the asm never DecRefs
// the local), then releases the passed-in *pFile (DecRef + delete-when-zero).
// ---------------------------------------------------------------------------
// FLAG (un-homed global): GlobalNotificationFunction @... is the Apt load-event
// callback registered by the host (the loader's "file linked" notification). It
// takes the AptFilePtr by address and consumes that reference. Modelled as an
// extern hook (the registration TU is a follow-on).
extern void GlobalNotificationFunction(AptFilePtr* pFile);

void AptLoader::notify(AptFilePtr* pFile)
{
    // Local copy handed to the notification (its reference is consumed there).
    AptFilePtr local;
    local.pData = pFile->pData;
    if (local.pData)
        AptSharedPtrIncRef(local.pData);

    GlobalNotificationFunction(&local);

    // Release the passed-in handle (consume it).
    AptFile* pConsumed = pFile->pData;
    pFile->pData = nullptr;
    if (pConsumed && AptSharedPtrDecRef(pConsumed) == 0)
        AptSharedPtrDelete(pConsumed);
}

// ---------------------------------------------------------------------------
// Update @0x82B020C8 -- per-frame loader tick. Walk the weak list; for each file,
// advance it by its load state. Re-run the whole pass while any file advanced
// (a freshly-linked import may unblock another file's AllImportsAvailable).
// ---------------------------------------------------------------------------
void AptLoader::Update()
{
    MutexAptLoader.Lock();

    bool bAnyAdvanced;
    do
    {
        AptLoaderNode* node = mpHead;
        bAnyAdvanced = false;

        while (node)
        {
            while (node)
            {
                // Take a counted reference on the node's file for the duration of
                // the state handling (the node is weak).
                AptFile* pFile = node->mpFile;
                if (pFile)
                    AptSharedPtrIncRef(pFile);

                const int32_t nState = pFile->mnState;   // [c:+0x08]
                if (nState == 1)
                {
                    // Requested: kick off the async stream. state -> 2 (loading),
                    // field12 records the previous state (1).
                    AptFilePtr handle;
                    handle.pData       = node->mpFile;
                    pFile->mnField12   = 1;              // [c:+0x0C] previous state
                    pFile->mnState     = 2;             // [c:+0x08] loading
                    AptSharedPtrIncRef(pFile);          // handle's reference
                    // FLAG: dword_8324E838 -- the async-load-start hook (extern).
                    AptLoader_StartAsyncLoad(pFile->mFileName.c_str(), &handle);
                    node = mpHead;                       // restart from the (possibly mutated) head
                    // Drop the per-iteration ref (asm tail @0x82B02218: unconditional
                    // DecRef + delete-if-zero), matching every other state branch.
                    if (AptSharedPtrDecRef(pFile) == 0)
                        AptSharedPtrDelete(pFile);
                }
                else if (nState == 2)
                {
                    // Loading: nothing to do this frame -- just drop our reference.
                    if (AptSharedPtrDecRef(pFile) == 0)
                        AptSharedPtrDelete(pFile);
                    break;
                }
                else if (nState == 3)
                {
                    // Data has arrived. Block until every import is available.
                    AptFilePtr handle;
                    handle.pData = node->mpFile;
                    AptSharedPtrIncRef(pFile);           // handle's reference
                    if (!AllImportsAvailable(handle))    // consumes `handle`
                    {
                        if (AptSharedPtrDecRef(pFile) == 0)
                            AptSharedPtrDelete(pFile);
                        break;
                    }

                    // All imports ready: link the movie + notify. state -> 4,
                    // field12 records the previous state.
                    void* pData      = pFile->mpData;        // [c:+0x14]
                    void* pDataBlock = pFile->mpDataBlock;   // [c:+0x18]
                    bAnyAdvanced     = true;
                    pFile->mnField12 = pFile->mnState;       // [c:+0x0C]
                    pFile->mnState   = 4;                    // [c:+0x08] linked
                    // FLAG: root+16 is the embedded AptCharacterAnimation (extern Link hook).
                    AptCharacterAnimation_Link(
                        reinterpret_cast<AptCharacterAnimation*>(static_cast<char*>(pData) + 16),
                        pData, pDataBlock);

                    AptFilePtr notifyHandle;
                    notifyHandle.pData = pFile;
                    AptSharedPtrIncRef(pFile);
                    notify(&notifyHandle);                   // consumes notifyHandle

                    // Drop our state-handling reference.
                    if (AptSharedPtrDecRef(pFile) == 0)
                        AptSharedPtrDelete(pFile);
                }
                else if (nState == 4 || nState == 5 || nState == 6)
                {
                    // Already linked / live: drop our reference.
                    if (AptSharedPtrDecRef(pFile) == 0)
                        AptSharedPtrDelete(pFile);
                    break;
                }
                else
                {
                    // Any other (failed) state: unlink the file's node, then drop
                    // our reference.
                    Invalidate(node->mpFile);
                    if (AptSharedPtrDecRef(pFile) == 0)
                        AptSharedPtrDelete(pFile);
                    break;
                }
            }

            if (node)
                node = node->mpNext;
        }
    }
    while (bAnyAdvanced);

    MutexAptLoader.Unlock();
}

// ---------------------------------------------------------------------------
// CancelPreloadedAnimation @0x82AFEA40 -- cancel a preloaded movie by name. Drop
// the named file's load and, if it was already resolved (state 3..5), recurse on
// each of its imports that no other linked movie still imports.
// ---------------------------------------------------------------------------
void AptLoader::CancelPreloadedAnimation(const EAStringC& fileName)
{
    MutexAptLoader.Lock();

    AptFilePtr found = findFile(fileName);
    AptFile* pFile = found.pData;
    if (pFile)
    {
        const int32_t nState = pFile->mnState;   // [c:+0x08]
        if (nState != 1)
        {
            if (nState == 2)
            {
                // In-flight stream: abort it (only when a data block is pending).
                if (pFile->mpDataBlock)              // [c:+0x18]
                    AptLoader_CancelAsyncLoad(pFile->mpDataBlock);   // FLAG: dword_8324E83C
            }
            else if (nState == 3 || nState == 4 || nState == 5)
            {
                // Resolved: walk this movie's import table and recurse on imports
                // no other linked movie still needs.
                const AptMovieData* pMovie = static_cast<const AptMovieData*>(pFile->mpData);   // [c:+0x14]
                if (pMovie->mnImportCount > 0)                                                  // [c:+0x30]
                {
                    for (int32_t iImport = 0; iImport < pMovie->mnImportCount; ++iImport)       // [c:+0x34]
                    {
                        EAStringC importName(pMovie->mpImportTable[iImport].mpImportFileName);
                        AptFilePtr imp = findFile(importName);

                        AptFile* pImp = imp.pData;
                        if (pImp)
                        {
                            AptSharedPtrIncRef(pImp);
                            AptFilePtr candidate;
                            candidate.pData = pImp;
                            // AptLinker_isFileImported consumes `candidate`. If no
                            // linked movie still imports it, recurse to cancel it.
                            if (!AptLinker_isFileImported(gpAptTarget->mpLinker, &candidate))   // off_8324E574->mpLinker
                            {
                                const AptMovieData* pSelfMovie =
                                    static_cast<const AptMovieData*>(pFile->mpData);
                                EAStringC recurseName(pSelfMovie->mpImportTable[iImport].mpImportFileName);
                                CancelPreloadedAnimation(recurseName);
                            }
                        }

                        // Release the findFile reference on this import.
                        if (pImp && AptSharedPtrDecRef(pImp) == 0)
                            AptSharedPtrDelete(pImp);
                    }
                }
            }
        }

        // Release the findFile reference on the named file.
        if (AptSharedPtrDecRef(pFile) == 0)
            AptSharedPtrDelete(pFile);
    }

    MutexAptLoader.Unlock();
}

// ---------------------------------------------------------------------------
// ~AptLoader @0x82AFF958 -- teardown. Cancel every still-registered preloaded
// animation (which may unlink its own node), then drain the weak list.
// ---------------------------------------------------------------------------
AptLoader::~AptLoader()
{
    MutexAptLoader.Lock();

    while (mpHead)
    {
        AptLoaderNode* node = mpHead;
        CancelPreloadedAnimation(node->mpFile->mFileName);   // v3+4 = &mpFile->mFileName

        // CancelPreloadedAnimation may have unlinked this node already (via the
        // file's ~AptFile -> Invalidate). Only erase it ourselves if it is still
        // the head.
        if (node == mpHead)
        {
            AptLoaderNode* next = mpHead->mpNext;
            gpNonGCPoolManager->Deallocate(mpHead, sizeof(AptLoaderNode));
            mpHead = next;
        }
    }

    MutexAptLoader.Unlock();

    // Drain any remaining nodes (PopFront until empty).
    while (mpHead)
    {
        AptLoaderNode* next = mpHead->mpNext;
        gpNonGCPoolManager->Deallocate(mpHead, sizeof(AptLoaderNode));
        mpHead = next;
    }
}
