#pragma once

// ===========================================================================
// EATech Apt -- AptLoader: the movie load registry / dedup + async request gate.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (full mangled Apt symbol table),
// cross-checked vs X360 ARTIST:
//   AptLoader::Load        @0x80CFF4   register-or-dedup a movie load request
//   AptLoader::findFile    @0x7FBAA0   walk the list by (normalised) file name
//   AptLoader::IsLoaded    @0x80D2C0   findFile + state==4||5 check
//   AptLoader::Invalidate  @0x7F2AE4   unlink an AptFile's node from the list
//
// The loader keeps a singly-linked list of weak {AptFile*, next} nodes (head at
// AptLoader+0). The nodes do NOT hold a counted reference -- AptFile's shared
// count is owned by the AptSharedPtr<AptFile>s handed out by Load; when the last
// one dies, ~AptFile calls back into Invalidate to unlink its (now-dangling)
// node. All list mutations are bracketed by the global MutexAptLoader.
//
// This commit reconstructs the REQUEST layer. The async load-COMPLETION path
// (Update/notify/CompleteLoad/GetFileVector/AllImportsAvailable -> the .apt
// parser -> the loaded AptData) is a follow-on; until it lands, a Load'd AptFile
// stays in the "requested" state (mnState==1) and IsLoaded returns null.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // AptSharedPtr<AptFile> / AptFilePtr
#include "SDKs/EATech/include/Apt/AptFile.h"

namespace EA { namespace Thread { class Mutex; } }

// The global lock guarding the loader's list (one instance for the whole Apt
// system). Defined in AptLoader.cpp.
extern EA::Thread::Mutex MutexAptLoader;

// A weak node in the loader's loaded-file list: {file, next}. Pool-allocated
// (console: 8 bytes = 2 x 32-bit).
struct AptLoaderNode
{
    AptFile*       mpFile;
    AptLoaderNode* mpNext;
};

// ---------------------------------------------------------------------------
// FLAG (homed by the AptTarget TU, not yet reconstructed): ~AptFile unregisters
// itself through the *current* target's loader. GetTarget() is the per-thread
// current AptTarget (TLS, _Z9GetTargetv @0x7E49B8); AptTarget_GetLoader returns
// the AptLoader the target holds (console: *(target+28)) -- routed through an
// accessor rather than the literal offset so the x64 layout stays correct. Both
// are extern until the AptTarget TU lands; during bring-up GetTarget() yields
// null and the target-driven unregister is skipped (the AptFile still releases
// its name + any loaded data).
// ---------------------------------------------------------------------------
struct AptTarget;
AptTarget* GetTarget();
struct AptLoader;
AptLoader* AptTarget_GetLoader(AptTarget* pTarget);

struct AptLoader
{
    // +0: head of the weak loaded-file list. (Other members -- the async request
    // queue / file vector used by Update/notify/CompleteLoad -- are added as the
    // completion path is reconstructed; the request-layer functions touch only
    // this one.)
    AptLoaderNode* mpHead;

    // @0x80CFF4 -- return the shared file handle for fileName, registering a new
    // (requested) AptFile if one is not already in the list.
    AptFilePtr Load(const EAStringC& fileName);

    // @0x7FBAA0 -- find the (already-registered) shared handle for fileName, or a
    // null AptFilePtr.
    AptFilePtr findFile(const EAStringC& fileName);

    // @0x80D2C0 -- like findFile, but returns null unless the file has finished
    // loading (mnState == 4 || 5).
    AptFilePtr IsLoaded(const EAStringC& fileName);

    // @0x82AEB270 -- true when every import referenced by `file`'s loaded movie has
    // itself finished loading (each import name passes IsLoaded). Consumes the passed
    // handle (the by-value AptFilePtr's release). Body in AptLoader.cpp.
    bool AllImportsAvailable(AptFilePtr file);

    // @0x7F2AE4 -- unlink the node owning pFile from the list (called by ~AptFile).
    void Invalidate(AptFile* pFile);

    // @0x80EF2C (X360 @0x82AFF9E8) -- the async load-completion: a streamed .apt blob
    // (pBase) + its const-file header (pConstFile) + the AptDataHeader (pBlock == a5)
    // have arrived for filePtr. Relocate the movie root, Resolve it, and publish it into
    // the AptFile (state -> loaded). pBlock is the AptDataHeader (X360 a5): it flows all
    // the way to Fixup so the case-1 pfnLoadRenderingUnit reads AptData+12 (the geometry).
    //
    // pPreResolvedRoot: // FLAG (x64 converted bundle). The console derives the movie root
    // as pBase + pConstFile->mnDataRootOffset (def base at root+16 for the 4-byte format).
    // Our converted 8-byte .apt's dataRootOffset does NOT locate the type-9 movie root (its
    // console layout diverged), so the host pre-locates the root CHARACTER HEADER (via the
    // 0x09876543 signature scan) and passes it here; when null the faithful console formula
    // is used. The def base (the AptCharacterAnimation) is root + the pointer-size header
    // (0x20 native-8 / 0x10 console-4).
    void CompleteLoad(AptFilePtr filePtr, void* pBase, struct AptConstFile* pConstFile,
                      void* pBlock, void* pPreResolvedRoot = nullptr);

    // @0x82B020C8 -- per-frame loader tick: walk the weak list and advance each
    // AptFile by its load state -- kick off the async stream for freshly-requested
    // files (state 1 -> 2), and for files whose data has arrived and whose imports
    // are all available (state 3) Link the movie + notify (-> state 4). Re-runs the
    // pass while any file advanced. Body in AptLoader.cpp.
    void Update();

    // @0x82B02030 -- publish a just-linked AptFile to the global Apt notification
    // hook, then consume the passed handle. Body in AptLoader.cpp.
    void notify(AptFilePtr* pFile);

    // @0x82AFEA40 -- cancel a preloaded movie animation by file name: drop its load
    // (and, recursively, any of its imports that no other linked movie still
    // imports). Body in AptLoader.cpp.
    void CancelPreloadedAnimation(const EAStringC& fileName);

    // @0x82AFF958 -- teardown: cancel every still-registered preloaded animation,
    // then drain the weak list. Body in AptLoader.cpp.
    ~AptLoader();
};

// ---------------------------------------------------------------------------
// AptCompleteAnimationAsyncLoad @0x82AFFD38 -- the async-completion glue the
// AptCallbackFile::LoadAnimation host callback calls once a movie's .apt data is in
// memory. Pins the handle, forwards to gpAptTarget->mpLoader->CompleteLoad(handle,
// pBase, pConstFile, pAptDataHeader), then drops the pin. Body in AptLoader.cpp.
// pAptDataHeader is the AptDataHeader (X360 a4 -> CompleteLoad a5 -> Resolve/Fixup a4).
// pPreResolvedRoot is the x64 converted-bundle root-header override (FLAG; default null).
struct AptConstFile;
void AptCompleteAnimationAsyncLoad(AptFilePtr* pHandle, void* pBase, struct AptConstFile* pConstFile,
                                   void* pAptDataHeader, void* pPreResolvedRoot = nullptr);
