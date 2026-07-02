// AptRenderLinkStubs.cpp -- FLAG link-skeleton (auto-gen). Empty stubs for un-homed Apt-engine
// function shims so the stack LINKS; homed faithfully as the render path hits each (link-then-home).
// Co-evolves with the parallel Apt homing -- regenerate against the current unresolved set.
#include "types.hpp"
#include "SDKs/EATech/Apt/AptActionDefineFunction2.h"
#include "SDKs/EATech/Apt/AptActionTryCatchFinallyBlock.h"
#include "SDKs/EATech/Apt/AptKeyMembersIndex.h"
#include "SDKs/EATech/Apt/AptMath.h"
#include "SDKs/EATech/Apt/AptObjectIndex.h"
#include "SDKs/EATech/Apt/AptTextFormatMembersIndex.h"
#include "SDKs/EATech/Apt/AptTextMembersIndex.h"
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"
#include "SDKs/EATech/include/Apt/Apt.h"
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"
#include "SDKs/EATech/include/Apt/AptActionQueue.h"
#include "SDKs/EATech/include/Apt/AptActionQueueC.h"
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"
#include "SDKs/EATech/include/Apt/AptArray.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.h"
#include "SDKs/EATech/include/Apt/AptCIHNone.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterDynamicText.h"
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterLevelInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterMorph.h"
#include "SDKs/EATech/include/Apt/AptCharacterMorphInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterShape.h"
#include "SDKs/EATech/include/Apt/AptCharacterShapeInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"
#include "SDKs/EATech/include/Apt/AptCharacterStaticText.h"
#include "SDKs/EATech/include/Apt/AptCharacterStaticTextInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"
#include "SDKs/EATech/include/Apt/AptConstFile.h"
#include "SDKs/EATech/include/Apt/AptDate.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"
#include "SDKs/EATech/include/Apt/AptDisplayList.h"
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"
#include "SDKs/EATech/include/Apt/AptError.h"
#include "SDKs/EATech/include/Apt/AptExtObject.h"
#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptFileSavedInputState.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptGC.h"
#include "SDKs/EATech/include/Apt/AptGlobal.h"
#include "SDKs/EATech/include/Apt/AptGlobalExtensionObject.h"
#include "SDKs/EATech/include/Apt/AptIntervalTimer.h"
#include "SDKs/EATech/include/Apt/AptKey.h"
#include "SDKs/EATech/include/Apt/AptLinker.h"
#include "SDKs/EATech/include/Apt/AptLinkerThingy.h"
#include "SDKs/EATech/include/Apt/AptListenerSlotList.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"
#include "SDKs/EATech/include/Apt/AptMathObj.h"
#include "SDKs/EATech/include/Apt/AptMovie.h"
#include "SDKs/EATech/include/Apt/AptMovieClip.h"
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"
#include "SDKs/EATech/include/Apt/AptPseudoData.h"
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"
#include "SDKs/EATech/include/Apt/AptRenderHooks.h"
#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptRenderItemAnimation.h"
#include "SDKs/EATech/include/Apt/AptRenderItemButton.h"
#include "SDKs/EATech/include/Apt/AptRenderItemCustomControl.h"
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"
#include "SDKs/EATech/include/Apt/AptRenderItemLevel.h"
#include "SDKs/EATech/include/Apt/AptRenderItemMorph.h"
#include "SDKs/EATech/include/Apt/AptRenderItemShape.h"
#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptRenderItemStaticText.h"
#include "SDKs/EATech/include/Apt/AptRenderManagerItem.h"
#include "SDKs/EATech/include/Apt/AptRenderManagerQueue.h"
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"
#include "SDKs/EATech/include/Apt/AptSavedInputCheckpoints.h"
#include "SDKs/EATech/include/Apt/AptScriptColour.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionByteCodeBlock.h"
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"
#include "SDKs/EATech/include/Apt/AptSingleListPolicy.h"
#include "SDKs/EATech/include/Apt/AptSound.h"
#include "SDKs/EATech/include/Apt/AptStage.h"
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptTarget.h"
#include "SDKs/EATech/include/Apt/AptTextFormat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptExtern.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"
#include "SDKs/EATech/include/Apt/AptValue/AptNone.h"
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"
#include "SDKs/EATech/include/Apt/AptValueFactory.h"
#include "SDKs/EATech/include/Apt/AptValueWithHash.h"
#include "SDKs/EATech/include/Apt/AptXml.h"
#include "SDKs/EATech/include/Apt/AptXmlNode.h"

