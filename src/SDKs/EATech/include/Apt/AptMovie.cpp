// ===========================================================================
// EATech Apt -- AptMovie::labelToFrame.   DECOMPILED from the PS3 EXTERNAL ELF
// (@0x7F936C). Look the label up in the timeline's label hash and return its
// stored frame index (an AptInteger), or -1 when absent.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptMovie.h"
#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

#include "types.hpp"

#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager
#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"            // AptPseudoCIH_t, gpAptPseudoDataPool
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"    // AptPseudoDisplayList
#include "SDKs/EATech/include/Apt/AptActionQueue.h"          // AptActionQueueC
#include "SDKs/EATech/include/Apt/AptTarget.h"               // gpAptTarget (the director chain)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"      // AptAnimationTarget::mpActionQueue

#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"    // AptActionInterpreter
#include "SDKs/EATech/include/Apt/AptScriptFunctionBase.h"   // PushStaticData/PopStaticData (the AS register window)
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"     // AptInteger::Create
#include "SDKs/EATech/include/Apt/AptDefine.h"               // gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

// ---- native place-command playback ----
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::placeObjectNCXForm (the homed place spine)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the parent node + char-inst chain)
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // findInst (MOVE tween keyframes + tag-4 removes)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"
#include "SDKs/EATech/include/Apt/AptPseudoData.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::GetRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"            // AptRenderItem::mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacter.h"             // AptCharacter (the movie character + mpAnimationFile)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation (charTable / import table)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile / AptFilePtr (the placed char's file bind)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"         // AptUint32CXForm (the place record packed colour)
#include "SDKs/EATech/include/Apt/Apt.h"                      // gAptFuncs (tag-5 BackgroundColour -> pfnSetBackgroundColour @+0x10)

#include <new>   // placement new (the pool-allocated AptPseudoCIH_t nodes)

int AptMovie::labelToFrame(const EAStringC* pLabel) const
{
    if (pLabel && mpLabelHash)
    {
        AptValue* pValue = mpLabelHash->Lookup(*pLabel);
        if (pValue)
            return pValue->toInteger();
    }
    return -1;
}

// ===========================================================================
// The timeline driver / relocate methods.   DECOMPILED FAITHFULLY from the X360
// ARTIST.XEX (the authoritative spine for this TU; there is no leak/DWARF for it).
//
//   AptMovie::DoTemporaryFrameControls @ 0x82AEEB98
//   AptMovie::doFrameControls          @ 0x82B0B7A0
//   AptMovie::queueFrameActions        @ 0x82AE0228
//   AptMovie::resolve                  @ 0x82AF80B0
//   AptMovie::unresolve                @ 0x82AF4830
//
// The native frame and command records are accessed through their typed fields.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

// ---------------------------------------------------------------------------
// Sibling-owned globals / callees (all HOMED -- AptGlobals.cpp for the globals,
// noted per symbol for the callees): declared extern so this TU links; the
// single underlying object is shared.
// ---------------------------------------------------------------------------

// off_8324D808 -- the shared Apt DOGMA pool the 20-byte pseudo nodes come from
// (AptPseudoCIH.h declares it as gpAptPseudoDataPool; reused by name).

// off_8324E574 -- the current AS animation target: gpAptTarget (typed AptTarget*,
// declared by AptTarget.h; its director @+0x18 owns the AptActionQueueC* @+0x0C --
// both real members now, read through the types).

// dword_8324E514 -- the current frame's queued-action sequence id passed as
// AddActionBack's 4th arg.
extern int   gnAptActionFrameId;                // dword_8324E514 (AptGlobals.cpp)

// &dword_8324E760 -- the process-wide AS action interpreter instance.
extern AptActionInterpreter gAptActionInterpreter;   // off_8324E760  (AptGlobals.cpp)

// byte_8324D807 -- the BACKGROUND-COLOUR once-latch: a tag-5 (BackgroundColour)
// frame command fires the host callback at most once per loaded animation
// ("bg color can only be set once" -- the SDK's gbBackgroundColorSet semantics);
// AptLoadAnimation @0x82B07AC8 resets it (stb 0 @0x82B07AF4) so the NEXT loaded
// movie's first tag-5 wins. (The old "back-to-script one-shot" reading of this
// latch was wrong -- RETIRED 2026-07-12. The callee at dword_8324E828 is not a
// standalone hook: 0x8324E828 == &gAptFuncs (dword_8324E818) + 0x10 ==
// gAptFuncs.pfnSetBackgroundColour, the render-family host callback the movie's
// doFrameControls tag-5 arm dispatches the authored colour word through.)
extern unsigned char gbAptBackgroundColourSet;    // byte_8324D807

