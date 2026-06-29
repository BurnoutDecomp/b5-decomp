#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include <new>  // placement new

// MSVC CRT: construct an array of objects with a per-element ctor function.
// Matches the X360 PPC bl to `vector constructor iterator' in the EffectsModule ctor.
extern "C" void __cdecl _vector_constructor_iterator_(
    void* pBase,
    unsigned int uElementSize,
    int nCount,
    void* (__cdecl* pCtor)(void*));

// sub_827DCA68 — per-slot initialiser for the 4 × 16-byte debris-cluster members
// at X360 byte +185160. Type and name unknown; reconstructed as opaque init call.
extern int sub_827DCA68(void* lpSelf, int a2, int a3);

// sub_827DCB10 — initialiser for the 16-byte cluster-header member at X360 +185272
// (sits between the debrisparams array and surfacelist). Type unknown.
extern int sub_827DCB10(void* lpSelf, int a2, int a3);

namespace CgsSceneManager { namespace CgsCollision {
    // BaseCollisionGenerator: ICF-folded default-init / Destruct body.
    // The X360 compiler folded the ctor and dtor to the same code path; the
    // resulting symbol is named Destruct. It is passed here as the per-element
    // ctor to _vector_constructor_iterator_ for the debris generator arrays.
    struct BaseCollisionGenerator
    {
        static void* __cdecl Destruct(void* lpSelf);
    };
} }

namespace BrnEffects {

// ---------------------------------------------------------------------------
// BrnEffects::EffectsModule::EffectsModule
//
// X360 ARTIST @ 0x827E35E0. Constructs the game's central VFX module, which
// is a CgsModule::ModuleSingleBuffered (handles vtable + two DataBuffer
// RWMutexes via the base ctor chain), followed by:
//
//   X360 +0x0A80  BrnParticle::ParticleModule  (placement new)
//   X360 +0x2C2E0 debris manager float params  (direct store)
//   X360 +0x2C330 8 debris effect generators, each 384 bytes:
//                   2× array of 4 BaseCollisionGenerator (32 B each), vtables
//   X360 +0x2D2B8 4× 16-byte debris-cluster slots  (sub_827DCA68)
//   X360 +0x2D2D8 3× Attrib::Gen::debrisparams     (placement new)
//   X360 +0x2D2F8 cluster header                   (sub_827DCB10)
//   X360 +0x2D308 Attrib::Gen::surfacelist          (placement new)
//
// All byte offsets are X360 (4-byte-pointer ABI).  PC offsets differ due to
// wider pointers; members are accessed only by offset here (no named members
// until the sub-type layouts land).
// ---------------------------------------------------------------------------
EffectsModule::EffectsModule()
{
    // Base class CgsModule::ModuleSingleBuffered ctor called by C++ ABI before
    // this body: sets vtable, constructs both DataBuffer::mMutex (EA::Thread::RWMutex)
    // at X360 byte +0x10 / +0x118.  The X360 pseudocode shows these as manual
    // vtable writes and explicit RWMutex calls because the base ctor was inlined.

    char* const lp = reinterpret_cast<char*>(this);

    // BrnParticle::ParticleModule at X360 byte +0xA80 (2688).
    new (lp + 0xA80) BrnParticle::ParticleModule();

    // Debris manager float / flag fields (X360 bytes +0x2C2E0 … +0x2C31C).
    // X360: *(a1 + 181040/4) = off_820CE870  — vtable of the debris manager sub-object;
    // the vtable word is at byte offset 181040 on X360. Its ctor is not yet recovered;
    // skip the vtable store (field will be overwritten when that ctor lands).
    *reinterpret_cast<u32*>(lp + 181140) = 0u;
    *reinterpret_cast<f32*>(lp + 181144) = 50.0f;
    *reinterpret_cast<f32*>(lp + 181148) = 0.1f;
    *reinterpret_cast<f32*>(lp + 181152) = 0.2f;
    *reinterpret_cast<f32*>(lp + 181156) = 0.2f;
    *reinterpret_cast<u32*>(lp + 181160) = 0u;
    *reinterpret_cast<u32*>(lp + 181164) = 0u;

    // 8 debris effect generators at X360 byte +0x2C330 (181040 + 192?).
    // Wait — X360: v3 = a1 + 181232 (= 0x2C350).
    // Each generator slot is 384 (0x180) bytes and contains:
    //   [+0x000, +0x080)  array of 4 BaseCollisionGenerator (32 B each)
    //   [+0x080, +0x100)  array of 4 BaseCollisionGenerator (32 B each)
    //   +0x100 (256)  vtable word (off_820CEBEC)
    //   +0x13C (316)  vtable word (off_820CEBFC)
    // The vtable stores at +256/+316 are set by the BaseCollisionGenerator::Destruct
    // ctor calls above (ICF-folded init path).
    // X360: loop v2 = 7 downto 0, v3 += 384 each iteration → 8 slots.
    char* lpGen = lp + 181232;
    for (int liI = 7; liI >= 0; --liI, lpGen += 384)
    {
        _vector_constructor_iterator_(lpGen,       32u, 4,
            CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct);
        _vector_constructor_iterator_(lpGen + 128, 32u, 4,
            CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct);
    }

    // 4 × 16-byte debris-cluster slot initialisers at X360 +0x2D2B8 (185160).
    // X360: loop v4 = 3 downto 0, v5 += 16 each iteration.
    char* lpCluster = lp + 185160;
    for (int liJ = 3; liJ >= 0; --liJ, lpCluster += 16)
        sub_827DCA68(lpCluster, 0, 0);

    // 3 Attrib::Gen::debrisparams at X360 +0x2D2D8, +0x2D2E8, +0x2D2F8
    // (185224, 185240, 185256 — 16-byte spacing from sizeof(debrisparams)).
    new (lp + 185224) Attrib::Gen::debrisparams();
    new (lp + 185240) Attrib::Gen::debrisparams();
    new (lp + 185256) Attrib::Gen::debrisparams();

    // Cluster header at X360 +0x2D308 (185272).
    sub_827DCB10(lp + 185272, 0, 0);

    // Attrib::Gen::surfacelist at X360 +0x2D318 (185288).
    new (lp + 185288) Attrib::Gen::surfacelist();
}

} // namespace BrnEffects
