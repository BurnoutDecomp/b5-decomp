// ============================================================================
// LionParticleEffectManager.cpp -- cLionParticleEffectManager runtime bodies.
//
// Vendor code (eauk_lion), reconstructed store-for-store from the X360 ARTIST asm:
//   0x829088A8 cLionParticleEffectManager::CreateBehaviour
//   0x82914530 cLionParticleEffectManager::BindingsAttach
//   0x82914CE0 cLionParticleEffectManager::BindingsRemove
// against the DecFIGS DWARF layout in LionParticleEffectManager.h.
//
// The binding attach/remove pair walks the effect's descriptor chain and drives the
// cParticleEmitterManager singleton (dword_831238E8, reached through Instance()):
// attach registers an emitter per descriptor then binds it (the X360 folds the
// emitter's Bind() -- mpBindings store + cLionBindings::SetEmitter -- inline; it is
// re-outlined here as the Bind() call the original source made), while remove calls
// the manager's UnRegister(descriptor, bindings, bindBase) form. Both are external to
// this TU (declared in ParticleEmitterManager.h / ParticleEmitter.h, linked from
// their own TUs). CreateBehaviour carves the node from the effect manager's tagged
// allocator with the LINE(6)->FILE(5)->NAME(1) TagValuePair chain the X360 stamps.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
// Full cParticleBehaviour layout -- CreateBehaviour allocates sizeof(cParticleBehaviour).
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"

namespace
{
// The X360 build tags the behaviour allocation with a (line, file, name) TagValuePair
// chain -- LINE(6)->FILE(5)->NAME(1), head = LINE, matching the sibling pool managers.
const char* const KPC_MANAGER_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/LionParticleEffectManager.cpp";
const char* const KPC_TAG_NAME_BEHAVIOUR = "Lion::ParticleEffect::cParticleBehaviour::";

const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;

// Source line the X360 build stamps onto the behaviour allocation (tag-6 value 0x170).
const s32 KI_LINE_BEHAVIOUR = 368;

// Descriptor mFlags bit that excludes a descriptor from top-level binding attach: the
// bit value 0x8000 is asm-attested (rlwinm r11,r11,0,16,16 masks bit 15); its semantic
// name is inferred (sub/child descriptors are spawned by their parent emitter, not
// registered directly). Only the bit test and its branch are load-bearing.
const u32 KU_DESC_FLAG_SKIP_AUTO_EMITTER = 0x8000;
}  // namespace

// cLionParticleEffectManager::CreateBehaviour @ 0x829088A8
cParticleBehaviour* cLionParticleEffectManager::CreateBehaviour()
{
    EA::TagValuePair lLine(KU_TAG_LINE, KI_LINE_BEHAVIOUR);
    EA::TagValuePair lFile(KU_TAG_FILE, static_cast<const void*>(KPC_MANAGER_FILE));
    EA::TagValuePair lName(KU_TAG_NAME, static_cast<const void*>(KPC_TAG_NAME_BEHAVIOUR));
    return static_cast<cParticleBehaviour*>(
        mpAllocator->Alloc(sizeof(cParticleBehaviour), lLine + lFile + lName));
}

// cLionParticleEffectManager::Free(cParticleBehaviour*) -- DWARF LionParticleEffectManager.h:74.
//
// NO STANDALONE X360 BODY: inlined into cParticleEmitter::DeInit @0x82913380, which loads this
// object's mpAllocator (`lwz r3, off_83123798`) and calls the ITaggedAllocator::Free(ptr, size)
// virtual through it (slot +0xC) with a size of 0. De-inlined back onto the owning method.
//
// ⚠ THE SIZE ARGUMENT IS ZERO, not sizeof(cParticleBehaviour). `li r5, 0` at 0x82913384 is
// unambiguous, and it is asymmetric with CreateBehaviour above, which allocates with the real
// size. Reproduced as the console asks it rather than "corrected".
void cLionParticleEffectManager::Free(cParticleBehaviour* apBehaviour)
{
    mpAllocator->Free(apBehaviour, 0);
}

