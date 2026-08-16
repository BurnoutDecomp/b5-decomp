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
#include "SDKs/EATech/include/NFSMix/NFSMixMap.hpp"       // NFSMixMap (AssignSFXCallbacks forward)
#include "SDKs/EATech/include/NFSMix/NFSMixMapState.hpp"  // NFSMixMapState (the CreateMainMapState builder chain)
#include "SDKs/EATech/include/NFSMix/NFSMixRecords.hpp"   // stMixMapHeader / stMixMapStateHdr (serialized MixMap blob records)
#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"  // g_pMixerAllocator (off_83250004)
#include <cstring>                                         // std::memset (SnapshotMixer::InitSnapshots)


    // AptResolveFontGlyph RETIRED (2026-07-02): homed in AptRenderItemStaticText.cpp.
    // AptResolveTextFontCharacter RETIRED (2026-07-02): homed in AptRenderItemStaticText.cpp.
    // findCharacterInLibrary RETIRED (2026-07-02): homed in
    // AptCIHNativeFunctionHelper.cpp (the X360 @0x82AFDF58 parent-chain
    // export/import library resolve).
    // AptApt_GetDragState RETIRED (2026-07-02): homed in
    // AptActionInterpreterSpecialOps.cpp (the view over the director's
    // mpDragMC..mGrabOffset run -- X360 StartDragMovie @0x82B03B00).
    // AptLoader_LoadX360 RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the by-value-return wrapper over the homed AptLoader::Load).
    // AptScriptFunctionBase_GetActiveFrameStack RETIRED (2026-07-02): homed in AptFrameStack.cpp.
    // AptValue_EmbeddedNativeHash RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp (the AptValueWithHash mHash @+8).
    // AptInterp_GetNodeFrameContextHash RETIRED (2026-07-02): homed in AptActionInterpreterInterpHelpers.cpp
    // (setVariable @0x82B03374: the display PARENT's char-inst property hash -- the enclosing clip's scope).
    // AptTextFormat_ConstructDefault RETIRED (2026-07-02): homed in
    // AptTextFormat.cpp (sub_82AFB2A8 == the public ctor + the homed
    // ConstructRecord with the all-inherit tail). THE LAST ENGINE STUB --
    // everything remaining in this file is the genuine PC-leaf KEEP list.
    // AptActionInterpreter_SetIntervalImpl RETIRED (2026-07-02): homed in
    // AptIntervalTimer.cpp (the X360 cbCallMethod_setInterval @0x82B019D8 body).
    // AptApt_DeriveFunctionAnimation RETIRED (2026-07-01): homed in AptScriptFunctionBase.cpp
    // (== AptCIH::GetRootAnimation, the enclosing-timeline walk).
    // AptApt_GetRootContext RETIRED (2026-07-02): homed in
    // AptActionInterpreterContext.cpp (== AptGetAnimationAtLevel(0), the
    // level-0 root the absolute "/" paths resolve from).
    // AptCIH_gotoAndX RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the real AptCIH::_gotoAndX @0x82B0D2F0 -- label/frame goto core).
    // host extern-object member-GET callback slot (X360 dword_8324E858); the host installs it for
    // AptVFT_Extern (type-11) script objects -- none are registered on the PC title path, so the
    // un-installed slot faithfully answers null (matches the X360 null fn-ptr slot, not an engine stub).
    AptValue* AptExtern_GetMember(const char* szName) { return nullptr; }   // FLAG PC-platform leaf
    // AptFrameStackFirstLocal RETIRED (2026-07-02): homed in AptArray.cpp (a tag-14 ARRAY's first element).
    // AptLookupScopeChain RETIRED (2026-07-02): homed in AptFrameStack.cpp (spFrameStack->GetInScopeChain).
    // AptUpdateZombieVector RETIRED (2026-07-02): homed in AptGC.cpp (the real
    // reap over gpAptZombieVector -- XB1 sub_140830A40; the vector itself is
    // allocated by AptUpdateInitialize from config word 14). The old "absent
    // from all dumps" claim was false -- the whole subsystem is in the XB1.
    // AptValue_GetMCParent RETIRED (2026-07-02): the shim was a reconstruction
    // invention -- the shipped isMCInParentChain @0x82AD8458 walks
    // GetNativeHashVirtual()->mp__Proto__ directly (corrected in AptValue.cpp).
    // isNaN RETIRED (2026-07-02): homed in AptActionInterpreterBuiltins.cpp
    // (the full @0x82AF9768 ECMA-ish NaN classification incl. the SWF7 arm).
    // AptLinkerIsFileImported RETIRED (2026-07-10): homed as the real member
    // AptLinker::isFileImported @0x82AECC58 (AptLinker.cpp) -- the false stub made
    // the cancel path treat every candidate import as un-imported.
    // AptResolveTextFieldFontName RETIRED (2026-07-10): homed in AptCIHText.cpp (the
    // fontID -> owning movie charTable -> type-3 font-char name walk; the "" stub
    // blanked the getTextFormat/setTextFormat round-trip's font name -- every
    // AS-re-formatted field fell back to the collection's first typeface).
    // Apt_atoff RETIRED (2026-07-02): homed in AptValueConvert.cpp
    // (PS3 @0x7E2990 == (float)strtod; the stub's 0 broke every string->number).
    // AptValueGCPool_GetAllocatedCount RETIRED (2026-07-02): homed in
    // AptValueGCPoolManager.cpp (the DOGMA mnItemsAllocated counter).
    // AptHook_GetBytesTotal RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the host gAptFuncs.pfnGetBytesTotal dispatch @+0x94).
    // AptActionInterpreter_InstanceOfChainWalk RETIRED (2026-07-02): homed in
    // AptActionInterpreter.cpp (the X360 isObjectOfType @0x82AEA5B8 object arm).
    // AptCIH_ShapeHitTest RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the host pfnPointHitTest dispatch, X360 dword_8324E8A4 == gAptFuncs+0x8C).
    // AptInterp_LabelToFrame RETIRED (2026-07-02): homed in AptCIHNativeFunctionHelper.cpp
    // (the clip movie's label-hash lookup, X360 @0x82B0C618 chain).
    int GetThreadId() { return 0; }   // FLAG PC-platform leaf: single-threaded PC (one thread id)
    uint32_t AptCurrentThreadId() { return 0; }   // FLAG PC-platform leaf: single-threaded PC (one thread id)
    // AptGetSwfVersion RETIRED (2026-07-02): homed in AptLinker.cpp -- the
    // dword_8324E530 SWF-version cache (parsed from the .apt "Apt Data:1:7:8"
    // header at first link; NOT a frame rate as previously misread).
    // AptRand RETIRED (2026-07-02): homed in AptRandom.cpp (the X360 MT19937
    // variant @0x82AE04F0 -- custom tempering b 0x9D2C56FF, auto-seed 0x1105).
    // AptActionInterpreter_ClearIntervalImpl RETIRED (2026-07-02): homed in
    // AptIntervalTimer.cpp (the X360 cbCallMethod_clearInterval @0x82AE3AE0 body).
    // AptAnimationAdd/ReleaseCharacterRef RETIRED (2026-07-10): the "+0x0C character
    // ref" is the CIH ZOMBIE-COUNT bitfield (bits 7-22, step 0x80 -- decoded from the
    // ScriptFunctionBase ctor asm); the ctor/dtor call Inc/DecZombieCount directly.
    // The {} stubs dropped every function-value zombie-count balance.
    // AptPrepareCallContextScope RETIRED (2026-07-10): the ctor's vtbl+0x60 dispatch
    // is CreateFrameStack (@0x82AF1260) -- the ScriptFunctionBase ctor calls the
    // member directly. The {} stub left nested closures with a null parent frame.
    // host extern-object member-SET callback slot (X360 dword_8324E854); un-installed on the PC
    // title path (no type-11 extern objects) -> faithful no-op, matching the X360 null fn-ptr slot.
    void      AptExtern_SetMember(const char* szName, const char* szValue) {}   // FLAG PC-platform leaf
    // AptActionInterpreter_runStream RETIRED (IGNITION 2026-07-01): the init passes call the real
    // member gAptActionInterpreter.runStream (AptActionRun.cpp dispatch loop) -- ActionScript executes.

    // AptStringPoolReleaseString RETIRED (2026-07-10): homed as
    // StringPool::ReleaseString (XB1 sub_14083F2A0, AptStringPool.cpp) -- decGCRoot
    // + last-use bucket unchain/Release, the inverse of FindOrCreate's hit path.
    // FLAG deferred (NOT a clean swap -- boot-verified 2026-07-04): the real member
    // AptCharacterAnimation::ExecuteInitActions (AptCharacterAnimation.cpp, X360 0x82AF4340) is homed
    // but its init-action VM path RUNS AWAY at boot frame 3 (a flood of mkitem instantiation -> crash)
    // because the deeper import->AptFile->embedded-movie chase is still deferred. Keep this no-op until
    // that sub-record path is homed, THEN swap the AptMovie::doFrameControls tag-8 caller to the member.
    void  AptExecuteInitActionsGate(void* pAnim, void* pCIH, int nId) {}
    void  AptFreeFontUnit(void* pUnit) {}   // FLAG PC-platform leaf: host render-unit free callback (un-installed on PC)
    void  AptFreeRenderingUnit(void* pUnit) {}   // FLAG PC-platform leaf: host render-unit free callback (un-installed on PC)
    // AptPseudoDisplayList_Insert RETIRED (2026-07-01): homed member AptPseudoDisplayList::Insert
    // (AptPseudoDisplayList.cpp, faithful list-insert) called directly in AptMovie; the {} stub dropped it.
    // AptPseudoDisplayListFindInst RETIRED (2026-07-10): the console
    // DoTemporaryFrameControls @0x82AEEB98 place arm calls the real 3-arg member
    // AptPseudoDisplayList::FindInst (homed in AptPseudoDisplayList.cpp) -- the
    // invented 6-arg shape here nulled every lookup, so jumpToFrame replays
    // re-created every already-live pseudo node instead of merging onto it.
    // AptValue_setGCRoot RETIRED: the real member AptValue::setGCRoot(int) is called directly
    // (AptAnimationTarget.cpp, AptLinker.cpp, AptString.cpp).
    // AptActionInterpreter_UnEscape RETIRED (2026-07-02): homed in
    // AptActionInterpreter.cpp (X360 _unEscape @0x82AEE110 + _escape2Char).
    // AptActionInterpreter_getName RETIRED (2026-07-02): homed in
    // AptCIHNativeFunctionHelper.cpp (getName @0x82AF75C8 + the recursive
    // sub_82AF7400 target-path builder).
    // AptActionInterpreter_stackPushIndirect RETIRED (2026-07-01): homed as the real member
    // AptActionInterpreter::stackPushIndirect in AptActionInterpreter.cpp; caller uses the member.
    // AptAnimationTargetSetConstruct is now HOMED faithfully in AptAnimationTarget.cpp
    // (sub_82AE1708: allocate the slot array + set capacity; native-8 pointer stride).
    // AptAnimationTargetSetDestruct/2 RETIRED (2026-07-10): homed in
    // AptAnimationTarget.cpp (sub_82AE1670/sub_82AE1780, ICF twins -- Release each
    // live slot + free the slot array; the {} stubs leaked both at teardown).
    // AptAnimationTarget_TickNewInsts RETIRED: the real static AptAnimationTarget::TickNewInsts()
    // (AptAnimationTarget.cpp:562, X360 0x82B0C8E0 -- drains the module new-instance table) is now
    // called directly at its one call site (AptCIHNativeFunctionHelper.cpp duplicateMovieClip).
    // AptFlushDeferredReleases RETIRED (2026-07-01): homed in AptGC.cpp as the real
    // gValuesToRelease.ReleaseValues() drain (the {} stub dropped every GC drain).
    // Host URL-fetch callback slot (dword_8324E84C/..850), null on the PC title path (no host
    // loadVariables installed) -- the same host boundary as AptExtern_SetMember above.
    // FLAG PC-platform leaf: host callback slot, faithfully null on PC.
    AptValue* AptApt_LoadVariablesFetch(const char* pUrl) { return 0; }   // dword_8324E84C/850 un-installed -> null

    // AptApt_GetDragTargetTranslate -- the drag target's clip matrix translation
    // (X360 StartDragMovie @0x82B03A20: v11 = *(*(*(Variable+0x20)+4)+8) -- the
    // CIH -> mpCharacterInst -> render item's position matrix, with the null-matrix
    // fallback to the identity flt_8324E2B0 == gAptIdentityMatrix; translate =
    // v11[4]/v11[5] == tx/ty). The homed AptRenderItem::GetPositionMatrixConst
    // performs exactly that raw read + identity fallback.
    void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY)
    {
        const AptMatrix* pPos = static_cast<AptCIH*>(pDragTarget)->GetCharacterInst()
                                    ->GetRenderItem()->GetPositionMatrixConst();
        *pOutX = pPos->tx;   // matrix +0x10 (console v11[4])
        *pOutY = pPos->ty;   // matrix +0x14 (console v11[5])
    }
    // AptApt_PopValues RETIRED: it IS AptActionInterpreter::stackPop(int) (AptActionInterpreter.cpp:65,
    // @0x7FDB68 -- "pop nCount values, releasing each"; ICF-folded as Burnout_X360_Artist_01e3_0). The
    // ControlOps/StackOps call sites call the member directly; the {} shim skipped every collapse.
    // AptCIH_GetWorldBounds RETIRED: its body IS the shared GetBoundingRectClamped
    // (AptCIHBehaviour.cpp, X360 sub_82AE2C58, asm-verified 2-arg (AptCIH* r3, float* r4)); the
    // getBounds/hitTest native methods now call it directly (AptValue clip -> AptCIH via static_cast).
    // AptCIH_SetDirtyState RETIRED (2026-07-01): the real member AptCIH::SetDirtyState
    // (AptCIH.cpp, faithful) is called directly; the {} stub dropped the dirty latch.
    // AptCIH_SetProceduralProperty RETIRED: the real member AptCIH::SetProceduralProperty
    // (AptCIHBehaviour.cpp:963, X360 0x82AE73C0 -- 4th arg bASChanged is r6, asm-verified) is called
    // directly by createTextField; the invented shim dropped the value AND the bASChanged flag.
    // AptCIH_jumpToFrame RETIRED (2026-07-01): homed member AptCIH::jumpToFrame (AptCIH.cpp,
    // faithful play-head seek) called directly at all 5 VM sites; the {} stub dropped every seek.
    // AptCIH_tick is now homed faithfully in AptCIHBehaviour.cpp (forwards to AptCIH::tick).

    // ---- AptCIH "link cluster" statics: the .data slots the homed
    // AptCIHBehaviour.cpp bodies (queueClipEvents / GeneralisedProcess / ClearCIH /
    // AddToDelayReleaseList / PreDestroyHook) reach. All boot zero/null on the
    // console exactly as defined here -- these ARE the faithful homes. ------------------
    // AptAnimationTargetAddToRemList RETIRED (2026-07-10): homed in
    // AptAnimationTarget.cpp (@0x82AEE3F8 -- queue on the shared delayed-release
    // table with the bit26 latch + CleanRemList overflow flush; the {} stub
    // dropped every delay-released clip).
    // gpAptCIHPreDestroyHook RETIRED (2026-08-11): console dword_8324E8A0 is not a
    // standalone global -- it is gAptFuncs+0x88 == gAptFuncs.pfnOnUnload (the slot
    // AptAux::ConstructApt installs with CgsGui::AptCallbackFile::OnUnload). The
    // parallel never-installed global meant AptCIH::PreDestroy never released the
    // AptCommunicator component registrations (256-entry table overflow at the
    // menus); AptCIHBehaviour.cpp now dispatches gAptFuncs.pfnOnUnload directly.
    // AptQueueClipEventsRunMatched RETIRED (2026-07-01): homed faithfully in
    // AptCIHBehaviour.cpp from the PS3 body @0x815BD0 (the clip-event record scan +
    // AddActionFront/Back enqueues; the byte-code-block + __proto__ tails staged there).
    // AptClearCIHDrainQueuesAndZombie RETIRED (2026-07-02): homed in
    // AptCIHBehaviour.cpp (the director-set/new-inst drain + the unload-event tail;
    // the zombie-vector decision stays documented + staged there).
    // The GeneralisedProcess gate + callback statics (AptCIH::bEarlyReturn /
    // sCIHProcessCb[0..2] / nTreeDepth). All boot zero/null on the console exactly
    // as here; AptUpdate.cpp installs/swaps the three callbacks around its process
    // pass and AptCIHBehaviour.cpp reads them -- faithful .data homes.
    bool AptCIH_sbGeneralisedProcessEarlyReturn = false;   // bEarlyReturn (boot 0)
    unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*)  = nullptr;   // dword_8324E41C
    unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*) = nullptr;   // dword_8324E420
    unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*) = nullptr;   // dword_8324E424
    int  AptCIH_snGeneralisedProcessTreeDepth = 0;   // nTreeDepth (boot 0)

    // AptCIH::ProcessCustomControls -- the per-frame custom-control refresh pass the
    // AptUpdate slot install (dword_8324E420) targets.
    //
    // ⚠️⚠️ THE BANNER THAT USED TO BE HERE WAS WRONG. It said "its X360 body has no
    // per-address export in the dump set". It has one: **AptCIH::ProcessCustomControls
    // @0x82B07788**, with complete pseudocode, assembly and xrefs. (Checked 2026-08-16
    // while chasing why the mounted GUI custom-renderer layer was never being called.)
    //
    // ⛔ IT IS STILL A STUB, AND THAT IS NOW THE ONE REMAINING BREAK IN THE CUSTOM-CONTROL
    // CHAIN. Everything downstream of it is live as of 2026-08-16:
    //     movie types the clip `_type='PlayerImage' _index=1`
    //       -> [THIS FUNCTION classifies the clip + fills the render item's Type/Target/
    //           Properties strings]                                  <-- returns false
    //       -> AptRenderItemCustomControl::Render @0x82AEF8F8         (mounted, real)
    //       -> gAptFuncs.pfnCustomControlRender                       (installed)
    //       -> CgsGui::AptCallbackCustom::ControlRender @0x8285BFA0   (reconstructed)
    //       -> AptRenderHandler::mpCustomRendererManager              (installed + read)
    //       -> BrnGui::CustomRendererManager::GetComponentTexture     (mounted)
    //       -> NetworkPlayerImageRenderer::GetRenderOutput            (mounted)
    // MEASURED: with this returning false the boot log records ZERO
    // `[custrend] ControlRender` lines. Nothing else in the chain is missing.
    //
    // What a reconstruction needs beyond the pseudocode (recorded so the next pass does
    // not re-derive it):
    //   * the AptCharacterInst +0x14 classification bitfield -- bits 4..5 hold
    //     {0 = unclassified, 1 = string-style custom control, 2 = NOT a custom control,
    //      3 = zid-style}; the pass caches its verdict there and only re-derives it when
    //     the field is 0.
    //   * the two native-hash keys it looks the clip's variables up by: unk_8324E5C8 (the
    //     `_type` key -- the same variable the licence movie's PLACE tag sets) and the
    //     literal "_CustomControlType".
    //   * AptActionInterpreter::getVariable(&dword_8324E760, node, 0, &unk_8324E5C0, 1,1,0)
    //     for the TARGET string.
    //   * AptValue::urlEncodeCustomRender @0x82AF9410 for the PROPERTIES blob -- that is
    //     what produces the "_index=1" text AptCallbackCustom::ControlRender parses.
    //   * gAptFuncs.pfnCustomControlUpdate (dword_8324E890) gates the properties refresh;
    //     a null slot means "always refresh", so it does NOT have to be installed first.
    //   * the zid arm additionally needs gbAptCustomControlRenderEnabled (byte_82F733F6),
    //     which this build leaves false -- so only the string arm matters.
    // ⚠️ It runs on EVERY display-list node EVERY frame and can promote a live sprite
    // render item to a custom control (AptRenderItemCustomControl::CopyFromSprite +
    // AptCharacterInst::SetRenderItem), so a wrong classification breaks the whole GUI,
    // not just custom controls. Reconstruct it with a control, not by eye.
    bool AptCIH::ProcessCustomControls() { return false; }

    // AptGC::CleanUnreachable -- the partial sweep AptUpdate runs on the
    // zombies-dirty flag (raised by AptPartialGarbageCollection). No per-address
    // export in the dump set; the empty body leaves the values to the full
    // CleanAll teardown until it is exported + reconstructed (AptUpdate still
    // clears the flag, matching the state before AptUpdate existed, where
    // nothing consumed the flag at all).
    void AptGC::CleanUnreachable() {}

    // The saved-input REPLAY driver (sub_82B0D7E8, ~4.3KB): drains the recorded
    // input stream instead of live-ticking. Gated on gbAptSavedInputActive (boot
    // default 0); the empty body is unreachable until a host arms the replay,
    // and its own TU lands before that feature does.
    void AptUpdateReplaySavedInputs(int, int) {}

    // Host debug-output sink (console dword_8324E82C, a printf-style hook the host installs).
    // FLAG PC-platform leaf: host debug sink, faithfully a no-op until the host wires it.
    void (*gpAptHookTraceFn)(const char* szFormat, const char* szMessage) = nullptr;   // dword_8324E82C
    void AptHook_Trace(const char* szFormat, const char* szMessage)
    {
        if (gpAptHookTraceFn)
            gpAptHookTraceFn(szFormat, szMessage);
    }

    // AptKeyManagerAddListener -- the Key-listener registration tail of
    // sMethod_addListener (X360 @0x82ADC6E0): scan the director's mListenerSet
    // (gpAptTarget->mpAnimationTarget+0x10; scan bound = mnCapacity, @0x82ADC764) --
    // already present -> no-op -- else the shared set `add` (__::add @0x82ADBCE0):
    // store head = count+1, probe forward from slots[head] for the first free slot
    // (wrapping at capacity: `li r10,-1; addi r10,r10,1`), store the listener and
    // AddRef it (vtbl[0] tail-call). The modulo probe is the committed sibling idiom
    // (AptCIHMembers.cpp AddNodeToInputSet) -- identical slot choice for head < cap,
    // in-bounds where the console's raw slots[cap] read is UB.
    void AptKeyManagerAddListener(AptValue* pListener)
    {
        AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mListenerSet;

        const u32 luCap = pSet->mnCapacity;                    // lhz +2
        for (u32 lu = 0; lu < luCap; ++lu)
            if (pSet->mppSlots[lu] == pListener)               // membership scan @0x82ADC770
                return;
        if (luCap == 0)
            return;                                            // un-built set: nothing to add into

        const u16 nHead = static_cast<u16>(pSet->mnCount + 1u);
        pSet->mnCount = nHead;                                 // sth head (stored before the probe)
        u32 luNext = static_cast<u32>(nHead) % luCap;
        u32 luScanned = 0u;
        while (luScanned < luCap && pSet->mppSlots[luNext] != nullptr)
        {
            luNext = (luNext + 1u) % luCap;                    // wrap at capacity
            ++luScanned;
        }
        if (pSet->mppSlots[luNext] == nullptr)
        {
            pSet->mppSlots[luNext] = pListener;
            pListener->AddRef();                               // vtbl[0] tail-call
        }
    }

    // AptKeyManagerRemoveListener -- the shared set `remove` (__::remove @0x82ADBC28)
    // over the same director mListenerSet: empty (count 0) -> false; linear-scan the
    // slots (bound = capacity) for pListener; on a hit decrement the count, Release
    // the slot's value (vtbl[1]) and null the slot. True iff one was removed.
    bool AptKeyManagerRemoveListener(AptValue* pListener)
    {
        AptAnimationTargetSet* const pSet = &gpAptTarget->GetAnimationTarget()->mListenerSet;

        if (pSet->mnCount == 0)                                // lhz +0; beq -> 0
            return false;

        const u32 luCap = pSet->mnCapacity;                    // lhz +2
        u32 luIndex = 0;
        while (luIndex < luCap && pSet->mppSlots[luIndex] != pListener)
            ++luIndex;
        if (luIndex >= luCap)
            return false;                                      // not found

        pSet->mnCount = static_cast<u16>(pSet->mnCount - 1);   // sth (count-1)
        pSet->mppSlots[luIndex]->Release();                    // vtbl[1]
        pSet->mppSlots[luIndex] = nullptr;
        return true;
    }
    // AptLinkerGetUrlLoad RETIRED (2026-07-10): the getURL/getURL2 .swf arm calls
    // the homed member AptLinker::Load(EAStringC*, EAStringC*) @0x82B06660 directly
    // (the {} forwarder dropped every getURL movie load).
    // FLAG PC-platform leaf: host async-stream cancel hook (dword_8324E83C) -- the
    // PC stream hook (AptLoaderStartAsyncLoad) loads synchronously, so nothing is
    // ever in flight to cancel; the empty body matches the un-installed slot.
    void AptLoaderCancelAsyncLoad(void* pDataBlock) {}
    // AptLoaderStartAsyncLoad is HOMED in BrnGuiAptRuntime.cpp (the platform stream hook: it
    // synchronously content-loads the import bundle + drives AptCompleteAnimationAsyncLoad). The
    // former link-stub here was removed so the strong host definition is the only one.
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
    // GlobalNotificationFunction RETIRED (2026-07-16): homed as the real @0x82B00C78
    // body (AptLinker.cpp) -- the loader's "file linked" notify into the current
    // target's linker pending list. The {} stub silently dropped every async movie
    // load completion (nothing ever mounted through the engine linker).
    void Mutex_Lock(void* pMutex, void* pName) {}   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    // AptMath::ClipStackShutdown RETIRED (2026-08-07): homed in AptMath.cpp from the
    // targeted export @0x82AE24E8 (the ClipStackInit inverse: pool-Deallocate the raw
    // allocation at the init-matching size, then clear the base; AptRenderShutdown
    // @0x82B0C2F0 calls it first). The old "body un-exported" park is closed.
    void Mutex_Unlock(void* pMutex) {}   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    // TextFormat_copyTextFormatObj RETIRED (2026-07-10): homed as the real member
    // TextFormat::copyTextFormatObj @0x82AE5820 (AptTextFormat.cpp) -- the {} stub
    // dropped every get/setTextFormat record copy.
    // escape/unescape RETIRED (2026-07-10): homed as AptActionInterpreter::escape
    // @0x82AEE008 / ::unEscape @0x82AEE110 (AptActionInterpreter.cpp) -- the {}
    // stubs made the AS escape()/unescape() builtins identity transforms.
    // AptActionInterpreter_CleanupAfterExecution RETIRED (IGNITION 2026-07-01): the real member
    // (thrown-value drop + PopStaticData window pop) is called directly with the saved base.
    // AptFileAssign DELETED (2026-07-10): the AptFile::operator= wrapper had no
    // remaining callers (the loader paths use the AptSharedPtr refcount helpers).
    // sub_82AFD150 (the remove-command dispatcher @0x82AFD150) is now HOMED faithfully as
    // AptDispatchRemoveCommand in AptMovie.cpp (findInst by depth + removeObject). The null
    // link-stub -- which dropped every timeline remove -- is retired.
    // sub_82B0AE08 (the place-command dispatcher @0x82B0AE08) is now HOMED faithfully as
    // AptDispatchPlaceCommand in AptMovie.cpp (reads the PlaceObject record + calls the homed
    // AptDisplayList::placeObjectNCXForm). The null link-stub is retired.
    // AptValueGC_PoolManager_GetAllAllocatedAptValues RETIRED (2026-07-02): homed in
    // AptValueGCPoolManager.cpp (the CleanAll pool-walk snapshotted flat).

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

    Device* Manager::RegisterDevice(const void* lpDeviceDesc, int liFlags)   // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpDeviceDesc; (void)liFlags; return nullptr; }

    int Manager::UnregisterDevice() { return 0; }   // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path

    Device* Device::GetInstance(const char* lpcPath, char* lpScratch)        // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpcPath; (void)lpScratch; return nullptr; }

    int Device::Wait(AsyncOp* lpOp, const void* lpTimeout)                    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpOp; (void)lpTimeout; return 0; }

    int Device::InsertOp(AsyncOp* lpOp) { (void)lpOp; return 0; }             // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path

    int Device::ChangeOpPriority(AsyncOp* lpOp, int liPriority)               // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path
    { (void)lpOp; (void)liPriority; return 0; }

    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path (off_8327F078 manager unused)
    Manager* gpFileSysManager = nullptr;
    // extern: namespace-scope const defaults to internal linkage; force external.
    // FLAG PC-platform leaf: PC bundle FS is the DeviceManager replay path (zero-init driver vtable)
    extern const DeviceDriverVTable gDeviceDriverVTable = {};

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

    void Mutex_Lock(void* pMutex, void* pName)  { (void)pMutex; (void)pName; }   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    void Mutex_Unlock(void* pMutex)             { (void)pMutex; }                // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    int  GetThreadId()                          { return 0; }                    // FLAG PC-platform leaf: single-threaded PC (one thread id)

    Thread::Status Thread::WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute)
    { (void)pThreadReturnValue; (void)pTimeoutAbsolute; return kStatusEnded; }   // FLAG PC-platform leaf: single-threaded PC (thread already ended)

    // One file-scope slot mirroring the single TLS value the bring-up needs.
    static void* gThreadLocalStorageSlot = nullptr;   // FLAG PC-platform leaf: single-threaded TLS (one slot)
    bool  ThreadLocalStorage::SetValue(const void* pData)
    { gThreadLocalStorageSlot = const_cast<void*>(pData); return true; }         // FLAG PC-platform leaf: single-threaded TLS (one slot)
    void* ThreadLocalStorage::GetValue() { return gThreadLocalStorageSlot; }     // FLAG PC-platform leaf: single-threaded TLS (one slot)

    IRunnable::~IRunnable() {}   // FLAG PC-platform leaf: single-threaded PC (empty virtual dtor)

}}

