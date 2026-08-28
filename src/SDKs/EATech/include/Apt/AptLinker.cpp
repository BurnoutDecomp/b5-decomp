#include "SDKs/EATech/include/Apt/AptLinker.h"

#include <new>   // placement new for the pool-allocated AptCIH/AptCharacterInst

#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"               // gpAptTarget, mpLoader, mpAnimationTarget, mpLinker
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // GetRootDisplayList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"          // removeObject, AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"     // insert
#include "SDKs/EATech/include/Apt/AptCIH.h"                  // AptCIH ctor/new, ReplaceZombieChild, ClearCIH, SetDirtyState, ForceCleanNativeHash
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"        // AptCharacterInst, GetRenderItemWritable
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mnGotoFrame / mnClipActionFlags (the linked anim inst's sprite-base)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"       // setIsDefined, setGCRoot
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"    // getVariable
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"      // AptGetAnimationAtLevel
#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"// gpAptSavedInputCheckpoints, CanLinkPendingFiles, AllLinked
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // GetParentAnim (the ReplaceReferences owner tests)
#include "SDKs/EATech/include/Apt/AptNativeHash.h"           // gpAptClassRegistry->RegisterReferences
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"           // AptValueGC_PoolManager (the live-value walk)
#include "SDKs/EATech/include/Apt/AptDefine.h"               // gpGCPoolManager (off_8324D834)
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // [aptlife] CgsDev::Log::WriteToLog
#include <cstdio>                                             // [aptlife] snprintf

// =====================================================================
//  Sibling-owned callees/globals referenced below; declared here so this TU
//  links against their single homes (noted per symbol).
// =====================================================================

// The shared Apt action interpreter instance (X360 &dword_8324E760). Defined in
// AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;            // &dword_8324E760

// @0x82AD85D8 -- the case-sensitive EAStringC equality test (the ICF-folded
// EAStringC::operator== twin, called here by its IDA name: length@+2 compare then
// a bytewise buffer scan). HOMED in AptString/EAString.cpp.
bool Burnout_X360_Artist_0040_0(EAStringC* pName, EAStringC* pReference);

// FLAG (un-homed AptCIH behavioural surface): set the character instance on a CIH
// (@0x82B00548) and tick it (@0x82B0BED8). Declared so Update/ConvertToZombie
// compile; bodies in the AptCIH behavioural cluster.
// AptCIH::SetCharacterInst is now called directly as a member (AptCIH.h is included).
// AptCIH::tick is now called directly as a member (AptCIH.h is included).

// @0x82AE4DF0 / PS3 _Z17ReplaceReferencesP8AptValueS0_PS0_i --
// ReplaceReferences(pOld, pNew, ppTable, nCount): retarget every live reference
// from pOld to pNew across the value graph. HOMED at the bottom of this TU (with
// the ReferenceReplaceCb / AptRegisterGlobalReferences machinery it drives).
int ReplaceReferences(AptValue* pOld, AptValue* pNew, AptValue** ppTable, int nCount);

// AptAnimationTarget::RunActions @0x82B0C9B0 -- flush the queued deferred ActionScript
// actions for the director; now called directly on the member (the AptAnimationTarget_RunActions shim is retired).

// The X360 reads/writes the int16 placed depth at AptRenderItem+0x14 and dispatches
// a vtbl slot (+0x14 == CopyRenderDataFrom) to copy visual state during a zombie
// swap; the reconstructed C++ calls the named AptRenderItem members directly
// (GetDepth()/SetDepth()/CopyRenderDataFrom()).

// dword_8324D7F0 -- nonzero when the saved-input record/replay system is active
// (gates Update's checkpoint logic). Defined in AptGlobals.cpp (0 = replay off,
// the shipped default).
extern int gbAptSavedInputActive;                              // dword_8324D7F0

// The intrusive refcount on AptLinkerThingy (mirroring AptSharedPtrIncRef/DecRef
// for AptFile -- the thingy carries its count at +0x00). Bodies BELOW in this TU
// (the out-of-line helper section).
int  AptSharedPtrIncRefThingy(AptLinkerThingy* pThingy);
int  AptSharedPtrDecRefThingy(AptLinkerThingy* pThingy);

// The singly-linked thingy-list ops (the compiler-emitted SingleList<>); bodies
// below in this TU:
//   ListPushFront == AptSingleLis @0x82AF4F88 (alloc 8-byte node, AddRef thingy, link head)
//   ListErase     == sub_82AF83A0 (find+unlink the node, release its thingy, free the node)
void ListPushFront(AptSingleListNode** ppHead, AptLinkerThingy* pThingy);
void ListErase(AptSingleListNode** ppHead, AptSingleListNode** ppNode);

// Pool-allocate-and-construct factories (now HOMED next to their classes; also
// declared in the corresponding headers this TU includes -- re-declared here for the
// address documentation). MakeLinkerThingy -> AptLinkerThingy.cpp (ctor @0x82ADBF58);
// MakeAptFile -> AptFile.cpp (AptFile::AptFile @0x82AE6268, a2 = the .apt name);
// MakeCharacterAnimationInst -> AptCharacterAnimationInst.cpp (ctor @0x82AFFDE8).
AptLinkerThingy*           MakeLinkerThingy(AptFilePtr& rFile, AptValue* pValue);   // ctor @0x82ADBF58
AptFile*                   MakeAptFile(void* pMem, EAStringC* pName);               // AptFile::AptFile @0x82AE6268 (a2 = the .apt name)
AptCharacterAnimationInst* MakeCharacterAnimationInst(AptFile* pFile);             // ctor @0x82AFFDE8
void InstallEmptyCharacterInstVtbl(AptCharacterInst* pInst);                        // *inst = &off_82145FD0
void EnsureAnimFrameRateCached(AptCharacterAnimationInst* pInst);                   // dword_8324E530 lazy init
void FireLinkNotifyCallbacks(AptFile* pFile, AptCIH* pCIH);                         // dword_8324E844/E830 hooks


// ---------------------------------------------------------------------
// AptLinker::AptLinker -- inline body emitted inside AptTarget::AptTarget
// @0x82B00160 (head=null; mPendingFiles is an empty SBO vector pointing at its
// own inline buffer). Reproduced here as the standalone ctor.
// ---------------------------------------------------------------------
AptLinker::AptLinker()
{
    mpThingyListHead          = nullptr;       // v7[0] = 0
    mPendingFiles.mnSize      = 0;             // v7[1] = 0
    mPendingFiles.mnCapacity  = 0;             // v7[2] = 0
    mPendingFiles.mpData      = mPendingFiles.mInlineStorage;   // v7[3] = v7 + 4 (-> +0x10)
    mPendingFiles.mInlineStorage[0].pData = nullptr;            // v7[4] = 0
    mPendingFiles.mInlineStorage[1].pData = nullptr;            // v7[5] = 0
}

// ---------------------------------------------------------------------
// AptLinker::ScalarDeletingDestructor -- @0x82B00D50.
//   Strin(this+1)            -> mPendingFiles (BasicString-as-vector) dtor
//   while(*this) sub_82AF50B8(this)  -> pop_front the thingy list until empty
//   if(flags&1) Deallocate(pool, this, 24)
// ---------------------------------------------------------------------
void* AptLinker::ScalarDeletingDestructor(char flags)
{
    // ~PendingFileVector: release every owned AptFilePtr + free any heap block.
    // (The X360 `Strin(a1+1)` is the BasicString<...> dtor over this+4.)
    mPendingFiles.~PendingFileVector();

    // Drain the singly-linked thingy list: pop_front releases each node's counted
    // thingy ref (and destroys the thingy at count 0).
    while (mpThingyListHead != nullptr)
    {
        AptSingleListNode* pHead = mpThingyListHead;          // result = *a1
        AptSingleListNode* pNext = pHead->mpNext;             // v3 = *(result+4)
        // node dtor (AptSingleL): release node[0] -> at 0, AptLinkerThingy dtor;
        // then free the 8-byte node.
        AptLinkerThingy* pThingy = pHead->mpThingy;
        if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
            pThingy->ScalarDeletingDestructor(1);
        gpAptSharedPtrPool->Deallocate(pHead, sizeof(AptSingleListNode));
        mpThingyListHead = pNext;                             // *a1 = v3
    }

    if (flags & 1)
        gpAptSharedPtrPool->Deallocate(this, sizeof(AptLinker));   // Deallocate(pool,this,0x18)

    return this;
}

// ---------------------------------------------------------------------
// AptLinker::Notify -- @0x82B00938.
//   Scan mPendingFiles for an entry whose pData == pFile->pData. If found, just
//   drop the incoming ref (it is already pending). If not found, push_back it
//   (sub_82B00498 == vector<AptFilePtr>::Insert at end), which takes the ref.
//   Either way the caller's *pFile is consumed (zeroed + released).
// ---------------------------------------------------------------------
void AptLinker::Notify(AptFilePtr* pFile)
{
    for (AptFilePtr* pEntry = mPendingFiles.begin(); pEntry != mPendingFiles.end(); ++pEntry)
    {
        if (pEntry->pData == pFile->pData)        // *v4 == *a2
        {
            // Already pending: just release the incoming reference (consume *a2).
            AptFile* pData = pFile->pData;        // result = *a2
            pFile->pData   = nullptr;             // *a2 = 0
            if (pData != nullptr && AptSharedPtrDecRef(pData) == 0)
                AptSharedPtrDelete(pData);
            return;
        }
    }

    // Not pending: append (the vector takes ownership of the ref)...
    mPendingFiles.PushBack(*pFile);               // sub_82B00498
    // ...then release the caller's now-transferred reference (consume *a2).
    {
        AptFile* pData = pFile->pData;
        pFile->pData   = nullptr;
        if (pData != nullptr && AptSharedPtrDecRef(pData) == 0)
            AptSharedPtrDelete(pData);
    }
}

// ---------------------------------------------------------------------
// GlobalNotificationFunction -- @0x82B00C78 (the loader's "file linked" hop).
//   AptLoader::notify hands every just-linked AptFile here; the function takes a
//   local reference, resolves the CURRENT target off the Apt target TLS
//   (unk_8324E814 -> value+0x20 == mpLinker), and forwards the handle into that
//   target's linker pending list (AptLinker::Notify -- which consumes the local
//   reference). The saved-input debug arm (dword_8324D7F0 gate ->
//   AptSavedInputCheckpoints::updateState(...)) records the link event for
//   deterministic replay; the checkpoint recorder's updateState is un-homed, so
//   that arm is FLAG'd (inactive at the boot default gbAptSavedInputActive == 0).
//   The caller's *pFile is consumed (zeroed + released), matching the asm tail.
// ---------------------------------------------------------------------
namespace EA { namespace Thread { class ThreadLocalStorage
{
public:
    bool  SetValue(const void* pData);
    void* GetValue();
private:
    unsigned int mTlsIndex;
}; } }
extern EA::Thread::ThreadLocalStorage gAptTargetTls;   // unk_8324E814

void GlobalNotificationFunction(AptFilePtr* pFile)
{
    // Local IncRef copy -- AptLinker::Notify consumes it.
    AptFilePtr local;
    local.pData = pFile->pData;
    if (local.pData != nullptr)
        AptSharedPtrIncRef(local.pData);

    AptTarget* pTarget = static_cast<AptTarget*>(gAptTargetTls.GetValue());
    if (pTarget != nullptr && pTarget->mpLinker != nullptr)
        pTarget->mpLinker->Notify(&local);

    if (gbAptSavedInputActive)
    {
        // FLAG (un-homed saved-input recorder): the console records the link event
        // (AptSavedInputCheckpoints::updateState(gpAptSavedInputCheckpoints,
        // &file->mFileName, 1, 3, 2)); inactive on the boot path (gate default 0).
    }

    // Consume the caller's handle (asm tail: *a1 = 0 + DecRef/delete-at-zero).
    AptFile* pConsumed = pFile->pData;
    pFile->pData = nullptr;
    if (pConsumed != nullptr && AptSharedPtrDecRef(pConsumed) == 0)
        AptSharedPtrDelete(pConsumed);
}

// ---------------------------------------------------------------------
// AptLinker::CancelLoad -- @0x82AFAE18.
//   Find the node keyed on pValue (node->mpThingy->mpValue == pValue) and
//   pop it from the list. No-op if not found.
// ---------------------------------------------------------------------
void AptLinker::CancelLoad(AptValue* pValue)
{
    AptSingleListNode* pNode = mpThingyListHead;                       // i = *result
    while (pNode != nullptr && pNode->mpThingy->mpValue != pValue)     // *(*i+8) != a2
        pNode = pNode->mpNext;                                         // i = i[1]

    if (pNode != nullptr)
        ListErase(&mpThingyListHead, &pNode);   // sub_82AF83A0(result, &v3) -- unlink + release
}

// ---------------------------------------------------------------------
// AptLinker::ConvertToZombie -- @0x82B00A18.
//   When pValue is mid-transition (CIH flags 0x60000000 == 0x20000000): find its
//   thingy, mark its file resolved, build a replacement AptCIH, splice it in for
//   the old node (via the parent's ReplaceZombieChild, or directly into the root
//   display list), fix up every reference, and drop the thingy. Otherwise set the
//   transition bits and return pValue unchanged.
// ---------------------------------------------------------------------
AptCIH* AptLinker::ConvertToZombie(AptValue* pValue)
{
    AptCIH* pCIH = static_cast<AptCIH*>(pValue);
    const uint32_t nFlags = pCIH->mFlagsA;                            // v4 = *(a2+12)

    if ((nFlags & 0x6u) != 0x2u)                                     // not transitioning (x64 CIHState bits 1-2)
    {
        pCIH->mFlagsA = nFlags | 0x6u;                               // CIHState = 3 (X360 reversed form 0x60000000)
        return pCIH;                                                  // result = a2
    }

    // Find the node keyed on pValue.
    AptSingleListNode* pNode = mpThingyListHead;
    while (pNode != nullptr && pNode->mpThingy->mpValue != pValue)    // *(*i+8) != a2
        pNode = pNode->mpNext;                                        // i = i[1]
    AptLinkerThingy* pThingy = pNode->mpThingy;                       // _R27 = *i

    // Pin the thingy across the work (it may be popped below).
    if (pThingy != nullptr)
        AptSharedPtrIncRefThingy(pThingy);

    // Resolve the owned file: bump+drop its ref (a balanced touch the X360 emits)
    // and read its state.
    AptFile* pFile = pThingy->mpFile.pData;                          // _R30 = *(_R27+4)
    if (pFile != nullptr)
    {
        AptSharedPtrIncRef(pFile);
        if (AptSharedPtrDecRef(pFile) == 0)
            AptSharedPtrDelete(pThingy->mpFile.pData);
    }

    // Mark the file "linked/zombified" (state 5) once, recording the prior state.
    if (pFile->mnState != 5)                                         // *(_R30+8) != 5
    {
        ListErase(&mpThingyListHead, &pNode);                       // sub_82AF83A0(a1,&v31)
        pFile->mnField12 = pFile->mnState;                          // *(_R30+12) = *(_R30+8)
        pFile->mnState   = 5;                                       // *(_R30+8) = 5
    }

    // Build the replacement AptCIH (parent == pValue's parent CIH).
    AptCIH* pParent = pCIH->mpDisplayListParent;                    // *(a2+28)  (== AptCIH[7])
    AptCIH* pNew    = nullptr;
    void* pMem = AptCIH::operator new(40);                          // AptCIH::operator new(0x28)
    if (pMem != nullptr)
        pNew = ::new (pMem) AptCIH(nullptr, pParent);                 // AptCIH::AptCIH(mem,0,parent)

    if (pParent != nullptr)
    {
        // Swap the new node in for the zombie within the parent's child list.
        pParent->ReplaceZombieChild(pNew, pCIH);                    // (parent, new, zombie)
        pValue->Release();                                          // (**a2)(a2) -- vtbl[?] Release/destroy step
        pValue->setIsDefined(true);                                 // AptValue::setIsDefined(a2,1)
    }
    else
    {
        // Top-level node: hand the visual state across + reseat in the root list.
        AptRenderItem* pNewItem = pNew->mpCharacterInst->GetRenderItemWritable();   // *(v20+32)
        pNewItem->CopyRenderDataFrom(pCIH->mpCharacterInst->mpRenderItem);          // (*item->vtbl[+0x14])(item, *(v22+4))
        const int16_t nDepth = pCIH->mpCharacterInst->mpRenderItem->GetDepth();     // *(*(*(a2+32)+4)+20)
        pNew->mpCharacterInst->GetRenderItemWritable()->SetDepth(nDepth);

        const int nInsertDepth = pCIH->GetDepth();                 // v24 = *(a2+20) (placed index/depth)
        AptDisplayList* pRoot = gpAptTarget->mpAnimationTarget->GetRootDisplayList();  // *(off_8324E574+6)+0x20
        pRoot->removeObject(pCIH);                                  // AptDisplayList::removeObject(root, a2)
        pValue->Release();                                          // (**a2)(a2)
        pValue->setIsDefined(true);                                 // AptValue::setIsDefined(a2,1)
        pRoot->AsState()->insert(nInsertDepth, pNew);              // AptDisplayListState::insert(root, v24, v20)
    }

    pNew->Release();                                                // (**v20)(v20)
    pValue->setGCRoot(1);                                           // v25 = AptValue::setGCRoot(a2,1)
    ReplaceReferences(pValue, pNew, 0, 0);                          // ReplaceReferences(v25, v20, 0, 0)

    // Drop our pin on the thingy; destroy at count 0.
    if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
        pThingy->ScalarDeletingDestructor(1);

    return pNew;                                                    // result = v20
}

// ---------------------------------------------------------------------
// AptLinker::Load -- @0x82B06660.
//   Resolve the loadMovie target value; if it is a movie-clip/CIH placeholder,
//   (re)load the AptFile through the loader and, when it is ready, enqueue a
//   Notify + an AptLinkerThingy linking the file to the value. Mirrors the X360
//   control flow (the lwarx/stwcx. blocks are AptFilePtr AddRef/Release, the
//   v96/v97/v98/v99 locals are temporary AptFilePtrs).
// ---------------------------------------------------------------------
void AptLinker::Load(EAStringC* pName, EAStringC* pFileName)
{
    AptCIH* pAnim = AptGetAnimationAtLevel(0);                                       // AptGetAnimationAtLevel(0)
    AptValue* pVar = gAptActionInterpreter.getVariable(pAnim, nullptr, pFileName,
                                                       /*allowSelf*/0,
                                                       /*scopeChain*/1,
                                                       /*direct*/1);                 // ...,1,1,0
    AptValue* pValue = pVar;
    // [aptlife] opt-in witness: which target this loadMovie/unloadMovie resolved to.
    // An EMPTY name is the UNMOUNT half (StateInterface::PlayAptMovie("", level)), and
    // it is exactly the arm that silently does nothing when the level variable does not
    // resolve -- so print the resolution, not just the request.
    if (AptLifeDiagEnabled())
    {
        char lac[256];
        std::snprintf(lac, sizeof(lac),
            "[aptlife] Linker::Load name='%s' target='%s' value=%p\n",
            pName ? pName->GetBuffer() : "(null)",
            pFileName ? pFileName->GetBuffer() : "(null)",
            static_cast<const void*>(pValue));
        CgsDev::Log::WriteToLog(lac);
    }
    if (pValue == nullptr)
        goto done;

    // Accept only a DEFINED CIH-handle (type 12), or a CIH-none (37). The console's
    // `(v9 >> 27) & 1` is bit 27 of the big-endian-packed value bitfield == mbIsDefined
    // (MSB-first packing: bits 31..27 = IsAllocated / HasRegisterReferenceMark /
    // IsInDeferredVector / DestroyedGC / IsDefined; the low 7 bits it masks with
    // (v9<<25)>>25 are meValueType, confirming the packing). The prior
    // mbAllowsDelayedDeletion read was bit 26 -- it rejected every level placeholder.
    {
        const AptVirtualFunctionTable_Indices eType = pValue->getVtblIndex();
        bool bAccept = false;
        if (eType == AptVFT_CharacterInstHandle &&
            pValue->getIsDefined() /* console (v9>>27)&1 == mbIsDefined */ )
            bAccept = true;
        else if (eType == AptVFT_CIHNone)
            bAccept = true;
        if (!bAccept)
            goto done;
    }

    {
        AptFilePtr   filePtr;        filePtr.pData = nullptr;     // v96 (the load result handle)
        AptFile*     pLinkedFile = nullptr;                       // _R28
        EAStringC    skipName;                                    // v97 scratch

        // (skipName is an empty RAII EAStringC; the X360 InitFromBuffer + the
        //  manual DecreaseInternalRefCount below are the dtor's job in this model.)
        const bool bSkip = Burnout_X360_Artist_0040_0(pName, &skipName);

        if (!bSkip)
        {
            // Already loaded?  -> grab its handle. The loader is keyed by the MOVIE
            // NAME (X360 a2 == pName) -- NOT the "_level%d" target string (a3), which
            // only names the destination variable resolved above.
            filePtr = gpAptTarget->mpLoader->IsLoaded(*pName);     // AptLoader::IsLoaded(&v98, loader, a2) + operator=
            pLinkedFile = filePtr.pData;
            if (pLinkedFile != nullptr)
            {
                // Pin it and notify (it is ready).
                AptSharedPtrIncRef(pLinkedFile);
                AptFilePtr notifyPtr; notifyPtr.pData = pLinkedFile;
                Notify(&notifyPtr);                                // AptLinker::Notify(a1, &v97)
            }
            else
            {
                // Not loaded yet: kick a load request (by the movie NAME, X360 a2).
                filePtr = gpAptTarget->mpLoader->Load(*pName);     // AptLoader::Load(&v99, loader, a2)
                pLinkedFile = filePtr.pData;
                // If the request just completed (state 6 -> swap state/field12 to 4),
                // notify immediately.
                if (pLinkedFile != nullptr && pLinkedFile->mnState == 6)   // v96[2] == 6
                {
                    const int32_t nNew = pLinkedFile->mnField12;           // v28 = v96[3]
                    const int32_t nOld = pLinkedFile->mnState;             // v29 = v96[2]
                    pLinkedFile->mnState   = nNew;                         // v96[2] = v28
                    pLinkedFile->mnField12 = nOld;                         // _R28[3] = v29
                    if (nNew == 4)
                    {
                        AptSharedPtrIncRef(pLinkedFile);
                        AptFilePtr notifyPtr; notifyPtr.pData = pLinkedFile;
                        Notify(&notifyPtr);                                // LABEL_24
                    }
                }
            }
        }

        // Mark the placeholder value defined + transitioning, dirty it.
        pValue->setIsDefined(true);                                        // AptValue::setIsDefined(v8,1)
        static_cast<AptCIH*>(pValue)->mFlagsA |= 0x6u;                     // CIHState = 3 (x64 bits 1-2; X360 form 0x60000000)
        static_cast<AptCIH*>(pValue)->SetDirtyState(false, false);         // AptCIH::SetDirtyState(IsDefined,0,0)

        // Drop any existing thingy keyed on this value (re-load supersedes it).
        {
            AptSingleListNode* pNode = mpThingyListHead;
            while (pNode != nullptr && pNode->mpThingy->mpValue != pValue) // *(*i+8) == v8
                pNode = pNode->mpNext;
            if (pNode != nullptr)
                ListErase(&mpThingyListHead, &pNode);                      // sub_82AF83A0(a1, &v97)
        }

        if (pLinkedFile != nullptr)
        {
            // Create + push a thingy linking pLinkedFile to pValue.
            AptLinkerThingy* pThingy = nullptr;
            {
                AptSharedPtrIncRef(pLinkedFile);
                AptFilePtr held; held.pData = pLinkedFile;
                // MakeLinkerThingy owns the 16-byte pool Allocate (the X360's
                // Allocate(off_8324D808,16) r3 threads straight into the ctor).
                pThingy = MakeLinkerThingy(held, pValue);                  // AptLinkerThingy::AptLinkerThingy(&file, value)
            }
            if (pThingy != nullptr)
                AptSharedPtrIncRefThingy(pThingy);
            ListPushFront(&mpThingyListHead, pThingy);                     // AptSingleLis(a1, &v97)
            if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
                pThingy->ScalarDeletingDestructor(1);
        }
        else
        {
            // No loaded file: synthesize a stub AptFile for unloaded/level cases.
            const uint32_t nTypeTag =
                static_cast<AptCIH*>(pValue)->mpCharacterInst->GetTypeTag();   // *(*(v8+32)+8) >> 26
            if (nTypeTag == 15)                                               // level inst -> give up
                goto consume_name;

            if (nTypeTag != 9)                                               // non-animation
            {
                AptFile* pStub = nullptr;
                // FLAG (x64 widening -- same class as the AptCharacterInst site below):
                // the console literal 28 is ITS sizeof(AptFile) (refcount + a 4-byte
                // EAStringC + state + field12 + three 4-byte pointers). The x64 record is
                // 48 bytes, so the literal let MakeAptFile's ctor write 20 bytes past the
                // block; the paired free (AptSharedPtr.cpp) already uses sizeof. Size from
                // the type.
                void* pMem = gpAptSharedPtrPool->Allocate(sizeof(AptFile));   // X360 Allocate(28)
                if (pMem != nullptr)
                    pStub = MakeAptFile(pMem, pName);                        // AptFile::AptFile(mem, a2)
                if (pStub != nullptr)
                    AptSharedPtrIncRef(pStub);
                AptFilePtr stubPtr; stubPtr.pData = pStub;
                filePtr = stubPtr;                                           // AptFile_::operator=(&v96, &v97)
                pLinkedFile = filePtr.pData;
                const int32_t nPrev = pLinkedFile->mnState;                  // v55 = v96[2]
                pLinkedFile->mnState   = 6;                                  // v96[2] = 6
                pLinkedFile->mnField12 = nPrev;                              // _R28[3] = v55

                AptLinkerThingy* pThingy = nullptr;
                if (gpAptSharedPtrPool->Allocate(16) != nullptr)
                {
                    AptSharedPtrIncRef(pLinkedFile);
                    AptFilePtr held; held.pData = pLinkedFile;
                    pThingy = MakeLinkerThingy(held, pValue);
                }
                if (pThingy != nullptr) AptSharedPtrIncRefThingy(pThingy);
                ListPushFront(&mpThingyListHead, pThingy);
                if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
                    pThingy->ScalarDeletingDestructor(1);

                if (pStub != nullptr && AptSharedPtrDecRef(pStub) == 0)
                    AptSharedPtrDelete(pStub);
            }
            else
            {
                // Animation inst: same stub path, slightly different ordering.
                AptFile* pStub = nullptr;
                // FLAG (x64 widening -- same class as the AptCharacterInst site below):
                // the console literal 28 is ITS sizeof(AptFile) (refcount + a 4-byte
                // EAStringC + state + field12 + three 4-byte pointers). The x64 record is
                // 48 bytes, so the literal let MakeAptFile's ctor write 20 bytes past the
                // block; the paired free (AptSharedPtr.cpp) already uses sizeof. Size from
                // the type.
                void* pMem = gpAptSharedPtrPool->Allocate(sizeof(AptFile));   // X360 Allocate(28)
                if (pMem != nullptr)
                    pStub = MakeAptFile(pMem, pName);
                if (pStub != nullptr) AptSharedPtrIncRef(pStub);
                pStub->mnField12 = pStub->mnState;                          // *(_R31+12) = *(_R31+8)
                pStub->mnState   = 6;                                       // *(_R31+8) = 6

                AptLinkerThingy* pThingy = nullptr;
                if (gpAptSharedPtrPool->Allocate(16) != nullptr)
                {
                    AptSharedPtrIncRef(pStub);
                    AptFilePtr held; held.pData = pStub;
                    pThingy = MakeLinkerThingy(held, pValue);
                }
                if (pThingy != nullptr) AptSharedPtrIncRefThingy(pThingy);
                ListPushFront(&mpThingyListHead, pThingy);
                if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
                    pThingy->ScalarDeletingDestructor(1);

                if (pStub != nullptr && AptSharedPtrDecRef(pStub) == 0)
                    AptSharedPtrDelete(pStub);
            }
        }

        // Drop the load handle's reference.
        if (pLinkedFile != nullptr && AptSharedPtrDecRef(pLinkedFile) == 0)
            AptSharedPtrDelete(pLinkedFile);
    }

consume_name:
done:
    // (pFileName is owned by the caller; its internal refcount is released by the
    //  caller's RAII EAStringC dtor -- the X360's manual drop here is unnecessary.)
    ;
}

// ---------------------------------------------------------------------
// AptLinker::Update -- @0x82B0CC68.
//   (1) Tick the loader. (2) Optionally gate on the saved-input checkpoint list.
//   (3) Walk the thingy list: for each thingy whose linked value is active and
//       whose file is ready (state 6), either ConvertToZombie the old node or
//       build a fresh AptCharacterInst and link it in, then drop the thingy.
//   (4) Walk mPendingFiles: for each pending file not already linked, build an
//       AptCharacterAnimationInst, attach it, mark dirty/defined, fire the
//       callbacks, and clear the thingy. (5) Clear mPendingFiles + run actions.
//   The lwarx/stwcx. blocks are AptFilePtr/AptLinkerThingy AddRef/Release; the
//   per-element refcount bookkeeping is de-optimised to balanced inc/dec here.
// ---------------------------------------------------------------------
void AptLinker::Update()
{
    gpAptTarget->mpLoader->Update();                                 // AptLoader::Update(off_8324E574->mpLoader)

    if (gbAptSavedInputActive &&
        !AptSavedInputCheckpoints::CanLinkPendingFiles(*gpAptSavedInputCheckpoints))
        return;                                                      // gate: not all pending files ready

    // ---- pass 1: resolve thingies whose files just finished loading ----
    for (AptSingleListNode* pNode = mpThingyListHead; pNode != nullptr; )
    {
        AptLinkerThingy* pThingy = pNode->mpThingy;                 // _R28 = *v5
        if (pThingy != nullptr) AptSharedPtrIncRefThingy(pThingy);

        bool bReady = false;
        if (pThingy->mpValue != nullptr)                            // *(_R28+8) != 0 (active)
        {
            AptFile* pFile = pThingy->mpFile.pData;                 // _R27 = *(_R28+4)
            if (pFile != nullptr) AptSharedPtrIncRef(pFile);
            bReady = (pFile != nullptr && pFile->mnState == 6);     // *(_R27+8) == 6
            if (pFile != nullptr && AptSharedPtrDecRef(pFile) == 0)
                AptSharedPtrDelete(pFile);
        }

        if (bReady)
        {
            AptCIH* pCIH = static_cast<AptCIH*>(pThingy->mpValue);          // v24 = *(_R28+8)
            const int16_t nDepth =
                pCIH->mpCharacterInst->mpRenderItem->GetDepth();            // v25 = *(*(*(v24+32)+4)+20)
            pCIH->mFlagsA &= ~0x6u;                                        // clear transition bits (x64 CIHState bits 1-2)
            pCIH->ClearCIH(false);                                         // AptCIH::ClearCIH(v24,0)

            if ((pCIH->mFlagsA & 0x6u) == 0x2u)                           // still transitioning
            {
                AptCIH* pZombie = ConvertToZombie(pCIH);                   // AptLinker::ConvertToZombie(a1, v24)
                pZombie->mpCharacterInst->GetRenderItemWritable()->SetDepth(nDepth);
            }
            else
            {
                AptCharacterInst* pInst = nullptr;
                // x64 widening (settled; Phase-0 native-8 rule): the console allocates its literal sizeof
                // (16 == 4 dwords: vtable / mpRenderItem / mTypeFlags / mpProperties).
                // On x64 the same record is 32 bytes, so the literal under-allocated by
                // half and the ctor's `mTypeFlags` read-modify-write ran PAST the block
                // -- straight onto the AptRenderItemLevel the very next pool carve hands
                // back inside that same ctor, clobbering byte 3 of its vtable pointer
                // (`and byte ptr [..+3],3` / `or byte ptr [..+3],3Ch` == the
                // `(mTypeFlags & 0x03FFFFFF) | (15 << 26)` stamp for a null character).
                // The AV then fired one frame later in AptRenderLeaf's `Render` dispatch.
                // The matching free (AptCIH.cpp / AptCIHBehaviour.cpp) already uses
                // sizeof, so the literal also returned the block to the wrong free-list
                // bucket. Size from the type.
                void* pMem = gpAptSharedPtrPool->Allocate(sizeof(AptCharacterInst));   // X360 Allocate(16)
                if (pMem != nullptr)
                {
                    pInst = ::new (pMem) AptCharacterInst(nullptr);          // AptCharacterInst::AptCharacterInst(v27,0)
                    // *v28 = &off_82145FD0 : install the concrete vtable (the
                    // empty-placeholder AptCharacterInst vtable -- modelled by the
                    // placement-new vtable install; see the helper below).
                    InstallEmptyCharacterInstVtbl(pInst);
                }
                pCIH->SetCharacterInst(pInst, true);                       // AptCIH::SetCharacterInst(v24, v29, 1)
                pCIH->mpCharacterInst->GetRenderItemWritable()->SetDepth(nDepth);
                ListErase(&mpThingyListHead, &pNode);                     // sub_82AF83A0(a1, &v82) -- v82 == the CURRENT node
            }
            pNode = mpThingyListHead;                                      // v5 = *a1 (the console RELOADS the head after handling; the handled node was just erased)
            if (pNode == nullptr)
                break;
        }

        // Release + (at 0) destroy this thingy, then advance.
        AptSingleListNode* pNext = pNode->mpNext;                          // v5 = v5[1]
        if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
            pThingy->ScalarDeletingDestructor(1);
        pNode = pNext;
    }

    // ---- pass 2: link any still-unlinked pending file into the scene ----
    for (AptFilePtr* pEntry = mPendingFiles.begin(); pEntry != mPendingFiles.end(); )
    {
        AptFile* pFile = pEntry->pData;                                    // _R28 = *v8
        if (pFile != nullptr) AptSharedPtrIncRef(pFile);
        bool bAdvance = true;

        for (AptSingleListNode* pNode = mpThingyListHead; pNode != nullptr; pNode = pNode->mpNext)
        {
            AptLinkerThingy* pThingy = pNode->mpThingy;                    // _R29 = *i
            if (pThingy != nullptr) AptSharedPtrIncRefThingy(pThingy);

            AptFile* pThingyFile = pThingy->mpFile.pData;                  // result = *(_R29+4)
            if (pThingyFile != nullptr) AptSharedPtrIncRef(pThingyFile);
            // A thingy matches this pending file iff same file AND not yet linked.
            const bool bMatch = (pThingyFile == pFile) && !pThingy->mbLinked;  // result==_R28 && !*(_R29+12)
            if (pThingyFile != nullptr && AptSharedPtrDecRef(pThingyFile) == 0)
                AptSharedPtrDelete(pThingyFile);

            if (bMatch)
            {
                // The X360's Allocate(off_8324D808, 44) IS the ctor's memory (its r3
                // threads straight into the ctor) and the r5 IncRef is the held-copy
                // build MakeCharacterAnimationInst models internally -- a separate
                // probe Allocate leaks a pool block and a second IncRef leaks one
                // AptFile ref per link.
                AptCharacterAnimationInst* pAnimInst = MakeCharacterAnimationInst(pFile);
                AptCIH* pCIH = static_cast<AptCIH*>(pThingy->mpValue);     // v61 = *(_R29+8)

                EnsureAnimFrameRateCached(pAnimInst);                      // dword_8324E530 lazy init (the SWF-version cache, homed below)

                if (pCIH->mpCharacterInst->mpProperties != nullptr)        // *(v61[8]+12)
                {
                    pCIH->ClearCIH(true);                                  // AptCIH::ClearCIH(v61,1)
                    pCIH->ForceCleanNativeHash();                          // AptCIH::ForceCleanNativeHash(v61)
                }
                pCIH->mFlagsA &= ~0x6u;                                    // clear CIHState (x64 bits 1-2; X360 form &=0x9FFFFFFF)
                pCIH->SetCharacterInst(/*as inst*/reinterpret_cast<AptCharacterInst*>(pAnimInst), true);
                pCIH->setIsDefined(true);                                  // AptValue::setIsDefined(v61,1)
                // SetCharacterInst installed the new AptCharacterAnimationInst -- an
                // AptCharacterSpriteInstBase. Reset its pending goto-frame to "none"
                // (-1) and OR in bJustLoaded (x64 bit 24; the console's reversed bit 7).
                // The console writes both through mpCharacterInst (v61[8]).
                AptCharacterSpriteInstBase* pSpriteInst =
                    static_cast<AptCharacterSpriteInstBase*>(pCIH->mpCharacterInst);
                pSpriteInst->mnGotoFrame        = -1;           // *(v61[8]+16) = -1
                pSpriteInst->mnClipActionFlags |= 0x1000000u;   // bJustLoaded (X360 form |= 0x80)
                pCIH->SetDirtyState(false, false);                        // AptCIH::SetDirtyState(...,0)
                pCIH->tick();                                            // AptCIH::tick
                pCIH->SetDirtyState(true, true);                          // AptCIH::SetDirtyState(v61,1,1)
                pThingy->mbLinked = true;                                 // *(_R29+12) = 1

                // The console saves the pending vector's data base before firing the
                // host notify hooks (whose re-entrant Notify can grow / reallocate
                // mPendingFiles) and restarts the scan when the base moved (`v7 != *v6`).
                const int32_t lnSizeSnapshot = mPendingFiles.mnSize;       // v7 == *(a1+4): the SIZE word, not the data base
                FireLinkNotifyCallbacks(pFile, pCIH);                     // dword_8324E844 / dword_8324E830 hooks (PC leaf below)

                // If the pending vector's COUNT changed mid-link, restart the scan
                // from the (possibly reallocated) base (X360: v7 != *v6 -> v8 = *(a1+12)).
                if (lnSizeSnapshot != mPendingFiles.mnSize)               // v7 != *v6
                {
                    pEntry = mPendingFiles.begin();                       // v8 = *(a1+12) (bAdvance=false keeps it at begin)
                    bAdvance = false;
                    if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
                        pThingy->ScalarDeletingDestructor(1);
                    break;
                }
            }

            if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
                pThingy->ScalarDeletingDestructor(1);
        }

        if (bAdvance) ++pEntry;
        if (pFile != nullptr && AptSharedPtrDecRef(pFile) == 0)
            AptSharedPtrDelete(pFile);
    }

    if (gbAptSavedInputActive)
        AptSavedInputCheckpoints::AllLinked(*gpAptSavedInputCheckpoints);

    // ---- finish: if anything was pending, clear the vector + run actions ----
    if (mPendingFiles.mnSize > 0)
    {
        PendingFileVector empty;
        empty.mnSize = 0; empty.mnCapacity = 0; empty.mpData = empty.mInlineStorage;
        empty.mInlineStorage[0].pData = nullptr; empty.mInlineStorage[1].pData = nullptr;
        mPendingFiles.Swap(empty);                                        // sub_82AFF018(a1+4, v83)
        gpAptTarget->mpAnimationTarget->RunActions();    // AptAnimationTarget::RunActions(off_8324E574->mpAnimationTarget)
        // ~empty (Strin(v83)) releases the swapped-out elements.
    }
}

// =====================================================================
//  Out-of-line helper bodies the linker spine calls (split out by the X360
//  decompile as small free functions). Homed faithfully here against the X360
//  ARTIST.XEX + PS3 DecFIGS exports.
// =====================================================================

// ---------------------------------------------------------------------
// AptSharedPtrIncRefThingy / AptSharedPtrDecRefThingy -- the intrusive ref-count
// on an AptLinkerThingy (count at +0x00, mnRefCount). PS3 DecFIGS:
//   _Z18AptSharedPtrIncRefP15AptLinkerThingy @0xF1F584 (lwarx/+1/stwcx., ret new)
//   _Z18AptSharedPtrDecRefP15AptLinkerThingy @0xF1F5B4 (lwarx/-1/stwcx., ret new)
// The console runs a true interrupt-masked load-linked / store-conditional atomic;
// on the single-threaded x64 Apt gate that folds to a plain ++/-- on the counter.
// FLAG PC-platform leaf (threading): atomicity dropped (no PC equivalent needed; the
// Apt runtime is single-thread for the linker pass). Returns the NEW count, exactly
// as the asm leaves it in r9.
// ---------------------------------------------------------------------
int AptSharedPtrIncRefThingy(AptLinkerThingy* pThingy)
{
    return ++pThingy->mnRefCount;   // lwarx r9; r9+1; stwcx. -> return r9
}

int AptSharedPtrDecRefThingy(AptLinkerThingy* pThingy)
{
    return --pThingy->mnRefCount;   // lwarx r9; r9-1; stwcx. -> return r9
}

// ---------------------------------------------------------------------
// ListPushFront -- push a new node holding a COUNTED reference to pThingy onto the
// front of the thingy list. X360 AptSingleLis @0x82AF4F88:
//   node = Allocate(pool, 8); if(node){ node[0]=thingy; if(thingy) AddRef(thingy);
//   node[1]=0; } v10=node; v10[1]=*head; *head=v10;
// The AddRef is the same lwarx/+1/stwcx. atomic as AptSharedPtrIncRefThingy. Note
// the console links the (possibly null on OOM) node either way.
// ---------------------------------------------------------------------
void ListPushFront(AptSingleListNode** ppHead, AptLinkerThingy* pThingy)
{
    AptSingleListNode* pNode =
        static_cast<AptSingleListNode*>(gpAptSharedPtrPool->Allocate(sizeof(AptSingleListNode)));   // Allocate(pool,8)
    if (pNode != nullptr)
    {
        pNode->mpThingy = pThingy;                 // *result = *a2
        if (pThingy != nullptr)
            AptSharedPtrIncRefThingy(pThingy);     // the lwarx/+1/stwcx. AddRef loop
        pNode->mpNext = nullptr;                   // result[1] = 0
    }
    pNode->mpNext = *ppHead;                        // v10[1] = *a1   (== old head)
    *ppHead       = pNode;                          // *a1 = v10
}

// ---------------------------------------------------------------------
// ListErase -- find the node *ppNode in the list headed by *ppHead, unlink it,
// release its counted thingy (destroy the thingy at count 0), and free the 8-byte
// node. X360 sub_82AF83A0; the body is attested verbatim by the PS3 DecFIGS inline
// in AptLinker::CancelLoad @0xF431D0 (find-or-pop-front, then Release the thingy
// via DecRef + AptSharedPtrDelete(thingy) and Deallocate the node).
// ---------------------------------------------------------------------
void ListErase(AptSingleListNode** ppHead, AptSingleListNode** ppNode)
{
    AptSingleListNode* pTarget = *ppNode;          // the node to remove
    AptSingleListNode* pCur    = *ppHead;
    if (pCur == nullptr)
        return;

    if (pCur == pTarget)
    {
        // Pop front: head = head->next, then release + free the old head.
        *ppHead = pCur->mpNext;                     // *v2 = v9
    }
    else
    {
        // Walk to the node whose next == pTarget, then splice it out.
        AptSingleListNode* pPrev = pCur;
        while (pPrev != nullptr && pPrev->mpNext != pTarget)
            pPrev = pPrev->mpNext;
        if (pPrev == nullptr)
            return;
        pPrev->mpNext = pTarget->mpNext;            // v3[1] = *(v10+4)
    }

    // Release the unlinked node's counted thingy (destroy at 0), then free the node.
    AptLinkerThingy* pThingy = pTarget->mpThingy;   // *node
    pTarget->mpThingy = nullptr;                     // *node = 0
    if (pThingy != nullptr && AptSharedPtrDecRefThingy(pThingy) == 0)
        pThingy->ScalarDeletingDestructor(1);        // == PS3 AptSharedPtrDelete(AptLinkerThingy*) @0xF43114 (dispose file + free 16-byte block)
    gpAptSharedPtrPool->Deallocate(pTarget, sizeof(AptSingleListNode));   // Deallocate(pool,node,8)
}

// ---------------------------------------------------------------------
// InstallEmptyCharacterInstVtbl -- the X360 stores the "empty placeholder"
// AptCharacterInst vtable (&off_82145FD0) into a freshly pool-constructed
// AptCharacterInst (`*inst = &off_82145FD0`). On the x64 PC gate the object is
// built with `::new (mem) AptCharacterInst(nullptr)`, so the C++ compiler has
// already installed the concrete vtable for this type; the raw .data vtable symbol
// has no homed method set to point at. The console raw-vtable poke is expressed by
// the C++ virtual-dispatch model -- no-op on PC (the empty-placeholder behaviour is
// the default-constructed AptCharacterInst the caller just placement-new'd).
// ---------------------------------------------------------------------
void InstallEmptyCharacterInstVtbl(AptCharacterInst* /*pInst*/)
{
    // *pInst = &off_82145FD0;  -- modelled by the placement-new vtable install.
}

// ---------------------------------------------------------------------
// EnsureAnimFrameRateCached -- HOMED 2026-07-02: the global is NOT a frame
// rate; dword_8324E530 is the module SWF-VERSION cache (the AS interpreter's
// AptGetSwfVersion() == 7 semantic switches read it -- undefined-compare /
// string-coercion behaviour changed in SWF7). The lazy init parses the .apt
// text header the file's resolve context points at -- "Apt Data:1:7:8" --
// where byte 0xA is the ':' before the version digit and byte 0xB is the
// digit itself ('7' - 48 == 7); a header without the tag defaults to 6.
//   X360: if(!dword_8324E530){ v62=*(*(animInst+40)+16); if(*(v62+10)==58)
//          v63=*(v62+11)-48; else v63=6; dword_8324E530=v63; }
// (animInst+40 == mAnimationFilePtr; file+16 == mpResolveContext.) The old
// misreading deferred this as "the frame-rate cache"; the misnomered function
// name is kept for the call-site contract.
// ---------------------------------------------------------------------
int gnAptSwfVersion = 0;   // dword_8324E530 (the module SWF-version cache)

unsigned int AptGetSwfVersion()
{
    return static_cast<unsigned int>(gnAptSwfVersion);
}

void EnsureAnimFrameRateCached(AptCharacterAnimationInst* pInst)
{
    if (gnAptSwfVersion == 0)
    {
        const unsigned char* const pHeader =
            static_cast<const unsigned char*>(
                pInst->mAnimationFilePtr.pData->mpResolveContext);
        gnAptSwfVersion = (pHeader != nullptr && pHeader[0xA] == 58)
                              ? (pHeader[0xB] - 48)
                              : 6;
    }
}

// ---------------------------------------------------------------------
// FireLinkNotifyCallbacks -- after a pending file is linked, fire the two host
// notification hooks (X360 dword_8324E844 / dword_8324E830, gated by E844 / E518).
//   X360: if(dword_8324E844) dword_8324E844(file->name+8, cih->assetString+8);
//         if(dword_8324E518){ build a 6-tag string from file->name; dword_8324E830(&s); }
// Both are host-installed function-pointer slots (the gAptFuncs host-callback
// family, the same indirection as pfnSetExternVariable). The console guards each
// call with its slot, so with no host installer the empty body IS the shipped
// behaviour.
// FLAG PC-platform leaf: host link-notify fn-ptr slots (dword_8324E844/8324E830),
// un-installed on the PC title path -- the console's if(slot) guards skip both.
// ---------------------------------------------------------------------
void FireLinkNotifyCallbacks(AptFile* /*pFile*/, AptCIH* /*pCIH*/)
{
}

// ---------------------------------------------------------------------
// AptLinker::isFileImported @0x82AECC58 -- HOMED 2026-07-10 (retiring the
// return-false link-stub, which made CancelPreloadedAnimation treat every
// candidate as un-imported). For each thingy the linker tracks, hand its file a
// FRESH COUNTED COPY of the candidate (AptFile::isFileImported consumes its
// argument each probe -- the console's per-iteration lwarx/stwcx IncRef); a hit
// answers true. Either way the ORIGINAL candidate handle is consumed at exit
// (Dispose + null), matching the asm's shared tail.
// ---------------------------------------------------------------------
bool AptLinker::isFileImported(AptFilePtr* ppCandidate)
{
    bool bImported = false;
    for (AptSingleListNode* pNode = mpThingyListHead; pNode != nullptr; pNode = pNode->mpNext)
    {
        // A fresh counted copy of the candidate for this probe (consumed by the callee).
        AptFile* pLocal = ppCandidate->pData;          // lwz r11, 0(r31)
        if (pLocal != nullptr)
            AptSharedPtrIncRef(pLocal);                // the lwarx/stwcx ++ on +0

        if (pNode->mpThingy->mpFile.pData->isFileImported(&pLocal))   // r3 = *(thingy+4)
        {
            bImported = true;                          // loc_82AECCF4
            break;
        }
    }

    // Shared tail: consume the original candidate (Dispose + null the slot).
    AptFile* const pOld = ppCandidate->pData;
    ppCandidate->pData = nullptr;
    AptFilePtr::Dispose(pOld);                         // bl AptFile___Dispose
    return bImported;
}

// RestartPendingScanIfGrew RETIRED (this pass): the real console guard -- snapshot
// mPendingFiles.mpData before FireLinkNotifyCallbacks, restart the scan when the
// base moved (`v7 != *v6`) -- is inlined at the Update call site above.

// =====================================================================
// ReplaceReferences @0x82AE4DF0 / PS3 _Z17ReplaceReferencesP8AptValueS0_PS0_i
// @0xF219B4 -- retarget every live reference from pOld to pNew across the GC
// value graph, plus the ReferenceReplaceCb / AptRegisterGlobalReferences
// machinery it drives. HOMED here (the same B4 module region as AptGetSwfVersion
// above -- B4 PDB 0x004268b0 sits beside it). Decompiled faithfully from the
// X360 ARTIST.XEX; every layout-bearing access goes through the x64-pinned named
// members/accessors (AptValue bitfield, AptCIH zombie/CIHState fields,
// AptScriptFunctionBase::GetParentAnim).
// =====================================================================

// The staging globals the swapped-in callback consumes (X360 .bss
// dword_8324E554..dword_8324E564; the PS3 external-ELF symbol spellings are kept
// verbatim, including the 'Refernce' typo).
static AptValue* gpRefernceValue        = nullptr;   // dword_8324E554 (the value being replaced)
static AptValue* gpRefernceValueReplace = nullptr;   // off_8324E558  (the replacement; null = drop)
static int       pnRefCount             = 0;         // dword_8324E55C (old references still to retarget)
static int       nTemp1                 = 0;         // dword_8324E560 (pool-walk visit counter)
static int       nTemp2                 = 0;         // dword_8324E564 (table-walk visit counter)

// dword_82F7337C -- the "use the snapshot table" mode gate ReplaceReferences
// reads (its only referencer in the ARTIST dump). Rodata VALUE RECOVERED (the
// 0x82F7337C dump): the shipped word is 0x00000001, so snUseNewWay == 1 --
// matching the shipped caller (CleanRemList snapshots + frees the live-value
// table purely to feed this path).
static int snUseNewWay = 1;   // dword_82F7337C (dump-pinned: 0x00000001)

// The GC live-value pool POINTER (off_8324D834) is gpGCPoolManager, declared by
// AptDefine.h (included above) and wired by AptAllocatorInitialize @0x82ADD118.
// UNIFIED 2026-08-11: this walk used to run over the namespace-scope
// `AptValueGC_PoolManager gAptValueGCPool` object -- a second C++ home for the one
// console slot that was permanently EMPTY (its (0,0) ctor takes the DogmaAllocator
// static-init early-out), so the live-value arm below never visited a value. The
// console loads the slot as a pointer and passes it as `this`: ReplaceReferences
// @0x82AE4E24 `lwz r3, off_8324D834@l(r28); bl ...GetFirstAptValue` and @0x82AE4F3C
// for GetNextAptValue -- with no null test (the slot is live between
// AptAllocatorInitialize and AptAllocatorShutdown @0x82AE9298).
//
// The shared sentinels/registry (AptGlobals.cpp / AptObject.cpp).
extern AptCIH*                gpAptEmptyCIH;      // dword_8324D700 (the pinned EmptyCIH)
extern AptValue*              gpUndefinedValue;   // off_8324D814 (the shared undefined)
extern AptNativeHash*         gpAptClassRegistry; // dword_8324E2D4 (AptObject.cpp)

// ---------------------------------------------------------------------
// AptRegisterGlobalReferences @0x82AE38B8 (B4 PDB: `void __cdecl
// AptRegisterGlobalReferences()`) -- run RegisterReferences over the process-
// global holders: every target instance's animation director (the off_8324E570
// list through mpNext == i[9]) and the Object.registerClass registry hash.
// ---------------------------------------------------------------------
void AptRegisterGlobalReferences()
{
    for (AptTarget* lpTarget = gpAptTargetCurrent; lpTarget != nullptr;
         lpTarget = lpTarget->mpNext)                                   // i = i[9]
    {
        lpTarget->mpAnimationTarget->RegisterReferences();              // (i[6] == +0x18)
    }
    if (gpAptClassRegistry != nullptr)                                  // dword_8324E2D4
        gpAptClassRegistry->RegisterReferences(nullptr);
}

// ---------------------------------------------------------------------
// ReferenceReplaceCb @0x82AE4B28 -- the reference-registration callback
// ReplaceReferences swaps into AptValue::sReferenceRegistrationCb
// (dword_8324E4E8). Every RegisterReferences pass then routes each held slot
// here: a slot still holding gpRefernceValue is repointed at the replacement (or
// a sentinel) with the refcounts / zombie counts balanced. Signature matches
// AptValue::ReferenceRegistrationCb; the return value is the slot's value (every
// caller discards it, exactly like AptGC::sReferenceRegistrationCb).
// ---------------------------------------------------------------------
static void* ReferenceReplaceCb(const AptValue* pOwner, void* pSlot,
                                const char* /*pDebugName*/, int nFlag)
{
    AptValue** const lppSlot = static_cast<AptValue**>(pSlot);
    AptValue*  const lpOld   = *lppSlot;

    // Only a slot still holding the value under replacement is touched; the
    // flag-2 registration class (the display-list-item weak form) never is.
    if (lpOld != gpRefernceValue || nFlag == 2)
        return lpOld;

    const AptVirtualFunctionTable_Indices leOldType = lpOld->getVtblIndex();
    const bool lbOldIsCIH = (leOldType == AptVFT_CharacterInstHandle
                          || leOldType == AptVFT_CIHNone);

    // Is the registering owner a defined script function (type 34..36)?
    const AptScriptFunctionBase* lpOwnerFn = nullptr;
    if (pOwner != nullptr
        && (static_cast<u32>(pOwner->getVtblIndex()) - AptVFT_ScriptFunction1) <= 2u
        && pOwner->getIsDefined())
    {
        lpOwnerFn = static_cast<const AptScriptFunctionBase*>(pOwner);
    }

    AptValue* const lpNew = gpRefernceValueReplace;

    if (lbOldIsCIH)
    {
        if (lpNew == nullptr)
        {
            // Dropping a CIH reference. A script function whose ParentAnim slot
            // this is balances the animation's zombie count (DecZombieCount reaps
            // at zero -- the console's inline dec + AptUpdateZombieVector(0)); the
            // slot falls back to the pinned EmptyCIH, or -- for a plain non-flag-1
            // holder -- the shared undefined sentinel.
            if (lpOwnerFn != nullptr)
            {
                if (lpOwnerFn->GetParentAnim() == lpOld)                 // v5[9] == v6
                    static_cast<AptCIH*>(lpOld)->DecZombieCount();
                *lppSlot = gpAptEmptyCIH;                                // dword_8324D700
            }
            else
            {
                *lppSlot = (nFlag == 1)
                    ? static_cast<AptValue*>(gpAptEmptyCIH)
                    : gpUndefinedValue;                                  // off_8324D814
            }
        }
        else
        {
            // Is the replacement itself a live CIH-family value?
            const AptVirtualFunctionTable_Indices leNewType = lpNew->getVtblIndex();
            const bool lbNewIsCIH =
                (leNewType == AptVFT_CharacterInstHandle && lpNew->getIsDefined())
                || leNewType == AptVFT_CIHNone;

            if (lpOwnerFn != nullptr)
            {
                // A script function's CIH reference: only its ParentAnim slot is
                // retargeted, and never while the old node is zombie-pinned
                // (console (v6[3] & 0x60000000) == 0x20000000 == CIHState 1).
                if (lpOwnerFn->GetParentAnim() != lpOld
                    || static_cast<AptCIH*>(lpOld)->GetCIHState() == 1u)
                    return lpOld;                                        // untouched exit

                static_cast<AptCIH*>(lpOld)->DecZombieCount();           // dec + reap-at-0

                if (lbNewIsCIH)
                {
                    *lppSlot = lpNew;
                    lpNew->AddRef();                                     // vtbl[0]
                }
                else
                {
                    *lppSlot = gpAptEmptyCIH;                            // stand-in
                }
            }
            else
            {
                // Plain CIH slot: flag 1 accepts only a live CIH-family value.
                if (nFlag != 1 || lbNewIsCIH)
                {
                    *lppSlot = lpNew;                                    // LABEL_51
                    lpNew->AddRef();
                    --pnRefCount;                                        // one reference retargeted
                }
                else
                {
                    *lppSlot = gpAptEmptyCIH;
                }
            }
        }
    }
    else
    {
        // Non-CIH value: straight swap (or drop to the undefined sentinel).
        if (lpNew != nullptr)
        {
            *lppSlot = lpNew;
            lpNew->AddRef();
        }
        else
        {
            *lppSlot = gpUndefinedValue;                                 // LABEL_53
        }
    }

    // Every path that repointed the slot releases the old reference (LABEL_57).
    if (*lppSlot != lpOld)
        lpOld->Release();                                                // vtbl[1]
    return *lppSlot;
}

// ---------------------------------------------------------------------
// ReplaceReferences @0x82AE4DF0 -- the remap driver: stage {old, new, count},
// swap the registration callback for the replace form, re-register the directors
// + globals, then drive RegisterReferences (console AptValue vtbl slot 13) over
// the live values -- the snapshot table when the caller supplies one
// (CleanRemList), else the whole GC pool -- until the old value's references
// drain. Finally the replacement inherits the retargeted count and the mark
// callback is restored.
// ---------------------------------------------------------------------
int ReplaceReferences(AptValue* pOld, AptValue* pNew, AptValue** ppTable, int nCount)
{
    const u32 lnOldRefs = pOld->getRefCount();               // console (v+4 >> 14) & 0xFFF
    gpRefernceValue        = pOld;                           // dword_8324E554
    gpRefernceValueReplace = pNew;                           // off_8324E558
    pnRefCount             = static_cast<int>(lnOldRefs);    // dword_8324E55C

    // The console fetches the pool head HERE, unconditionally, before the mode branch
    // (@0x82AE4E24 `lwz r3, off_8324D834`; @0x82AE4E30 `bl ...GetFirstAptValue`). The
    // null test is the PC pre-init guard only (see the gpGCPoolManager note above); a
    // null pool yields a null head, so the pool-mode loop below simply does not run and
    // needs no further test.
    AptValue* lpWalk = (gpGCPoolManager != nullptr)
                           ? gpGCPoolManager->GetFirstAptValue()   // off_8324D834
                           : nullptr;

    // Swap in the replace callback (dword_8324E4E8), saving the mark form.
    AptValue::ReferenceRegistrationCb const lpSavedCb = AptValue::sReferenceRegistrationCb;
    AptValue::sReferenceRegistrationCb = &ReferenceReplaceCb;

    // The non-pool-resident holders first: the current director's own slots +
    // the process globals (every target's director + the class registry).
    gpAptTarget->mpAnimationTarget->RemoveCIHReferences();   // *(off_8324E574 + 0x18)
    AptRegisterGlobalReferences();

    nTemp1 = 0;                                              // dword_8324E560
    nTemp2 = 0;                                              // dword_8324E564

    if (pOld->getRefCount() > 1u)                            // console (bits & 0x3FFC000) > 0x4000
    {
        if (snUseNewWay && ppTable != nullptr)
        {
            // Snapshot-table mode: visit only the captured live values.
            for (int liIndex = 0; liIndex < nCount; ++ppTable)
            {
                AptValue* const lpEntry = *ppTable;
                if (lpEntry != nullptr && lpEntry->mValueBitfield.mbIsAllocated)   // v15[1] < 0
                {
                    lpEntry->RegisterReferences();           // vtbl[13] -> the swapped cb
                    ++nTemp2;
                    if (pOld->getRefCount() == 1u || pnRefCount == 0)
                        break;                               // drained
                }
                if (lpEntry == pOld)
                    *ppTable = nullptr;                      // scrub the snapshot's own entry
                ++liIndex;
            }
        }
        else
        {
            // Pool mode: walk every live value until the references drain.
            while (lpWalk != nullptr && pnRefCount > 0)
            {
                lpWalk->RegisterReferences();
                lpWalk = gpGCPoolManager->GetNextAptValue(lpWalk);   // off_8324D834 (reloaded, as the asm does)
                ++nTemp1;
                if (pOld->getRefCount() == 1u)
                    break;
            }
        }
    }

    // The replacement inherits the retargeted references' count
    // (newCount = new - remaining + old).
    if (pNew != nullptr)
        pNew->setRefCount(pNew->getRefCount()
                          - static_cast<u32>(pnRefCount) + lnOldRefs);

    // Restore the mark callback + clear the staging state.
    AptValue::sReferenceRegistrationCb = lpSavedCb;
    gpRefernceValue        = nullptr;
    gpRefernceValueReplace = nullptr;
    pnRefCount             = 0;
    return 0;   // console r3 pass-through; every caller discards it
}

// =====================================================================
//  AptLinker::PendingFileVector -- the EA BasicString<StringAsVectorEncoding<
//  AptFilePtr>> reconstructed as a typed SBO vector of owned AptFilePtr. The two
//  mutators the linker spine names: PushBack (append, taking a counted ref) and Swap
//  (SBO-aware swap-assign). Decompiled faithfully from the X360 ARTIST.XEX:
//      PushBack -> sub_82B00498 (BasicString<...>::Insert at end, grow if full)
//      Swap     -> sub_82AFF018 (swap-assign, SBO-aware, releases temporaries)
//  AptFilePtr is a single counted AptFile* (AptSharedPtr<AptFile>); its operator=
//  carries the reference (incref new / decref+delete old), so the element copies below
//  maintain the refcount exactly as the X360 does.
// =====================================================================

// PushBack -- sub_82B00498. Append a copy of rFile at the end (the X360 routes through
// BasicString::Insert at position end = mpData + mnSize, growing the storage when the
// logical count has reached capacity). The inline SBO buffer holds 2 elements (the
// ctor's initial capacity); once both are used a heap block is allocated from the
// AptSharedPtr pool (off_8324D808) and the existing elements are relocated into it.
void AptLinker::PendingFileVector::PushBack(const AptFilePtr& rFile)
{
    // Capacity in usable slots: the inline SBO buffer is 2 (mnCapacity stays 0 while the
    // vector still points at mInlineStorage); a heap block records its slot count in
    // mnCapacity. Grow when the next index would overflow the current usable slots.
    const int32_t nUsable = (mpData == mInlineStorage) ? 2 : mnCapacity;
    if (mnSize >= nUsable)
    {
        const int32_t nNewCap = nUsable ? nUsable * 2 : 2;   // BasicString geometric grow
        AptFilePtr* const pNew =
            static_cast<AptFilePtr*>(gpAptSharedPtrPool->Allocate(sizeof(AptFilePtr) * nNewCap));

        // Relocate the existing elements (operator= carries each counted ref to the new
        // block; the source slots are then cleared so the old storage owns nothing).
        for (int32_t i = 0; i < mnSize; ++i)
        {
            pNew[i].pData = nullptr;
            pNew[i] = mpData[i];        // incref new == old, then nothing to decref (new slot was null)
            mpData[i].pData = nullptr;  // old slot relinquishes (the live ref now lives in pNew)
        }
        // Initialise the unused tail of the new block.
        for (int32_t i = mnSize; i < nNewCap; ++i)
            pNew[i].pData = nullptr;

        if (mpData != mInlineStorage)
            gpAptSharedPtrPool->Deallocate(mpData, sizeof(AptFilePtr) * mnCapacity);

        mpData     = pNew;
        mnCapacity = nNewCap;
    }

    // Place the new element at the end (operator= takes the counted ref) + advance.
    mpData[mnSize].pData = nullptr;
    mpData[mnSize]       = rFile;
    ++mnSize;
}

// Swap -- sub_82AFF018. Exchange this vector's {size, capacity, data} triple with
// rOther's. SBO-aware: when either side's data pointer aliases its own inline buffer,
// the raw pointer swap would leave the other vector pointing at this object's inline
// storage, so the X360 relocates the inline elements element-by-element (AptFilePtr
// operator=) through a scratch buffer and releases the temporaries. The non-SBO case
// (both on the heap) is a plain pointer swap.
void AptLinker::PendingFileVector::Swap(PendingFileVector& rOther)
{
    const bool bOtherInline = (rOther.mpData == rOther.mInlineStorage);   // a2+3 == a2[2]
    const bool bThisInline  = (mpData == mInlineStorage);                 // result+3 == v9

    // Swap the {size, capacity} scalars (v6/v7/v8 dance in the X360).
    const int32_t nTmpSize = rOther.mnSize;
    rOther.mnSize = mnSize;
    const int32_t nTmpCap  = rOther.mnCapacity;
    rOther.mnCapacity = mnCapacity;
    AptFilePtr* const pTmpData = mpData;       // v9 = result[2] (this->mpData, captured pre-swap)
    mnSize     = nTmpSize;
    mnCapacity = nTmpCap;

    // Re-point each data slot: an inline-owning side must keep pointing at its OWN
    // inline buffer after the swap (the X360 v10 / `result+3==v9` tests).
    rOther.mpData = bThisInline  ? rOther.mInlineStorage : pTmpData;
    mpData        = bOtherInline ? mInlineStorage        : rOther.mpData;

    // When either side was inline, the two inline buffers themselves must be exchanged
    // (their contents, not their addresses). Relocate through a scratch pair, copying
    // this <- other and other <- this via AptFilePtr operator= (refcount-correct), then
    // release the scratch temporaries (the X360's two-iteration decref loop).
    if (bThisInline || bOtherInline)
    {
        AptFilePtr scratch[2];
        scratch[0].pData = nullptr;
        scratch[1].pData = nullptr;

        // scratch <- this inline
        scratch[0] = mInlineStorage[0];
        scratch[1] = mInlineStorage[1];
        // this inline <- other inline
        mInlineStorage[0] = rOther.mInlineStorage[0];
        mInlineStorage[1] = rOther.mInlineStorage[1];
        // other inline <- scratch (the original this inline)
        rOther.mInlineStorage[0] = scratch[0];
        rOther.mInlineStorage[1] = scratch[1];

        // Release the scratch temporaries (decref + delete at zero) -- the X360's
        // `for (i=1; i>=0; --i)` LL/SC decrement loop over the scratch block.
        for (int32_t i = 1; i >= 0; --i)
        {
            AptFile* const pData = scratch[i].pData;
            scratch[i].pData = nullptr;
            if (pData != nullptr && AptSharedPtrDecRef(pData) == 0)
                AptSharedPtrDelete(pData);
        }
    }
}