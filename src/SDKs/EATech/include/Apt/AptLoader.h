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

    // @0x80EF2C -- the async load-completion: a streamed .apt blob (pBase) + its
    // header (pConstFile) + the raw block (pBlock) have arrived for filePtr.
    // Resolve the movie root and publish it into the AptFile (state -> loaded).
    void CompleteLoad(AptFilePtr filePtr, void* pBase, struct AptConstFile* pConstFile, void* pBlock);

    // @0x82B0C... -- per-frame loader tick: pump the async request queue + publish
    // completed loads. FLAG: body is the loader-completion follow-on TU (the request
    // layer above); declared so AptLinker::Update (which calls it each frame) compiles.
    void Update();
};
