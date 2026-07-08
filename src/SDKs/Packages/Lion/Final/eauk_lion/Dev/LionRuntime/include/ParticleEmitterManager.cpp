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
    cParticleDescriptor* lpParent = apDescriptor->mpParent;
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
