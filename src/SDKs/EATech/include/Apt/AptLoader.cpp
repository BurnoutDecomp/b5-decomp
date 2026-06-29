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
#include "SDKs/EATech/Apt/DogmaAllocator.h"        // DOGMA_PoolManager::Allocate/Deallocate

#include "eathread/eathread_mutex.h"               // EA::Thread::Mutex

#include <new>       // placement new (init the AptFile's EAStringC member in pool memory)
#include <cstring>   // strlen

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

// CompleteLoad @0x80EF2C -- a streamed .apt has arrived; resolve + publish it.
void AptLoader::CompleteLoad(AptFilePtr filePtr, void* pBase, AptConstFile* pConstFile, void* pBlock)
{
    if (!pBase)
        return;

    // The data root, and the movie's AptCharacterAnimation embedded at root+16.
    // FLAG: the console relocates the offset in place in the 32-bit file slot;
    // x64 computes the absolute address and stores a 64-bit pointer in the AptFile.
    void* pRoot = static_cast<char*>(pBase) + pConstFile->mnDataRootOffset;
    AptCharacterAnimation* pCharAnim =
        reinterpret_cast<AptCharacterAnimation*>(static_cast<char*>(pRoot) + 16);

    pCharAnim->Resolve(pBase, pConstFile, pBlock);

    AptFile* f = filePtr.pData;
    f->mpDataBlock      = pBlock;
    f->mpData           = pRoot;
    f->mpResolveContext = pBase;
    f->mnField12        = f->mnState;   // record the previous state
    f->mnState          = 3;            // loaded / resolved

    // FLAG: the console then notifies/frees through the load hook (dword_1059C670).
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