// ===================================================================================
// Engine link-stub headers (the 42 off-render-path ENGINE symbols, below). These are
// the EXISTING decls the Apt code already references; included so each stub matches the
// canonical signature/mangling exactly. NOTE: the reconstructed BrnEAThreadX360.h is
// deliberately NOT pulled (it declares EA::Thread::GetThreadId() returning ThreadId
// (void*), which would collide with the int-returning EA::Thread::GetThreadId() the Apt
// call sites reference -- AptCharacterAnimation.cpp); the EA::Thread surface is therefore
// declared minimally inline below. job_thread.h / local_backend.h are likewise NOT pulled
// (they transitively include BrnEAThreadX360.h); their two symbols are declared minimally.
// ===================================================================================
// NOTE: rwcore/filesys/device.h is deliberately NOT included -- it pulls
// BrnEAThreadX360.h (declaring EA::Thread::GetThreadId() returning ThreadId/void*), which
// collides with the int-returning EA::Thread::GetThreadId() the Apt call sites reference.
// The handful of rw::core::filesys types stubbed below are declared minimally instead.
#include "coreallocator/icoreallocator_interface.h"       // EA::Allocator::ICoreAllocator
#include "SDKs/EATech/eajobs/job_types.h"                 // EA::Jobs enums + Detail::SchedulerBackend + Param
#include "SDKs/EATech/eajobs/entry_point.h"               // EA::Jobs::EntryPoint
#include "SDKs/EATech/eajobs/event.h"                     // EA::Jobs::Event
#include "SDKs/EATech/eajobs/job.h"                       // EA::Jobs::Job
#include "SDKs/EATech/eajobs/job_thread_handle.h"         // EA::Jobs::JobThreadHandle / JobThreadParameters
#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp" // Nicotine::SnapshotMixer
#include "SDKs/EATech/include/Nicotine/SnapshotChannel.hpp" // Nicotine::SnapshotVolumeCurve
#include "SDKs/EATech/include/NFSMix/NFSMixMaster.hpp"    // NFSMixMaster

