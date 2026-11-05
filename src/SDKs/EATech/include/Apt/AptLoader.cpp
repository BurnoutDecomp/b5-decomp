#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h" // native embedded movie accessor
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
#include "SDKs/EATech/include/Apt/AptFile.h"               // loaded AptFile
#include "SDKs/EATech/include/Apt/AptDefine.h"   // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptTarget.h"    // gpAptTarget->mpLinker (off_8324E574+0x20)
#include "SDKs/EATech/include/Apt/AptLinker.h"    // AptLinker::isFileImported (the cancel-path probe)
#include "SDKs/EATech/Apt/DogmaAllocator.h"        // DOGMA_PoolManager::Allocate/Deallocate
#include "SDKs/EATech/include/Apt/Apt.h"           // gAptFuncs.pfnFreeAnimation (the cancel path)

#include "eathread/eathread_mutex.h"               // EA::Thread::Mutex

#include <new>       // placement new (init the AptFile's EAStringC member in pool memory)
#include <cstring>   // strlen

#include "SDKs/EATech/include/Apt/AptCIH.h"   // KU_AptEmbeddedMovieOff ([aptlife] def-base probe)


// The one global lock for the loader's list. (EA::Thread::Mutex is recursive,
// which IsLoaded/Load rely on.)
EA::Thread::Mutex MutexAptLoader;

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
    MutexAptLoader.Lock();

    AptLoaderNode* node = mpHead;
    if (!node)
    {
        MutexAptLoader.Unlock();
        return AptFilePtr();
    }

    while (!FileNameCompare(node->mpFile->mFileName.c_str(), fileName.c_str()))
    {
        node = node->mpNext;
        if (!node)
        {
            MutexAptLoader.Unlock();
            return AptFilePtr();
        }
    }

    MutexAptLoader.Unlock();
    return AptFilePtr(node->mpFile);
}

// ---------------------------------------------------------------------------
// IsLoaded @0x80D2C0 -- findFile + accept only the fully-loaded states (4/5).
// ---------------------------------------------------------------------------
AptFilePtr AptLoader::IsLoaded(const EAStringC& fileName)
{
    AptFilePtr result;

    MutexAptLoader.Lock();
    AptFilePtr found = findFile(fileName);

    int state;
    if (found.pData && ((state = found.pData->mnState) == 4 || state == 5))
    {
        MutexAptLoader.Unlock();
        result = found;
    }
    else
    {
        MutexAptLoader.Unlock();
    }

    return result;
}