// The Apt animation-unresolve current-target TLS object (unk_8324E814). The linker
// wants it at GLOBAL scope (?gAptTargetTls@@3V...), NOT in EA::Thread -- it is a global
// variable whose TYPE is EA::Thread::ThreadLocalStorage. Default-constructed via the
// inline ctor -- FLAG: zeroed storage, no TlsAlloc (single-threaded bring-up).
// FLAG PC-platform leaf: single-threaded TLS (zeroed storage; the faithful ctor does TlsAlloc)
EA::Thread::ThreadLocalStorage gAptTargetTls;

// ---- EA::Jobs ----------------------------------------------------------------------
// FLAG: jobs run synchronously on the main thread (PC bring-up). The two LocalBackend
// worker entry points (JobInstance::Run / JobThread::Start) are declared minimally to
// avoid pulling job_thread.h / local_backend.h (which transitively include
// BrnEAThreadX360.h and would collide with the int GetThreadId() above).
namespace EA { namespace Jobs {

    Event::Event() {}   // FLAG PC-platform leaf: synchronous jobs on PC (no event state)

    JobThreadHandle::JobThreadHandle(Detail::SchedulerBackend* pBackend, u32 uHandle)   // FLAG PC-platform leaf: synchronous jobs on PC
    { (void)pBackend; (void)uHandle; }
    JobThreadHandle::JobThreadHandle() {}   // FLAG PC-platform leaf: synchronous jobs on PC

