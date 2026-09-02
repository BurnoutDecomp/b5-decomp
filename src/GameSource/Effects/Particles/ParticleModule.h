#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; vtable + the two RWMutexes the ctor constructs inline via its DataBuffers)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h" // CgsModule::EventReceiverQueue<16384,16> (mReceiverQueue)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::SafeResourceHandle<T>
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random (mRandom, BY VALUE)
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h" // EA::Allocator::ITaggedAllocator (IInternalAllocator's base)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::Camera (ParticleRenderData::mCgsCamera, BY VALUE)
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"         // BrnParticle::TextureNameMap (mTextureNameMap handle target)
#include "GameSource/Effects/Particles/LionParticleRender.h"           // BrnParticle::LionParticleRender (mLionRenderer, BY VALUE)
#include "GameSource/Effects/Particles/EffectsVertexBufferManager.h"   // EffectsVertexBufferManager (x3, BY VALUE)
#include "GameSource/Effects/Particles/Native/FXBuckets.h"             // BrnParticle::FXBucketManager (mBucketManager, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h"  // BrnGraphics::Im3dSkidsRenderer (mSkidsRenderer, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"        // BrnParticle::Native::TrailSystem (mTrailSystem, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnDebrisRenderer.h"     // BrnParticle::Native::BrnDebrisRenderer (mDebrisRenderer, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"        // BrnParticle::Native::BrnDebrisArray (maDebris[5], BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleRenderer.h" // BrnParticle::Native::BrnSimpleParticleRenderer (BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"    // BrnParticle::Native::BrnSimpleParticleArray (maSimpleParticles[13], BY VALUE)

namespace CgsMemory { class HeapMalloc; }   // GameShared/GameClasses/Memory/CgsHeapMalloc.h (fwd; avoids a cross-module include cycle)
namespace renderengine { class Texture; }   // ParticleRenderData::mpEnvironmentMap (pointer-only)
namespace BrnResource { namespace GameDataIO { class AllocatorList; } }   // Prepare's allocator list (pointer-only)
namespace BrnGame { struct DispatchThreadInputBuffer; }                  // GenerateRenderRequests / PreRenderUpdate target (pointer-only)
namespace BrnDirector { namespace Camera { class Camera; } }              // Update's camera (pointer-only)
class cTime;                                                              // Lion SDK time (mpLionCurrentTime, pointer-only)
namespace BrnParticle { namespace ParticleIO { struct PrepareOutputBuffer; } }   // ParticleModuleIO.h (the prepare payload; pointer-only here)

// ============================================================================
// GameSource/Effects/Particles/ParticleModule.h
//
// BrnParticle::ParticleModule and its per-playing-effect record BrnParticle::
// LionEffect. Shape recovered from the DecFIGS DWARF
// (GameSource/Effects/Particles/ParticleModule.h) and gated on the ARTIST ledger.
//
//   * LionEffect - a playing effect slot. Verified offsets from the ARTIST asm
//     (GetLionEffect 0x82278380 + the BrnEffectsDebugComponent junkyard funcs):
//       +0x00 muHandle, +0x10 mTransform (the affine drawn by DrawAxis; its
//       translation row is read at the +0x30/+0x34/+0x38 X/Y/Z), +0x64 muFlags.
//     sizeof == 0x70 (the array stride the asm uses: 112 * (handle & 0x7F)).
//
//   * ParticleModule - a full CgsModule::ModuleSingleBuffered (its first two
//     stored words in the X360 ctor @0x827E2218 are the base ModuleSingleBuffered
//     vtable off_820CE500 then the derived ParticleModule vtable off_820D0400, and
//     it constructs the two EA::Thread::RWMutex members the base owns at +0x10 /
//     +0x118 == exactly the ModuleSingleBuffered base sub-object construction).
//
// LAYOUT MODEL (same discipline as BrnPhysics::PhysicsModule). Members are the DWARF's
// (ParticleModule.h:4-175), in the DWARF's order, typed with the committed type wherever
// one exists; the console byte offset each one was pinned at is given per member. The
// sub-objects that have NO reconstructed type yet (the Im3d family bar the skids
// renderer, the spark renderer + arrays, the Lion batch array, the EA::Jobs::Job jobs
// and their data, the two spawn-buffer headers) stay as correctly-named, asm-sized
// OPAQUE PLACEHOLDERS so that every ctor-touched offset lands where the X360 asm proves
// it. Each placeholder is FLAGGED; they fold into real members as those passes land.
// Console offsets are the 4-byte-pointer ABI; member access is by name, so the bodies
// are faithful regardless of host pointer width.
//
// 2026-09-02 (tyre-mark wave): the head of the object (the four stage / count words,
// the heap, the 16 KB receiver queue and the two resource handles that LoadFXBundle
// @0x8229C950 drives), the Lion renderer, the playing-effect bookkeeping, the render
// data record, the random generator, the enable bytes, the bucket manager, the three
// vertex-buffer managers, the debris / simple-particle families are typed by name.
// ============================================================================

namespace BrnParticle
{
    struct ParticleDescriptionCollection;   // SharedClasses/Graphics/ParticleDescriptionResourceType.h (handle target; pointer-only here)

