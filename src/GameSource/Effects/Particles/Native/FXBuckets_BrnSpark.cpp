// ============================================================================
// GameSource/Effects/Particles/Native/FXBuckets_BrnSpark.cpp
//
// Explicit instantiation of BrnParticle::FXBucket<BrnParticle::Native::BrnSpark, 4>.
//
// The generic Clear() / GetNewParticle() bodies are defined inline in FXBuckets.h and
// reconstructed store-for-store from this instantiation's X360 asm:
//   FXBucket<BrnSpark,4>::Clear          @ 0x822869C8
//   FXBucket<BrnSpark,4>::GetNewParticle @ 0x82285460  (the "Get" symbol in the dossier)
//
// With TParticleType = BrnSpark (sizeof 48) and N = 4:
//   KuTotalParticleSizeInBytes = 48 + 4 = 52          (DWARF FXBuckets.h:313)
//   KuMaxNumParticlesUnAligned = (8192 - 16) / 52 = 157 (DWARF FXBuckets.h:316)
//   KuMaxNumParticles          = 157 & ~1 = 156        (DWARF FXBuckets.h:320; == 0x9C)
//   maParticleBirthTimes @ +0x10, maParticleData @ +0x280, particle stride 48.
// These reproduce every displacement in the asm exactly.
//
// Called by (X360 xref): BrnParticle::Native::SparkArray::SparkBank::GetNewSpark
// (GetNewParticle) and the spark-bucket allocator (Clear).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"

namespace BrnParticle
{
    template struct FXBucket<BrnParticle::Native::BrnSpark, 4>;
}