    // The vendor accessors (declaration-only in entry_point.h): each returns its
    // member (DWARF entry_point.h:80-82; layout ARTIST-verified via SetAffinity
    // @0x82BC98B0 storing mAffinity at +20). Trivial reads, inlined on the console.
    // (Merge 2026-08-11: these real member reads supersede origin/dev's hard-coded
    // JOB_AFFINITY_NONE/LOCAL/HIGH link-stub answers.)
    JobAffinity    EntryPoint::GetAffinity()    const { return mAffinity; }
    JobEnvironment EntryPoint::GetEnvironment() const { return mEnvironment; }
    JobPriority    EntryPoint::GetPriority()    const { return mPriority; }
    // ⛔⛔ SILENT-DROP STUB DELETED 2026-08-10 (fill-worker wave 2). What stood here was
    //     void EntryPoint::SetName(const char* lpcName) { (void)lpcName; }
    // labelled "FLAG PC-platform leaf: synchronous jobs on PC (no worker names)". The REAL
    // 22-instruction body (X360 0x82BC9858) was in entrypoint.cpp the whole time -- but that
    // TU declared its own forked `class EntryPoint` whose SetName returned `char*`, so it
    // mangled to a DIFFERENT symbol and this stub won every link, silently, for every caller.
    // Retiring ODR fork #3 (entrypoint.cpp's local class) is what made the two collide and
    // exposed it. Deleted; the real body serves now. (origin/dev's matching ⚠ about the
    // GetNumDependencies 0-stub is retired by the real body below, from this branch.)