    // A single playing LION (particle) effect slot. DWARF home ParticleModule.h:87.
    struct LionEffect
    {
        // Handle layout / per-effect flags (DWARF ParticleModule.h:90-100,184-187).
        static const u32 KU_HANDLE_INVALID    = 0xFFFFFFFFu;
        static const u32 KU_HANDLE_INDEX_MASK = 127u;   // handle & this -> slot index
        static const u32 KU_HANDLE_INCREMENT  = 128u;   // DWARF :99 -- the generation step StopLionEffect adds
        static const u32 KU_HANDLE_VALID_MASK = 0x7FFFFFFFu; // DWARF :100

        // muFlags bits (DWARF "ePPEFlag*").
        static const u16 EPPE_FLAG_IN_USE           = 1;
        static const u16 EPPE_FLAG_ENABLED          = 2;
        static const u16 EPPE_FLAG_CHANGED          = 4;
        static const u16 EPPE_FLAG_CREATE           = 8;
        static const u16 EPPE_FLAG_KILL             = 16;
        static const u16 EPPE_FLAG_OVERRIDE_VELOCITY= 32;

        const rw::math::vpu::Matrix44Affine& GetTransform() const { return mTransform; }

        // X360 0x82289908 (called per slot by ParticleModule::Construct @0x82294220 and by
        // StopLionEffect @0x8228A238 for a CREATE-flagged slot). Store-for-store: the four
        // identity rows into mTransform (+0x10..+0x4F), then +0x50/+0x54/+0x58 = 0.0 (the
        // velocity), +0x5C = 0 (muWorldIndex), +0x0C = 0.0 (mfStateBlend), +0x64 = 0
        // (muFlags), +0x04/+0x08 = 0 (the name hash / definition slots). muHandle (+0x00)
        // is NOT touched -- the caller stamps it.
        void Construct()
        {
            mTransform.SetIdentity();
            mPad50[0] = mPad50[1] = mPad50[2] = mPad50[3] = 0;     // +0x50 velocity.x
            mPad50[4] = mPad50[5] = mPad50[6] = mPad50[7] = 0;     // +0x54 velocity.y
            mPad50[8] = mPad50[9] = mPad50[10] = mPad50[11] = 0;   // +0x58 velocity.z
            muWorldIndex = 0;
            mfStateBlend = 0.0f;
            muFlags      = 0;
            mPad04[0] = mPad04[1] = mPad04[2] = mPad04[3] = 0;     // +0x04
            mPad04[4] = mPad04[5] = mPad04[6] = mPad04[7] = 0;     // +0x08
        }

        // Set the effect's world transform and mark it CHANGED so the render pass re-reads it
        // (X360 UpdateVehicleEffectPositions/FireGlassEffect inline: store 4 rows to
        // mTransform@+0x10 then `muFlags |= EPPE_FLAG_CHANGED`).
        void SetTransform(const rw::math::vpu::Matrix44Affine& lrTransform)
        {
            mTransform = lrTransform;
            muFlags   |= EPPE_FLAG_CHANGED;   // 4
        }

        u32 muHandle;                              // +0x00 - the handle this slot holds
        u8  mPad04[0x08];                          // +0x04 - hashed name / definition ptr
        f32 mfStateBlend;                          // +0x0C - state blend factor (BoostStateMachine
                                                   //         SetBlendValue stores here; +0x53FC)
        rw::math::vpu::Matrix44Affine mTransform;  // +0x10 - the effect's world transform
        u8  mPad50[0x0C];                          // +0x50 - velocity / death time
        u32 muWorldIndex;                          // +0x5C - world index (SetWorldIndex stores
                                                   //         here; slot +0x5C / module +0x544C)
        u8  mPad60[0x04];                          // +0x60
        u16 muFlags;                               // +0x64 - ePPEFlag* bitmask
        u8  mPad66[0x0A];                          // +0x66 - pad to the 0x70 array stride
    };

    // BrnParticle::IInternalAllocator -- ParticleModule's private EA::Allocator::
    // ITaggedAllocator front-end onto a CgsMemory::HeapMalloc (DWARF home
    // ParticleModule.cpp:108). The Lion runtime (cLionBlockAlloc / LionSmallAlloc)
    // allocates through an EA::Allocator::ITaggedAllocator*; this class is the
    // concrete adapter ParticleModule hands it, forwarding onto mpHeapMalloc.
    //
    // TWO-VTABLE DTOR NOTE (why the deleting dtor stores at both this+0 AND
    // this+4). IInternalAllocator's own scalar deleting destructor (X360
    // 0x822898A0) is NOT the single-store pattern every other single-inheritance
    // allocator front-end in this codebase uses (compare CgsGraphics::
    // MoviePlayerCoreAllocator::`vector deleting destructor' @0x827DBAC0 and
    // EA::Allocator::IAllocator::~IAllocator @0x82277E80, each of which stores
    // exactly one vtable pointer at this+0). Here the asm stores TWO vtable
    // pointers:
    //   *(this+4) = off_8200FDB4   -- the EA::Allocator::IAllocator vtable
    //                                  (the SAME constant IAllocator's own
    //                                  destructor stores into *its* this+0 --
    //                                  see SDKs/.../iallocator.cpp)
    //   *(this+0) = off_8200F5B4   -- IInternalAllocator's own (derived) vtable
    // and both the vector-deleting-destructor and the Free thunk are
    // ADJUSTOR{4} thunks (`addi r3,r3,-4` before tail-calling the this-relative-
    // to-primary body). An adjustor thunk with a non-zero `this` delta is the
    // PPC/Itanium-ABI signature of a call arriving through a SECONDARY base
    // subobject: the compiler lays IAllocator's vtable slot down as its own
    // (non-primary) sub-object at +0x4 inside IInternalAllocator, distinct from
    // IInternalAllocator's own primary vtable at +0x0, and thunks calls that
    // arrive via the IAllocator-shaped secondary pointer back to the primary
    // `this` before dispatching. The DWARF's single `: public ITaggedAllocator`
    // line names only the primary base (matching every X360-ledger-attested
    // method here, which all resolve through the primary vtable at +0x0); no
    // second C++-source base is added, since the X360 ledger attests no method
    // reached only through the secondary IAllocator pointer -- the secondary
    // slot is a compiler-emitted ABI artifact of the ITaggedAllocator/IAllocator
    // pure-interface chain, not a distinct user-written base to model.
    class IInternalAllocator : public EA::Allocator::ITaggedAllocator
    {
    public:
        IInternalAllocator(CgsMemory::HeapMalloc* lpHeapMalloc);