// ---------------------------------------------------------------------------
// Load @0x80CFF4 -- return the shared handle for fileName, registering a new
// requested AptFile on a miss.
// ---------------------------------------------------------------------------
AptFilePtr AptLoader::Load(const EAStringC& fileName)
{
    AptFilePtr result;

    MutexAptLoader.Lock();
    AptFilePtr found = findFile(fileName);

    if (found.pData)
    {
        // Already registered -- hand back a counted reference.
        MutexAptLoader.Unlock();
        result = found;
    }
    else
    {
        // Register a fresh AptFile in the "requested" state.
        AptFile* f = static_cast<AptFile*>(gpNonGCPoolManager->Allocate(sizeof(AptFile)));
        f->mnRefCount = 0;
        new (&f->mFileName) EAStringC(fileName);   // copy ctor: share buffer + IncreaseInternalRefCount
        f->mpResolveContext = 0;
        f->mnField12        = 1;
        f->mnState          = 1;                    // requested
        f->mpData           = 0;
        f->mpDataBlock      = 0;
        AptFilePtr requested(f);

        // Push a weak node onto the list head.
        AptLoaderNode* n = static_cast<AptLoaderNode*>(gpNonGCPoolManager->Allocate(sizeof(AptLoaderNode)));
        n->mpFile = f;
        n->mpNext = mpHead;
        mpHead    = n;

        MutexAptLoader.Unlock();

        result = requested;
    }
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
//   dword_8324E840(a4);                                   // the host load-complete hook (leaf note at the call site below)
//   dispose(*a2);
//
// pBlock (a5) is the AptDataHeader: it is what f->mpDataBlock is set to AND what flows to
// Resolve->Fixup so the case-1 pfnLoadRenderingUnit reads AptData+12 (the geometry object).
void AptLoader::CompleteLoad(AptFilePtr filePtr, void* pBase, AptConstFile* pConstFile,
                             void* pBlock)
{
    if (!pBase)
        return;

    if (pConstFile->mnDataRootOffset != 0)
        pConstFile->mnDataRootOffset += reinterpret_cast<uintptr_t>(pBase);
    void* pRoot = reinterpret_cast<void*>(pConstFile->mnDataRootOffset);
    const unsigned int luHdrSize = 16u;

    AptCharacterAnimation* pCharAnim =
        reinterpret_cast<AptCharacterAnimation*>(static_cast<char*>(pRoot) + luHdrSize);

    // (The XB1's `mov qword ptr [r10+50h], 0` appears in ITS CompleteLoad only because that
    // build INLINED Resolve into it. The X360 CompleteLoad @0x82AFF9E8 has no such store --
    // it is the first thing Resolve @0x82AFF5E0 does, and that is where we keep it, so the
    // def+0x50 accumulator has exactly one writer.)

    // Resolve the (serialised) movie root against the load base. a5 (pBlock == the
    // AptDataHeader) is threaded through Resolve -> Fixup faithfully.
    pCharAnim->Resolve(pBase, pConstFile, pBlock);

    AptFile* f = filePtr.pData;
    f->mpResolveContext = pBase;    // f[4]  (+0x10)  = pBase
    f->mpDataBlock      = pBlock;   // f[6]  (+0x18)  = a5 (the AptDataHeader)  <-- item 2 fix
    f->mpData           = pRoot;    // f[5]  (+0x14)  = the movie root
    f->mnField12        = f->mnState;   // record the previous state
    f->mnState          = 3;            // loaded / resolved

    if (pConstFile->mnDataRootOffset != 0)
        pConstFile->mnDataRootOffset -= reinterpret_cast<uintptr_t>(pBase);

    gAptFuncs.pfnFreeConstantTable(pConstFile);
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
// ---------------------------------------------------------------------------
void AptCompleteAnimationAsyncLoad(AptFilePtr handle, void* pBase, AptConstFile* pConstFile,
                                   void* pAptDataHeader)
{
    gpAptTarget->mpLoader->CompleteLoad(handle, pBase, pConstFile, pAptDataHeader);
}

// AllImportsAvailable @0x82AEB270 -- true when every import referenced by `file`'s
// movie has finished loading (AptLoader::IsLoaded accepts it). Consumes the passed
// handle (the by-value AptFilePtr argument's teardown).
bool AptLoader::AllImportsAvailable(AptFilePtr file)
{
    bool bAllAvailable = true;

    const AptCharacterAnimation* pMovie = AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(file.pData->mpData));
    if (pMovie->mnImportCount > 0)
    {
        for (int32_t iImport = 0; iImport < pMovie->mnImportCount; ++iImport)
        {
            // Temporary EAStringC around the import file name (ctor InitFromBuffer /
            // dtor DecreaseInternalRefCount = the asm's per-iteration bracket).
            EAStringC importName(pMovie->mpImportTable[iImport].mpImportFileName);
            const bool bLoaded = (IsLoaded(importName).pData != 0);

            if (!bLoaded)
            {
                bAllAvailable = false;
                break;
            }
        }
    }

    return bAllAvailable;
}

// ---------------------------------------------------------------------------
// notify @0x82B02030 -- publish a just-linked AptFile through the global Apt
// notification hook, then consume the passed handle.
//
// The asm takes a local copy of the file (IncRef), hands that copy to the
// notification function (which takes ownership of it -- note the asm never DecRefs
// the local), then releases the passed-in *pFile (DecRef + delete-when-zero).
// ---------------------------------------------------------------------------
// GlobalNotificationFunction @0x82B00C78 -- the Apt load-event callback (the
// loader's "file linked" notification into the current target's linker pending
// list). Defined in AptLinker.cpp.
extern void GlobalNotificationFunction(AptFilePtr file);

void AptLoader::notify(AptFilePtr file)
{
    GlobalNotificationFunction(file);
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
                AptFilePtr file(node->mpFile);
                AptFile* pFile = file.pData;

                const int32_t nState = pFile->mnState;   // [c:+0x08]
                if (nState == 1)
                {
                    // Requested: kick off the async stream. state -> 2 (loading),
                    // field12 records the previous state (1).
                    pFile->mnField12   = 1;              // [c:+0x0C] previous state
                    pFile->mnState     = 2;             // [c:+0x08] loading
                    // dword_8324E838 -- the async-load-start hook.
                    gAptFuncs.pfnLoadAnimation(pFile->mFileName.c_str(), file);
                    node = mpHead;                       // restart from the (possibly mutated) head
                }
                else if (nState == 2)
                {
                    // Loading: nothing to do this frame -- just drop our reference.
                    break;
                }
                else if (nState == 3)
                {
                    // Data has arrived. Block until every import is available.
                    if (!AllImportsAvailable(file))
                        break;

                    // All imports ready: link the movie + notify. state -> 4,
                    // field12 records the previous state.
                    void* pData      = pFile->mpData;        // [c:+0x14]
                    void* pDataBlock = pFile->mpDataBlock;   // [c:+0x18]
                    bAnyAdvanced     = true;
                    pFile->mnField12 = pFile->mnState;       // [c:+0x0C]
                    pFile->mnState   = 4;                    // [c:+0x08] linked
                    AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(pData))->Link(pData, pDataBlock);
                    notify(file);
                }
                else if (nState == 4 || nState == 5 || nState == 6)
                {
                    // Already linked / live: drop our reference.
                    break;
                }
                else
                {
                    // Any other (failed) state: unlink the file's node, then drop
                    // our reference.
                    Invalidate(node->mpFile);
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

    {
        AptFilePtr found = findFile(fileName);
        AptFile* pFile = found.pData;
        if (pFile)
        {
            const int32_t nState = pFile->mnState;   // [c:+0x08]
            if (nState != 1)
            {
                if (nState == 2)
                {
                    // In-flight stream: hand its pending data block back to the host.
                    if (pFile->mpDataBlock)              // [c:+0x18]
                        gAptFuncs.pfnFreeAnimation(pFile->mpDataBlock);
                }
                else if (nState == 3 || nState == 4 || nState == 5)
                {
                    // Resolved: walk this movie's import table and recurse on imports
                    // no other linked movie still needs.
                    const AptCharacterAnimation* pMovie = AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(pFile->mpData));   // [c:+0x14]
                    if (pMovie->mnImportCount > 0)                                                  // [c:+0x30]
                    {
                        for (int32_t iImport = 0; iImport < pMovie->mnImportCount; ++iImport)       // [c:+0x34]
                        {
                            EAStringC importName(pMovie->mpImportTable[iImport].mpImportFileName);
                            AptFilePtr imp = findFile(importName);

                            if (imp.pData && !gpAptTarget->mpLinker->isFileImported(imp))
                            {
                                const AptCharacterAnimation* pSelfMovie =
                                    AptGetMovieCharacterAnimation(static_cast<AptCharacter*>(pFile->mpData));
                                EAStringC recurseName(pSelfMovie->mpImportTable[iImport].mpImportFileName);
                                CancelPreloadedAnimation(recurseName);
                            }
                        }
                    }
                }
            }
        }
    } // found is released while the loader lock is still held.
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
