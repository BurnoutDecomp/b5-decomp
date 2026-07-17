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
    AptValue* AptApt_LoadVariablesFetch(const char* pUrl) { return 0; }   // FLAG link-stub (host URL fetch; null until installed)
    void AptApt_GetDragTargetTranslate(AptValue* pDragTarget, float* pOutX, float* pOutY) {}   // FLAG link-stub
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

    // ---- AptCIH "link cluster" deferred sub-paths: the deep callees the now-homed
    // AptCIHBehaviour.cpp bodies (queueClipEvents / GeneralisedProcess / ClearCIH /
    // AddToDelayReleaseList / PreDestroyHook) FLAG out. Faithful "deferred subsystem"
    // defaults until the AS-interpreter execution / generalised-process gate / GC zombie
    // subsystems land. -------------------------------------------------------------------
    // AptAnimationTargetAddToRemList RETIRED (2026-07-10): homed in
    // AptAnimationTarget.cpp (@0x82AEE3F8 -- queue on the shared delayed-release
    // table with the bit26 latch + CleanRemList overflow flush; the {} stub
    // dropped every delay-released clip).
    void (*gpAptCIHPreDestroyHook)(AptCIH* pCIH) = nullptr;   // FLAG link-stub (dword_8324E8A0; null until installed)
    // AptQueueClipEventsRunMatched RETIRED (2026-07-01): homed faithfully in
    // AptCIHBehaviour.cpp from the PS3 body @0x815BD0 (the clip-event record scan +
    // AddActionFront/Back enqueues; the byte-code-block + __proto__ tails staged there).
    // AptClearCIHDrainQueuesAndZombie RETIRED (2026-07-02): homed in
    // AptCIHBehaviour.cpp (the director-set/new-inst drain + the unload-event tail;
    // the zombie-vector decision stays a documented staged FLAG there).
    bool AptCIH_sbGeneralisedProcessEarlyReturn = false;   // FLAG link-stub (bEarlyReturn; gate off by default)
    unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*)  = nullptr;   // FLAG link-stub
    unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*) = nullptr;   // FLAG link-stub
    unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*) = nullptr;   // FLAG link-stub
    int  AptCIH_snGeneralisedProcessTreeDepth = 0;   // FLAG link-stub (nTreeDepth)

    // AptCIH::ProcessCustomControls -- the per-frame custom-control refresh pass the
    // AptUpdate slot install (dword_8324E420) targets. Its X360 body has no
    // per-address export in the dump set; returns false (nothing refreshed) until
    // it is exported + reconstructed.
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
    void AptHook_Trace(const char* szFormat, const char* szMessage) {}   // FLAG link-stub
    void AptKeyManagerAddListener(AptValue* pListener) {}   // FLAG link-stub
    bool AptKeyManagerRemoveListener(AptValue* pListener) { return false; }   // FLAG link-stub
    // AptLinkerGetUrlLoad RETIRED (2026-07-10): the getURL/getURL2 .swf arm calls
    // the homed member AptLinker::Load(EAStringC*, EAStringC*) @0x82B06660 directly
    // (the {} forwarder dropped every getURL movie load).
    // FLAG PC-platform leaf: host async-stream cancel hook (dword_8324E83C) -- the
    // PC stream hook (AptLoaderStartAsyncLoad) loads synchronously, so nothing is
    // ever in flight to cancel; the empty body matches the un-installed slot.
    void AptLoaderCancelAsyncLoad(void* pDataBlock) {}
    // AptLoaderStartAsyncLoad is HOMED in BrnGuiAptRuntime.cpp (the platform stream hook: it
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
    // GlobalNotificationFunction RETIRED (2026-07-16): homed as the real @0x82B00C78
    // body (AptLinker.cpp) -- the loader's "file linked" notify into the current
    // target's linker pending list. The {} stub silently dropped every async movie
    // load completion (nothing ever mounted through the engine linker).
    void Mutex_Lock(void* pMutex, void* pName) {}   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    // AptMath::ClipStackShutdown -- referenced by the (dev-wave) AptRenderShutdown;
    // the clip-stack free's real body is un-reconstructed and the PC boot never shuts
    // the Apt renderer down mid-run. FLAG link-stub until the AptMath TU lands.
    namespace AptMath { intptr_t ClipStackShutdown() { return 0; } }
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

    void Mutex_Lock(void* pMutex, void* pName)  { (void)pMutex; (void)pName; }   // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    void Mutex_Unlock(void* pMutex)             { (void)pMutex; }                // FLAG PC-platform leaf: single-threaded PC (no lock needed)
    int  GetThreadId()                          { return 0; }                    // FLAG PC-platform leaf: single-threaded PC (one thread id)

    Thread::Status Thread::WaitForEnd(intptr_t* pThreadReturnValue, const int* pTimeoutAbsolute)
    { (void)pThreadReturnValue; (void)pTimeoutAbsolute; return kStatusEnded; }   // FLAG PC-platform leaf: single-threaded PC (thread already ended)

    // One file-scope slot mirroring the single TLS value the bring-up needs.
    static void* gThreadLocalStorageSlot = nullptr;   // FLAG link-stub (single-threaded TLS)
    bool  ThreadLocalStorage::SetValue(const void* pData)
    { gThreadLocalStorageSlot = const_cast<void*>(pData); return true; }         // FLAG PC-platform leaf: single-threaded TLS (one slot)
    void* ThreadLocalStorage::GetValue() { return gThreadLocalStorageSlot; }     // FLAG PC-platform leaf: single-threaded TLS (one slot)

    IRunnable::~IRunnable() {}   // FLAG PC-platform leaf: single-threaded PC (empty virtual dtor)

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

    Event::Event() {}   // FLAG PC-platform leaf: synchronous jobs on PC (no event state)

    JobThreadHandle::JobThreadHandle(Detail::SchedulerBackend* pBackend, u32 uHandle)   // FLAG PC-platform leaf: synchronous jobs on PC
    { (void)pBackend; (void)uHandle; }
    JobThreadHandle::JobThreadHandle() {}   // FLAG PC-platform leaf: synchronous jobs on PC

    JobAffinity    EntryPoint::GetAffinity()    const { return JOB_AFFINITY_NONE; }    // FLAG link-stub
    JobEnvironment EntryPoint::GetEnvironment() const { return JOB_ENVIRONMENT_LOCAL; }// FLAG link-stub
    JobPriority    EntryPoint::GetPriority()    const { return JOB_PRIORITY_HIGH; }    // FLAG link-stub
    void           EntryPoint::SetName(const char* lpcName) { (void)lpcName; }         // FLAG PC-platform leaf: synchronous jobs on PC (no worker names)

    int Job::GetNumDependencies() const { return 0; }   // FLAG PC-platform leaf: synchronous jobs on PC (no dependencies)

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