    // The mDependencies twin of the homed Job::GetNumDependents @0x82BCA390
    // (job.cpp): this node's bucket count + the overflow chain's ListSize
    // (@0x82BCA030). The 0 stub starved JobScheduler::AddTree's dependency walk
    // (job_scheduler.cpp:199) of every real dependency.
    int Job::GetNumDependencies() const
    {
        u32 luOverflow = 0;
        if (mDependencies.mNext)
            luOverflow = mDependencies.mNext->ListSize();
        return static_cast<int>(mDependencies.mSize + luOverflow);
    }

    // Minimal LocalBackend-scope decls for the two worker entry points (jobs run
    // synchronously on the main thread, so both are no-ops).
    namespace LocalBackend {
        class LocalBackend;
        struct JobInstance { void Run(); };
        class  JobThread   { public: void Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend); };

        void JobInstance::Run() {}   // FLAG PC-platform leaf: synchronous jobs on PC (main-thread run)
        void JobThread::Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend)   // FLAG PC-platform leaf: synchronous jobs on PC (no worker threads)
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
// Homed (2026-08-07 targeted export): AssignSFXCallbacks @0x82B45A80, SnapshotMixer::
// InitSnapshots @0x82B47350 / DestroySnapshots @0x82B46D20, SnapshotVolumeCurve
// @0x82B453C0 (+ the dumped unk_82F86F88 LUT), NFSMixMaster::DestroyMainMainMap
// @0x82B457E0, NFSMixMap::CreateMainMapState @0x82B49680. Remaining parks are
// per-site below. FLAG: audio mix bring-up incomplete.
namespace Nicotine {
    // Nicotine::SnapshotMixer::InitSnapshots @0x82B47350 (targeted export 2026-08-07)
    // -- (re)build the per-channel ramp array + per-snapshot status array from the
    // loaded snapshot header. Store-for-store: teardown-if-built, bind the header +
    // counts, allocate/zero each array through the mixer allocator (off_83250004
    // vtbl+8, debug-named), then per-entry init (channels: all-zero + the ceiling
    // link; statuses: timer -1.0 == off, flags 0), and state -> 2 (built).
    //
    // FLAG (arg plumbing parked): the console entry receives the snapshot header
    // blob in r4 (stw r30,4(r31) == mpSnapshotHdr = r4), forwarded through
    // IDynamicMixer::InitSnapshots @0x82B44D68; the in-tree declaration chain is
    // no-arg (SnapshotMixer.hpp / IDynamicMixer.hpp -- out of this TU's file set),
    // so this body sources the header from mpSnapshotHdr, null-guarded, until the
    // argument is threaded through those headers.
    void SnapshotMixer::InitSnapshots()
    {
        SnapshotHeader* const lpHdr = mpSnapshotHdr;   // console: r4 (see the FLAG above)

        if (meState == 2)                    // +0x08: already built -> tear down first
            DestroySnapshots();

        if (lpHdr == 0)
            return;                          // FLAG: guard for the un-threaded arg (the console derefs r4)
        mpSnapshotHdr = lpHdr;               // stw 4(r31) -- re-stored after the teardown

        // The serialized head words: [0] = snapshot count, [1] = channel count (blob +0x00/+0x04).
        const s32* const lpCounts = reinterpret_cast<const s32*>(lpHdr->maHead);   // serialized blob head
        miNumChannels  = lpCounts[1];        // +0x14 (serialized blob word 1)
        miNumSnapshots = lpCounts[0];        // +0x18 (serialized blob word 0)

        if (miNumChannels > 0)
        {
            // sizeof stride (console 24 == X360 sizeof SnapshotChannel; x64 widens),
            // clamped to -1 past the console 0xAAAAAAA overflow guard.
            const u32 luSize = (miNumChannels > 0xAAAAAAA)
                ? 0xFFFFFFFFu
                : static_cast<u32>(sizeof(SnapshotChannel)) * static_cast<u32>(miNumChannels);
            SnapshotChannel* const lpChannels = static_cast<SnapshotChannel*>(
                g_pMixerAllocator->Allocate(luSize, 16, "SnapshotChannels"));   // off_83250004 vtbl+8
            if (lpChannels)
                std::memset(lpChannels, 0, luSize);
            mpChannels = lpChannels;         // +0x0C (null on alloc failure, as shipped)

            for (s32 li = 0; li < miNumChannels; ++li)
            {
                // Descriptor word 2 (serialized blob +0x18 + 12*li) = the CEILING channel index, -1 = none.
                const s32 liCeiling = static_cast<s32>(lpHdr->maChannels[li].mField08);
                SnapshotChannel& lrChannel = mpChannels[li];
                lrChannel.mfElapsed        = 0.0f;   // +0x10 (stfs flt_82001CC0)
                lrChannel.mi16TargetVolume = 0;      // +0x00 (sth)
                lrChannel.mfDuration       = 0.0f;   // +0x14
                lrChannel.mi16BaseVolume   = 0;      // +0x04 (sth)
                lrChannel.mi16ProcVolume   = 0;      // +0x02 (sth)
                lrChannel.mpMixChProc      = 0;      // +0x08
                lrChannel.mpCeiling        = (liCeiling == -1) ? 0 : &mpChannels[liCeiling];   // +0x0C
            }
        }

        if (miNumSnapshots > 0)
        {
            const u32 luSize = (miNumSnapshots > 0x1FFFFFFF)
                ? 0xFFFFFFFFu
                : static_cast<u32>(sizeof(SnapshotStatus)) * static_cast<u32>(miNumSnapshots);
            SnapshotStatus* const lpStatuses = static_cast<SnapshotStatus*>(
                g_pMixerAllocator->Allocate(luSize, 16, "SnapshotStatuses"));   // off_83250004 vtbl+8
            if (lpStatuses)
                std::memset(lpStatuses, 0, luSize);
            mpSnapshots = lpStatuses;        // +0x10

            for (s32 li = 0; li < miNumSnapshots; ++li)
            {
                mpSnapshots[li].mfTimeRemaining = -1.0f;   // stfs flt_82147C24 (hold timer off)
                mpSnapshots[li].mFlags          = 0;       // stb +4
            }
        }

        meState = 2;                         // +0x08: built
    }