// The AS register-window globals (off_8324E3D0 / dword_8324E3D4) are
// AptScriptFunctionBase::spRegBlockCurrentFrameBase / snRegBlockCurrentFrameCount;
// save/restore around runStream is the public PushStaticData()/PopStaticData() pair
// (the duplicate gpAptRegisterBase/gnAptRegisterCount externs -- the latter also
// colliding with AptRegister.cpp's config global of the same name -- are retired).

// FLAG (deferred VM / display-list callees -- owned by other TUs, declared with the
// raw-but-faithful call-site signatures so the timeline driver links):
struct AptCIH;
extern AptCIH* AptGetAnimationAtLevel(int nLevel);                       // _AptGetAnimationAtLevel @0x82B00788 (canonical AptCIH*; void* blob walk binds)
// (The invented 6-arg AptPseudoDisplayListFindInst extern was RETIRED 2026-07-10:
// the console DoTemporaryFrameControls place arm calls the real 3-arg member
// AptPseudoDisplayList::FindInst(key, &existing, &pred) -- asm-verified r4=depth
// key, r5=&pred, r6=&existing. The stub it bound to nulled every lookup, so a
// jumpToFrame replay re-created every already-live pseudo node.)
// AptPseudoDisplayList_Insert retired: the real member AptPseudoDisplayList::Insert is called directly.

// sub_82B0AE08 @0x82B097D8's caller-side dispatcher (the place-command handler doFrameControls
// invokes for each tag-3 record). HOMED below (2026-07-01) as AptDispatchPlaceCommand -- it reads
// the serialised place-info record + calls the faithfully-homed AptDisplayList::placeObjectNCXForm.
static AptCIH* AptDispatchPlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent);

// sub_82AFD150 @0x82AFD150 -- the remove-command dispatcher (doFrameControls' tag-4 twin of
// AptDispatchPlaceCommand).
// X360 body (two calls, both already homed):
//   AptDisplayListState::findInst(*a1, a2, 0, &prev, &match);   // r3=*list (the state), r4=depth
//   AptDisplayList::removeObject(a1, match);
static void AptDispatchRemoveCommand(AptDisplayList* pDisplayList, int nDepth)
{
    AptCIH* pPrev;
    AptCIH* pMatch;
    pDisplayList->AsState()->findInst(nDepth, 0, &pPrev, &pMatch);
    pDisplayList->removeObject(pMatch);
}

// ARTIST place command 0x82B0AE08.
static AptCIH* AptDispatchPlaceCommand(AptDisplayList* pDisplayList, const void* pPlaceInfo, AptCIH* pParent)
{
    const AptPlaceObjectBody_t& body = *static_cast<const AptPlaceObjectBody_t*>(pPlaceInfo);
    const uint32_t flags = body.muxFlags;
    const float* matrix = (flags & 4u) ? reinterpret_cast<const float*>(body.maMatrix) : 0;
    const AptUint32CXForm* colour = (flags & 8u) ? reinterpret_cast<const AptUint32CXForm*>(body.maColorTransform) : 0;
    const void* handlers = (flags & 0x80u) ? body.mpClipActions : 0;
    if (!(flags & 2u))
    {
        if (!(flags & 1u)) return 0;
        AptCIH* previous;
        AptCIH* match;
        pDisplayList->AsState()->findInst(body.mi32Depth, 0, &previous, &match);
        if (match)
        {
            if (!match->GetASChanged())
                return pDisplayList->placeObjectNCXForm(match, 0, 0, 0, pParent, 0, -1, body.mfRatio, matrix, handlers, colour);
            return 0;
        }
    }
    AptCharacter* owner = pParent->GetCharacterInst()->GetRenderItem()->mpCharacter->mpFixupLink;
    AptCharacterAnimation* anim = AptGetMovieCharacterAnimation(owner);
    AptCharacter* character = anim->mpCharacterTable[body.mi32CharacterId];
    EAStringC name;
    const EAStringC* namePtr = 0;
    if (flags & 0x20u) { name = EAStringC(body.mpName); namePtr = &name; }
    return pDisplayList->placeObjectNCXForm(0, body.mi32Depth, character, namePtr, pParent, 0,
                                            static_cast<int16_t>(body.miClipDepth), body.mfRatio, matrix, handlers, colour);
}

