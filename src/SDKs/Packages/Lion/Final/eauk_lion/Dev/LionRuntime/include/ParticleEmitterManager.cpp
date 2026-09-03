// ============================================================================
// ParticleEmitterManager.cpp -- cParticleEmitterManager runtime bodies.
//
// Vendor code (eauk_lion), reconstructed store-for-store from the X360 ARTIST asm:
//   0x82913470 cParticleEmitterManager::AppInit
//   0x82913590 cParticleEmitterManager::Register
//   0x82913668 cParticleEmitterManager::RegisterSubEmitter
//   0x82915700 cParticleEmitterManager::Update
// against the DecFIGS DWARF layout in ParticleEmitterManager.h. The manager threads its
// free/used lists through each emitter's next-pointer; the X360 build folds the emitter's
// SetActiveFlag / SetNext / GetNextEmitter / Init calls inline, and they are re-outlined
// here as the calls the original source made (per AGENTS "inlining reversal").
//
// UnRegister(cParticleEmitter*) (used by Update) is external to this TU -- declared in the
// manager header and linked from its own TU.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"

// Register inspects the descriptor's first behaviour's flags, so it needs the full
// cParticleBehaviour layout (mFlags @ console +0x2C4). ParticleBehaviour.h supplies the
// single cVector home for this TU -- ParticleBucket.h (the other cVector definition) is
// deliberately NOT included, to avoid an ODR clash.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"

namespace
{
// The X360 build tags the emitter-pool allocation with a (line, file, name) TagValuePair
// chain -- LINE(6)->FILE(5)->NAME(1), head = LINE, matching the sibling pool managers.
const char* const KPC_MANAGER_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/ParticleEmitterManager.cpp";
const char* const KPC_TAG_NAME_EMITTER = "Lion::Emitter::";

const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;

// Source line the X360 build stamps onto the emitter-array allocation (tag-6 value 0x2F).
const s32 KI_LINE_EMITTERS = 47;

// Register gates a used-list duplicate scan on cParticleBehaviour::mFlags bit 0x1000. The
// bit value is asm-attested (rlwinm r11,r11,0,19,19); the semantic name below is inferred
// (a "single live instance" behaviour). The scan's result is discarded in this release
// build (see Register), so only the bit test and its branch are load-bearing.
const u32 KU_BEHAVIOUR_FLAG_SINGLE_INSTANCE = 0x1000;

// The emitter's "active" flag word carries the active state in bit 0 (the X360 folds
// SetActiveFlag(1) to `mFlags |= 1`).
const u32 KU_EMITTER_ACTIVE = 1;
}  // namespace

// cParticleEmitterManager::AppInit @ 0x82913470
void cParticleEmitterManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                                      u32 aEmitterCount)
{
    mpAllocator   = apAllocator;
    mUsedCount    = 0;
    mEmitterCount = aEmitterCount;

    // Allocate the emitter array (X360 mulli by 0x2D0 == sizeof(cParticleEmitter)).
    EA::TagValuePair lLine(KU_TAG_LINE, KI_LINE_EMITTERS);
    EA::TagValuePair lFile(KU_TAG_FILE, static_cast<const void*>(KPC_MANAGER_FILE));
    EA::TagValuePair lName(KU_TAG_NAME, static_cast<const void*>(KPC_TAG_NAME_EMITTER));
    mpEmitters = static_cast<cParticleEmitter*>(mpAllocator->Alloc(
        static_cast<size_t>(aEmitterCount) * sizeof(cParticleEmitter),
        lLine + lFile + lName));

    mpUsed = nullptr;
    mpFree = mpEmitters;

    // Bring every pooled emitter to its idle state (no descriptor yet).
    for (u32 lIndex = 0; lIndex < mEmitterCount; ++lIndex)
    {
        mpEmitters[lIndex].Init(nullptr);
    }

    // Thread the free list: each emitter points at the next; the last keeps Init's null.
    for (u32 lIndex = 0; lIndex + 1 < mEmitterCount; ++lIndex)
    {
        mpEmitters[lIndex].SetNext(&mpEmitters[lIndex + 1]);
    }
}

