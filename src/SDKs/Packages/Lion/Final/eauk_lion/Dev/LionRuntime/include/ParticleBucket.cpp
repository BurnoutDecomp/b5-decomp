// ============================================================================
// ParticleBucket.cpp -- cParticleBucket runtime bodies.
//
// Bodied store-for-store from the X360 asm. The single function recon'd in this
// TU is AllocateParticle @ 0x82908750.
//
// dep_flags (HONEST PLACEHOLDERS not yet homed elsewhere, modelled minimally in
// ParticleBucket.h): cVector, cMatrix, cTime, cParticleRandomSeed,
// sParticleNucleus, cParticleEmitter. Replace each with its real home (grow
// additively) when that home lands; cVector in particular duplicates a sibling
// placeholder in ParticleBehaviour.h and both should collapse onto one real home.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h"

// cParticleBucket::AllocateParticle @ 0x82908750
//
// X360 store order reproduced exactly:
//   1. read count = mnNextParticlePositionToFill (r10 <- 0x50); if == 16 return 0
//   2. mActiveBits |= (1u << count)                        (stw 0x54)
//   3. *arSlot   = count                                   (stw 0(r4))
//   4. *appNucleus = &mParticles[count]   (count re-read; base+0x60, stride 0xE0)
//   5. if mpVectors != 0:
//        *appVector = &mpVectors[count]   (count re-read; stride 0x10)
//        *appMatrix = 0
//        ++mnNextParticlePositionToFill
//        return true
//      else:
//        *appVector = 0
//        *appMatrix = mpMatrices ? &mpMatrices[count] : 0   (count re-read; 0x40)
//        ++mnNextParticlePositionToFill
//        return true
bool cParticleBucket::AllocateParticle(u32& arSlot,
                                       sParticleNucleus** appNucleus,
                                       cVector** appVector,
                                       cMatrix** appMatrix)
{
    const u32 luSlot = mnNextParticlePositionToFill;        // r10 <- 0x50
    if (luSlot == KU_MAX_PARTICLES)                          // cmplwi 0x10
        return false;                                        // li r3,0 ; blr

    mActiveBits |= (1u << luSlot);                           // or + stw 0x54

    arSlot = luSlot;                                         // stw 0(r4)

    // &mParticles[count]: base this+0x60, element stride 0xE0 (mulli 0xE0 + addi 0x60).
    *appNucleus = &mParticles[mnNextParticlePositionToFill]; // re-read 0x50

    if (mpVectors != 0)                                      // lwz r10,0xE64 ; cmplwi 0
    {
        // &mpVectors[count]: stride 0x10 (slwi r9,r9,4 + add).
        *appVector = &mpVectors[mnNextParticlePositionToFill];
        *appMatrix = 0;                                      // stw 0 at r7
        ++mnNextParticlePositionToFill;                      // ++0x50
        return true;                                         // li r3,1
    }
    else
    {
        cMatrix* lpMatrices = mpMatrices;                    // lwz r10,0xE60
        *appVector = 0;                                      // stw 0 at r6
        cMatrix* lpResult = 0;                               // li r10,0
        if (lpMatrices != 0)                                 // cmplwi 0
            // &mpMatrices[count]: stride 0x40 (slwi r10,r10,6 + add).
            lpResult = &mpMatrices[mnNextParticlePositionToFill];
        *appMatrix = lpResult;                               // stw at r7
        ++mnNextParticlePositionToFill;                      // ++0x50
        return true;                                         // li r3,1
    }
}

// ================================================================================================
// cParticleBucket::GetpMatrix  @ 0x8290F188      (DWARF ParticleBucket.h:81)
//
// Produce the world transform for ONE particle in this bucket. Which of three sources it comes
// from is decided by what the bucket was allocated WITH, and the three are mutually exclusive:
//
//   mpMatrices != 0  -- the bucket carries a full per-particle matrix; copy it (four `lvx128` /
//                       `stvx128` row moves, 0x8290F1E8..0x8290F204).
//   mpVectors  != 0  -- the bucket carries only a per-particle POSITION; build an identity and
//                       drop that position into the translation row (0x8290F218..0x8290F284).
//   neither          -- the particle has no transform of its own and rides the emitter's
//                       locator; sample it for arTime and copy that (0x8290F28C..0x8290F2D0).
//
// ⭐ THAT THREE-WAY IS THE SAME SPLIT cParticleRender::EmitterRender @0x82913928 USES TO PICK ITS
// SIMULATION KERNEL -- `bucket[920]` then `bucket[921]` then the fallthrough, i.e. mpMatrices,
// mpVectors, else "local". Reading the two together is what names the third case: it is not a
// missing feature, it is the local/locator-relative bucket type.
//
// ⚠ THE mpVectors ARM DELIBERATELY SKIPS ONE STORE THE OTHER TWO SHARE. Both matrix arms fall
// into `stvx128 v0, r0, r11` at 0x8290F2D0, which writes the fourth row they just loaded; the
// vector arm branches PAST it (`b loc_8290F2D4` at 0x8290F288) because it has already written
// its own translation row. Reproduced by structure rather than by a flag.
//
// ⚠ DO_IGNORE_ROT IS APPLIED HERE TOO, AFTER the fact, and it is the same descriptor bit 0x100
// cParticleEmitter::InitialiseParticle tests at spawn (`rlwinm r8, r8, 0,23,23` @0x8290F2DC).
// The console saves the finished translation row with two 8-byte integer loads, rebuilds the
// identity over the whole matrix, and puts the three translation components back -- so the
// rotation is discarded EVERY frame, not only at spawn. Written as BuildIdentity + SetTrans,
// which is what those sixteen `stfs` of flt_82001C98 (1.0) and flt_82001CC0 (0.0) are.
//
// ⚠ THE DWARF RETURNS void AND THE ASM LEAVES r3 LIVE. r3 holds whatever the last call left
// there (the locator arm's GetMat result, or `this` otherwise); no caller reads it, and the
// DWARF (ParticleBucket.h:81) types the function void. Written void.
// ================================================================================================
void cParticleBucket::GetpMatrix(u32 auIndex, cMatrix* apMatrix, const cTime& arTime)
{
    const cParticleEmitter* lpEmitter = mpEmitter;

    if (mpMatrices != nullptr)
    {
        *apMatrix = mpMatrices[auIndex];
    }
    else if (mpVectors != nullptr)
    {
        const cVector& lrPosition = mpVectors[auIndex];
        apMatrix->BuildIdentity();
        apMatrix->SetTrans(lrPosition.x, lrPosition.y, lrPosition.z);
    }
    else
    {
        *apMatrix = lpEmitter->GetBindings().GetpLocator()->GetMat(arTime);
    }

    if ((lpEmitter->GetDescriptor()->Flags() & cParticleDescriptor::E_FLAG_IGNORE_ROT) != 0)
    {
        const cVector lTranslation = apMatrix->wa;
        apMatrix->BuildIdentity();
        apMatrix->SetTrans(lTranslation.x, lTranslation.y, lTranslation.z);
    }
}
