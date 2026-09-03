// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.cpp
//
// cLionEffectManager -- the singleton and the app-lifetime init.
//
// ⭐ NEITHER FUNCTION HAS A BODY OF ITS OWN IN THE X360 IMAGE. Both are inlined into
// cLionFX::Init @0x82914A98, which is itself an EXPORT-SET HOLE (IDA names it in every
// callee's xrefs_to but emits no 0x82914A98.json). They are recovered from the raw image
// -- disassembled straight out of the .i64 flag array -- and cross-checked against the
// DecFIGS DWARF, which names both (LionEffectManager.h:33 GetMe, h:38 AppInit).
//
// ⭐ 2026-09-03 (boost-exhaust wave): EffectCreate @0x829149E8 and EffectDestroy @0x82914FF0
// join them. They are the whole REGISTRATION half of the gap between this build and a drawn
// particle -- until EffectCreate existed, no cLionEffectInstance was ever created, so
// cLionParticleEffectManager::BindingsAttach was never called, so
// cParticleEmitterManager::Register was never called, so the emitter free list built by
// AppInit was populated and never drawn from.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"

// ----------------------------------------------------------------------------
// cLionEffectManager::GetMe (DWARF LionEffectManager.h:33)
//
// The DWARF declares `mSingleton` (h:71) as a static member of the class; the X360 build
// reaches its storage directly at 0x83121D94 from every call site (cLionFX::Init,
// cLionFX::EffectCreate @0x82914CB8, cLionFX::EffectDestroy @0x82915148 all pass that
// literal address as `this`), which is exactly what an inlined GetMe() looks like.
//
// A file-scope object here, NOT a function-local static: the console object lives in .bss
// with no guard word beside it (the neighbouring globals are LionSmallAlloc's, and the one
// magic-static guard in this cluster -- dword_82FAD080 -- belongs to cParticleRender's
// m_instance, 0x1B28C bytes away). A function-local static would add a guard the console
// does not have and would make first use non-trivially ordered.
// ----------------------------------------------------------------------------
namespace
{
    cLionEffectManager gLionEffectManagerSingleton;   // X360 mSingleton @0x83121D94
}

cLionEffectManager* cLionEffectManager::GetMe()
{
    return &gLionEffectManagerSingleton;
}

// ----------------------------------------------------------------------------
// cLionEffectManager::AppInit (DWARF LionEffectManager.h:38)
//
// Recovered from cLionFX::Init @0x82914A98, 0x82914B08..0x82914B58:
//     82914B10  addi r31, r11, 0x1D94         ; r31 = &mSingleton
//     82914B14  li   r5, 0x90                 ; item size
//     82914B18  mr   r4, r30                  ; apAllocator
//     82914B1C  addi r3, r31, 0x14            ; &mAllocator
//     82914B20  bl   cLionBlockAlloc::Init    ; (alloc, 0x90, r6 == r29 == auEffectLimit)
//     82914B28  stw  r30, 0x10(r31)           ; mpAllocator   = apAllocator
//     82914B44  stw  r11(0), 4(r31)           ; mEffectCount  = 0
//     82914B48  stw  r11(0), 0(r31)           ; mEffectDefCount = 0
//     82914B4C  stw  r11(0), 8(r31)           ; mpEffectDefs  = 0
//     82914B50  stw  r11(0), 0xC(r31)         ; mpEffects     = 0
// (the console's cLionBlockAlloc::Init argument order is (this, allocator, itemSize,
//  itemCount) -- r3/r4/r5/r6 -- which is the order the reconstructed Init already takes.)
//
// ⭐⭐ THE ITEM SIZE WAS A CONSOLE LITERAL AND IS NOW A sizeof() (fixed 2026-09-03). This
// constant used to read `0x90` with a warning attached: 144 is sizeof(cLionEffectInstance) on
// the X360's 4-BYTE-POINTER ABI, the type had no home in this tree, and "the FIRST caller
// that carves an instance out of it at 144 bytes overruns every slot". THAT CALLER IS NOW
// HERE -- cLionEffectManager::EffectCreate, below -- and cLionEffectInstance is homed in
// LionEffect.h, so the pool is sized by the host record. The console number is kept beside it
// as the cross-check it always was: if the host record is not WIDER than 144 the model is
// wrong (the two pointers must have widened), and the static_assert says so.
// ----------------------------------------------------------------------------
namespace
{
    // X360 `li r5, 0x90` @0x82914B14 -- the console's own sizeof(cLionEffectInstance).
    const u32 KU_CONSOLE_SIZEOF_EFFECT_INSTANCE = 0x90;

    static_assert(sizeof(cLionEffectInstance) >= KU_CONSOLE_SIZEOF_EFFECT_INSTANCE,
                  "the host cLionEffectInstance must be at least the console's 144 bytes "
                  "(cLionFX::Init @0x82914B14 `li r5, 0x90`); a SMALLER host record means the "
                  "two 4-byte console pointers did not widen and the layout is wrong");
}

void cLionEffectManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                                 u32 auEffectLimit)
{
    mAllocator.Init(apAllocator, sizeof(cLionEffectInstance), auEffectLimit);

    mpAllocator     = apAllocator;
    mEffectCount    = 0;
    mEffectDefCount = 0;
    mpEffectDefs    = nullptr;
    mpEffects       = nullptr;
}

// ================================================================================================
// cLionEffectManager::EffectCreate  @0x829149E8
//
// The whole body, in the console's own order:
//
//     result = 0;
//     if ( a2 )                                          -- the definition
//     {
//       ++a1[1];                                         -- ++mEffectCount
//       v13 = cLionBlockAlloc::Alloc(a1 + 5);            -- carve from mAllocator (this+0x14)
//       if ( v13 )
//       {
//         v15 = v13 + 16;                                -- &instance->mBindings
//         *(v13 + 128) = 0;                              -- mFlags = 0
//         cLionBindings::Init(v13 + 16);
//         v14[7] = a3; v14[8] = a4; v14[9] = a5;         -- mpLocator / mpScaler / mpTrigger
//         v14[5] = a6;                                   -- mWorldIndex
//         v14[1] = a2;                                   -- mpDefinition
//         cLionParticleEffectManager::BindingsAttach(&off_83123798, *(a2 + 72), v14 + 4);
//         v14[28] = *(a2 + 76);                          -- mBindings.mpNext = def->mpBindings
//         *(a2 + 76) = v15;                              -- def->mpBindings  = &mBindings
//         *v14 = a1[3]; a1[3] = v14;                     -- push onto mpEffects
//       }
//       result = v14;
//     }
//
// ⭐ THE COUNT IS BUMPED BEFORE THE ALLOCATION AND IS NOT BACKED OUT WHEN IT FAILS. That is
// the console's arithmetic, not a transcription slip: the increment sits at 0x82914A04, before
// the call to cLionBlockAlloc::Alloc at 0x82914A0C, and nothing on the null path decrements
// it. EffectDestroy's decrement is unconditional on a non-null instance, so the pair balances
// for every effect that actually exists and mEffectCount drifts up by one per FAILED create.
// Reproduced as-is -- "fixing" it would be inventing behaviour the binary does not have.
//
// ⚠ THE BINDING PUSH TRUNCATES ON THE HOST, BY DESIGN. cLionEffectDefinition::mpBindings is a
// tLionSerialisedPtr -- a 4-byte slot, because the 84-byte .lef record says so (cLionFX::
// BinSave @0x82914438 stores exactly 84) -- and what goes into it here is a live heap pointer,
// not a serialised offset. That is the project's standing below-4 GB convention, already
// relied on by cLionFX::BinLoad's own mpNext push and by both PARTICLES.BUNDLE FixUp handlers.
// The instance comes out of the Lion tagged allocator; if that heap is ever placed above 4 GB
// the truncation is silent, and this chain is where it bites first.
// ================================================================================================
cLionEffectInstance* cLionEffectManager::EffectCreate(cLionEffectDefinition* apDefinition,
                                                      cParticleLocator* apLocator,
                                                      cParticleScaler* apScaler,
                                                      cParticleTrigger* apTrigger,
                                                      u32 auWorldIndex)
{
    if (apDefinition == 0)
        return 0;

    ++mEffectCount;

    cLionEffectInstance* lpInstance = static_cast<cLionEffectInstance*>(mAllocator.Alloc());
    if (lpInstance == 0)
        return 0;

    lpInstance->Init();                     // mFlags = 0; mBindings.Init()

    cLionBindings& lrBindings = lpInstance->GetBindings();
    lrBindings.SetpLocator(apLocator);
    lrBindings.SetpScaler(apScaler);
    lrBindings.SetpTrigger(apTrigger);
    lrBindings.SetWorldIndex(auWorldIndex);

    lpInstance->SetpEffectDefinition(apDefinition);

    // The line that actually starts the effect: cLionParticleEffectManager::BindingsAttach
    // registers ONE EMITTER per top-level descriptor of the definition's cLionParticleEffect
    // and binds each to these bindings. Nothing else in the runtime creates an emitter.
    cLionParticleEffectManager::Instance().BindingsAttach(apDefinition->GetParticles(),
                                                         lrBindings);

    // Push these bindings onto the definition's binding chain.
    lrBindings.SetNextBinding(apDefinition->mpBindings.Get());
    apDefinition->mpBindings.Set(&lrBindings);

    // Push the instance onto the manager's live-instance chain.
    lpInstance->SetpNext(mpEffects);
    mpEffects = lpInstance;

    return lpInstance;
}