// cParticleEmitterManager::Register @ 0x82913590
cParticleEmitter* cParticleEmitterManager::Register(cParticleDescriptor* apDescriptor)
{
    if (!apDescriptor)
    {
        return nullptr;
    }

    // The descriptor must own a behaviour chain and a material to be playable.
    cParticleBehaviour* lpBehaviour = apDescriptor->GetBehaviours();
    if (!lpBehaviour)
    {
        return nullptr;
    }
    if (!apDescriptor->mpMaterial)
    {
        return nullptr;
    }

    // Behaviour flag 0x1000 gates a scan of the used list for an emitter already playing
    // this descriptor. The release build computes the match but discards it (its result
    // was consumed only in a debug path ARTIST dropped); reproduced here for branch fidelity.
    if ((lpBehaviour->mFlags & KU_BEHAVIOUR_FLAG_SINGLE_INSTANCE) != 0)
    {
        for (cParticleEmitter* lpExisting = mpUsed; lpExisting != nullptr;
             lpExisting = lpExisting->GetNextEmitter())
        {
            if (lpExisting->GetDescriptor() == apDescriptor)
            {
                break;
            }
        }
    }

    // Pop the head of the free list; bail if the pool is exhausted.
    cParticleEmitter* lpEmit = mpFree;
    if (!lpEmit)
    {
        return nullptr;
    }

    mpFree = lpEmit->GetNextEmitter();   // read next BEFORE Init overwrites it
    lpEmit->Init(apDescriptor);
    lpEmit->SetActiveFlag(KU_EMITTER_ACTIVE);

    // Push onto the head of the used list.
    lpEmit->SetNext(mpUsed);
    mpUsed = lpEmit;
    ++mUsedCount;
    return lpEmit;
}

// cParticleEmitterManager::RegisterSubEmitter @ 0x82913668
cParticleEmitter* cParticleEmitterManager::RegisterSubEmitter(cParticleDescriptor* apDescriptor)
{
    cParticleEmitter* lpPrev = nullptr;
    cParticleEmitter* lpEmit = mpUsed;
    if (!lpEmit)
    {
        return nullptr;
    }

    // Locate the used emitter this sub-emitter attaches under: the one whose descriptor is
    // apDescriptor's parent, or a descriptor the parent is an ancestor of.
    cParticleDescriptor* lpParent = apDescriptor->mpParent.Get();
    for (;;)
    {
        cParticleEmitter* lpNext = lpEmit->GetNextEmitter();
        const cParticleDescriptor* lpEmitDes = lpEmit->GetDescriptor();
        if (lpParent != nullptr &&
            (lpEmitDes == lpParent || lpParent->IsChildOf(*lpEmitDes)))
        {
            break;  // insert ahead of lpEmit (i.e. after lpPrev)
        }

        lpPrev = lpEmit;
        lpEmit = lpNext;
        if (!lpNext)
        {
            return nullptr;  // no parent emitter live
        }
    }

    // Pop a fresh emitter off the free list.
    cParticleEmitter* lpNew = mpFree;
    if (!lpNew)
    {
        return nullptr;
    }

    mpFree = lpNew->GetNextEmitter();   // read next BEFORE Init overwrites it
    lpNew->Init(apDescriptor);
    lpNew->SetActiveFlag(KU_EMITTER_ACTIVE);

    if (lpPrev != nullptr)
    {
        // Splice in after lpPrev (immediately before the matched emitter).
        lpNew->SetNext(lpPrev->GetNextEmitter());
        lpPrev->SetNext(lpNew);
    }
    else
    {
        // Matched the head of the used list: push onto the head.
        lpNew->SetNext(mpUsed);
        mpUsed = lpNew;
    }
    ++mUsedCount;
    return lpNew;
}

// cParticleEmitterManager::Update @ 0x82915700
void cParticleEmitterManager::Update(const cTime& arTime)
{
    cParticleEmitter* lpEmit = mpUsed;
    while (lpEmit != nullptr)
    {
        // Cache the successor first: UnRegister recycles lpEmit and rewrites its next-ptr.
        cParticleEmitter* lpNext = lpEmit->GetNextEmitter();
        if (!lpEmit->Update(arTime))
        {
            UnRegister(lpEmit);
        }
        lpEmit = lpNext;
    }
}

