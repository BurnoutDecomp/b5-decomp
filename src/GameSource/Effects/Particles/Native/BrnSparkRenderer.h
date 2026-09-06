#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSparkRenderer.h
//
// BrnParticle::Native::BrnSpark -- one spark particle, and the FXBucket<BrnSpark,4>
// specialisation used by the spark array's bucket banks.
//
// DWARF AUTHORITY (DecFIGS BrnSparkRenderer.h dump):
//   struct BrnSpark { Vector4 mPosition; Vector4 mVelocity; float mfSize; };
//     -- 2*16 + 4 = 36 bytes, 16-byte aligned -> sizeof == 48 (0x30). This matches
//        the 48-byte particle stride and +0x280 particle-array base attested by the
//        FXBucket<BrnSpark,4>::GetNewParticle asm (@ 0x82285460): maParticleData starts
//        at +0x10 + 4*156 = 0x280 = 640, stride 48.
//   typedef FXBucket<BrnParticle::Native::BrnSpark,4> SparkArray::SparkBank::SparkBucket.
//
// ADDITIVE GROW 2026-09-06 -- SparkArray / SparkArray::SparkBank / SparkRenderer.
//
// The four spark arrays and the spark renderer were an asm-sized placeholder inside
// ParticleModule.h ("Gap to the trail system at +0x9710 ... FLAG: PLACEHOLDER"), which
// is why the whole grinding-metal spark family -- the most recognisable thing in a
// Burnout crash -- had no destination to be written into. Their layout is not inferred:
// ParticleModule::Prepare @0x8229BEA0 constructs all five objects INLINE and its own
// stores pin every offset (0x8229C138..0x8229C210):
//
//   stwx  r27, r31, 0x94C0            mSparkRenderer.mpRenderer = the module's Im3d
//   r10 = r31 + 0x94D0                &maSparks[0]
//   r11 = r10 + 0x50   stw 0,0(r11) / mBucketManager,4(r11) / 0,8(r11) / 0x7D0,0xC(r11)
//   r10'= r10 + 0x60   ...same with 0x3E8                    -> the two SparkBanks
//   then r31+0x9560, r31+0x95F0, r31+0x9680  -> stride 0x90, four arrays
//
// so maSparks[0] is at +0x94D0, sizeof(SparkArray) == 0x90 == 144, and
// 0x94D0 + 4*0x90 == 0x9710 == mTrailSystem, exactly closing the gap. The same 0x90
// stride and the same two bank offsets are re-attested twice more, independently:
//   * BeginParticleRenderJob @0x8228A7C0 walks `v21 = module + 38160` (== +0x9510 ==
//     &maSparks[0].mafLifetimes[0]), calls FreeUnusedBuckets(v21+16) and (v21+32)
//     -- +0x9520 / +0x9530 -- and advances `v21 += 144` four times;
//   * EffectsModule::Prepare @0x8229E690 (0x8229E910..0x8229E9E0) copies each
//     Attrib::Gen::sparkeffect record into one array with `addi r11, r11, 0x90`.
// maTextures is STATIC (the DWARF spells it `extern`): LoadFXBundle @0x8229C950
// publishes into the fixed four-entry table at unk_82FAC230 (its loop bound is
// qword_82FAC250 == +0x20 == 4 * 8) while walking the arrays by their 0x90 stride.
//
// Only the BrnSpark element and the SparkBucket typedef are modelled here (the surface
// the reconstructed Clear/GetNewParticle instantiation needs); the renderer/array
// machinery grows additively.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h" // rw::math::vpu::Vector4 / Matrix44 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT (GetFrame)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // SafeResourceHandle (maTextures)
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<SparkBatch,4>
#include "GameSource/Effects/Particles/Native/FXBuckets.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h" // EffectsVertexBufferBatch base

// Pointer-only uses -- documented forward-declaration exception (b): including
// CgsIm3d.h / the renderengine texture header here would drag the whole immediate-mode
// and resource cascade into ParticleModule.h, which includes this file.
namespace CgsGraphics  { struct Im3d; }
namespace CgsMemory    { class  HeapMalloc; }
namespace renderengine { class  Texture; class VertexBuffer; }

namespace BrnParticle
{
namespace Native
{
    // BrnSparkRenderer.h:57 (DWARF) -- motion-blur frame ring size.
    static const u32 knSparksMaxBlurFrames = 8;

