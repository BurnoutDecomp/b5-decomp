#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFog.h"

CGS_STATIC_ASSERT((sizeof(cLionFog) == 0x814), "Native Lion fog size");

// ARTIST 0x83122E08 is zero-filled and is passed to the particle renderer.
cLionFog cLionFog::mSingleton;
