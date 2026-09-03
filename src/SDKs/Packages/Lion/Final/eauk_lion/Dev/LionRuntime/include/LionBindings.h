#pragma once

// cLionBindings -- binds a Lion particle effect instance to its locator/scaler/trigger
// inputs (SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h).
//
// Layout recovered from the X360 DWARF (LionBindings.h). DeInit (0x82908240) frees the
// owned mppLocators array through the effect manager's tagged allocator and clears the
// count/pointer.
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home. The particle input
// types (cParticleLocator/Scaler/Trigger/Emitter) are forward-declared here; mSeed comes
// from the real cParticleRandomSeed home.
//
// ⛔⛔ THE SEED WAS A FORK, AND THE FORK MOVED TWO MEMBERS (fixed 2026-09-03). This header
// used to define its own `struct cParticleRandomSeed { u32 muSeed; }` -- FOUR bytes, against
// the real type's 0x40 -- and then laid mpNext at +0x1C and mpEmitter at +0x20 behind it.
// The asm says otherwise: cParticleEmitterManager::UnRegister(descriptor,...) @0x829146D0
// back-links the binding's emitter with `*(a4 + 100) = v8`, i.e. mpEmitter at +0x64. That
// only comes out right with the REAL seed: mpTrigger ends at 0x18, the 16-byte-aligned
// cParticleRandomSeed starts at 0x20, its 0x40 bytes end at 0x60, mpNext takes 0x60 and
// mpEmitter lands on 0x64. Exactly the asm's offset, with nothing left over.
// (The DecFIGS DWARF, LionBindings.h:110, types the member cParticleRandomSeed too -- the
// fork disagreed with both authorities at once, and it linked silently because no
// reconstructed body had yet touched anything past mppLocators.)

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRandomSeed.h"

class cParticleLocator;
class cParticleScaler;
class cParticleTrigger;
class cParticleEmitter;

struct cLionBindings
{
public:
    void Init();
    void DeInit();

    void SetLocatorCount(u32 aCount);

    // DWARF LionBindings.h:57. The emitter's locator, i.e. the transform a particle spawns
    // in. cParticleEmitter::InitialiseParticle @0x82911978 reaches it with `lwz r11, 0x1FC(r25)`
    // (mpBindings) then `lwz r3, 0xC(r11)` (mpLocator) -- inline, as an accessor compiles.
    cParticleLocator* GetpLocator() const { return mpLocator; }

    cLionBindings* GetNextBinding() { return mpNext; }
    void           SetNextBinding(cLionBindings* apNext) { mpNext = apNext; }

private:
    u32                mLocatorCount;  // +0x00  LionBindings.h:103
    u32                mWorldIndex;    // +0x04  LionBindings.h:104
    cParticleLocator** mppLocators;    // +0x08  LionBindings.h:106 (owned locator array)
    cParticleLocator*  mpLocator;      // +0x0C  LionBindings.h:107
    cParticleScaler*   mpScaler;       // +0x10  LionBindings.h:108
    cParticleTrigger*  mpTrigger;      // +0x14  LionBindings.h:109
    u8                 maPad18[0x08];  // +0x18  the seed's 16-byte alignment (console)
    cParticleRandomSeed mSeed;         // +0x20  LionBindings.h:110 (console span 0x40)
    u8                 maPadSeed[0x10];// +0x50  the seed's tail padding to +0x60 (console)
    cLionBindings*     mpNext;         // +0x60  LionBindings.h:111
    cParticleEmitter*  mpEmitter;      // +0x64  LionBindings.h:115 (m_p_emitter) -- asm-pinned
                                       //        inside UnRegister @0x829146D0
};