struct AptDragState;  // FLAG fwd-decl (pointer-only use)

    // AptResolveFontGlyph RETIRED (2026-07-02): homed in AptRenderItemStaticText.cpp.
    // AptResolveTextFontCharacter RETIRED (2026-07-02): homed in AptRenderItemStaticText.cpp.
    // findCharacterInLibrary RETIRED (2026-07-02): homed in
    // AptCIHNativeFunctionHelper.cpp (the X360 @0x82AFDF58 parent-chain
    // export/import library resolve).
    // AptApt_GetDragState RETIRED (2026-07-02): homed in
    // AptActionInterpreterSpecialOps.cpp (the view over the director's
    // mpDragMC..mGrabOffset run -- X360 StartDragMovie @0x82B03B00).
    AptFilePtr* AptLoader_LoadX360(AptFilePtr* pOut, AptLoader* pLoader, const EAStringC* pName) { return nullptr; }   // FLAG link-stub
    // AptScriptFunctionBase_GetActiveFrameStack RETIRED (2026-07-02): homed in AptFrameStack.cpp.
    // AptValue_EmbeddedNativeHash RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp (the AptValueWithHash mHash @+8).
    // AptInterp_GetNodeFrameContextHash RETIRED (2026-07-02): homed in AptActionInterpreterInterpHelpers.cpp
    // (setVariable @0x82B03374: the display PARENT's char-inst property hash -- the enclosing clip's scope).
    AptTextFormat* AptTextFormat_ConstructDefault(void* pBlock, AptValue* pSource, double dArg) { return nullptr; }   // FLAG link-stub
    // AptActionInterpreter_SetIntervalImpl RETIRED (2026-07-02): homed in
    // AptIntervalTimer.cpp (the X360 cbCallMethod_setInterval @0x82B019D8 body).
    // AptApt_DeriveFunctionAnimation RETIRED (2026-07-01): homed in AptScriptFunctionBase.cpp
    // (== AptCIH::GetRootAnimation, the enclosing-timeline walk).
    // AptApt_GetRootContext RETIRED (2026-07-02): homed in
    // AptActionInterpreterContext.cpp (== AptGetAnimationAtLevel(0), the
    // level-0 root the absolute "/" paths resolve from).
    // AptCIH_gotoAndX RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the real AptCIH::_gotoAndX @0x82B0D2F0 -- label/frame goto core).
    AptValue* AptExtern_GetMember(const char* szName) { return nullptr; }   // FLAG link-stub
    // AptInterp_FrameStackFirstLocal RETIRED (2026-07-02): homed in AptArray.cpp (a tag-14 ARRAY's first element).
    // AptInterp_LookupScopeChain RETIRED (2026-07-02): homed in AptFrameStack.cpp (spFrameStack->GetInScopeChain).
    // AptUpdateZombieVector RETIRED (2026-07-02): homed in AptGC.cpp (the real
    // reap over gpAptZombieVector -- XB1 sub_140830A40; the vector itself is
    // allocated by AptUpdateInitialize from config word 14). The old "absent
    // from all dumps" claim was false -- the whole subsystem is in the XB1.
    // AptValue_GetMCParent RETIRED (2026-07-02): the shim was a reconstruction
    // invention -- the shipped isMCInParentChain @0x82AD8458 walks
    // GetNativeHashVirtual()->mp__Proto__ directly (corrected in AptValue.cpp).
    // isNaN RETIRED (2026-07-02): homed in AptActionInterpreterBuiltins.cpp
    // (the full @0x82AF9768 ECMA-ish NaN classification incl. the SWF7 arm).
    bool AptLinker_isFileImported(AptLinker* pLinker, AptFilePtr* ppCandidate) { return false; }   // FLAG link-stub
    const char*    AptResolveTextFieldFontName(AptCharacterInst* pTextInst) { return nullptr; }   // FLAG link-stub
    // Apt_atoff RETIRED (2026-07-02): homed in AptValueConvert.cpp
    // (PS3 @0x7E2990 == (float)strtod; the stub's 0 broke every string->number).
    int    AptValueGCPool_GetAllocatedCount(void* pPool) { return 0; }   // FLAG link-stub
    int  AptHook_GetBytesTotal(const char* pcFilePath, int a2, double a3) { return 0; }   // FLAG link-stub
    // AptActionInterpreter_InstanceOfChainWalk RETIRED (2026-07-02): homed in
    // AptActionInterpreter.cpp (the X360 isObjectOfType @0x82AEA5B8 object arm).
    // AptCIH_ShapeHitTest RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the host pfnPointHitTest dispatch, X360 dword_8324E8A4 == gAptFuncs+0x8C).
    // AptInterp_LabelToFrame RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the clip movie's label-hash lookup, X360 @0x82B0C618 chain).
    int GetThreadId() { return 0; }   // FLAG link-stub
    uint32_t AptValue_CurrentThreadId() { return 0; }   // FLAG link-stub
    // AptGetSwfVersion RETIRED (2026-07-02): homed in AptLinker.cpp -- the
    // dword_8324E530 SWF-version cache (parsed from the .apt "Apt Data:1:7:8"
    // header at first link; NOT a frame rate as previously misread).
    // AptRand RETIRED (2026-07-02): homed in AptRandom.cpp (the X360 MT19937
    // variant @0x82AE04F0 -- custom tempering b 0x9D2C56FF, auto-seed 0x1105).
    // AptActionInterpreter_ClearIntervalImpl RETIRED (2026-07-02): homed in
    // AptIntervalTimer.cpp (the X360 cbCallMethod_clearInterval @0x82AE3AE0 body).
    void      AptApt_AnimationAddCharacterRef(AptValue* pAnimation) {}   // FLAG link-stub
    void      AptApt_AnimationReleaseCharacterRef(AptValue* pAnimation) {}   // FLAG link-stub
    void      AptApt_PrepareCallContextScope(AptValue* pCallContext) {}   // FLAG link-stub
    void      AptExtern_SetMember(const char* szName, const char* szValue) {}   // FLAG link-stub
    // AptActionInterpreter_runStream RETIRED (IGNITION 2026-07-01): the init passes call the real
    // member gAptActionInterpreter.runStream (AptActionRun.cpp dispatch loop) -- ActionScript executes.

    // FLAG link-stub (dormant movie-UNLOAD path): the per-string return to the temporary
    // string pool (xb1 sub_14083F2A0), reached only by _parseStream's unresolve direction;
    // StringPool exposes only ClearTemporaryPool so far. Home with the unload bring-up.
    class AptString;
    void AptStringPool_ReleaseString(AptString* pString) { (void)pString; }
    void  AptCharacterAnimation_ExecuteInitActions(void* pAnim, void* pCIH, int nId) {}   // FLAG link-stub
    void  AptFreeFontUnit(void* pUnit) {}   // FLAG link-stub
    void  AptFreeRenderingUnit(void* pUnit) {}   // FLAG link-stub
    // AptPseudoDisplayList_Insert RETIRED (2026-07-01): homed member AptPseudoDisplayList::Insert
    // (AptPseudoDisplayList.cpp, faithful list-insert) called directly in AptMovie; the {} stub dropped it.
    // The 6-arg place-command resolver the temporary-frame timeline path calls (X360
    // DoTemporaryFrameControls @0x82AEEB98 reaches it for the place tag). It is the
    // larger AptDisplayListState::findInst-family callee (X360 @0x82AD99F0 takes this
    // + key + a3 + ppPred + ppExisting; the AptMovie call site threads the extra
    // place-info context/record words). NOT the 3-arg AptPseudoDisplayList::FindInst
    // method (that one is homed faithfully in AptPseudoDisplayList.cpp); this deferred
    // VM-timeline callee is a FLAG link-stub until the temporary-frame skip path is
    // brought up (it is off the boot trace -- reached only via AptCIH::jumpToFrame).
    void* AptPseudoDisplayList_FindInst(void* pList, void* pSource, unsigned char* pOutHit,
                                        void** ppExisting, void* pContext, void* pInfo)
    { if (pOutHit) *pOutHit = 0; if (ppExisting) *ppExisting = 0; return 0; }   // FLAG link-stub
    void  AptValue_setGCRoot(AptValue* pValue, int bRoot) {}   // FLAG link-stub
    void AptActionInterpreter_UnEscape(EAStringC* pStr) {}   // FLAG link-stub
    // AptActionInterpreter_getName RETIRED (2026-07-02): homed in
    // AptCIHNativeFunctionHelper.cpp (getName @0x82AF75C8 + the recursive
    // sub_82AF7400 target-path builder).
    // AptActionInterpreter_stackPushIndirect RETIRED (2026-07-01): homed as the real member
    // AptActionInterpreter::stackPushIndirect in AptActionInterpreter.cpp; caller uses the member.
    // AptAnimationTargetSet_Construct is now HOMED faithfully in AptAnimationTarget.cpp
    // (sub_82AE1708: allocate the slot array + set capacity; native-8 pointer stride).
    void AptAnimationTargetSet_Destruct (AptAnimationTargetSet* pSet) {}   // FLAG link-stub
    void AptAnimationTargetSet_Destruct2(AptAnimationTargetSet* pSet) {}   // FLAG link-stub
    void AptAnimationTarget_TickNewInsts(AptAnimationTarget* pAnim) {}   // FLAG link-stub
    // AptApt_FlushDeferredReleases RETIRED (2026-07-01): homed in AptGC.cpp as the real
    // gValuesToRelease.ReleaseValues() drain (the {} stub silently dropped every GC drain).
    AptValue* AptApt_LoadVariablesFetch(const char* pUrl) { return 0; }   // FLAG link-stub (host URL fetch; null until installed)
    void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY) {}   // FLAG link-stub
    void AptApt_PopValues(AptActionInterpreter* pInterp, int nCount) {}   // FLAG link-stub
    void AptCIH_GetWorldBounds(AptValue* pNode, float* pOutRect) {}   // FLAG link-stub
    // AptCIH_SetDirtyState RETIRED (2026-07-01): the real member AptCIH::SetDirtyState
    // (AptCIH.cpp, faithful) is called directly; the {} stub silently dropped the dirty latch.
    void AptCIH_SetProceduralProperty(AptCIH* pNode, int nProperty, double fValue) {}   // FLAG link-stub
    // AptCIH_jumpToFrame RETIRED (2026-07-01): homed member AptCIH::jumpToFrame (AptCIH.cpp,
    // faithful play-head seek) called directly at all 5 VM sites; the {} stub dropped every seek.
    // AptCIH_tick is now homed faithfully in AptCIHBehaviour.cpp (forwards to AptCIH::tick).

    // ---- AptCIH "link cluster" deferred sub-paths: the deep callees the now-homed
    // AptCIHBehaviour.cpp bodies (queueClipEvents / GeneralisedProcess / ClearCIH /
    // AddToDelayReleaseList / PreDestroyHook) FLAG out. Faithful "deferred subsystem"
    // defaults until the AS-interpreter execution / generalised-process gate / GC zombie
    // subsystems land. -------------------------------------------------------------------
    void AptAnimationTarget_AddToRemList(AptAnimationTarget* pAnim, AptCIH* pItem) {}   // FLAG link-stub
    void (*gpAptCIHPreDestroyHook)(AptCIH* pCIH) = nullptr;   // FLAG link-stub (dword_8324E8A0; null until installed)
    // AptCIH_queueClipEvents_RunMatched RETIRED (2026-07-01): homed faithfully in
    // AptCIHBehaviour.cpp from the PS3 body @0x815BD0 (the clip-event record scan +
    // AddActionFront/Back enqueues; the byte-code-block + __proto__ tails staged there).
    // AptCIH_ClearCIH_DrainQueuesAndZombie RETIRED (2026-07-02): homed in
    // AptCIHBehaviour.cpp (the director-set/new-inst drain + the unload-event tail;
    // the zombie-vector decision stays a documented staged FLAG there).
    bool AptCIH_sbGeneralisedProcessEarlyReturn = false;   // FLAG link-stub (bEarlyReturn; gate off by default)
    unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*)  = nullptr;   // FLAG link-stub
    unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*) = nullptr;   // FLAG link-stub
    unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*) = nullptr;   // FLAG link-stub
    int  AptCIH_snGeneralisedProcessTreeDepth = 0;   // FLAG link-stub (nTreeDepth)

    void AptHook_Trace(const char* szFormat, const char* szMessage) {}   // FLAG link-stub
    void AptKeyManagerAddListener(AptValue* pListener) {}   // FLAG link-stub
    bool AptKeyManagerRemoveListener(AptValue* pListener) { return false; }   // FLAG link-stub
    void AptLinker_GetUrlLoad(AptLinker* pLinker, EAStringC* pUrl, EAStringC* pTarget) {}   // FLAG link-stub
    void AptLoader_CancelAsyncLoad(void* pDataBlock) {}   // FLAG link-stub
    // AptLoader_StartAsyncLoad is HOMED in BrnAptRuntimeBringUp.cpp (the platform stream hook: it
    // synchronously content-loads the import bundle + drives AptCompleteAnimationAsyncLoad). The
    // FLAG link-stub that used to live here is removed so the strong host definition is the only one.
    // AptMovie_runFrameActions RETIRED (2026-07-01): homed as the real const member
    // AptMovie::runFrameActions(AptCIH*, int) (AptMovie.cpp, PS3 @0x820FA4 -- the invented
    // void* shim shape was wrong); the CallFrame handler calls it on the clip's embedded movie.
    // AptObject_SetImplementedObjects RETIRED (2026-07-01): the real member
    // AptObject::SetImplementedObjects (AptObject.cpp) is called directly; the {} stub dropped it.
    // AptScriptFunctionBase_InitializeStaticData RETIRED (2026-07-01): the real static member
    // AptScriptFunctionBase::InitializeStaticData allocates the register block at boot (the {}
    // stub left the block unallocated, so the AS register file never existed).
    // AptScriptFunctionBase_PopStaticData RETIRED (2026-07-01): homed as the real static member
    // AptScriptFunctionBase::PopStaticData (AptScriptFunctionBase.cpp, asm-decoded register-block pop).
    void GlobalNotificationFunction(AptFilePtr* pFile) {}   // FLAG link-stub
    void Mutex_Lock(void* pMutex, void* pName) {}   // FLAG link-stub
    void Mutex_Unlock(void* pMutex) {}   // FLAG link-stub
    void TextFormat_copyTextFormatObj(TextFormat* pDest, const TextFormat* pSource) {}   // FLAG link-stub
    void escape(EAStringC* pString) {}   // FLAG link-stub
    void unescape(EAStringC* pString) {}   // FLAG link-stub
    // AptActionInterpreter_CleanupAfterExecution RETIRED (IGNITION 2026-07-01): the real member
    // (thrown-value drop + PopStaticData window pop) is called directly with the saved base.
    void* AptFile_operator(void* pDst, void* pSrc) { return nullptr; }   // FLAG link-stub
    void* sub_82AFD150(void* a1, int a2) { return nullptr; }   // FLAG link-stub
    // sub_82B0AE08 (the place-command dispatcher @0x82B0AE08) is now HOMED faithfully as
    // AptMovie_PlaceCommand in AptMovie.cpp (reads the PlaceObject record + calls the homed
    // AptDisplayList::placeObjectNCXForm). The null link-stub is retired.
    void** AptValueGC_PoolManager_GetAllAllocatedAptValues(void* pPool) { return nullptr; }   // FLAG link-stub