// ------------------------------------------------------------------------------------------------
// cParticleEmitterManager::Instance (DWARF ParticleEmitterManager.h:39)
//
// NO STANDALONE X360 BODY -- inlined at every call site, which is why cParticleSystem::AppInit
// @0x82913810, cLionFX::Update @0x82915758 and cLionFX::Render @0x82914C50 all pass the literal
// `&dword_831238E8` as `this`. It was DECLARED in this class's header and defined NOWHERE in the
// tree until now: LionParticleEffectManager.cpp already called it twice, so the Lion effect
// manager could never have linked. (`progress/status.json` marks every row here `reviewed`; that
// is the default, and it is not evidence a body exists -- ask the tree.)
//
// A file-scope object, NOT a function-local static: no guard word sits beside it on the console.
// ------------------------------------------------------------------------------------------------
namespace
{
    cParticleEmitterManager gEmitterManagerSingleton;   // X360 dword_831238E8
}

cParticleEmitterManager& cParticleEmitterManager::Instance()
{
    return gEmitterManagerSingleton;
}

// ================================================================================================
// cParticleEmitterManager::UnRegister(cParticleEmitter*)  @0x82913760
//
// AN EXPORT-SET HOLE -- IDA names it in cParticleEmitter::DeInit's and Update's xrefs but emits
// no 0x82913760.json, so it had no ledger row and no pseudocode. Disassembled out of the image
// (tools/re/ppcdis.py); 39 instructions:
//
//     if (!apEmitter) return;
//     r11 = mpUsed (0x18), r10 = &mpUsed
//     ...prev-link walk over mpNext (+0x204), unlinking apEmitter and zeroing its link...
//     cParticleEmitter::DeInit(apEmitter)          bl 0x82913330
//     apEmitter->mpNext = mpFree (0x14)            lwz r11,0x14(r30) ; stw r11,0x204(r31)
//     mpFree = apEmitter                           stw r31,0x14(r30)
//     --mUsedCount (0x04)                          lwz/addi -1/stw 4(r30)
//
// ⭐ WHY IT WAS PARKED AND WHY THAT REASON IS GONE. LionRuntimeLinkStubs.cpp refused to body
// this one on the grounds that "its DeInit is not bodied either -- a faithful UnRegister that
// calls a trap is worse than the trap, because it does its list surgery FIRST and leaves the
// pool half-modified when the trap fires". cParticleEmitter::DeInit @0x82913330 IS bodied
// (ParticleEmitter.cpp), and has been; the note went stale. The hazard it describes is real and
// is exactly why the check was worth redoing rather than trusting the comment.
//
// ⭐ THE UNLINK IS ATTEMPTED EVEN WHEN THE EMITTER IS NOT ON mpUsed, and the walk simply falls
// off the end (0x829137B8 branches past the surgery to the DeInit). So an emitter that is
// already off the list is still DeInit'd and still pushed onto mpFree -- which is what makes
// this safe to call from cParticleEmitterManager::Update's "returned 0" path and from
// UnRegister(descriptor,...) in the same frame.
// ================================================================================================
void cParticleEmitterManager::UnRegister(cParticleEmitter* apEmitter)
{
    if (apEmitter == 0)
        return;

    // Unlink from the used list (link field is mpNext, console +0x204).
    if (mpUsed != 0)
    {
        cParticleEmitter* lpPrev = 0;
        cParticleEmitter* lpNode = mpUsed;
        while (lpNode != 0 && lpNode != apEmitter)
        {
            lpPrev = lpNode;
            lpNode = lpNode->GetNextEmitter();
        }
        if (lpNode != 0)
        {
            if (lpPrev != 0)
                lpPrev->SetNext(lpNode->GetNextEmitter());
            else
                mpUsed = lpNode->GetNextEmitter();
            lpNode->SetNext(0);
        }
    }

    apEmitter->DeInit();

    // Push onto the free list and drop the live count.
    apEmitter->SetNext(mpFree);
    mpFree = apEmitter;
    --mUsedCount;
}

