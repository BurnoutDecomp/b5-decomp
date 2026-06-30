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

struct AptDragState;  // FLAG fwd-decl (pointer-only use)

    AptCharacter* AptResolveFontGlyph(AptCharacter* pFontChar, int nGlyphIndex) { return nullptr; }   // FLAG link-stub
    AptCharacter* AptResolveTextFontCharacter(AptCharacter* pFontOwner, int nFontIndex) { return nullptr; }   // FLAG link-stub
    AptCharacter* findCharacterInLibrary(AptCIH* pNode, EAStringC* pName, char bSearchImports) { return nullptr; }   // FLAG link-stub
    AptDragState* AptApt_GetDragState() { return nullptr; }   // FLAG link-stub
    AptFilePtr* AptLoader_LoadX360(AptFilePtr* pOut, AptLoader* pLoader, const EAStringC* pName) { return nullptr; }   // FLAG link-stub
    AptFrameStack* AptScriptFunctionBase_GetActiveFrameStack() { return nullptr; }   // FLAG link-stub
    AptNativeHash* AptValue_EmbeddedNativeHash(AptValue* pValue) { return nullptr; }   // FLAG link-stub
    AptNativeHash* AptInterp_GetNodeFrameContextHash(AptValue* pContext) { return nullptr; }   // FLAG link-stub
    AptTextFormat* AptTextFormat_ConstructDefault(void* pBlock, AptValue* pSource, double dArg) { return nullptr; }   // FLAG link-stub
    AptValue* AptActionInterpreter_SetIntervalImpl(AptValue* pCallback, int nArgCount) { return nullptr; }   // FLAG link-stub
    AptValue* AptApt_DeriveFunctionAnimation(AptValue* pCIH) { return nullptr; }   // FLAG link-stub
    AptValue* AptApt_GetRootContext() { return nullptr; }   // FLAG link-stub
    AptValue* AptCIH_gotoAndX(AptValue* pContext, int nArgCount, int bPlay) { return nullptr; }   // FLAG link-stub
    AptValue* AptExtern_GetMember(const char* szName) { return nullptr; }   // FLAG link-stub
    AptValue* AptInterp_FrameStackFirstLocal(AptValue* pFrameStack) { return nullptr; }   // FLAG link-stub
    AptValue* AptInterp_LookupScopeChain(AptActionInterpreter* pInterp, const EAStringC* pName) { return nullptr; }   // FLAG link-stub
    void* AptUpdateZombieVector(char bClear) { return nullptr; }   // FLAG link-stub (BLOCKED: zombie-vector GC subsystem un-homed -- gpZombieVector/AptPartialGarbageCollection absent from all dumps); canonical void* return reconciled
    AptValue* AptValue_GetMCParent(AptValue* pValue) { return nullptr; }   // FLAG link-stub
    bool         isNaN(AptValue* pValue) { return false; }   // FLAG link-stub
    bool AptLinker_isFileImported(AptLinker* pLinker, AptFilePtr* ppCandidate) { return false; }   // FLAG link-stub
    const char*    AptResolveTextFieldFontName(AptCharacterInst* pTextInst) { return nullptr; }   // FLAG link-stub
    float        Apt_atoff(const char* pStr) { return 0; }   // FLAG link-stub
    int    AptValueGCPool_GetAllocatedCount(void* pPool) { return 0; }   // FLAG link-stub
    int  AptHook_GetBytesTotal(const char* pcFilePath, int a2, double a3) { return 0; }   // FLAG link-stub
    int AptActionInterpreter_InstanceOfChainWalk(AptValue* pObject, AptValue* pClass) { return 0; }   // FLAG link-stub
    int AptCIH_ShapeHitTest(AptValue* pNode, float fX, float fY) { return 0; }   // FLAG link-stub
    int AptInterp_LabelToFrame(AptCIH* pNode, const EAStringC* pLabel) { return 0; }   // FLAG link-stub
    int GetThreadId() { return 0; }   // FLAG link-stub
    uint32_t AptValue_CurrentThreadId() { return 0; }   // FLAG link-stub
    unsigned int AptGetSwfVersion() { return {}; }   // FLAG link-stub
    unsigned int AptRand() { return {}; }   // FLAG link-stub
    void      AptActionInterpreter_ClearIntervalImpl(int nId) {}   // FLAG link-stub
    void      AptApt_AnimationAddCharacterRef(AptValue* pAnimation) {}   // FLAG link-stub
    void      AptApt_AnimationReleaseCharacterRef(AptValue* pAnimation) {}   // FLAG link-stub
    void      AptApt_PrepareCallContextScope(AptValue* pCallContext) {}   // FLAG link-stub
    void      AptExtern_SetMember(const char* szName, const char* szValue) {}   // FLAG link-stub
    void  AptActionInterpreter_runStream(void* pVM, void* pStream, void* pCIH, int nFrame, void* pScope) {}   // FLAG link-stub
    void  AptCharacterAnimation_ExecuteInitActions(void* pAnim, void* pCIH, int nId) {}   // FLAG link-stub
    void  AptFreeFontUnit(void* pUnit) {}   // FLAG link-stub
    void  AptFreeRenderingUnit(void* pUnit) {}   // FLAG link-stub
    void  AptPseudoDisplayList_Insert(void* pList, AptPseudoCIH_t* pNode) {}   // FLAG link-stub
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
    void AptActionInterpreter_getName(AptCIH* pNode, EAStringC* pOut) {}   // FLAG link-stub
    void AptActionInterpreter_stackPushIndirect(AptActionInterpreter* pInterp, AptValue* pValue) {}   // FLAG link-stub
    void AptAnimationTargetSet_Construct(AptAnimationTargetSet* pSet, u16 nCapacity) {}   // FLAG link-stub
    void AptAnimationTargetSet_Destruct (AptAnimationTargetSet* pSet) {}   // FLAG link-stub
    void AptAnimationTargetSet_Destruct2(AptAnimationTargetSet* pSet) {}   // FLAG link-stub
    void AptAnimationTarget_TickNewInsts(AptAnimationTarget* pAnim) {}   // FLAG link-stub
    void AptApt_FlushDeferredReleases() {}   // FLAG link-stub
    AptValue* AptApt_LoadVariablesFetch(const char* pUrl) { return 0; }   // FLAG link-stub (host URL fetch; null until installed)
    void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY) {}   // FLAG link-stub
    void AptApt_PopValues(AptActionInterpreter* pInterp, int nCount) {}   // FLAG link-stub
    void AptCIH_GetWorldBounds(AptValue* pNode, float* pOutRect) {}   // FLAG link-stub
    void AptCIH_SetDirtyState(AptCIH* pNode, bool bDirty, bool bProp) {}   // FLAG link-stub
    void AptCIH_SetProceduralProperty(AptCIH* pNode, int nProperty, double fValue) {}   // FLAG link-stub
    void AptCIH_jumpToFrame(AptCIH* pNode, int nFrame) {}   // FLAG link-stub
    // AptCIH_tick is now homed faithfully in AptCIHBehaviour.cpp (forwards to AptCIH::tick).

    // ---- AptCIH "link cluster" deferred sub-paths: the deep callees the now-homed
    // AptCIHBehaviour.cpp bodies (queueClipEvents / GeneralisedProcess / ClearCIH /
    // AddToDelayReleaseList / PreDestroyHook) FLAG out. Faithful "deferred subsystem"
    // defaults until the AS-interpreter execution / generalised-process gate / GC zombie
    // subsystems land. -------------------------------------------------------------------
    void AptAnimationTarget_AddToRemList(AptAnimationTarget* pAnim, AptCIH* pItem) {}   // FLAG link-stub
    void (*gpAptCIHPreDestroyHook)(AptCIH* pCIH) = nullptr;   // FLAG link-stub (dword_8324E8A0; null until installed)
    int  AptCIH_queueClipEvents_RunMatched(AptCIH* pNode, int nEventMask, unsigned int nFrameId, int bDeferred) { return 0; }   // FLAG link-stub
    int  AptCIH_ClearCIH_DrainQueuesAndZombie(AptCIH* pNode, bool bClearGCRoots) { return 0; }   // FLAG link-stub
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
    void AptLoader_StartAsyncLoad(const char* pFileName, AptFilePtr* pFile) {}   // FLAG link-stub
    void AptMovie_runFrameActions(void* pFrameActionList) {}   // FLAG link-stub
    void AptObject_SetImplementedObjects(AptObject* pObject, AptArray* pInterfaces, int nCount) {}   // FLAG link-stub
    void AptScriptFunctionBase_InitializeStaticData(const AptInitParmsT* pParms) {}   // FLAG link-stub
    void AptScriptFunctionBase_PopStaticData(AptScriptFunctionBase::SavedExecutionState* pSaved) {}   // FLAG link-stub
    void GlobalNotificationFunction(AptFilePtr* pFile) {}   // FLAG link-stub
    void Mutex_Lock(void* pMutex, void* pName) {}   // FLAG link-stub
    void Mutex_Unlock(void* pMutex) {}   // FLAG link-stub
    void TextFormat_copyTextFormatObj(TextFormat* pDest, const TextFormat* pSource) {}   // FLAG link-stub
    void escape(EAStringC* pString) {}   // FLAG link-stub
    void unescape(EAStringC* pString) {}   // FLAG link-stub
    void* AptActionInterpreter_CleanupAfterExecution(void* pVM, void* pSavedScratch, void* pLocalState) { return nullptr; }   // FLAG link-stub
    void* AptFile_operator(void* pDst, void* pSrc) { return nullptr; }   // FLAG link-stub
    void* sub_82AFD150(void* a1, int a2) { return nullptr; }   // FLAG link-stub
    void* sub_82B0AE08(void* a1, float* a2, void* a3) { return nullptr; }   // FLAG link-stub
    void** AptValueGC_PoolManager_GetAllAllocatedAptValues(void* pPool) { return nullptr; }   // FLAG link-stub