// ===================================================================================
// ENGINE link-stubs (42) -- off the PC render-critical path. The PC bring-up is
// single-threaded; the bundle FS uses the existing DeviceManager replay path (NOT
// rw::core::filesys); audio mix is wired after the render works. Each is a deliberate
// bring-up bridge to reach a RUNNING link; homed faithfully once we see which are hit.
// ===================================================================================

// ---- rw::core::filesys -------------------------------------------------------------
// FLAG: PC-simplification. The faithful async bundle path IS rw::core::filesys per
// async-filesystem-blueprint; PC uses the DeviceManager replay FS instead, so these
// scheduler/handle/manager entry points are stubbed until that faithful path is wired.
// Types declared minimally (NOT via device.h) so BrnEAThreadX360.h is not transitively
// pulled; the minimal decls reproduce only what each stubbed symbol's mangling needs.
namespace rw { namespace core { namespace filesys {

    class  Device;
    struct AsyncOp;

    // Minimal Handle matching the asyncop.h layout the ctor zero/stores.
    struct Handle
    {
        Handle(const char* lpcPath, u32 luPositionHi, Device* lpDevice);
        u32   mField0;
        u32   mField1;
        u32   mbIsOpen;
        u32   mField3;
        void* mpDevice;
    };

    // Minimal Manager / Device / DeviceDriverVTable -- just the stubbed members.
    struct Manager
    {
        Device* RegisterDevice(const void* lpDeviceDesc, int liFlags);
        int     UnregisterDevice();
    };

