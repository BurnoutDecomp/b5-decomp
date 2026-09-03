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
// Only these two are here. cLionEffectManager::EffectCreate @0x829149E8 and
// EffectDestroy @0x82914FF0 are real, unreconstructed ledger rows in this TU; they are
// deliberately NOT declared, so a caller that needs them fails at link rather than
// against a quiet body.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h"

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
// ⛔⛔ THE ITEM SIZE IS A CONSOLE LITERAL AND MUST NOT STAY ONE. 0x90 == 144 ==
// sizeof(cLionEffectInstance) on the X360's 4-BYTE-POINTER ABI (mpNext + mpDefinition +
// cLionBindings + mFlags). cLionEffectInstance is NOT homed in this tree, so there is no
// sizeof() to write instead, and on the 64-bit host the real record is WIDER. Nothing
// allocates from this pool on this build (cLionEffectManager::EffectCreate is not
// reconstructed), so 144 is currently only a reservation -- but the FIRST caller that
// carves an instance out of it at 144 bytes overruns every slot. Replace this constant
// with sizeof(cLionEffectInstance) the moment LionEffect.h homes that type.
// ----------------------------------------------------------------------------
namespace
{
    // X360 `li r5, 0x90` @0x82914B14. See the warning above.
    const u32 KU_CONSOLE_SIZEOF_EFFECT_INSTANCE = 0x90;
}

void cLionEffectManager::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                                 u32 auEffectLimit)
{
    mAllocator.Init(apAllocator, KU_CONSOLE_SIZEOF_EFFECT_INSTANCE, auEffectLimit);

    mpAllocator     = apAllocator;
    mEffectCount    = 0;
    mEffectDefCount = 0;
    mpEffectDefs    = nullptr;
    mpEffects       = nullptr;
}