    // Nicotine::SnapshotMixer::DestroySnapshots @0x82B46D20 (targeted export
    // 2026-08-07) -- tear the snapshot/channel arrays back down. Built (state 2):
    // free each non-empty array through the mixer allocator (off_83250004 vtbl+0xC,
    // flag 0), zero the array/count/header fields, state -> 1. Not built: just
    // state -> 1.
    void SnapshotMixer::DestroySnapshots()
    {
        if (meState == 2)                    // +0x08
        {
            if (miNumChannels > 0)           // +0x14
                g_pMixerAllocator->Free(mpChannels, 0);    // vtbl+0xC
            if (miNumSnapshots > 0)          // +0x18
                g_pMixerAllocator->Free(mpSnapshots, 0);   // vtbl+0xC
            mpChannels     = 0;              // +0x0C
            mpSnapshots    = 0;              // +0x10
            miNumChannels  = 0;              // +0x14
            miNumSnapshots = 0;              // +0x18
            mpSnapshotHdr  = 0;              // +0x04
            meState        = 1;              // +0x08
        }
        else
        {
            meState = 1;                     // +0x08
        }
    }

    // SetSnapshot (no-arg) is still un-addressed in the export set.
    void SnapshotMixer::SetSnapshot()      {}   // FLAG link-stub (X360 body un-exported)

