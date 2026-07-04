#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/AttribSys/Generated/classes/debrisparams.h"
#include "GameSource/AttribSys/Generated/classes/surfacelist.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"     // CgsModule::BaseEventReceiverQueue / Event
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // CgsResource::Events::AcquireResourceResponse
#include "GameSource/Effects/Particles/BrnParticleDescription.h"         // BrnParticle::ParticleDescription::HashString
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
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

// The convoy slip-stream effect name (referenced only by inline string pointer in the X360
// build; the pointer is baked into HandleConvoySlipStream). De-inlined here as a TU-local
// constant -- its literal is the gamedb path the X360 HashString/StartLionEffect are handed.
static const char* const KAC_SLIPSTREAM_EFFECT =
    "gamedb://Instances/Effects/SlipStream/SlipStream";

// =============================================================================
// GetNextAcquireResourceResponse  @ 0x8227F098
//   Iterate the module's resource-acquire reply queue (mReceiverQueue, an
//   EventReceiverQueue<2048,16> embedded at X360 byte +0x244). With no previous
//   response, return the FIRST queued response's payload (or NULL when the queue
//   is empty); otherwise return the response that follows lpPrevious (NULL at the
//   end). PrepareResources drives this to walk the pool's acquire replies.
//
//   The X360 build inlined GetFirstEvent for the no-previous branch (it returns
//   mpBuffer + miStartOffset + 8 -- the first record's payload, past the 8-byte
//   [type][size] header -- exactly what the committed GetFirstEvent computes) and
//   called BaseEventReceiverQueue::GetNextEvent for the has-previous branch.
// =============================================================================
const CgsResource::Events::AcquireResourceResponse*
EffectsModule::GetNextAcquireResourceResponse(const CgsResource::Events::AcquireResourceResponse* lpPrevious)
{
    // mReceiverQueue -- EventReceiverQueue<2048,16> at X360 byte +0x244 (word 145).
    // Accessed by fixed X360 byte offset (this TU is a flat opaque body with every
    // sub-object placed at its console offset; see the ctor). Cast to the queue base
    // whose public accessors carry the iteration behaviour.
    CgsModule::BaseEventReceiverQueue* const lpQueue =
        reinterpret_cast<CgsModule::BaseEventReceiverQueue*>(
            reinterpret_cast<char*>(this) + 0x244);

    if (lpPrevious != NULL)
    {
        const CgsModule::Event* lpNext = NULL;
        s32 liSize = 0;
        lpQueue->GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpPrevious),
                              &lpNext, &liSize);
        return reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpNext);
    }

    if (lpQueue->GetCount() <= 0)
    {
        return NULL;
    }

    const CgsModule::Event* lpFirst = NULL;
    s32 liSize = 0;
    lpQueue->GetFirstEvent(&lpFirst, &liSize);
    return reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpFirst);
}

// =============================================================================
// HandleConvoySlipStream  @ 0x822926C8   (called by HandleGameActions)
//   Drive the convoy slip-stream LION effect: lazily start it the first time
//   (keying KAC_SLIPSTREAM_EFFECT through ParticleDescription::HashString), then
//   each call re-point its world transform, clamp the passed blend to <= 1.0,
//   flag the slot changed and store the blend.
//
//   The X360 build inlined ParticleModule::GetLionEffect (the index assert +
//   handle-match slot resolve) and the min(blend,1.0) as a scalar fsel. r4 (an
//   int/bool) is passed in the ABI but never read by this body.
//
//   The slip-stream handle lives at X360 byte +0x2F54C (muSlipStreamEffectHandle);
//   the embedded ParticleModule is at X360 byte +0xA80 (see the ctor). Accessed by
//   fixed console offset like the rest of this flat-opaque-body TU.
// =============================================================================
void EffectsModule::HandleConvoySlipStream(f32 lfBlend, u32 luUnused,
                                           const rw::math::vpu::Matrix44Affine& lrTransform)
{
    (void)luUnused;   // ABI param (r4); unused by this body.

    char* const lp = reinterpret_cast<char*>(this);
    BrnParticle::ParticleModule& lrParticleModule =
        *reinterpret_cast<BrnParticle::ParticleModule*>(lp + 0xA80);
    u32& lruSlipStreamEffectHandle = *reinterpret_cast<u32*>(lp + 0x2F54C);

    // Lazily start the slip-stream effect (name pre-hashed, world index 0).
    if (lruSlipStreamEffectHandle == BrnParticle::LionEffect::KU_HANDLE_INVALID)
    {
        const u32 luNameHash = BrnParticle::ParticleDescription::HashString(KAC_SLIPSTREAM_EFFECT);
        lruSlipStreamEffectHandle = lrParticleModule.StartLionEffect(luNameHash, KAC_SLIPSTREAM_EFFECT, 0);
    }

    // Resolve the playing-effect slot (GetLionEffect inlined in the X360 build:
    // asserts (handle & 0x7F) < KU_MAX_PLAYING_EFFECTS, then matches the stored
    // handle). It is asserted non-NULL here -- the slip-stream slot must be live.
    const u32 luHandle = lruSlipStreamEffectHandle;
    CGS_ASSERT((luHandle & 0x7Fu) < BrnParticle::ParticleModule::KU_MAX_PLAYING_EFFECTS,
               "luArrayIndex < KU_MAX_PLAYING_EFFECTS");
    BrnParticle::LionEffect* lpEffect = lrParticleModule.GetLionEffect(luHandle);
    CGS_ASSERT(lpEffect != NULL,
               "Lion Effect is NULL when it shouldn't be (SlipStream Effect)");

    // Re-point the effect's world transform (64-byte affine copy).
    lpEffect->mTransform = lrTransform;

    // Clamp the blend to 1.0 (asm: fsel(blend - 1.0, 1.0, blend) == min(blend,1.0)).
    const f32 lfClampedBlend = (lfBlend >= 1.0f) ? 1.0f : lfBlend;

    lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
    lpEffect->mfStateBlend = lfClampedBlend;
}

} // namespace BrnEffects