    // BrnSparkRenderer.h:63 (DWARF) -- the four spark-array types (+ the _Max count).
    enum ESparkArrayID
    {
        eSparkArray_GrindingWorld    = 0,
        eSparkArray_GrindingRaceCars = 1,
        eSparkArray_Crashing         = 2,
        eSparkArray_BodyPart_Contact = 3,
        eSparkArray_Max              = 4,
    };

    // BrnSparkRenderer.h:140 (DWARF). 8 (EffectsVertexBufferBatch base) + 4 (enum) = 12
    // bytes, no tail padding (max align 4) -> Array<SparkBatch,4> count word lands at
    // +0x30 (== 4*12), matching Append @0x8291FAD0 and operator[] @0x8227CAD8.
    struct SparkBatch : public EffectsVertexBufferBatch
    {
        ESparkArrayID meArrayId;   // BrnSparkRenderer.h:143 -- @ +0x08
    };

    // BrnSparkRenderer.h:146 (DWARF), spelled inside SparkRenderer::Dispatch's own
    // declaration: typedef CgsContainers::Array<SparkBatch,4u> SparkBatchArray. The
    // committed container is the unqualified Array<T,N> (see CgsArray.h's banner); the
    // count word lands at +0x30 == 4 * sizeof(SparkBatch), which is what
    // SparkRenderer::Dispatch's `lwz r11, 0x30(r21)` and Append @0x8291FAD0 both read.
    typedef Array<SparkBatch, 4> SparkBatchArray;

    // BrnSparkRenderer.h:75 (DWARF). 192 payload bytes + a 16-byte alignment hole ->
    // sizeof 0xD0 == 208 (stride attested by SparkFrameDataSet::GetFrame @0x8291FA40
    // `mulli r29,0xD0`).
    //
    // ⭐ THE DEBUG VIEW COPY IS AT +0x90, NOT +0x88. Reset @0x8227A610 writes each frame
    // as `view -> +0x00, proj -> +0x40, f1 -> +0x80 and +0x84, view again -> +0x90`
    // (`addi r26, r3, 0x90` / `addi r26, r28, 0x90`), i.e. the two debug floats are
    // followed by an 8-byte hole so the second Matrix44Affine keeps its 16-byte
    // alignment. The C++ below reproduces that by construction -- Matrix44Affine is
    // alignas(16) -- and 0x90 + 0x40 == 0xD0 closes the stride exactly. (The previous
    // comment said +0x88; the layout was already right, only the note was wrong.)
    struct alignas(16) SparkFrameData
    {
        rw::math::vpu::Matrix44Affine mViewMatrix;                       // +0x00 (64)
        rw::math::vpu::Matrix44       mProjectionMatrix;                 // +0x40 (64)
        f32                           mfTimeStamp;                       // +0x80
        f32                           mfTimeStampCopyForDebugBuildsOnly; // +0x84
        rw::math::vpu::Matrix44Affine mViewMatrixCopyForDebugBuildsOnly; // +0x90 (64)

        // BrnSparkRenderer.h:92 (DWARF). Latch one frame of the motion-blur ring. Inlined
        // at every use on the console -- Reset @0x8227A610 emits it eight times and
        // Update @0x82283C10 once, always as the same five stores in the same order.
        void Set(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                 rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                 f32                                    lfTimeStamp);
    };

    // BrnSparkRenderer.h:105 (DWARF). The ring of knSparksMaxBlurFrames (== 8) motion-blur
    // frame snapshots. GetFrame indexes maFrames[] by frame id.
    struct SparkFrameDataSet
    {
        SparkFrameData maFrames[knSparksMaxBlurFrames]; // +0x00, stride 0xD0

        // X360 @ 0x8227A610. Stamp every one of the eight frames with the same view /
        // projection / timestamp, so the motion-blur trail collapses to a point.
        void Reset(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                   rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                   f32                                    lfTimeStamp);

        // X360 @ 0x82283C10. Advance the ring by lfDeltaTime and re-stamp frame 0.
        void Update(rw::math::vpu::Matrix44Affine::InParam lViewMatrix,
                    rw::math::vpu::Matrix44::InParam       lProjectionMatrix,
                    f32                                    lfDeltaTime);

        // X360 @ 0x8291FA40 (called from SparkArray::RenderBank). Bounds-asserts the frame
        // id then returns &maFrames[luFrameId] (this + 208*luFrameId).
        const SparkFrameData& GetFrame(u32 luFrameId) const;
    };