// ================================================================================================
// cLionEffectManager::EffectDestroy  @0x82914FF0
//
// Read from the ASM (0x82914FF0..0x82915144) rather than the pseudocode, because Hex-Rays
// dropped the SECOND argument of all three cLionBlockAlloc::DeAlloc calls -- it renders them
// as DeAlloc(&pool) when r4 carries the entry every time:
//
//     lwz r4, 0x24(r30) ; if (trigger) DeAlloc(gLionTriggerAllocator, trigger)
//     lwz r4, 0x1C(r30) ; if (locator) DeAlloc(gLionLocatorAllocator, locator)
//     lwz r4, 0x20(r30) ;              DeAlloc(gLionScalerAllocator,  scaler)   <-- UNGUARDED
//     cLionBindings::DeInit(r30 + 0x10)
//     ...unlink &mBindings from mpDefinition->mpBindings (chain link at +0x60 == mpNext)...
//     cLionParticleEffectManager::BindingsRemove(&mSingleton, def->mpParticles, mBindings,
//                                                def->mpBindings)      <-- read AFTER the unlink
//     ...unlink the instance from this->mpEffects (chain link at +0x00)...
//     DeAlloc(&mAllocator, instance);  --mEffectCount
//
// ⭐ THE SCALER FREE IS GENUINELY UNCONDITIONAL where the other two are guarded: 0x8291503C
// runs straight into its call with none of the compare/branch pairs that precede the other
// two (0x82915010, 0x82915028). cLionBlockAlloc::DeAlloc tolerates a null entry, so the
// asymmetry is harmless -- but it is the console's, and it is transcribed, not tidied.
//
// ⭐ BindingsRemove's FOURTH ARGUMENT IS READ AFTER THE UNLINK, NOT BEFORE. r6 comes from
// `lwz r6, 0x4C(r9)` at 0x829150C8, i.e. the definition's binding-chain HEAD as it stands once
// this instance's node has been taken out. Reading it earlier would hand the emitter manager
// the node being destroyed as the sibling base.
//
// ⚠ mpDefinition IS DEREFERENCED UNGUARDED at that call (`lwz r10,4(r30); lwz r4,0x48(r10)`)
// even though the unlink above it is guarded. The console assumes a live instance always has a
// definition -- EffectCreate refuses to make one without. Transcribed as the console has it;
// an added guard here would hide the invariant breaking rather than report it.
// ================================================================================================
void cLionEffectManager::EffectDestroy(cLionEffectInstance* apInstance)
{
    if (apInstance == 0)
        return;

    cLionBindings& lrBindings = apInstance->GetBindings();

    if (lrBindings.GetpTrigger() != 0)
        gLionTriggerAllocator.DeAlloc(lrBindings.GetpTrigger());
    if (lrBindings.GetpLocator() != 0)
        gLionLocatorAllocator.DeAlloc(lrBindings.GetpLocator());
    gLionScalerAllocator.DeAlloc(lrBindings.GetpScaler());

    lrBindings.DeInit();

    // ---- unlink these bindings from the definition's chain --------------------------------
    cLionEffectDefinition* lpDefinition = apInstance->mpDefinition;
    if (lpDefinition != 0 && !lpDefinition->mpBindings.IsNull())
    {
        cLionBindings* lpPrev = 0;
        cLionBindings* lpNode = lpDefinition->mpBindings.Get();
        while (lpNode != 0 && lpNode != &lrBindings)
        {
            lpPrev = lpNode;
            lpNode = lpNode->GetNextBinding();
        }
        if (lpNode != 0)
        {
            if (lpPrev != 0)
                lpPrev->SetNextBinding(lpNode->GetNextBinding());
            else
                lpDefinition->mpBindings.Set(lpNode->GetNextBinding());
            lpNode->SetNextBinding(0);
        }
    }

    // ---- unregister every emitter these bindings drove ------------------------------------
    cLionParticleEffectManager::Instance().BindingsRemove(
        apInstance->mpDefinition->GetParticles(),
        lrBindings,
        lpDefinition->mpBindings.Get());

    // ---- unlink the instance from the manager's live chain --------------------------------
    if (mpEffects != 0)
    {
        cLionEffectInstance* lpPrev = 0;
        cLionEffectInstance* lpNode = mpEffects;
        while (lpNode != 0 && lpNode != apInstance)
        {
            lpPrev = lpNode;
            lpNode = lpNode->GetpNext();
        }
        if (lpNode != 0)
        {
            if (lpPrev != 0)
                lpPrev->SetpNext(lpNode->GetpNext());
            else
                mpEffects = lpNode->GetpNext();
            lpNode->SetpNext(0);
        }
    }

    mAllocator.DeAlloc(apInstance);
    --mEffectCount;
}