// ================================================================================================
// cParticleEmitterManager::UnRegister(const cParticleDescriptor&, cLionBindings&, cLionBindings*)
//                                                                                    @0x829146D0
// (X360 exports it unnamed as sub_829146D0; the DWARF names it, ParticleEmitterManager.h:64.)
//
// Reached only from cLionParticleEffectManager::BindingsRemove, once per descriptor of the
// effect being destroyed. It walks the USED list and retires -- or RE-BINDS -- every emitter
// whose descriptor is, or descends from, arDescriptor.
//
// THE TWO ARMS ARE SELECTED BY THE DESCRIPTOR'S BEHAVIOUR FLAG 0x1000 (asm 0x829146EC:
// `lwz r11, 0x40(r30)` == arDescriptor.mpBehaviours, then `lwz r11, 0x2C4(r11)` ==
// cParticleBehaviour::mFlags, `rlwinm r11,r11,0,19,19` == bit 12). That is the same
// SINGLE-INSTANCE bit cParticleEmitterManager::Register tests at 0x82913590.
//
//   SINGLE-INSTANCE arm (flag set): match on the DESCRIPTOR alone, ignoring which binding set
//   the emitter belongs to -- because there is only supposed to be one emitter for it. When a
//   sibling binding chain exists (apBindBase non-null), the emitter is not destroyed but
//   HANDED OVER to that chain (0x82914764/68 -- the same two stores as cParticleEmitter::Bind),
//   unless the descriptor is flagged 0x8000 (E_FLAG_SKIP_AUTO_EMITTER), in which case it is
//   unregistered outright.
//
//   ORDINARY arm (flag clear): match on the BINDINGS first (`lwz r11,0x1FC(r31)` compared
//   against arBindings at 0x829147A0) and only then on the descriptor, and always unregister.
//
// ⭐ THE ANCESTRY TEST IS `emitterDescriptor.mpParent->IsChildOf(arDescriptor)`, NOT the other
// way round, and Hex-Rays hides it: it renders the call with no arguments at all. The asm is
// unambiguous -- r3 comes from `lwz r3, 0x58(r11)` (the EMITTER's descriptor's mpParent) and r4
// from the incoming descriptor. There are three ways to match, tested in order: the emitter's
// descriptor IS arDescriptor; its mpParent IS arDescriptor; its mpParent descends from
// arDescriptor. A null mpParent fails the whole test (0x829147B8).
//
// ⚠ THE SUCCESSOR IS CACHED BEFORE THE BODY, on both arms (`lwz r29, 0x204(r31)` at 0x82914710
// and 0x8291479C, i.e. before any call). It has to be: UnRegister(emitter) below zeroes the
// node's mpNext and pushes it onto the free list, so re-reading the link afterwards would walk
// the FREE list instead.
// ================================================================================================
namespace
{
    // The three-way descriptor match both arms run, factored out so each arm can call it
    // exactly where the console does (the ordinary arm tests the BINDINGS first, and running
    // this eagerly would call IsChildOf on emitters the console never asks about).
    bool DescriptorMatches(const cParticleDescriptor* apEmitterDescriptor,
                           const cParticleDescriptor& arDescriptor)
    {
        if (apEmitterDescriptor == &arDescriptor)
            return true;

        const cParticleDescriptor* lpParent = apEmitterDescriptor->mpParent.Get();
        if (lpParent == 0)
            return false;

        return (lpParent == &arDescriptor) || (lpParent->IsChildOf(arDescriptor) != 0);
    }
}

void cParticleEmitterManager::UnRegister(const cParticleDescriptor& arDescriptor,
                                         cLionBindings& arBindings,
                                         cLionBindings* apBindBase)
{
    // arDescriptor.mpBehaviours->mFlags & 0x1000 -- the single-instance selector.
    const cParticleBehaviour* lpBehaviour = arDescriptor.GetBehaviours();
    const bool lbSingleInstance =
        (lpBehaviour->mFlags & KU_BEHAVIOUR_FLAG_SINGLE_INSTANCE) != 0;

    cParticleEmitter* lpEmit = mpUsed;
    while (lpEmit != 0)
    {
        // Cache the successor BEFORE anything can recycle this node.
        cParticleEmitter* lpNext = lpEmit->GetNextEmitter();

        if (lbSingleInstance)
        {
            if (DescriptorMatches(lpEmit->GetDescriptor(), arDescriptor))
            {
                if (apBindBase == 0
                    || lpEmit->mpBindings == apBindBase
                    || (arDescriptor.mFlags & cParticleDescriptor::E_FLAG_SKIP_AUTO_EMITTER) != 0)
                {
                    UnRegister(lpEmit);
                }
                else
                {
                    // Hand the emitter over to the sibling binding chain rather than
                    // destroying it -- the same two stores as cParticleEmitter::Bind.
                    lpEmit->Bind(*apBindBase);
                }
            }
        }
        else if (lpEmit->mpBindings == &arBindings
                 && DescriptorMatches(lpEmit->GetDescriptor(), arDescriptor))
        {
            UnRegister(lpEmit);
        }

        lpEmit = lpNext;
    }
}