    // BrnSparkRenderer.h:149 (DWARF). 16-byte aligned so sizeof rounds 36 up to 48,
    // giving the attested per-particle stride.
    struct alignas(16) BrnSpark
    {
        rw::math::vpu::Vector4 mPosition; // BrnSparkRenderer.h:152 -- @ +0x00
        rw::math::vpu::Vector4 mVelocity; // BrnSparkRenderer.h:153 -- @ +0x10
        f32                    mfSize;    // BrnSparkRenderer.h:154 -- @ +0x20
    };

    // BrnSparkRenderer.h:158 (DWARF): the spark bank's bucket type. With N=4 the derived
    // KuMaxNumParticles == 156 (KuMaxNumParticlesUnAligned 157 rounded down to even),
    // matching the 0x9C loop bound in Clear/GetNewParticle.
    typedef BrnParticle::FXBucket<BrnSpark, 4> SparkBucket;

    // ========================================================================
    // BrnSparkRenderer.h:170 (DWARF) -- SparkArray.
    //
    // One spark FAMILY: its four colour ramps, its four lifetimes, its two bucket banks
    // (the ordinary one and the crash one, which is drained on a different schedule), the
    // seven scalars that shape a spark's flight, and the name of the texture it draws
    // with. Four of these live in the particle module, one per ESparkArrayID.
    //
    // Layout, pinned by ParticleModule::Prepare's own stores (see the banner):
    //   +0x00 maColours[4]   +0x40 mafLifetimes[4]   +0x50 mRegularBank   +0x60 mCrashBank
    //   +0x70 gravity  +0x74 bounce  +0x78 motionBlur  +0x7C radius
    //   +0x80 dragInitialVelocityScale  +0x84 dragTerminalVelocityScale  +0x88 dragDuration
    //   +0x8C mpcSparkTextureName                                     sizeof == 0x90
    // The host object is wider only where a console 4-byte pointer widens to 8 (the two
    // banks and the name), exactly as every other widened record in this tree.
    // ========================================================================
    struct alignas(16) SparkArray
    {
        // BrnSparkRenderer.h:257 (DWARF) -- one bank of spark buckets.
        //
        // A bank is an intrusive list of FXBucket<BrnSpark,4> borrowed from the module's
        // shared FXBucketManager, plus the cap on how many sparks this bank may hold.
        // ParticleModule::Prepare constructs each array's pair as
        //   mRegularBank = { 0, &mBucketManager, 0, 2000 }   (`li r7, 0x7D0`)
        //   mCrashBank   = { 0, &mBucketManager, 0, 1000 }   (`li r8, 0x3E8`)
        // in that member order (`stw 0,0` / `stw mgr,4` / `stw 0,8` / `stw cap,0xC`).
        struct SparkBank
        {
            SparkBucket*     mpBuckets;        // :289 -- @ +0x00, the live bucket list head
            FXBucketManager* mpBucketManager;  // :290 -- @ +0x04
            u32              muNumBuckets;     // :291 -- @ +0x08
            u32              muMaxNumSparks;   // :293 -- @ +0x0C

            // :264 -- inlined into ParticleModule::Prepare (the four-store record above).
            // lbIsCrashBank selects the cap; the console spells the two caps as literals
            // at the two call sites rather than deriving them here.
            void Construct(FXBucketManager* lpBucketManager, bool lbIsCrashBank);

            // :268 / :272 / :276 -- Destruct / Prepare / Release. The X360 build has no
            // out-of-line body for any of the three (nothing in the ledger, no xref), so
            // they are not declared here; the bank owns no memory of its own -- every
            // bucket goes back to the manager through FreeUnusedBuckets.

            // :280 -- X360 @0x82295478. Return a slot for a spark born at lfBirthTime,
            // claiming another bucket from the manager when the current head is full and
            // the cap allows it, or recycling the oldest bucket when it does not.
            BrnSpark* GetNewSpark(f32 lfBirthTime);

            // :286 -- X360 @0x82283B60. Hand back every bucket whose youngest particle
            // died before lfCurrentTime - lfLifetime.
            void FreeUnusedBuckets(f32 lfCurrentTime, f32 lfLifetime);
        };

        rw::math::vpu::Vector4 maColours[4];              // :340 -- @ +0x00
        f32                    mafLifetimes[4];           // :341 -- @ +0x40
        SparkBank              mRegularBank;              // :343 -- @ +0x50
        SparkBank              mCrashBank;                // :344 -- @ +0x60 (console)
        f32                    mfGravityStrength;         // :346 -- @ +0x70
        f32                    mfBounceStrength;          // :347 -- @ +0x74
        f32                    mfMotionBlurTime;          // :348 -- @ +0x78
        f32                    mfSparkRadius;             // :349 -- @ +0x7C
        f32                    mfDragInitialVelocityScale;  // :351 -- @ +0x80
        f32                    mfDragTerminalVelocityScale; // :352 -- @ +0x84
        f32                    mfDragDuration;            // :353 -- @ +0x88
        const char*            mpcSparkTextureName;       // :355 -- @ +0x8C (console)