    // The 512-entry snapshot volume-curve LUT (X360 rodata unk_82F86F88; dumped
    // bit-exact 2026-08-07, _data_volume_curve_lut: 2048 bytes of big-endian f32).
    // A monotonic 0.0 -> 1.0 ease table sampled at trunc(ratio * 511).
    static const f32 KF_SnapshotVolumeCurve[512] =
    {
        0.0f, 0.00307000009f, 0.00614000019f, 0.0092000002f, 0.0122699998f, 0.0153400004f, 0.0184099991f, 0.0214699991f,
        0.0245399997f, 0.0276100002f, 0.0306700002f, 0.0337399989f, 0.0368099995f, 0.0398700014f, 0.0429399982f, 0.0460000001f,
        0.0490700006f, 0.0521299988f, 0.0551999994f, 0.0582600012f, 0.0613199994f, 0.0643799976f, 0.0674400032f, 0.0705000013f,
        0.0735599995f, 0.0766199976f, 0.0796800032f, 0.0827400014f, 0.0857999995f, 0.088849999f, 0.0919099972f, 0.0949599966f,
        0.0980200022f, 0.101070002f, 0.104120001f, 0.107170001f, 0.11022f, 0.11327f, 0.116319999f, 0.119369999f,
        0.122409999f, 0.12545f, 0.1285f, 0.13154f, 0.134580001f, 0.137620002f, 0.140660003f, 0.143690005f,
        0.146730006f, 0.149759993f, 0.152799994f, 0.155829996f, 0.158859998f, 0.16189f, 0.164910004f, 0.167940006f,
        0.170959994f, 0.173979998f, 0.177000001f, 0.180020005f, 0.183039993f, 0.186049998f, 0.189070001f, 0.192080006f,
        0.195089996f, 0.198100001f, 0.201100007f, 0.204109997f, 0.207110003f, 0.210109994f, 0.21311f, 0.216110006f,
        0.219099998f, 0.222090006f, 0.225079998f, 0.228070006f, 0.231059998f, 0.234040007f, 0.237020001f, 0.239999995f,
        0.242980003f, 0.245949998f, 0.248930007f, 0.251899987f, 0.254869998f, 0.257829994f, 0.26078999f, 0.263749987f,
        0.266710013f, 0.26967001f, 0.272619992f, 0.275570005f, 0.278519988f, 0.281459987f, 0.28441f, 0.287349999f,
        0.290280014f, 0.293220013f, 0.296149999f, 0.299080014f, 0.30201f, 0.304930001f, 0.307850003f, 0.310770005f,
        0.313679993f, 0.316590011f, 0.319499999f, 0.322409987f, 0.325309992f, 0.328209996f, 0.331110001f, 0.333999991f,
        0.336890012f, 0.339780003f, 0.34266001f, 0.345539987f, 0.348419994f, 0.351289988f, 0.354160011f, 0.357030004f,
        0.359890014f, 0.362760007f, 0.365610003f, 0.368470013f, 0.371320009f, 0.374159992f, 0.377009988f, 0.37985f,
        0.382679999f, 0.385520011f, 0.388339996f, 0.391169995f, 0.39399001f, 0.396809995f, 0.399619997f, 0.402429998f,
        0.405239999f, 0.408039987f, 0.410840005f, 0.413639992f, 0.416429996f, 0.419220001f, 0.421999991f, 0.424780011f,
        0.427549988f, 0.430330008f, 0.433090001f, 0.435860008f, 0.438620001f, 0.44137001f, 0.44411999f, 0.446869999f,
        0.449609995f, 0.452349991f, 0.455080003f, 0.457810014f, 0.460539997f, 0.463259995f, 0.465979993f, 0.468690008f,
        0.471399993f, 0.474099994f, 0.476799995f, 0.479490012f, 0.482179999f, 0.484869987f, 0.48754999f, 0.490229994f,
        0.492900014f, 0.49555999f, 0.49823001f, 0.500880003f, 0.503539979f, 0.506190002f, 0.508830011f, 0.51147002f,
        0.514100015f, 0.516730011f, 0.519360006f, 0.521969974f, 0.524590015f, 0.527199984f, 0.529799998f, 0.532400012f,
        0.535000026f, 0.537590027f, 0.540170014f, 0.542750001f, 0.545319974f, 0.547890007f, 0.550459981f, 0.55302f,
        0.555570006f, 0.558120012f, 0.560660005f, 0.563199997f, 0.565729976f, 0.568260014f, 0.570779979f, 0.573300004f,
        0.575810015f, 0.578310013f, 0.58081001f, 0.583310008f, 0.585799992f, 0.588280022f, 0.590759993f, 0.593230009f,
        0.595700026f, 0.598160028f, 0.600619972f, 0.603070021f, 0.605509996f, 0.607949972f, 0.610379994f, 0.612810016f,
        0.615230024f, 0.617649972f, 0.620060027f, 0.622460008f, 0.624859989f, 0.627250016f, 0.629639983f, 0.632019997f,
        0.634389997f, 0.636759996f, 0.639119983f, 0.641480029f, 0.643830001f, 0.646179974f, 0.648509979f, 0.650849998f,
        0.65316999f, 0.655489981f, 0.657809973f, 0.660109997f, 0.662419975f, 0.664709985f, 0.666999996f, 0.669279993f,
        0.671559989f, 0.673829973f, 0.676090002f, 0.678349972f, 0.680599988f, 0.682850003f, 0.685079992f, 0.68730998f,
        0.689540029f, 0.691760004f, 0.693970025f, 0.696179986f, 0.698379993f, 0.700569987f, 0.702750027f, 0.704930007f,
        0.707109988f, 0.70927f, 0.711430013f, 0.713580012f, 0.715730011f, 0.717869997f, 0.720000029f, 0.722130001f,
        0.724250019f, 0.726360023f, 0.728460014f, 0.730560005f, 0.732649982f, 0.734740019f, 0.736819983f, 0.738889992f,
        0.740949988f, 0.743009984f, 0.745060027f, 0.747099996f, 0.749140024f, 0.751160026f, 0.753189981f, 0.755200028f,
        0.757210016f, 0.759209991f, 0.761200011f, 0.763189971f, 0.765169978f, 0.767139971f, 0.76910001f, 0.77105999f,
        0.773010015f, 0.774950027f, 0.77688998f, 0.778819978f, 0.780740023f, 0.782649994f, 0.784560025f, 0.786450028f,
        0.788349986f, 0.790229976f, 0.792110026f, 0.793980002f, 0.795840025f, 0.797689974f, 0.799539983f, 0.801379979f,
        0.80321002f, 0.805029988f, 0.806850016f, 0.808659971f, 0.810459971f, 0.812250018f, 0.814040005f, 0.815810025f,
        0.817579985f, 0.819350004f, 0.821099997f, 0.822849989f, 0.824590027f, 0.826319993f, 0.828040004f, 0.829760015f,
        0.831470013f, 0.833169997f, 0.834860027f, 0.836549997f, 0.83822f, 0.839890003f, 0.841549993f, 0.843209982f,
        0.844850004f, 0.846490026f, 0.848119974f, 0.849740028f, 0.851350009f, 0.852959991f, 0.854560018f, 0.856149971f,
        0.857729971f, 0.859300017f, 0.860870004f, 0.862420022f, 0.863969982f, 0.865509987f, 0.867049992f, 0.86857003f,
        0.870090008f, 0.871590018f, 0.873090029f, 0.87458998f, 0.876070023f, 0.877539992f, 0.879010022f, 0.880469978f,
        0.88191998f, 0.883360028f, 0.884800017f, 0.886219978f, 0.887639999f, 0.889050007f, 0.890450001f, 0.891839981f,
        0.893220007f, 0.894599974f, 0.895969987f, 0.897319973f, 0.898670018f, 0.900020003f, 0.901350021f, 0.902670026f,
        0.903989971f, 0.905300021f, 0.906599998f, 0.907890022f, 0.909169972f, 0.910440028f, 0.911710024f, 0.912959993f,
        0.914210021f, 0.915449977f, 0.916679978f, 0.917900026f, 0.91911f, 0.920319974f, 0.921509981f, 0.922699988f,
        0.923879981f, 0.92505002f, 0.926209986f, 0.927359998f, 0.92851001f, 0.929639995f, 0.93076998f, 0.931879997f,
        0.932990015f, 0.934090018f, 0.935180008f, 0.936269999f, 0.937340021f, 0.938399971f, 0.93945998f, 0.940509975f,
        0.941540003f, 0.942569971f, 0.943589985f, 0.944599986f, 0.945609987f, 0.94660002f, 0.947589993f, 0.948559999f,
        0.949530005f, 0.950489998f, 0.951430023f, 0.952369988f, 0.953310013f, 0.954230011f, 0.955139995f, 0.956040025f,
        0.956939995f, 0.957830012f, 0.958700001f, 0.959569991f, 0.960430026f, 0.961279988f, 0.962119997f, 0.962949991f,
        0.963779986f, 0.964590013f, 0.965390027f, 0.966189981f, 0.96697998f, 0.967750013f, 0.968519986f, 0.969280005f,
        0.97003001f, 0.970770001f, 0.971499979f, 0.972230017f, 0.972940028f, 0.973640025f, 0.974340022f, 0.975030005f,
        0.975700021f, 0.976369977f, 0.977029979f, 0.977680027f, 0.978320003f, 0.978950024f, 0.979569972f, 0.980180025f,
        0.980790019f, 0.981379986f, 0.981959999f, 0.982540011f, 0.983110011f, 0.983659983f, 0.984210014f, 0.984749973f,
        0.985279977f, 0.985800028f, 0.986310005f, 0.986810029f, 0.987299979f, 0.987779975f, 0.988259971f, 0.98872f,
        0.989180028f, 0.98961997f, 0.990059972f, 0.990480006f, 0.99089998f, 0.99131f, 0.991710007f, 0.9921f,
        0.99247998f, 0.992850006f, 0.993210018f, 0.993560016f, 0.993910015f, 0.994239986f, 0.994560003f, 0.994880021f,
        0.995180011f, 0.995480001f, 0.995769978f, 0.996039987f, 0.996309996f, 0.996569991f, 0.996819973f, 0.997060001f,
        0.997290015f, 0.997510016f, 0.997720003f, 0.99792999f, 0.99812001f, 0.998300016f, 0.998480022f, 0.998640001f,
        0.99879998f, 0.998939991f, 0.999080002f, 0.999199986f, 0.999319971f, 0.999430001f, 0.999530017f, 0.99962002f,
        0.99970001f, 0.999769986f, 0.999830008f, 0.999880016f, 0.999920011f, 0.999960005f, 0.999979973f, 1.0f,
    };