// ---------------------------------------------------------------------------
// runFrameActions @PS3 0x820FA4 -- run frame nFrame's tag-1 ACTION commands
// IMMEDIATELY on the VM singleton (the CallFrame opcode's synchronous sibling of
// queueFrameActions): for each action command, push a fresh register-block window
// (the console's PrepareForExecution is a pure thunk to PushStaticData), runStream
// the command's action stream with pCIH's root-animation character-inst as the run
// scope (GetRootAnimation()[8] on the console), then pop the window back
// (CleanupAfterExecution). The console re-reads the frame record each iteration
// (the stream may mutate the timeline); reproduced. Returns `this` (the console's
// incidental r3; the CallFrame caller ignores it).
// ---------------------------------------------------------------------------
const AptMovie* AptMovie::runFrameActions(AptCIH* pCIH, int nFrame) const
{
    for (int32_t i = 0; i < mpFrames[nFrame].mnCommandCount; ++i)
    {
        AptTimelineAction* command = static_cast<AptTimelineAction*>(mpFrames[nFrame].mpCommands[i]);
        if (command->mnTag != 1) continue;
        void* saved = AptScriptFunctionBase::PushStaticData();
        AptCharacterInst* inst = pCIH ? pCIH->GetRootAnimation()->GetCharacterInst() : 0;
        gAptActionInterpreter.runStream(command->mpStream, pCIH, -1, inst);
        gAptActionInterpreter.CleanupAfterExecution(saved);
    }
    return this;
}

// ARTIST 0x82AE0228: enqueue addresses of each tag-1 stream slot.
void* AptMovie::queueFrameActions(void* pCIH, int nFrame)
{
    void* result = this;
    for (int32_t i = 0; i < mpFrames[nFrame].mnCommandCount; ++i)
    {
        AptTimelineAction* command = static_cast<AptTimelineAction*>(mpFrames[nFrame].mpCommands[i]);
        if (command->mnTag == 1)
            result = gpAptTarget->mpAnimationTarget->mpActionQueue->AddActionBack(&command->mpStream, static_cast<AptCIH*>(pCIH), gnAptActionFrameId);
    }
    return result;
}

// ARTIST 0x82B0B7A0: run pending init actions first, then place/remove.
void AptMovie::doFrameControls(AptDisplayList* pDisplayList, AptCIH* pParent, int nFrame)
{
    for (int32_t i = 0; i < mpFrames[nFrame].mnCommandCount; ++i)
    {
        AptTimelineInitAction* command = static_cast<AptTimelineInitAction*>(mpFrames[nFrame].mpCommands[i]);
        if (command->mnTag == 8 && command->mnIndicator >= 0)
        {
            void* saved = AptScriptFunctionBase::PushStaticData();
            AptCharacterInst* inst = pParent ? pParent->GetRootAnimation()->GetCharacterInst() : 0;
            gAptActionInterpreter.runStream(command->mpStream, pParent, -1, inst);
            command->mnIndicator = -command->mnIndicator;
            gAptActionInterpreter.CleanupAfterExecution(saved);
        }
    }
    for (int32_t i = 0; i < mpFrames[nFrame].mnCommandCount; ++i)
    {
        AptTimelineCommand* command = static_cast<AptTimelineCommand*>(mpFrames[nFrame].mpCommands[i]);
        if (command->mnTag == 3)
        {
            AptPlaceObjectInfo_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command);
            AptPlaceObjectBody_t* body = place->GetBody();
            AptCharacter* owner = pParent->GetCharacterInst()->GetRenderItem()->mpCharacter;
            AptCharacterAnimation* anim = AptGetMovieCharacterAnimation(owner->mpFixupLink);
            anim->ExecuteInitActions(pParent, body->mi32CharacterId);
            if (body->mi32CharacterId != -1)
            {
                AptCharacter* character = anim->mpCharacterTable[body->mi32CharacterId];
                if (!character->mpAnimationFile)
                {
                    const int32_t importIndex = anim->IsImport(body->mi32CharacterId);
                    AptFile** file = importIndex == -1 ? &owner->mpAnimationFile : &anim->mpImportTable[importIndex].mpFile;
                    *reinterpret_cast<AptFilePtr*>(&character->mpAnimationFile) = *reinterpret_cast<AptFilePtr*>(file);
                }
            }
            AptDispatchPlaceCommand(pDisplayList, body, pParent);
        }
        else if (command->mnTag == 4)
            AptDispatchRemoveCommand(pDisplayList, reinterpret_cast<AptTimelineWord*>(command)->mnValue);
        else if (command->mnTag == 5 && !gbAptBackgroundColourSet)
        {
            gAptFuncs.pfnSetBackgroundColour(reinterpret_cast<AptTimelineWord*>(command)->mnValue);
            gbAptBackgroundColourSet = 1;
        }
    }
}