        // :359 -- STATIC, shared by all four arrays. The DWARF spells it `extern` and
        // LoadFXBundle @0x8229C950 publishes into the fixed table at unk_82FAC230
        // (`addi r26, r26, 8` per array, loop bound qword_82FAC250 == base + 0x20), while
        // walking the arrays themselves by their 0x90 stride -- so the table is NOT a
        // member. SparkRenderer::Dispatch reads the same base for its SetTexture.
        static CgsResource::SafeResourceHandle<renderengine::Texture> maTextures[4];

        // :178 -- inlined into ParticleModule::Prepare: construct the two banks. The
        // console writes nothing else here (no colour, no scalar, no name): every other
        // member is published by UpdateParams before the first frame.
        void Construct(FXBucketManager* lpBucketManager, ESparkArrayID leArrayId);

        // :195 -- inlined into BeginParticleRenderJob @0x8228A7C0, which computes the
        // longest of the four lifetimes with three fsels and passes it to both banks.
        void FreeUnusedBuckets(f32 lfCurrentTime);

        // :204 -- X360 @0x822955E0. Launch one spark.
        void SpawnSpark(rw::math::vpu::Vector3::InParam lPosition,
                        rw::math::vpu::Vector3::InParam lVelocity,
                        f32  lfSize,
                        f32  lfCurrentTime,
                        f32  lfTimeSinceEvent,
                        f32  lfBirthTimeOffset,
                        f32  lfHeightAbovePlane,
                        bool lbIsCrashSpark);

        // :210 -- inlined into LoadFXBundle stage 12. Publish one texture into the shared
        // table, under the console's own `( leArrayId >= 0 ) && ( leArrayId < eSparkArray_Max )`
        // assert (BrnSparkRenderer.h:229).
        static void AcquireTexture(CgsResource::SafeResourceHandle<renderengine::Texture> lTexture,
                                   ESparkArrayID leArrayId);

        // :221 -- the same bounds assert, then the table read. Inlined into
        // SparkRenderer::Dispatch (BrnSparkRenderer.h:240 there).
        static renderengine::Texture* GetTexture(ESparkArrayID leArrayId);

        // :230 -- the name LoadFXBundle hashes against the FX bundle's texture-name map.
        const char* GetTextureName() const { return mpcSparkTextureName; }

        // :250 -- inlined into EffectsModule::Prepare @0x8229E690 and
        // EffectsModule::Update @0x8229EC28, which each run it once per array straight out
        // of that array's Attrib::Gen::sparkeffect record.
        void UpdateParams(f32 lfGravityStrength, f32 lfBounceStrength, f32 lfMotionBlurTime,
                          f32 lfSparkRadius, f32 lfDragInitialVelocityScale,
                          f32 lfDragTerminalVelocityScale, f32 lfDragDuration,
                          rw::math::vpu::Vector4::InParam lColour0,
                          rw::math::vpu::Vector4::InParam lColour1,
                          rw::math::vpu::Vector4::InParam lColour2,
                          rw::math::vpu::Vector4::InParam lColour3,
                          rw::math::vpu::Vector4::InParam lLifetimes,
                          const char* lpcSparkTextureName);
    };

    // BrnSparkRenderer.h:374 (DWARF) -- SparkRenderer. One pointer: the module's textured
    // immediate-mode renderer. ParticleModule::Prepare's `stwx r27, r31, 0x94C0` is the
    // whole of its Construct.
    struct SparkRenderer
    {
        CgsGraphics::Im3d* mpRenderer;   // :407 -- @ +0x00

        // :381 -- inlined into ParticleModule::Prepare (the single store above). The heap
        // argument is unread by the X360 body, exactly as in TrailRenderer::Construct.
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, CgsGraphics::Im3d* lpRenderer);

        // :400 -- X360 @0x8228BBC8. Replay the frame's spark batches to the device.
        void Dispatch(rw::math::vpu::Matrix44::InParam lViewProjectionMatrix,
                      renderengine::VertexBuffer* lpVertexBuffer,
                      const SparkBatchArray& lrBatches);
    };
}
}
