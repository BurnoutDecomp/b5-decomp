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