// ARTIST 0x82AEEB98: merge move commands into the temporary display list.
void AptMovie::DoTemporaryFrameControls(AptPseudoDisplayList* list, int frameNumber, int a4, void* a5)
{
    (void)a4; (void)a5;
    if (frameNumber < 0) return;
    AptCIH* owner = static_cast<AptCIH*>(list->mpOwner);
    for (int32_t i = 0; i < mpFrames[frameNumber].mnCommandCount; ++i)
    {
        AptTimelineCommand* command = static_cast<AptTimelineCommand*>(mpFrames[frameNumber].mpCommands[i]);
        if (command->mnTag == 3)
        {
            AptPlaceObjectInfo_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command);
            AptPlaceObjectBody_t* body = place->GetBody();
            AptPseudoCIH_t* previous;
            AptPseudoCIH_t* existing;
            list->FindInst(reinterpret_cast<void*>(body->mi32Depth), &previous, &existing);
            AptCharacter* character = 0;
            if (body->mi32CharacterId != -1)
            {
                AptCharacter* root = owner->GetCharacterInst()->GetRenderItem()->mpCharacter->mpFixupLink;
                character = AptGetMovieCharacterAnimation(root)->mpCharacterTable[body->mi32CharacterId];
            }
            if (existing && body->mi32CharacterId == -1)
            {
                AptPseudoData_t* snapshot = existing->mpPseudoData;
                if (body->muxFlags & 4u) snapshot->mpMatrix = body->maMatrix;
                if (body->muxFlags & 8u) snapshot->mpColorTransform = body->maColorTransform;
                if (body->muxFlags & 0x80u) snapshot->mpClipActions = body->mpClipActions;
                if (body->muxFlags & 0x10u) snapshot->mfRatio = body->mfRatio;
                snapshot->muxFlags |= body->muxFlags;
            }
            else
            {
                void* memory = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));
                AptPseudoCIH_t* node = memory ? ::new (memory) AptPseudoCIH_t(reinterpret_cast<AptCharacterInfo_t*>(place), static_cast<s16>(frameNumber), reinterpret_cast<void*>(body->mi32Depth), character) : 0;
                list->Insert(node);
            }
        }
        else if (command->mnTag == 4)
        {
            void* memory = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoCIH_t));
            AptPseudoCIH_t* node = memory ? ::new (memory) AptPseudoCIH_t(reinterpret_cast<AptCharacterInfo_t*>(command), static_cast<s16>(frameNumber), reinterpret_cast<void*>(reinterpret_cast<AptTimelineWord*>(command)->mnValue), 0) : 0;
            list->Insert(node);
        }
    }
}

// ===========================================================================
// resolve @ 0x82AF80B0 -- relocate the (just-loaded) timeline against the load base
// nBase: allocate the label hash, add the base to every file-relative pointer slot
// in the timeline (frame table, command list, per-command pointers), and re-parse
// each action stream (AptActionInterpreter::_parseStream) + register each label.
//
// `this`/a1 is the AptMovie*; nBase the relocation base (a2); a3/a4 the parse
// context. Returns the last value cl held in r3 (the last sub-call result), faithful
// to the asm.
// ===========================================================================
namespace
{
    // Native command records: ARTIST 0x82AF80B0 and 0x82AF4830.
    template<class T> void AptTimelineRelocate(T*& p, intptr_t base, bool unresolve)
    {
        if (p) p = reinterpret_cast<T*>(unresolve ? reinterpret_cast<intptr_t>(p) - base : reinterpret_cast<intptr_t>(p) + base);
    }
}