        // Out-of-line destructor: anchors the vtable. See the two-vtable-dtor
        // note above -- the X360 deleting-destructor thunk block (scalar dtor
        // @0x822898A0, its vector-deleting-destructor adjustor{4} thunk
        // @0x82289890, and the Free adjustor{4} thunk @0x82289898) is entirely
        // compiler-generated from this virtual ~dtor; no owned resources are
        // released here (mpHeapMalloc is owned elsewhere).
        virtual ~IInternalAllocator();

        // ITaggedAllocator overrides. DECLARE-ONLY: not attested individually in
        // the X360 ledger (only Free and the dtor thunks are function-tracked
        // for this TU) -- bodying them would fabricate behavior. HeapMalloc-
        // backed allocation only exposes a 2-arg Malloc(size, alignment), so a
        // faithful Alloc body cannot be written without guessing the tag/name/
        // flags -> alignment mapping.
        virtual void* Alloc(size_t anSize, const char* apName, u32 auFlags) override;
        virtual void* Alloc(size_t anSize, const char* apName, u32 auFlags,
                            u32 auAlign, u32 auAlignOffset) override;
        virtual void* Alloc(size_t anSize, const EA::TagValuePair& arTags) override;

        // X360 0x822898A0 (adjustor{4} thunk @0x82289898 tail-calls this).
        virtual void  Free(void* apData, size_t anSize) override;

        // AddRef/Release: DECLARE-ONLY (not X360-ledger-attested for this TU).
        virtual s32   AddRef() override;
        virtual s32   Release() override;

    private:
        CgsMemory::HeapMalloc* mpHeapMalloc;   // ParticleModule.cpp:155 -- the backing heap
    };

    // The particle / LION effects module -- a full CgsModule::ModuleSingleBuffered.
    struct ParticleModule : public CgsModule::ModuleSingleBuffered
    {
        // GetLionEffect asserts (handle & 0x7F) < this before indexing maPlayingEffects.
        // X360 immediate 0x80 (GetLionEffect 0x82278380 asm: cmplwi r30,0x80).
        static const u32 KU_MAX_PLAYING_EFFECTS = 128;
        static const u32 KU_NUM_DEBRIS_ARRAYS   = 5;    // DWARF maDebris[5]
        static const u32 KU_NUM_SIMPLE_ARRAYS   = 13;   // DWARF maSimpleParticles[13]
        static const u32 KU_NUM_SPARK_ARRAYS    = 4;    // DWARF maSparks[4] (eSparkArray_Max)

        // DWARF ParticleModule.h:4-13 -- the four stage / count words the lifecycle drives.
        enum EPrepareStage      { E_PREPARESTAGE_START = 0, E_PREPARESTAGE_MANAGER = 1, E_PREPARESTAGE_LOADING = 2, E_PREPARESTAGE_DONE = 3 };
        enum EReleaseStage      { E_RELEASESTAGE_START = 0, E_RELEASESTAGE_MANAGER = 1, E_RELEASESTAGE_DONE = 2 };
        // LoadFXBundle @0x8229C950's stage ladder (the switch's case labels; the DWARF names
        // the enum, the X360 body is the authority for the values and the order):
        //   0/1 load the bundle -> 2 wait -> 13 acquire vfx_props_collection -> 14 wait ->
        //   3 acquire texture_name_map -> 4 wait -> 9 acquire particle_description_collection ->
        //   10 wait -> 5 acquire the 5 mesh collections -> 6 wait -> 7 acquire their textures ->
        //   8 wait -> 11 acquire every name-map texture -> 12 wait + bind -> 17 load the prop
        //   VFX collisions -> 18 wait + bind -> 19 done.
        enum EInitialLoadStage
        {
            E_LOADSTAGE_START                     = 0,
            E_LOADSTAGE_LOAD_BUNDLE               = 1,
            E_LOADSTAGE_WAIT_BUNDLE               = 2,
            E_LOADSTAGE_ACQUIRE_TEXTURE_NAME_MAP  = 3,
            E_LOADSTAGE_WAIT_TEXTURE_NAME_MAP     = 4,
            E_LOADSTAGE_ACQUIRE_MESH_COLLECTIONS  = 5,
            E_LOADSTAGE_WAIT_MESH_COLLECTIONS     = 6,
            E_LOADSTAGE_ACQUIRE_MESH_TEXTURES     = 7,
            E_LOADSTAGE_WAIT_MESH_TEXTURES        = 8,
            E_LOADSTAGE_ACQUIRE_DESCRIPTIONS      = 9,
            E_LOADSTAGE_WAIT_DESCRIPTIONS         = 10,
            E_LOADSTAGE_ACQUIRE_TEXTURES          = 11,
            E_LOADSTAGE_WAIT_TEXTURES             = 12,
            E_LOADSTAGE_ACQUIRE_VFX_PROPS         = 13,
            E_LOADSTAGE_WAIT_VFX_PROPS            = 14,
            E_LOADSTAGE_LOAD_PROP_COLLISIONS      = 17,
            E_LOADSTAGE_WAIT_PROP_COLLISIONS      = 18,
            E_LOADSTAGE_DONE                      = 19,
        };