    class Device
    {
    public:
        static Device* GetInstance(const char* lpcPath, char* lpScratch);
        int Wait(AsyncOp* lpOp, const void* lpTimeout);
        int InsertOp(AsyncOp* lpOp);
        int ChangeOpPriority(AsyncOp* lpOp, int liPriority);
    };

    struct DeviceDriverVTable
    {
        void* mpfnSlot0;
        void* mpfnOpen;
        void* mpfnClose;
        void* mapfnReserved0C[7];
        void* mpfnGetBlockSize;
    };

    // ctor: zero/store args into the X360 field order.
    Handle::Handle(const char* lpcPath, u32 luPositionHi, Device* lpDevice)
        : mField0(0)
        , mField1(luPositionHi)
        , mbIsOpen(0)
        , mField3(0)
        , mpDevice(lpDevice)
    {
        (void)lpcPath;   // FLAG link-stub: path not opened (DeviceManager replay path used)
    }

    Device* Manager::RegisterDevice(const void* lpDeviceDesc, int liFlags)   // FLAG link-stub
    { (void)lpDeviceDesc; (void)liFlags; return nullptr; }

    int Manager::UnregisterDevice() { return 0; }   // FLAG link-stub

    Device* Device::GetInstance(const char* lpcPath, char* lpScratch)        // FLAG link-stub
    { (void)lpcPath; (void)lpScratch; return nullptr; }