void AptMovie::resolve(int nBase, AptConstFile* pConstFile, int32_t* pValueCount)
{
    void* memory = gpAptPseudoDataPool->Allocate(sizeof(AptNativeHash));
    mpLabelHash = memory ? ::new (memory) AptNativeHash(2) : 0;
    AptTimelineRelocate(mpFrames, nBase, false);
    for (int32_t f = 0; f < mnFrameCount; ++f)
    {
        AptMovieFrame& frame = mpFrames[f];
        AptTimelineRelocate(frame.mpCommands, nBase, false);
        for (int32_t i = 0; i < frame.mnCommandCount; ++i)
        {
            AptTimelineRelocate(frame.mpCommands[i], nBase, false);
            AptTimelineCommand* command = static_cast<AptTimelineCommand*>(frame.mpCommands[i]);
            switch (command->mnTag)
            {
            case 1:
            {
                AptTimelineAction* action = reinterpret_cast<AptTimelineAction*>(command);
                AptTimelineRelocate(action->mpStream, nBase, false);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, pConstFile, pValueCount);
                break;
            }
            case 2:
            {
                AptTimelineAction* label = reinterpret_cast<AptTimelineAction*>(command);
                AptTimelineRelocate(label->mpStream, nBase, false);
                EAStringC name(reinterpret_cast<const char*>(label->mpStream));
                mpLabelHash->Set(name, AptInteger::Create(f));
                break;
            }
            case 3:
            {
                AptPlaceObjectBody_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command)->GetBody();
                AptTimelineRelocate(place->mpName, nBase, false);
                AptTimelineRelocate(place->mpClipActions, nBase, false);
                AptClipEventHandlerList* list = static_cast<AptClipEventHandlerList*>(place->mpClipActions);
                if (list)
                {
                    AptTimelineRelocate(list->mpHandlers, nBase, false);
                    for (int32_t k = 0; k < list->mnCount; ++k)
                    {
                        AptTimelineRelocate(list->mpHandlers[k].mpActionStream, nBase, false);
                        AptActionInterpreter::_parseStream(const_cast<unsigned char*>(list->mpHandlers[k].mpActionStream), nBase, pConstFile, pValueCount);
                    }
                }
                break;
            }
            case 8:
            {
                AptTimelineInitAction* action = reinterpret_cast<AptTimelineInitAction*>(command);
                AptTimelineRelocate(action->mpStream, nBase, false);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, pConstFile, pValueCount);
                break;
            }
            default: break;
            }
        }
    }
}

void AptMovie::unresolve(int nBase, int32_t* pValueCount)
{
    for (int32_t f = 0; f < mnFrameCount; ++f)
    {
        AptMovieFrame& frame = mpFrames[f];
        for (int32_t i = 0; i < frame.mnCommandCount; ++i)
        {
            AptTimelineCommand* command = static_cast<AptTimelineCommand*>(frame.mpCommands[i]);
            switch (command->mnTag)
            {
            case 1:
            {
                AptTimelineAction* action = reinterpret_cast<AptTimelineAction*>(command);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, 0, pValueCount);
                AptTimelineRelocate(action->mpStream, nBase, true);
                break;
            }
            case 2:
                AptTimelineRelocate(reinterpret_cast<AptTimelineAction*>(command)->mpStream, nBase, true);
                break;
            case 3:
            {
                AptPlaceObjectBody_t* place = reinterpret_cast<AptPlaceObjectInfo_t*>(command)->GetBody();
                AptClipEventHandlerList* list = static_cast<AptClipEventHandlerList*>(place->mpClipActions);
                if (list)
                {
                    for (int32_t k = 0; k < list->mnCount; ++k)
                    {
                        AptActionInterpreter::_parseStream(const_cast<unsigned char*>(list->mpHandlers[k].mpActionStream), nBase, 0, pValueCount);
                        AptTimelineRelocate(list->mpHandlers[k].mpActionStream, nBase, true);
                    }
                    AptTimelineRelocate(list->mpHandlers, nBase, true);
                }
                AptTimelineRelocate(place->mpName, nBase, true);
                AptTimelineRelocate(place->mpClipActions, nBase, true);
                break;
            }
            case 8:
            {
                AptTimelineInitAction* action = reinterpret_cast<AptTimelineInitAction*>(command);
                AptActionInterpreter::_parseStream(action->mpStream, nBase, 0, pValueCount);
                AptTimelineRelocate(action->mpStream, nBase, true);
                if (action->mnIndicator < 0) action->mnIndicator = -action->mnIndicator;
                break;
            }
            default: break;
            }
            AptTimelineRelocate(frame.mpCommands[i], nBase, true);
        }
        AptTimelineRelocate(frame.mpCommands, nBase, true);
    }
    AptTimelineRelocate(mpFrames, nBase, true);
    if (mpLabelHash)
    {
        mpLabelHash->DestroyGCPointers();
        AptNativeHash* hash = mpLabelHash;
        hash->~AptNativeHash();
        gpAptPseudoDataPool->Deallocate(hash, sizeof(AptNativeHash));
        mpLabelHash = 0;
    }
}