        // ==============================================================================
        // THE TWO IO PAYLOADS THIS MODULE EXCHANGES WITH THE DISPATCH THREAD.
        //
        // They live HERE because the DWARF nests both inside this struct --
        // references/DecFIGS/dwarfdump/GameSource/Effects/Particles/ParticleModule.h,
        // `struct DispatchThreadUpdateData` at ParticleModule.h:565 and
        // `struct ParticleRenderData` at :576, both inside `struct ParticleModule` -- and
        // because BrnGame::DispatchThreadInputBuffer's DWARF spells its two members with the
        // fully-qualified nested names `BrnParticle::ParticleModule::DispatchThreadUpdateData`
        // and `...::ParticleRenderData`.
        // ==============================================================================

        // DWARF ParticleModule.h:565. FLAG (ad-hoc opaque, not recovered): the DWARF gives it
        // mfCurrentTime / mfCurrentTimeStep / muChangedEffects / Matrix44Affine mViewMatrix /
        // Matrix44 mProjectionMatrix / LionEffect[128] maChangedLionEffects (:566-571), which
        // sizes to 0x90 + 128 * 0x70 == 0x3890 -- EXACTLY the console's gap between
        // DispatchThreadInputBuffer's mParticleData (+0x10) and mParticleRenderData (+0x38A0).
        // Recovering it is the Lion dispatch pass's job; the 1-byte placeholder is kept so
        // nothing about the enclosing buffer changes by accident.
        struct DispatchThreadUpdateData
        {
            u8 maStorage[1];   // FLAG: opaque, true size is 0x3890 -- see above
        };

        // DWARF ParticleModule.h:576-606 -- the render-side snapshot this module publishes
        // once a frame for the dispatch/render thread.
        //
        // OFFSET ATTESTATION: BrnRendererModule::Render @0x8240BFA8 reads exactly three of
        // these off the read-locked buffer (asm 0x8240DD04-0x8240DD0C): `lfs f1, 0xC(r24)` ==
        // mfCurrentTimeStep, `addi r4, r24, 0x60` == the camera VIEW matrix and
        // `addi r5, r24, 0xA0` == the camera PROJECTION matrix, handed to
        // MotionBlurState::Update. All three land on the layout below without a single
        // adjustment -- +0x0C is the fourth 4-byte scalar, +0x60 is where mCgsCamera starts
        // once mCameraTransform is 16-aligned at +0x20, and +0xA0 is mCgsCamera's own
        // m_projectionMatrix at its +0x40 (CgsCamera.h pins that offset with a static_assert).
        //
        // FLAG (guest tail): the console places the NEXT member of the enclosing buffer
        // (mBufferCrashTriangleCache) at +0x3B00 while this layout ends at +0x210 from
        // +0x38A0 == +0x3AB0. The 0x50-byte gap is NOT accounted for; recorded, not rounded.
        struct ParticleRenderData
        {
            // DWARF ParticleModule.h:579-586 -- the muFlags bits.
            static const u16 eRenderDataFlagCameraSwitched   = 1;
            static const u16 eRenderDataFlagRenderSparks     = 2;
            static const u16 eRenderDataFlagRenderDebris     = 4;
            static const u16 eRenderDataFlagRenderSimple     = 8;
            static const u16 eRenderDataFlagRenderLion       = 16;
            static const u16 eRenderDataFlagRenderTrails     = 32;
            static const u16 eRenderDataFlagReducedFrameRate = 64;
            static const u16 eRenderDataFlagInSlowMotion     = 128;

            ParticleModule*               mpParticleModule;      // DWARF :589 (guest +0x00)
            u32                           muCurrentFrame;        // DWARF :592 (guest +0x04)
            f32                           mfCurrentTime;         // DWARF :593 (guest +0x08)
            f32                           mfCurrentTimeStep;     // DWARF :594 (guest +0x0C) <- Update's f1
            f32                           mfTimeStepMultiplier;  // DWARF :595 (guest +0x10)
            rw::math::vpu::Matrix44Affine mCameraTransform;      // DWARF :597 (guest +0x20)
            CgsGraphics::Camera           mCgsCamera;            // DWARF :598 (guest +0x60;
                                                                 //   its mView @+0x60, mProjection @+0xA0)
            rw::math::vpu::Vector3        mvSunDirection;        // DWARF :601 (guest +0x1D0)
            rw::math::vpu::Vector3        mvSunColour;           // DWARF :602 (guest +0x1E0)
            rw::math::vpu::Vector3        mvAmbientColour;       // DWARF :603 (guest +0x1F0)
            u16                           muFlags;               // DWARF :604 (guest +0x200)
            const renderengine::Texture*  mpEnvironmentMap;      // DWARF :605 (guest +0x204)
            f32                           mfWhiteLevel;          // DWARF :606 (guest +0x208)
        };