    int Device::Wait(AsyncOp* lpOp, const void* lpTimeout)                    // FLAG link-stub
    { (void)lpOp; (void)lpTimeout; return 0; }

    int Device::InsertOp(AsyncOp* lpOp) { (void)lpOp; return 0; }             // FLAG link-stub

    int Device::ChangeOpPriority(AsyncOp* lpOp, int liPriority)               // FLAG link-stub
    { (void)lpOp; (void)liPriority; return 0; }

    Manager* gpFileSysManager = nullptr;                                      // FLAG link-stub (off_8327F078)
    // extern: namespace-scope const defaults to internal linkage; force external.
    extern const DeviceDriverVTable gDeviceDriverVTable = {};                 // FLAG link-stub (zero-init driver vtable)

}}}

// ---- rw::collision -----------------------------------------------------------------
// FLAG: collision, not on the Apt render path. No shared header declares
// rw::collision::VolumeLineQuery (it lives inside volumelinequery.cpp); a minimal
// matching declaration is provided so the GetIntersections() mangling is exact.
namespace rw { namespace collision {

    class VolumeLineQuery { public: int GetIntersections(); };   // FLAG link-stub (minimal decl for mangling)
    int VolumeLineQuery::GetIntersections() { return 0; }        // FLAG link-stub

