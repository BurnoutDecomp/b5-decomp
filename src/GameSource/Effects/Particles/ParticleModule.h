#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; vtable + the two RWMutexes the ctor constructs inline via its DataBuffers)
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h" // EA::Allocator::ITaggedAllocator (IInternalAllocator's base)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::Camera (ParticleRenderData::mCgsCamera, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h"  // BrnGraphics::Im3dSkidsRenderer (mSkidsRenderer, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"        // BrnParticle::Native::TrailSystem (mTrailSystem, BY VALUE)

namespace CgsMemory { class HeapMalloc; }   // GameShared/GameClasses/Memory/CgsHeapMalloc.h (fwd; avoids a cross-module include cycle)
namespace renderengine { class Texture; }   // ParticleRenderData::mpEnvironmentMap (pointer-only)

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
//   * ParticleModule - now a full CgsModule::ModuleSingleBuffered (its first two
//     stored words in the X360 ctor @0x827E2218 are the base ModuleSingleBuffered
//     vtable off_820CE500 then the derived ParticleModule vtable off_820D0400, and
//     it constructs the two EA::Thread::RWMutex members the base owns at +0x10 /
//     +0x118 == exactly the ModuleSingleBuffered base sub-object construction). The
//     constructor body (in ParticleModule.cpp) reproduces, BY NAMED MEMBER, every
//     store the X360 asm makes past the base: an intrusive list + a 500-entry zeroed
//     pair table, the embedded LionParticleRender, several contained-interface
//     vtable + empty-list stamps, the playing-effect slot array's sentinel words,
//     the EA::Jobs::Job sub-objects, and the two SparkFrameDataSet sets.
//
// LAYOUT MODEL (same discipline as BrnPhysics::PhysicsModule). The constructor's
// X360 asm touches members at large absolute offsets. The heavy embedded types
// (BrnParticle::LionParticleRender, BrnParticle::Native::SparkFrameDataSet) have NO
// complete reconstructed layout yet, so they (and the un-touched gaps between the
// stores) are modelled as correctly-named, asm-sized OPAQUE PLACEHOLDER members so
// every ctor-touched offset lands where the X360 asm proves it and the body
// reproduces every store BY NAME (no raw-offset pointer hacks). Each placeholder is
// FLAGGED. When the LionParticleRender / SparkFrameDataSet layout passes land they
// fold into the real typed members. X360 byte offsets are the 4-byte-pointer console
// ABI; member access here is by name, so the body is faithful regardless of host
// pointer width.
// ============================================================================

namespace BrnParticle
{
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
        //
        // ⚠ THEY USED TO LIVE IN BrnDispatchThreadInputBuffer.h, WHICH DECLARED
        // `namespace BrnParticle { namespace ParticleModule { ... } }` -- i.e. it modelled
        // `BrnParticle::ParticleModule` as a NAMESPACE while THIS header models it as a
        // struct. That is a split-brain, and not a harmless one: it is a hard
        // `error C2757: a symbol with this name already exists` in any translation unit that
        // includes both headers. Nothing did yet, which is the only reason it had not fired.
        // Moving the two types to their DWARF home retires the namespace and the conflict
        // together (2026-08-15, post-fx step-6 producers wave).
        // ==============================================================================

        // DWARF ParticleModule.h:565. FLAG (ad-hoc opaque, MOVED VERBATIM, not recovered this
        // wave): the DWARF gives it mfCurrentTime / mfCurrentTimeStep / muChangedEffects /
        // Matrix44Affine mViewMatrix / Matrix44 mProjectionMatrix / LionEffect[128]
        // maChangedLionEffects (:566-571), which sizes to 0x90 + 128 * 0x70 == 0x3890 -- and
        // that is EXACTLY the console's gap between DispatchThreadInputBuffer's mParticleData
        // (+0x10) and mParticleRenderData (+0x38A0). So the real layout is known and
        // corroborated; recovering it is a separate change (LionEffect's own 0x70 stride is
        // already pinned above), and until then the 1-byte placeholder is kept EXACTLY as it
        // was so nothing about the enclosing buffer changes by accident.
        struct DispatchThreadUpdateData
        {
            u8 maStorage[1];   // FLAG: opaque, true size is 0x3890 -- see above
        };