    // SnapshotVolumeCurve @0x82B453C0 (sub_82B453C0; targeted export 2026-08-07) --
    // the Nicotine volume-ramp curve evaluator. Selector map (even = 1 - odd twin):
    //   0/2/4/6/8 -> 1 - curve(ratio, type+1)         (fsubs 1.0 - recursion)
    //   1 -> LUT[trunc(ratio*511)]                    (console: fmuls ratio * -511.0
    //                                                  flt_820AD414, negative-indexed
    //                                                  off the table base)
    //   3 -> curve(ratio, 1)^2                        (fmuls)
    //   5 -> 1 - LUT[-trunc(ratio*511 - 511)]         (console: fmsubs flt_821478B0)
    //   7 -> curve(ratio, 5)^2                        (fmuls)
    //   9 -> ratio (raw)            default -> 0.0
    // The [0,1] bounds check only feeds the console's IDynamicMixer vtbl+0x10 assert
    // hook ("lfInput out of bounds. [0,1]") -- that virtual slot is un-modelled in
    // IDynamicMixer.hpp (out of this TU's file set) and is side-effect-free on the
    // shipped path, so it is documented rather than dispatched; the raw (unasserted)
    // ratio indexes the LUT exactly as on the console.
    double SnapshotVolumeCurve(double lfRatio, int liCurveType)
    {
        switch (liCurveType)
        {
            case 0: case 2: case 4: case 6: case 8:              // even = inverted odd twin
                return 1.0 - SnapshotVolumeCurve(lfRatio, liCurveType + 1);
            case 1:
            {
                const s32 liIndex = static_cast<s32>(static_cast<f32>(lfRatio) * -511.0f);   // flt_820AD414 (fctiwz)
                return KF_SnapshotVolumeCurve[-liIndex];         // base - 4*liIndex
            }
            case 3:
            {
                const f32 lfUp = static_cast<f32>(SnapshotVolumeCurve(lfRatio, 1));
                return lfUp * lfUp;                              // fmuls
            }
            case 5:
            {
                const s32 liIndex = static_cast<s32>(
                    static_cast<f32>(lfRatio) * 511.0f - 511.0f);   // flt_821478B0 (fmsubs; <= 0)
                return 1.0f - KF_SnapshotVolumeCurve[-liIndex];  // fsubs 1.0 - LUT
            }
            case 7:
            {
                const f32 lfDown = static_cast<f32>(SnapshotVolumeCurve(lfRatio, 5));
                return lfDown * lfDown;                          // fmuls
            }
            case 9:
                return lfRatio;                                  // identity (raw, unclamped)
            default:
                return 0.0;                                      // flt_82001CC0
        }
    }
}

// NFSMixMaster::InitMixMap @0x82B45920 -- the asm is exported and decoded, and its
// callee NFSMixMap::CreateMainMapState @0x82B49680 is now homed (below), but the
// body still needs NFSMixMap::AllocateInputArrays @0x82B4A120 and InitMainMapStates
// @0x82B4ABD0 (declared-only in NFSMixMap.hpp; bodies deferred in the NFSMix
// cluster -- both now have per-address dossiers in the export dir), so a faithful
// body here would still trade the stub for unresolved externals.
void NFSMixMaster::InitMixMap()                  {}   // FLAG link-stub (blocked on AllocateInputArrays/InitMainMapStates)

// NFSMixMaster::DestroyMainMainMap @0x82B457E0 (targeted export 2026-08-07) -- tear
// down the owned main map + the load bookkeeping. The console: DestroyMainMixMap on
// the map, then (*vt[0])(map, 1) == the NFSMixMap scalar-deleting dtor, null the
// slot; m_bMapReady = 0 (unconditional stb 0x70); null m_pMainMixMapData if set.
// The virtual delete is expressed as the explicit dtor + mixer-allocator Free -- the
// committed ~NFSMixMaster idiom (NFSMixMaster.cpp). The console reloads
// m_pMainMixMap between the two calls; it is the same pointer either way
// (DestroyMainMixMap does not clear it).
// FLAG (deferred): the console FIRST calls NFSMixMap::DestroyMainMixMap @0x82B47E78
// (the AllocateMixerMemory block-free inverse) -- un-exported in this dossier set
// AND undeclared in NFSMixMap.hpp (out of this TU's file set); until it lands the
// mixer-memory blocks are not freed here (the PC boot path never tears the map
// down mid-run).
void NFSMixMaster::DestroyMainMainMap()
{
    if (m_pMainMixMap)                    // +0x00
    {
        // FLAG deferred (see above): m_pMainMixMap->DestroyMainMixMap() @0x82B47E78.
        m_pMainMixMap->~NFSMixMap();                   // (*vt[0])(map, 1) ...
        g_pMixerAllocator->Free(m_pMainMixMap, 0);     // ... == dtor + mixer-heap free
        m_pMainMixMap = 0;                             // stw 0 -> +0x00
    }
    m_bMapReady = false;                  // stb 0x70 (unconditional)
    if (m_pMainMixMapData)                // +0x04 (the lwz/beq store guard, reproduced)
        m_pMainMixMapData = 0;
}
// NFSMixMaster::AssignSFXCallbacks @0x82B45A80 -- forward the SFX-callback owner into
// the main map (tail-call NFSMixMap::AssignSFXCallbacks @0x82B481B8, homed; the
// console derefs m_pMainMixMap unguarded -- reproduced).
void NFSMixMaster::AssignSFXCallbacks(void* lpOwner)
{
    m_pMainMixMap->AssignSFXCallbacks(lpOwner);
}

// NFSMixMap::CreateMainMapState @0x82B49680 (targeted export 2026-08-07) -- build /
// extend the per-state NFSMixMapState and wire its serialized state header, then run
// the builder passes on the state copy for this object index:
//   * first touch of liState: carve the next NFSMixMapState from the state object
//     block (m_pStateProcMemBlock + the m_CurrentStateProcBlockOffset byte cursor;
//     console step 0x60 == X360 sizeof) and virtual-Initialize it (vtbl+4).
//   * AddMixState(liObjIdx, state) @0x82B4D660 (registers/creates the copy).
//   * the copy's m_pMMStateHdr = blob + stateOffsetTable[liState].
//   * CreateMixCtls @0x82B4C890 / [Create3DMixCtls -- FLAG below] / CreateEvtMixCtls
//     @0x82B4CE00 on the copy.
void NFSMixMap::CreateMainMapState(int liState, int liCopy, int liObjIdx)
{
    if (!m_pStateProcs[liState])                                  // +0x98 (lwzx)
    {
        // x64 carve stride = sizeof(NFSMixMapState) (the console's raw +0x60 step on
        // the x64 host is the recurring console-stride corruption class).
        const int liOffset = m_CurrentStateProcBlockOffset;       // +0x20C (byte cursor)
        NFSMixMapState* const lpFresh = reinterpret_cast<NFSMixMapState*>(
            reinterpret_cast<char*>(m_pStateProcMemBlock) + liOffset);   // +0x9C
        m_CurrentStateProcBlockOffset = liOffset + static_cast<int>(sizeof(NFSMixMapState));
        m_pStateProcs[liState] = lpFresh;
        m_pStateProcs[liState]->Initialize(this, liState, liCopy, liObjIdx);   // vtbl+4
    }

    NFSMixMapState* const lpState = m_pStateProcs[liState];
    lpState->AddMixState(liObjIdx, lpState);                      // @0x82B4D660

    // The per-state header record inside the loaded MixMap blob: the state-offset
    // table sits at blob + StateTableOffset (blob word 2); entry liState is the
    // record's own blob offset.
    char* const lpBlob = reinterpret_cast<char*>(m_pMMHdr);       // +0x74 (the serialized MixMap blob)
    const int liStateOffset =
        reinterpret_cast<const int*>(lpBlob + m_pMMHdr->StateTableOffset)[liState];   // serialized blob state table
    NFSMixMapState* const lpProc = lpState->GetMixMapProc(liObjIdx);   // @0x82B4D648
    lpProc->m_pMMStateHdr =
        reinterpret_cast<stMixMapStateHdr*>(lpBlob + liStateOffset);   // +0x1C (serialized blob record)

    lpProc->CreateMixCtls();      // @0x82B4C890
    // FLAG (deferred): the console calls NFSMixMapState::Create3DMixCtls @0x82B4D100
    // here -- un-reconstructed AND undeclared in NFSMixMapState.hpp (out of this
    // TU's file set); the state's 3D mix-ctl procs are not built until it lands.
    lpProc->CreateEvtMixCtls();   // @0x82B4CE00
}