    // The six per-type volume-handler bytes the volume vtable points at (volume.cpp
    // declares these extern const u8). Zero-init storage -- off the Apt render path.
    // extern: a namespace-scope `const` defaults to INTERNAL linkage in C++; the volume
    // vtable in another TU references these as external -> force external linkage.
    extern const u8 gVolumeHandler_82F91740 = 0;   // FLAG link-stub
    extern const u8 gVolumeHandler_82F9176C = 0;   // FLAG link-stub
    extern const u8 gVolumeHandler_82F91894 = 0;   // FLAG link-stub
    extern const u8 gVolumeHandler_82F918C0 = 0;   // FLAG link-stub
    extern const u8 gVolumeHandler_82F919A4 = 0;   // FLAG link-stub
    extern const u8 gVolumeHandler_82F919D0 = 0;   // FLAG link-stub

}}

// ---- EA::Thread --------------------------------------------------------------------
// FLAG: single-threaded PC bring-up. Locks/thread-ids/TLS/runnable are no-ops. Declared
// minimally here (NOT via BrnEAThreadX360.h) so the int-returning GetThreadId() the Apt
// call sites reference is the symbol defined, and so the EATech reconstructed/vendor
// EAThread headers are not transitively pulled.
namespace EA { namespace Thread {

    // Minimal matching decls for the class members the Apt path references.
    struct IRunnable
    {
        virtual ~IRunnable();
        virtual intptr_t Run(void* pContext) = 0;   // pure: shape only; never instantiated here
    };

    class Thread
    {
    public:
        enum Status { kStatusNone = 0, kStatusRunning = 1, kStatusEnded = 2 };
        Status WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute);
    };

    // Minimal ThreadLocalStorage with an INLINE trivial ctor so gAptTargetTls needs no
    // out-of-line ctor symbol (FLAG: zeroed storage; faithful ctor does TlsAlloc).
    class ThreadLocalStorage
    {
    public:
        ThreadLocalStorage() : mTlsIndex(0) {}   // inline -- no external ctor symbol
        bool  SetValue(const void* pData);
        void* GetValue();
        u32   mTlsIndex;
    };

    void Mutex_Lock(void* pMutex, void* pName)  { (void)pMutex; (void)pName; }   // FLAG link-stub (single-threaded)
    void Mutex_Unlock(void* pMutex)             { (void)pMutex; }                // FLAG link-stub (single-threaded)
    int  GetThreadId()                          { return 0; }                    // FLAG link-stub (single-threaded)

    Thread::Status Thread::WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute)
    { (void)pThreadReturnValue; (void)pTimeoutAbsolute; return kStatusEnded; }   // FLAG link-stub

    // One file-scope slot mirroring the single TLS value the bring-up needs.
    static void* gThreadLocalStorageSlot = nullptr;   // FLAG link-stub (single-threaded TLS)
    bool  ThreadLocalStorage::SetValue(const void* pData)
    { gThreadLocalStorageSlot = const_cast<void*>(pData); return true; }         // FLAG link-stub
    void* ThreadLocalStorage::GetValue() { return gThreadLocalStorageSlot; }     // FLAG link-stub

    IRunnable::~IRunnable() {}   // FLAG link-stub (empty virtual dtor)

}}