        // DWARF ParticleModule.h:576-606 -- the render-side snapshot this module publishes
        // once a frame for the dispatch/render thread. RECOVERED 2026-08-15 (post-fx step-6
        // producers wave); it was an ad-hoc `u8 maStorage[1]` opaque before.
        //
        // OFFSET ATTESTATION: BrnRendererModule::Render @0x8240BFA8 reads exactly three of
        // these off the read-locked buffer (asm 0x8240DD04-0x8240DD0C): `lfs f1, 0xC(r24)` ==
        // mfCurrentTimeStep, `addi r4, r24, 0x60` == the camera VIEW matrix and
        // `addi r5, r24, 0xA0` == the camera PROJECTION matrix, handed to
        // MotionBlurState::Update. All three land on the layout below without a single
        // adjustment -- +0x0C is the fourth 4-byte scalar, +0x60 is where mCgsCamera starts
        // once mCameraTransform is 16-aligned at +0x20, and +0xA0 is mCgsCamera's own
        // m_projectionMatrix at its +0x40 (CgsCamera.h pins that offset with a static_assert).
        // Three independent offsets agreeing with a layout derived only from the DWARF member
        // list is the check that this is the right struct.
        //
        // FLAG (guest tail): the console places the NEXT member of the enclosing buffer
        // (mBufferCrashTriangleCache) at +0x3B00 while this layout ends at +0x210 from
        // +0x38A0 == +0x3AB0. The 0x50-byte gap is NOT accounted for -- either the X360
        // CgsGraphics::Camera is larger than the 368 bytes the PS3 DWARF and the X360 Camera
        // copy-constructor attest, or the X360 struct carries members the FIGS DWARF does not.
        // It changes nothing here (parity is by named member and every offset any code reads
        // is confirmed above), but it is recorded rather than rounded away.
        //
        // FLAG (mpParticleModule's type): the DWARF types it `BrnParticle::ParticleModule*`.
        // It cannot be spelled that way inside the class itself without the class being
        // complete, and it is not -- so it is carried as a host `void*` of the same width,
        // with the name and the slot intact. Re-type it to `ParticleModule*` once this
        // declaration is moved after the closing brace, or leave it: nothing in this tree
        // dereferences it.
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

            void*                         mpParticleModule;      // DWARF :589 (guest +0x00)
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

        // X360 0x82278380. Resolve a handle to its playing-effect slot, or NULL when the
        // slot has been recycled (its stored handle no longer equals luHandle).
        LionEffect* GetLionEffect(u32 luHandle);

