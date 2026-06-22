#pragma once

// cLionBindings -- binds a Lion particle effect instance to its locator/scaler/trigger
// inputs (SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h).
//
// Layout recovered from the X360 DWARF (LionBindings.h). DeInit (0x82908240) frees the
// owned mppLocators array through the effect manager's tagged allocator and clears the
// count/pointer.
//
// Vendor code (eauk_lion), reconstructed in its canonical Lion home. The particle input
// types (cParticleLocator/Scaler/Trigger/Emitter) and cParticleRandomSeed are owned by
// other Lion TUs; forward-declared / minimally modelled here as HONEST placeholders.

#include "types.hpp"

class cParticleLocator;
class cParticleScaler;
class cParticleTrigger;
class cParticleEmitter;

// cParticleRandomSeed -- value member of cLionBindings (one u32 seed). Owned by the
// particle subsystem; minimal placeholder so the binding layout is complete.
struct cParticleRandomSeed
{
    u32 muSeed;
};

struct cLionBindings
{
public:
    void Init();
    void DeInit();

    void SetLocatorCount(u32 aCount);

    cLionBindings* GetNextBinding() { return mpNext; }
    void           SetNextBinding(cLionBindings* apNext) { mpNext = apNext; }

private:
    u32                mLocatorCount;  // +0x00
    u32                mWorldIndex;    // +0x04
    cParticleLocator** mppLocators;    // +0x08 owned locator array
    cParticleLocator*  mpLocator;      // +0x0C
    cParticleScaler*   mpScaler;       // +0x10
    cParticleTrigger*  mpTrigger;      // +0x14
    cParticleRandomSeed mSeed;         // +0x18
    cLionBindings*     mpNext;         // +0x1C
    cParticleEmitter*  mpEmitter;      // +0x20
};