// The Apt animation-unresolve current-target TLS object (unk_8324E814). The linker
// wants it at GLOBAL scope (?gAptTargetTls@@3V...), NOT in EA::Thread -- it is a global
// variable whose TYPE is EA::Thread::ThreadLocalStorage. Default-constructed via the
// inline ctor -- FLAG: zeroed storage, no TlsAlloc (single-threaded bring-up).
EA::Thread::ThreadLocalStorage gAptTargetTls;   // FLAG link-stub

// ---- EA::Jobs ----------------------------------------------------------------------
// FLAG: jobs run synchronously on the main thread (PC bring-up). The two LocalBackend
// worker entry points (JobInstance::Run / JobThread::Start) are declared minimally to
// avoid pulling job_thread.h / local_backend.h (which transitively include
// BrnEAThreadX360.h and would collide with the int GetThreadId() above).
namespace EA { namespace Jobs {

    Event::Event() {}   // FLAG link-stub (ctor no-op)

    JobThreadHandle::JobThreadHandle(Detail::SchedulerBackend* pBackend, u32 uHandle)   // FLAG link-stub
    { (void)pBackend; (void)uHandle; }
    JobThreadHandle::JobThreadHandle() {}   // FLAG link-stub

    JobAffinity    EntryPoint::GetAffinity()    const { return JOB_AFFINITY_NONE; }    // FLAG link-stub
    JobEnvironment EntryPoint::GetEnvironment() const { return JOB_ENVIRONMENT_LOCAL; }// FLAG link-stub
    JobPriority    EntryPoint::GetPriority()    const { return JOB_PRIORITY_HIGH; }    // FLAG link-stub
    void           EntryPoint::SetName(const char* lpcName) { (void)lpcName; }         // FLAG link-stub

    int Job::GetNumDependencies() const { return 0; }   // FLAG link-stub

    // Minimal LocalBackend-scope decls for the two worker entry points (jobs run
    // synchronously on the main thread, so both are no-ops).
    namespace LocalBackend {
        class LocalBackend;
        struct JobInstance { void Run(); };
        class  JobThread   { public: void Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend); };

        void JobInstance::Run() {}   // FLAG link-stub (synchronous)
        void JobThread::Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend)   // FLAG link-stub
        { (void)pParameters; (void)pBackend; }
    }

}}

// ---- EA::Allocator -----------------------------------------------------------------
namespace EA { namespace Allocator {
    // FLAG link-stub: null default allocator. Prefer nullptr per the bring-up plan; a
    // non-null is only needed if an immediate deref happens (then home a file-scope one).
    ICoreAllocator* ICoreAllocator::GetDefaultAllocator() { return nullptr; }   // FLAG link-stub
}}

// ---- audio (Nicotine / NFSMixMaster) -----------------------------------------------
// FLAG: audio mix deferred; wire after render. Mixer snapshot/map setup is no-op'd and
// the volume curve returns unity gain until the audio path is brought up.
namespace Nicotine {
    void SnapshotMixer::InitSnapshots()    {}   // FLAG link-stub (audio mix deferred)
    void SnapshotMixer::DestroySnapshots() {}   // FLAG link-stub (audio mix deferred)
    void SnapshotMixer::SetSnapshot()      {}   // FLAG link-stub (audio mix deferred)
    double SnapshotVolumeCurve(double lfRatio, int liCurveType)                  // FLAG link-stub
    { (void)lfRatio; (void)liCurveType; return 1.0; }   // unity gain
}

void NFSMixMaster::InitMixMap()                  {}   // FLAG link-stub (audio mix deferred)
void NFSMixMaster::DestroyMainMainMap()          {}   // FLAG link-stub (audio mix deferred)
void NFSMixMaster::AssignSFXCallbacks(void* lpOwner) { (void)lpOwner; }   // FLAG link-stub (audio mix deferred)