        // X360 0x822867E0. Start the named LION effect (the caller precomputes the name
        // hash via ParticleDescription::HashString) at the given world index, returning the
        // new playing-effect handle. Own-TU body; declared here for the boost/jump machines.
        u32 StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex);

        // X360 0x8228A238. Stop a playing LION effect, given its resolved slot pointer
        // (asserts the slot is non-NULL and in use). Own-TU body.
        void StopLionEffect(LionEffect* lpEffect);

        // X360 0x8227EAC8. Reset both spark-frame data sets (Prepare / EffectsModule::Update
        // drive this). DECLARE-ONLY: its body depends on BrnParticle::Native::SparkFrameDataSet
        // (no committed layout) and three un-recovered rodata constant vectors; see the .cpp.
        void ResetSparkFrameData();

    public:
        // ===================================================================
        // Constructor-faithful layout. Members are named where the X360 ctor /
        // GetLionEffect asm touch them; the un-touched gaps are asm-sized opaque
        // placeholders. Offsets in comments are absolute from `this` on the X360
        // 4-byte-pointer ABI (the base ModuleSingleBuffered occupies the head).
        // ===================================================================

        // Gap from the base sub-object end to the first ctor-touched member at +0x4270.
        // FLAG: PLACEHOLDER (un-touched module state between the base and the list).
        u8 maPadBaseTo4270[0x4270 - sizeof(CgsModule::ModuleSingleBuffered)]; // -> +0x4270

        // +0x4270: a contained interface-with-intrusive-list sub-object. The ctor
        // empty-initialises it: three head ints at +0x4270/+0x4274/+0x4278 (the asm
        // re-stores head0), then self-referencing next/prev/iter at +0x427C/+0x4280/
        // +0x4284 (all == &miListHead0), then a count word at +0x4288 == 0.
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
        ContainedListInterface mList;                 // +0x4270

        // +0x4298: a 500-entry table the ctor zeroes pair-by-pair (v3 = +0x4298,
        // 500 iterations writing two zero dwords each: the asm's `li r9,0x1F3`
        // counter == 499 with a `>= 0` do/while == 500 entries x 8 bytes).
        struct EffectPair { u32 mu0; u32 mu1; };      // +0x00 / +0x04 (both zeroed)
        static const u32 KU_NUM_EFFECT_PAIRS = 500;
        EffectPair maEffectPairs[KU_NUM_EFFECT_PAIRS]; // +0x4298 .. +0x5258

        // Gap to the embedded LionParticleRender at +0x5270.
        u8 maPad5258To5270[0x5270 - (0x4298 + KU_NUM_EFFECT_PAIRS * sizeof(EffectPair))]; // -> +0x5270

        // +0x5270: the embedded BrnParticle::LionParticleRender. FLAG: PLACEHOLDER --
        // no complete reconstructed layout; sized to the next named member (+0x53F0).
        // Its sub-constructor (X360 BrnParticle::LionParticleRender::LionParticleRender)
        // is DEFERRED in the .cpp (see note) -- chaining it would require fabricating
        // its type/vtable, which the project rules forbid.
        u8 maLionParticleRenderPlaceholder[0x53F0 - 0x5270]; // +0x5270

        // +0x53F0: the playing-effect slot array (GetLionEffect base, stride 0x70).
        LionEffect maPlayingEffects[KU_MAX_PLAYING_EFFECTS]; // +0x53F0 .. +0x9000-ish

        // Gap to the first contained-interface vtable stamp at +0x9010 (36880).
        u8 maPadEffectsTo9010[0x9010 - (0x53F0 + KU_MAX_PLAYING_EFFECTS * sizeof(LionEffect))]; // -> +0x9010

        // The five contained-interface sub-objects the ctor stamps. Each is a vtable
        // pointer at its base then two zeroed words; modelled as named placeholders so
        // each store lands by name. FLAG: the owning interface types / vtables
        // (off_820CF69C / off_820CEBE0 / off_820CEBE4 / off_820CEBE8 / off_820CFA1C)
        // are not reconstructed; each mpVTable is left null here.
        struct ContainedInterface
        {
            void* mpVTable;   // +0x00 : the X360 vtable symbol (left null -- FLAGGED)
            u32   mu04;       // +0x04 : 0
            u32   mu08;       // +0x08 : 0
        };
        ContainedInterface mInterfaceA;                // +0x9010 (36880) off_820CF69C
        u8 maPadIfaceAToB[0x91A0 - (0x9010 + sizeof(ContainedInterface))]; // -> +0x91A0
        ContainedInterface mInterfaceB;                // +0x91A0 (37280) off_820CEBE0
        u8 maPadIfaceBToC[0x9210 - (0x91A0 + sizeof(ContainedInterface))]; // -> +0x9210
        // +0x9210 (37392): BrnGraphics::Im3dSkidsRenderer mSkidsRenderer (DWARF ParticleModule.h:771)
        // -- the skid / tyre-mark immediate-mode renderer, 100 bytes on the console (0x9210..
        // +0x9274), constructed by ParticleModule::Prepare @0x8229BEA0 (`Im3dSkidsRenderer::
        // Construct(this + 37392, heap)`) and handed to the trail system's renderer. The "vtable
        // + two zero words" the ctor stamps here are its ImRenderer<SkidVertex> base's own
        // construction (off_820CEBE4 is that vtable), reproduced by the base sub-object itself.
        BrnGraphics::Im3dSkidsRenderer mSkidsRenderer; // +0x9210 .. +0x9274
        ContainedInterface mInterfaceD;                // +0x9274 (37492) off_820CEBE8
        u8 maPadIfaceDToE[0x92E0 - (0x9274 + sizeof(ContainedInterface))]; // -> +0x92E0
        ContainedInterface mInterfaceE;                // +0x92E0 (37600) off_820CFA1C

        // Gap to the trail system at +0x9710 (the DWARF's mLionImmediateModeRenderer /
        // mSparkRenderer / maSparks[4] -- :773-776 -- not yet typed). FLAG: PLACEHOLDER.
        u8 maPadIfaceETo9710[0x9710 - (0x92E0 + sizeof(ContainedInterface))]; // -> +0x9710

        // +0x9710 (38672): BrnParticle::Native::TrailSystem mTrailSystem (DWARF :778), 102652
        // bytes on the console (.. +0x2280C). The 0x7FFFFFFF the ctor stamps at +0x22190
        // (139664 == 38672 + 100608 + 384) is this object's mFreeEmitters Stack length --
        // the inlined Stack<TrailEmitter*,96> constructor, reproduced by TrailSystem's own
        // ctor. RenderFullResParticles @0x8229AFD0 (`TrailSystem::Render(this + 38672)`),
        // ParticleModule::Prepare @0x8229BEA0 and EffectsModule::HandleWheels @0x82296C80
        // (`this + 0xA80 + 0x9710`) all address it here.
        Native::TrailSystem mTrailSystem;              // +0x9710 .. +0x2280C

        // Gap to the first job/sentinel cluster at +0x249C4: the debris renderer + arrays
        // (DWARF :780..; BrnDebrisRenderer::Construct(this + 141328 == +0x22810)). FLAG:
        // PLACEHOLDER.
        u8 maPad2280CTo249C4[0x249C4 - 0x2280C];       // -> +0x249C4

        // +0x249C4 (149956): -1 sentinel, then an EA::Jobs::Job at +0x249D0 (149968).
        s32 miJobSentinel249C4;                        // +0x249C4 == -1
        u8  maPad249C8To249D0[0x249D0 - (0x249C4 + 4)];// -> +0x249D0
        // FLAG: PLACEHOLDER. EA::Jobs::Job sub-object (sizeof 0x350 == 848). Its
        // EA::Jobs::Job::Job(this, 0) sub-construction is DEFERRED (see .cpp note).
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
        // The asm anchors both stores off r11 = this + 0x25A80 (addis r31,2 / addi
        // 0x5A80): stw r10,0x250(r11) -> +0x25CD0 and stw r9,0x288(r11) -> +0x25D08
        // (both r9/r10 == r29 == -1). Earlier this pair was mislabeled +0x25D50/+0x25D88.
        u8  maPad25700EndTo25CD0[0x25CD0 - (0x25700 + 0x350)]; // -> +0x25CD0
        s32 miSentinel25CD0;                           // +0x25CD0 == -1
        u8  maPad25CD4To25D08[0x25D08 - (0x25CD0 + 4)];// -> +0x25D08
        s32 miSentinel25D08;                           // +0x25D08 == -1
        u8  maPad25D0CTo25D30[0x25D30 - (0x25D08 + 4)];// -> +0x25D30 (0x24, mirrors set0's pad)
        // FLAG: PLACEHOLDER. The second BrnParticle::Native::SparkFrameDataSet.
        // ResetSparkFrameData asm (0x8227EBF0 addi r3,r3,0x5D30) targets a1 + 154928 == +0x25D30
        // (154928 == 0x25D30, NOT 0x25D70 — sits 0x24 after the +0x25D08 sentinel+4, mirroring set0).
        u8  maSparkFrameDataSet1Placeholder[0x26400 - 0x25D30]; // +0x25D30 (size 0x6D0, mirrors set0)

        // +0x26400 (156672): an array of 5 EA::Jobs::Job (stride 0x350; the ctor loops
        // i = 4..0 calling Job::Job(v5, 0), v5 += 0x350). FLAG: PLACEHOLDER (sub-ctors
        // DEFERRED). sizeof 5 * 0x350.
        static const s32 KI_NUM_FRAME_JOBS = 5;
        u8  maFrameJobsPlaceholder[KI_NUM_FRAME_JOBS * 0x350]; // +0x26400

        // Gap to the trailing bool sentinel at +0x27784.
        u8  maPad26400EndTo27784[0x27784 - (0x26400 + KI_NUM_FRAME_JOBS * 0x350)]; // -> +0x27784

        // +0x27784 (161668): a bool the ctor zeroes last (stbx r30 == 0).
        bool mbFlag27784;                              // +0x27784 == false
    };
}

#endif // GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