// cLionParticleEffectManager::BindingsAttach @ 0x82914530
void cLionParticleEffectManager::BindingsAttach(const cLionParticleEffect* apEffect,
                                                cLionBindings& arBindings)
{
    if (!apEffect)
    {
        return;
    }

    // Walk the effect's descriptor chain; register + bind an emitter for each
    // descriptor that is not flagged as a sub/child descriptor.
    for (cParticleDescriptor* lpDes = apEffect->GetDescriptors(); lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        if ((lpDes->mFlags & KU_DESC_FLAG_SKIP_AUTO_EMITTER) != 0)
        {
            continue;
        }

        cParticleEmitter* lpEmitter = cParticleEmitterManager::Instance().Register(lpDes);
        if (lpEmitter != nullptr)
        {
            // X360 folds cParticleEmitter::Bind inline (emitter->mpBindings = &bindings;
            // bindings.SetEmitter(emitter)); re-outlined here as the Bind() call.
            lpEmitter->Bind(arBindings);
        }
    }
}

// cLionParticleEffectManager::BindingsRemove @ 0x82914CE0
void cLionParticleEffectManager::BindingsRemove(const cLionParticleEffect* apEffect,
                                                cLionBindings& arBindings,
                                                cLionBindings* apBindBase)
{
    if (!apEffect)
    {
        return;
    }

    // Unregister the emitter bound to every descriptor of the effect.
    for (cParticleDescriptor* lpDes = apEffect->GetDescriptors(); lpDes != nullptr;
         lpDes = lpDes->GetNextDescriptor())
    {
        cParticleEmitterManager::Instance().UnRegister(*lpDes, arBindings, apBindBase);
    }
}

// ------------------------------------------------------------------------------------------------
// cLionParticleEffectManager::Instance / ::AppInit  (DWARF LionParticleEffectManager.h:42 / h:49)
//
// NEITHER HAS A STANDALONE X360 BODY. Instance is inlined at every call site -- which is why
// cParticleEmitter::Init @0x82913228 calls `CreateBehaviour(&off_83123798)` with that literal --
// and AppInit is inlined into cParticleSystem::AppInit @0x82913810, where it is four operations
// on this object, in this order:
//     cLionBlockAlloc::Init(&unk_8312379C, alloc, 12, count)   ; mAllocator  (this + 0x04)
//     off_83123798   = alloc                                    ; mpAllocator (this + 0x00)
//     dword_831237BC = 0                                        ; mpEffects   (this + 0x24)
//     64-word clear at unk_831237C0                             ; mpEffectGroups[64] (this+0x28)
//
// ⚠ THE ITEM SIZE 12 IS A CONSOLE LITERAL. This pool's element type is not identified in this
// tree (nothing reconstructed allocates from mAllocator yet -- CreateBehaviour uses mpAllocator,
// the TAGGED one), so there is no sizeof() to write instead. If a host record of that element
// type contains a pointer it is wider than 12 bytes here; re-derive the size from the type the
// day something carves out of this pool.
//
// A file-scope object, NOT a function-local static: no guard word sits beside it on the console.
// ------------------------------------------------------------------------------------------------
namespace
{
    cLionParticleEffectManager gLionParticleEffectManagerSingleton;   // X360 off_83123798

    // X360 `li r5, 0xC` in cParticleSystem::AppInit. See the warning above.
    const u32 KU_CONSOLE_EFFECT_MANAGER_ITEM_SIZE = 12;
}

cLionParticleEffectManager& cLionParticleEffectManager::Instance()
{
    return gLionParticleEffectManagerSingleton;
}

void cLionParticleEffectManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator, u32 auCount)
{
    mAllocator.Init(apAllocator, KU_CONSOLE_EFFECT_MANAGER_ITEM_SIZE, auCount);

    mpAllocator = apAllocator;
    mpEffects   = nullptr;

    for (u32 lu = 0; lu < 64; ++lu)
    {
        mpEffectGroups[lu] = nullptr;
    }
}

void cLionParticleEffectManager::AppDeInit()
{
    mAllocator.DeInit();
}