        ParticleModule();

        // ---- the module lifecycle (ParticleModule_Lifecycle.cpp) ----------------------------
        // X360 0x82294220 -- CgsModule::Module::Construct override (vtable slot 0 on the console).
        void Construct() override;
        // DWARF :425 `bool Prepare(const AllocatorList*)` -- X360 0x8229BEA0. The heap / RW
        // allocators, the Im3d renderers, the Lion renderer, the buckets, the trail system,
        // the debris / simple-particle arrays. Returns false while still preparing.
        bool Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);
        // X360 0x8229E5D0 -- the second prepare pass: drive LoadFXBundle until the bundle is bound.
        bool PostPreparePrepare(ParticleIO::PrepareOutputBuffer* lpOutput);
        // X360 0x8229C950 -- the 19-stage FX-bundle load ladder over lpOutput's request queue.
        bool LoadFXBundle(ParticleIO::PrepareOutputBuffer* lpOutput);
        // DWARF :434 `virtual void Update(float32_t, float32_t, float32_t, const Camera*)` --
        // X360 0x822817D8 (the render-data record + muFlags rebuild, once per sim sub-step).
        void Update(f32 lfTimeStep, f32 lfTime, f32 lfTimeStepMultiplier, const BrnDirector::Camera::Camera* lpCamera);
        // X360 0x82294C30 -- the frame's end: latch mbStalled, then the trail system's buffer flip.
        void EndOfFrame(bool lbStalled);
        // X360 0x8228AC20 -- render thread: the trail system's per-frame time + view-projection
        // (+ the Lion vertex buffers, carved out on PC -- see the .cpp).
        void BuildLionVertexBuffers(const ParticleRenderData* lpRenderData);
        // X360 0x8229AFD0 -- render thread: the trail strips (RenderTrails), debris, sparks, Lion.
        void RenderFullResParticles(const ParticleRenderData* lpRenderData);

        // X360 0x82278380. Resolve a handle to its playing-effect slot, or NULL when the
        // slot has been recycled (its stored handle no longer equals luHandle).
        LionEffect* GetLionEffect(u32 luHandle);

        // X360 0x822867E0. Start the named LION effect (the caller precomputes the name
        // hash via ParticleDescription::HashString) at the given world index, returning the
        // new playing-effect handle. Own-TU body; declared here for the boost/jump machines.
        u32 StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex);

        // X360 0x8228A238. Stop a playing LION effect, given its resolved slot pointer
        // (asserts the slot is non-NULL and in use). Body in ParticleModule.cpp.
        void StopLionEffect(LionEffect* lpEffect);

        // X360 0x8227EAC8. Reset both spark-frame data sets (Prepare / EffectsModule::Update
        // drive this). DECLARE-ONLY: its body depends on BrnParticle::Native::SparkFrameDataSet
        // (no committed layout) and three un-recovered rodata constant vectors; see the .cpp.
        void ResetSparkFrameData();

        // The skid / tyre-mark system and its renderer, by name (HandleWheels / the renderer).
        Native::TrailSystem&            TrailSystem()            { return mTrailSystem; }
        BrnGraphics::Im3dSkidsRenderer& SkidsRenderer()          { return mSkidsRenderer; }
        const ParticleRenderData&       RenderData() const       { return mRenderData; }
        bool                            IsSuspended() const      { return mbPlayingEffectsSuspended; }
        bool                            IsFXBundleLoaded() const { return meInitialLoadStage == E_LOADSTAGE_DONE; }

    public:
        // ===================================================================
        // Constructor-faithful layout. Members are named where the X360 ctor /
        // Construct / Prepare / GetLionEffect asm touch them; the un-touched gaps are
        // asm-sized opaque placeholders. Offsets in comments are absolute from `this`
        // on the X360 4-byte-pointer ABI (the base ModuleSingleBuffered occupies the head).
        // ===================================================================

        // ---- the head: DWARF :4-25 (Construct @0x82294220 / Prepare @0x8229BEA0 / LoadFXBundle) ----
        EPrepareStage                                          mePrepareStage;         // +0x228 (552)   Construct: 0
        EReleaseStage                                          meReleaseStage;         // +0x22C (556)   Construct: 2
        EInitialLoadStage                                      meInitialLoadStage;     // +0x230 (560)   Construct: 0
        s32                                                    miResourceCount;        // +0x234 (564)   the pending-request count LoadFXBundle waits on
        CgsMemory::HeapMalloc*                                 mpHeapMalloc;           // +0x238 (568)   Prepare: GetHeapAllocator(0x29)
        CgsModule::EventReceiverQueue<16384, 16>               mReceiverQueue;         // +0x23C (572)   Construct: buffer @+596, 0x4000, align 16
        CgsResource::SafeResourceHandle<ParticleDescriptionCollection> mDescriptionCollection; // +0x4254 (16980)
        CgsResource::SafeResourceHandle<TextureNameMap>        mTextureNameMap;        // +0x425C (16988)
        u8                                                     maPadHeadTo4270[0x4270 - 0x4264]; // -> +0x4270 (decorative on the host)

        // +0x4270: DWARF :28 BrnEffects::PropCollisions mPropCollisions. FLAG: PropCollisions
        // has no reconstructed type (GameSource/Effects/Props/PropCollisions.h declares only
        // VFXRuntimeMaterialLef); what the ctor stamps here is its intrusive list head (three
        // ints, then next/prev/iter -> self, then a count) and a 500-entry pair table, kept
        // as the named sub-objects the ctor writes.
        struct ContainedListInterface
        {
            s32   miListHead0;   // +0x00 (+0x4270)
            s32   miListHead1;   // +0x04 (+0x4274)
            s32   miListHead2;   // +0x08 (+0x4278)
            void* mpListNext;    // +0x0C (+0x427C): &miListHead0 (self)
            void* mpListPrev;    // +0x10 (+0x4280): &miListHead0 (self)
            void* mpListIter;    // +0x14 (+0x4284): &miListHead0 (self)
            s32   miListCount;   // +0x18 (+0x4288): 0
        };
        ContainedListInterface mList;                 // +0x4270 (PropCollisions head)

        // +0x4298: a 500-entry table the ctor zeroes pair-by-pair (v3 = +0x4298,
        // 500 iterations writing two zero dwords each: the asm's `li r9,0x1F3`
        // counter == 499 with a `>= 0` do/while == 500 entries x 8 bytes).
        struct EffectPair { u32 mu0; u32 mu1; };      // +0x00 / +0x04 (both zeroed)
        static const u32 KU_NUM_EFFECT_PAIRS = 500;
        EffectPair maEffectPairs[KU_NUM_EFFECT_PAIRS]; // +0x4298 .. +0x5258 (PropCollisions body)

        // Gap to the embedded LionParticleRender at +0x5270.
        u8 maPad5258To5270[0x5270 - (0x4298 + KU_NUM_EFFECT_PAIRS * sizeof(EffectPair))]; // -> +0x5270

        // +0x5270 (21104): DWARF :31 the embedded LION renderer -- the real committed type
        // (LionParticleRender.h; its X360 ctor is chained by this module's ctor, Prepare
        // stores its heap (+0x08) and its renderer (+0x160), Setup builds its state library,
        // LoadFXBundle hands it every acquired texture through AcquireTexture).
        LionParticleRender mLionRenderer;             // +0x5270 .. +0x53E0
        // +0x53E0 (21472): DWARF :34 -- Construct stores &dword_82FAD274, the Lion runtime's
        // process-wide current-time cell. FLAG: the cell is the Lion core's static (not landed);
        // left null until cLionFX lands.
        cTime* mpLionCurrentTime;                     // +0x53E0
        u8     maPad53E4To53F0[0x53F0 - 0x53E4];      // -> +0x53F0

        // +0x53F0: DWARF :37 maUpdateThreadLionEffects -- the playing-effect slot array
        // (GetLionEffect base, stride 0x70). Kept under the name every committed reader uses.
        LionEffect maPlayingEffects[KU_MAX_PLAYING_EFFECTS]; // +0x53F0 .. +0x8BF0

        u32   muUpdateThreadNextLionEffect;           // +0x8BF0 (35824)  DWARF :40  Construct: 0
        // DWARF :43 cLionEffectInstance*[128] -- the dispatch-thread twins. FLAG: the Lion
        // instance type is not reconstructed; carried as untyped host pointers (Construct zeroes them).
        void* mapDispatchThreadLionEffects[KU_MAX_PLAYING_EFFECTS]; // +0x8BF4 (35828)
        bool  mbPlayingEffectsSuspended;              // +0x8DF4 (36340)  DWARF :46  Update/PreRender/BuildLion gate
        bool  mbStalled;                              // +0x8DF5 (36341)  DWARF :49  EndOfFrame latches it
        u8    maPad8DF6To8E00[0x8E00 - 0x8DF6];       // -> +0x8E00 (16-aligned)

        // +0x8E00 (36352): DWARF :52 the module-side render-data record Update refreshes and
        // GenerateRenderRequests copies into the dispatch-thread input buffer (528 bytes on
        // the console). Construct: mpParticleModule = this, muCurrentFrame = 0,
        // mfCurrentTimeStep = 0.0.
        ParticleRenderData mRenderData;               // +0x8E00 .. +0x9010

        // The five contained Im3d renderers the ctor stamps (each: its vtable then two zero
        // words == the ImRenderer<V> base's own construction). Only the skids renderer has a
        // reconstructed type; the other four stay named placeholders. FLAG.
        struct ContainedInterface
        {
            void* mpVTable;   // +0x00 : the X360 vtable symbol (left null -- FLAGGED)
            u32   mu04;       // +0x04 : 0
            u32   mu08;       // +0x08 : 0
        };
        ContainedInterface mImmediateModeRenderer;     // +0x9010 (36880) DWARF :55 CgsGraphics::Im3d          (off_820CF69C)
        u8 maPadIfaceAToB[0x91A0 - (0x9010 + sizeof(ContainedInterface))]; // -> +0x91A0
        ContainedInterface mWorldTexRenderer;          // +0x91A0 (37280) DWARF :58 BrnGraphics::Im3dTexPlusLighting (off_820CEBE0)
        u8 maPadIfaceBToC[0x9210 - (0x91A0 + sizeof(ContainedInterface))]; // -> +0x9210
        // +0x9210 (37392): DWARF :61 BrnGraphics::Im3dSkidsRenderer mSkidsRenderer -- the skid /
        // tyre-mark immediate-mode renderer, 100 bytes on the console, constructed by
        // ParticleModule::Prepare @0x8229BEA0 and handed to the trail system's renderer.
        BrnGraphics::Im3dSkidsRenderer mSkidsRenderer; // +0x9210 .. +0x9274
        ContainedInterface mSmokeRenderer;             // +0x9274 (37492) DWARF :64 BrnGraphics::Im3dSmokeRenderer (off_820CEBE8)
        u8 maPadIfaceDToE[0x92E0 - (0x9274 + sizeof(ContainedInterface))]; // -> +0x92E0
        ContainedInterface mLionImmediateModeRenderer; // +0x92E0 (37600) DWARF :67 LionBlendRenderer (Im3dBlend base, off_820CFA1C)

        // Gap to the trail system at +0x9710: DWARF :70 SparkRenderer mSparkRenderer (+0x94C0)
        // and :73 SparkArray maSparks[4] -- not typed. FLAG: PLACEHOLDER.
        u8 maPadIfaceETo9710[0x9710 - (0x92E0 + sizeof(ContainedInterface))]; // -> +0x9710

        // +0x9710 (38672): DWARF :76 BrnParticle::Native::TrailSystem mTrailSystem, 102652
        // bytes on the console (.. +0x2280C). The 0x7FFFFFFF the ctor stamps at +0x22190
        // (139664 == 38672 + 100608 + 384) is this object's mFreeEmitters Stack length --
        // the inlined Stack<TrailEmitter*,96> constructor, reproduced by TrailSystem's own
        // ctor. RenderFullResParticles @0x8229AFD0 (`TrailSystem::Render(this + 38672)`),
        // ParticleModule::Prepare @0x8229BEA0 and EffectsModule::HandleWheels @0x82296C80
        // (`this + 0xA80 + 0x9710`) all address it here.
        Native::TrailSystem mTrailSystem;              // +0x9710 .. +0x2280C

        // DWARF :79-88 -- the debris / simple-particle families (Prepare constructs them).
        Native::BrnDebrisRenderer          mDebrisRenderer;                          // +0x22810 (141328)
        Native::BrnDebrisArray             maDebris[KU_NUM_DEBRIS_ARRAYS];           // +0x22818 (141336), stride 32
        Native::BrnSimpleParticleRenderer  mSimpleParticleRenderer;                  // +0x228B8 (141496)
        // FLAG: the committed BrnSimpleParticleArray is a partial layout (its X360 stride is 160;
        // Prepare's per-array spawn-time seeding needs its full shape and is carved out).
        Native::BrnSimpleParticleArray     maSimpleParticles[KU_NUM_SIMPLE_ARRAYS];  // +0x228D0 (141520), stride 160

        // DWARF :91-121 -- the frame-rate scale, the generator, the enables, the spark accumulator.
        f32                mfSimulationRate;             // +0x230F0 (143600)  Construct: 1.0
        u8                 maPad230F4To23100[0x23100 - 0x230F4];
        CgsNumeric::Random mRandom;                      // +0x23100 (143616)  Construct: the inlined LCG priming
        bool               mbSparksEnabled;              // +0x23130 (143664)  Construct: 1
        bool               mbTrailsEnabled;              // +0x23131 (143665)  Construct: 1
        bool               mbDebrisEnabled;              // +0x23132 (143666)  Construct: 1
        bool               mbSimpleEnabled;              // +0x23133 (143667)  Construct: 1
        bool               mbLionEnabled;                // +0x23134 (143668)  Construct: 1
        bool               mbZFadeEnabled;               // +0x23135 (143669)  Construct: 0
        bool               mbIsInJunkyard;               // +0x23136 (143670)  Construct: 0
        bool               mbHasCameraSwitched;          // +0x23137 (143671)  Construct: 1 (the camera-switched latch, seeded SET)
        f32                mrSparkAccumulator;           // +0x23138 (143672)  Construct: 0.9999
        // DWARF :124-133 -- the bucket manager and the three vertex-buffer managers.
        FXBucketManager            mBucketManager;                 // +0x2313C (143676)  Prepare: Construct(heap, 819200)
        EffectsVertexBufferManager mVertexBufferManagerLion;       // +0x2315C (143692)  Prepare: Construct(rw, 196608, 1)
        EffectsVertexBufferManager mVertexBufferManagerSparks;     // +0x23184 (143732)  Prepare: Construct(rw, 0x80000, 0)
        EffectsVertexBufferManager mVertexBufferManagerParticles;  // +0x231AC (143772)  Prepare: Construct(rw, 163840, 0)

        // +0x231D4 (143812): DWARF :136 LionBatchArray mLionBatchArray .. +0x249C8 (its trailing
        // count word at +0x249C4 is the -1 the ctor stamps / the 0 BuildLionVertexBuffers
        // resets). FLAG: PLACEHOLDER (the Lion batch array type is the Lion core's).
        u8  maLionBatchArrayPlaceholder[0x249C4 - 0x231D4];
        s32 miLionBatchCount;                          // +0x249C4 == -1 (ctor)
        u8  maPad249C8To249D0[0x249D0 - (0x249C4 + 4)];// -> +0x249D0
        // FLAG: PLACEHOLDER. EA::Jobs::Job mParticleRenderJobSparks (DWARF :139, sizeof 0x350 ==
        // 848). Its EA::Jobs::Job::Job(this, 0) sub-construction is DEFERRED (see the .cpp).
        u8  maJob0Placeholder[0x350];                  // +0x249D0

        // +0x24FD0 (151504): -1, +0x25008 (151560): -1, then SparkFrameDataSet @+0x25030.
        u8  maPad249D0EndTo24FD0[0x24FD0 - (0x249D0 + 0x350)]; // -> +0x24FD0
        s32 miSentinel24FD0;                           // +0x24FD0 == -1
        u8  maPad24FD4To25008[0x25008 - (0x24FD0 + 4)];// -> +0x25008
        s32 miSentinel25008;                           // +0x25008 == -1
        u8  maPad2500CTo25030[0x25030 - (0x25008 + 4)];// -> +0x25030
        // FLAG: PLACEHOLDER. BrnParticle::Native::SparkFrameDataSet (no reconstructed
        // layout). Sized to the next named member (+0x25700). ResetSparkFrameData
        // targets this set (a1 + 151600 == +0x25030).
        u8  maSparkFrameDataSet0Placeholder[0x25700 - 0x25030]; // +0x25030

        // +0x25700 (153344): an EA::Jobs::Job. FLAG: PLACEHOLDER (sub-ctor DEFERRED).
        u8  maJob1Placeholder[0x350];                  // +0x25700

        // +0x25CD0 (154832): -1, +0x25D08 (154888): -1, then SparkFrameDataSet @+0x25D30.
        u8  maPad25700EndTo25CD0[0x25CD0 - (0x25700 + 0x350)]; // -> +0x25CD0
        s32 miSentinel25CD0;                           // +0x25CD0 == -1
        u8  maPad25CD4To25D08[0x25D08 - (0x25CD0 + 4)];// -> +0x25D08
        s32 miSentinel25D08;                           // +0x25D08 == -1
        u8  maPad25D0CTo25D30[0x25D30 - (0x25D08 + 4)];// -> +0x25D30 (0x24, mirrors set0's pad)
        // FLAG: PLACEHOLDER. The second BrnParticle::Native::SparkFrameDataSet.
        u8  maSparkFrameDataSet1Placeholder[0x26400 - 0x25D30]; // +0x25D30 (size 0x6D0, mirrors set0)

        // +0x26400 (156672): an array of 5 EA::Jobs::Job (stride 0x350; the ctor loops
        // i = 4..0 calling Job::Job(v5, 0), v5 += 0x350). FLAG: PLACEHOLDER (sub-ctors
        // DEFERRED). sizeof 5 * 0x350.
        static const s32 KI_NUM_FRAME_JOBS = 5;
        u8  maFrameJobsPlaceholder[KI_NUM_FRAME_JOBS * 0x350]; // +0x26400

        // Gap to the trailing members at +0x27780.
        u8  maPad26400EndTo27780[0x27780 - (0x26400 + KI_NUM_FRAME_JOBS * 0x350)]; // -> +0x27780

        // +0x27780 (161664): DWARF :160 muNumDebrisUpdateJobsToWaitOn (Construct: -1).
        // NOTE: the earlier model called the byte at +0x27784 "a bool the ctor zeroes last";
        // the ctor's `stbx r30` is the low byte of this word's store neighbour -- the DWARF
        // places the debris-job wait count here and the inter-thread queue right after.
        s32 miNumDebrisUpdateJobsToWaitOn;             // +0x27780 == -1
        bool mbFlag27784;                              // +0x27784 == false (the ctor's trailing byte store)
        u8  maPad27785To27788[0x27788 - 0x27785];
        // +0x27784 (161668): DWARF :163 CappedInterThreadEventQueue mInterThreadEventQueue
        // (VariableEventQueue<16384,16>, Construct'd by ParticleModule::Construct). FLAG:
        // PLACEHOLDER on the host (its consumer is the Lion dispatch pass, not landed).
        u8  maInterThreadEventQueuePlaceholder[0x2B7A0 - 0x27788];
        // +0x2B7A0 (178080): DWARF :166 SparkBatchSpawnEvent mSparkSpawnBufferHeader (16 bytes:
        // count first) and :169 mpSparkSpawnBuffer (+0x2B7B0, Prepare: Malloc(2560, 16)).
        u32   muSparkSpawnCount;                       // +0x2B7A0 (the header's leading count; Prepare: 0)
        u8    maSparkSpawnHeaderTail[0x2B7B0 - 0x2B7A4];
        void* mpSparkSpawnBuffer;                      // +0x2B7B0 (178096)
    };

}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
