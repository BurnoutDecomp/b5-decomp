// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailSystem.cpp
//
// BrnParticle::Native::TrailSystem / TrailEmitter / EmitterArray -- the skid /
// tyre-mark system. Reconstructed store-for-store from the X360 ARTIST asm:
//
//   TrailSystem::Prepare            @0x8228BE78
//   TrailSystem::AttachTrailEmitter @0x8228BF50
//   TrailSystem::AddTrailSegment    @0x8228C310
//   TrailSystem::EndOfFrame         @0x8228C058
//   TrailSystem::UpdateTrailType    @0x8228C248
//   TrailSystem::Render             @0x82295C58
//   TrailEmitter::AddTrailSegment   @0x8227A9E0
//   EmitterArray::AddEntry          @0x82277D50
//   EmitterArray::RemoveEntry(int)  @0x82277DC8
//   TrailSystem::Construct  -- inlined into ParticleModule::Prepare @0x8229BEA0
//   TrailSystem::Update     -- inlined into ParticleModule::BuildLionVertexBuffers @0x8228AC20
//
// THE MARK-LAYING RULES, as the asm has them (every number is a rodata read):
//   * a new segment needs >= 0.3 m of travel from the previous one
//     (flt_82CDB3E0 == 0.09 == krTrailsMinSegmentLengthSquared, vcmpgefp @0x8227AA80);
//   * a new segment whose direction is within cos 0.9995 (~1.8 deg,
//     flt_82010C18) of the previous direction MOVES the previous segment
//     instead of adding one (vcmpgtfp @0x8227AB90) -- a straight-line skid
//     costs one segment, a curve one per 0.3 m;
//   * the contact point is lifted by kTrailHeightAdjustment (0.03 m up) so the
//     decal clears the road (vaddfp @0x8227AA28);
//   * an emitter holds 16 segments; when full, a NEW emitter is attached and
//     seeded with the old one's last segment (KX_TRAIL_EMITTER_CONTINUANCE) so
//     the strip is unbroken;
//   * the wheel keeps its emitter only while marks keep coming: after
//     1.5 * timestep without one (or a surface / trail-type change) the next
//     mark starts a fresh emitter and the old one just ages out;
//   * an emitter is released 10 s after its last segment (EndOfFrame, the
//     rodata 10.0 @0x8228C058) -- the renderer fades it over that same life.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"
#include "GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h"   // BrnGraphics::Im3dSkidsRenderer (EndRender fold)
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::{operator+,operator-,MagnitudeSquared,Normalize,Cross,Dot}

#include <cstring>   // memset / memcpy (the X360 calls both by name)

namespace BrnParticle
{
namespace Native
{
    // ---- file-scope constants (DWARF BrnTrailSystem.cpp:32-38) --------------------------
    namespace
    {
        // krTrailHeightAdjustment / kTrailHeightAdjustment (BrnTrailSystem.cpp:33/34). The X360
        // vector is a dynamically-initialised .data object (unk_82FAC1E0 reads 0.0 in the image;
        // its CRT-init thunk is an export hole, not located this wave). The value is the DecFIGS
        // static initialiser's (__static_initialization_and_destruction_0 @0xE2B5C, the stvx
        // @0xE4950 of {0, 0x3CF5C28F, 0, .}): a 3 cm lift along +Y. FLAG: X360 value corroborated
        // only through the PS3 near-ancestor, not read out of the X360 image.
        const f32     KR_TRAIL_HEIGHT_ADJUSTMENT = 0.03f;
        const Vector3 K_TRAIL_HEIGHT_ADJUSTMENT  = { 0.0f, KR_TRAIL_HEIGHT_ADJUSTMENT, 0.0f, 0.0f };

        // krTrailsMinSegmentLength / krTrailsMinSegmentLengthSquared (:35/36). Only the square is
        // read by the binary (flt_82CDB3E0 == 0.09f); the length is its root.
        const f32 KR_TRAILS_MIN_SEGMENT_LENGTH         = 0.3f;
        const f32 KR_TRAILS_MIN_SEGMENT_LENGTH_SQUARED = 0.09f;

        // flt_82010C18 (TrailEmitter::AddTrailSegment @0x8227AB34): the cosine above which a new
        // direction is treated as a continuation of the previous segment.
        const f32 KF_TRAIL_DIRECTION_MERGE_COS = 0.99949998f;

        // TrailSystem::AddTrailSegment @0x8228C310: a wheel's emitter is dropped when more than this
        // many timesteps passed since its last mark (rodata 1.5).
        const f32 KF_TRAIL_TIMEOUT_TIMESTEPS = 1.5f;

        // TrailSystem::EndOfFrame @0x8228C058: an emitter idle this long (seconds) is released.
        const f32 KF_TRAIL_EMITTER_LIFE = 10.0f;

        // rw::math::vpu::detail::gIVector (.rdata 0x82181500 == {1,0,0,0}): the tangent a strip's
        // very first segment is laid with, before any direction exists.
        const Vector3 K_I_VECTOR = { 1.0f, 0.0f, 0.0f, 0.0f };
    }

    // X360 .data flt_82CDAE78 (8 floats per type: start RGBA, end RGBA). Read out of the image:
    // every type starts {0, 0, 0, 0.77} and ends {0, 0, 0, 0} -- fully transparent black at the
    // end of the 10 s life. The live per-surface values arrive through UpdateTrailType.
    TrailParams TrailSystem::mgaDefaultParams[KI_MAX_NUM_TRAIL_TYPES] =
    {
        { { 0.0f, 0.0f, 0.0f, 0.77f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 0.0f, 0.77f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 0.0f, 0.77f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
        { { 0.0f, 0.0f, 0.0f, 0.77f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    };

    // =========================================================================================
    // TrailEmitterData::Detatch -- the inlined unlink in TrailSystem::AddTrailSegment
    // (@0x8228C53C: `stw 0, 0x14(emitter); stw 0, 0x10(data)`) and EndOfFrame (@0x8228C0F0..).
    // =========================================================================================
    void TrailEmitterData::Detatch()
    {
        if (mpTrailEmitter != 0)
        {
            mpTrailEmitter->mpOwner = 0;
        }
        mpTrailEmitter = 0;
    }

    // =========================================================================================
    // EmitterArray
    // =========================================================================================
    bool EmitterArray::Prepare()
    {
        // Inlined into TrailSystem::Prepare @0x8228BE78: memset the 384-byte pointer table, size 0.
        memset(mapEmitters, 0, sizeof(mapEmitters));
        mnCurrentSize = 0;
        return true;
    }

    void EmitterArray::AddEntry(TrailEmitter* lpEmitter)
    {
        CGS_ASSERT(mnCurrentSize < KN_TRAIL_EMITTER_POOL_SIZE,
                   "mnCurrentSize < knTrailEmitterPoolSize");

        mapEmitters[mnCurrentSize] = lpEmitter;
        ++mnCurrentSize;
    }

    void EmitterArray::RemoveEntry(s32 liIndex)
    {
        CGS_ASSERT(liIndex < mnCurrentSize, "Element not found in array");

        const s32 liLastIndex = mnCurrentSize - 1;
        if (liIndex == liLastIndex)
        {
            mapEmitters[liIndex] = nullptr;
        }
        else
        {
            mapEmitters[liIndex]            = mapEmitters[liLastIndex];
            mapEmitters[mnCurrentSize - 1]  = nullptr;
        }
        --mnCurrentSize;
    }

    TrailEmitter* EmitterArray::operator[](s32 lnIndex)
    {
        // BrnTrailSystem.h:182 -- the assert EndOfFrame fires (twice, on the two reads).
        CGS_ASSERT(lnIndex < mnCurrentSize, "lnIndex < mnCurrentSize");
        return mapEmitters[lnIndex];
    }

    // =========================================================================================
    // TrailEmitter::AddTrailSegment  @0x8227A9E0
    //   r3 = this, f1 = lfCurrentTime, v1 = lContactPoint, v2 = lContactNormal, f2 = lfSkidStrength
    // =========================================================================================
    bool TrailEmitter::AddTrailSegment(f32 lfCurrentTime, Vector3 lContactPoint,
                                       Vector3 lContactNormal, f32 lfSkidStrength)
    {
        using namespace rw::math::vpu;

        // v126: the segment position -- the contact point lifted off the road.
        const Vector3 lPosition = lContactPoint + K_TRAIL_HEIGHT_ADJUSTMENT;
        Vector3       lTangent;                                              // v127
        const s32     lnNumSegments = mn8NumSegments;

        if (lnNumSegments != 0)
        {
            const s32     lnPrevIndex   = lnNumSegments - 1;
            const Vector3 lPrevPos      = mpCurrentSegments->ReadSegmentPosition(lnPrevIndex);
            const Vector3 lDirectionVec = lPosition - lPrevPos;

            // Too close to the previous segment: nothing laid (vcmpgefp 0.09 >= |d|^2, all lanes).
            if (KR_TRAILS_MIN_SEGMENT_LENGTH_SQUARED >= MagnitudeSquared(lDirectionVec))
            {
                return false;
            }

            // The half-width axis is direction x normal (vrsqrtefp + two Newton steps, then the
            // two-multiply cross @0x8227AAE0-0x8227AAEC).
            const Vector3 lUnitDirection = Normalize(lDirectionVec);
            lTangent = Cross(lUnitDirection, lContactNormal);

            if (lnNumSegments != 1 || (mx8Flags & KX_TRAIL_EMITTER_CONTINUANCE) != 0)
            {
                // The previous direction, from the previous two segments. For a 1-segment emitter
                // carrying the continuance flag this reads segment index -1 (`addi r6, r7, -2;
                // slwi r5, r6, 5; lvx128 v0, r5, r8` @0x8227AB1C): the console reads the 32 bytes
                // BEFORE this collection, i.e. the preceding collection's segment 15. Reproduced
                // as-is -- it is the binary's behaviour, not a reconstruction guess.
                const Vector3 lPrevPrevPos      = mpCurrentSegments->ReadSegmentPosition(lnNumSegments - 2);
                const Vector3 lPrevDirectionVec = Normalize(lPrevPos - lPrevPrevPos);

                if (Dot(lUnitDirection, lPrevDirectionVec) > KF_TRAIL_DIRECTION_MERGE_COS)
                {
                    // Nearly collinear: extend the previous segment to the new point instead of
                    // spending a fresh one.
                    mpCurrentSegments->WriteSegmentPosition(lPosition, lnPrevIndex);
                    mpCurrentSegments->WriteSegmentTangent(lTangent, lnPrevIndex);
                    mpCurrentSegments->WriteSegmentTime(lfCurrentTime, lnPrevIndex);
                    mpCurrentSegments->WriteSegmentStrength(lfSkidStrength, lnPrevIndex);
                    mrTimeLastSegmentAdded = lfCurrentTime;
                    return true;
                }
            }
            else
            {
                // A fresh emitter's first segment was laid with the placeholder tangent (below);
                // now that a direction exists, give segment 0 the real one (@0x8227AB04-0x8227AB14).
                mpCurrentSegments->WriteSegmentTangent(lTangent, 0);
            }
        }
        else
        {
            // No direction yet: the first segment's tangent is the identity X axis (@0x8227AC14).
            lTangent = K_I_VECTOR;
        }

        // Append (@0x8227AC20..).
        CGS_ASSERT(mn8NumSegments < KN_MAX_TRAIL_SIZE, "mn8NumSegments < knMaxTrailSize");
        mpCurrentSegments->WriteSegmentPosition(lPosition, mn8NumSegments);
        mpCurrentSegments->WriteSegmentTangent(lTangent, mn8NumSegments);
        mpCurrentSegments->WriteSegmentTime(lfCurrentTime, mn8NumSegments);
        mpCurrentSegments->WriteSegmentStrength(lfSkidStrength, mn8NumSegments);
        ++mn8NumSegments;
        mrTimeLastSegmentAdded = lfCurrentTime;
        return true;
    }

    // =========================================================================================
    // TrailSystem::Construct -- inlined into ParticleModule::Prepare @0x8229BEA0:
    //   *(this+100992) = 0        Stack<TrailEmitter*,96>::Construct
    //   *(this+102556) = 0        mnCurrentBuffer
    //   *(this+102560) = renderer mRenderer.mpRenderer   (TrailRenderer::Construct)
    //   *(this+102648) = 0        mbIsReady
    // The DWARF's per-emitter loop (TrailEmitter::Construct) has no stores in the X360.
    // =========================================================================================
    void TrailSystem::Construct(CgsMemory::HeapMalloc* lpHeapMalloc, BrnGraphics::Im3dSkidsRenderer* lpRenderer)
    {
        mFreeEmitters.Construct();
        mnCurrentBuffer = 0;
        mRenderer.Construct(lpHeapMalloc, lpRenderer);
        mbIsReady = false;
    }

    // =========================================================================================
    // TrailSystem::Prepare  @0x8228BE78
    //   Every pool emitter is pushed on the free stack and reset, its current collection being
    //   buffer 0's slot and its old collection buffer 1's (this + 49152 + 512 i); the four
    //   per-type active arrays are cleared; buffer 0 is current.
    // =========================================================================================
    bool TrailSystem::Prepare()
    {
        for (s32 lnIndex = 0; lnIndex < KN_TRAIL_EMITTER_POOL_SIZE; ++lnIndex)
        {
            TrailEmitter* lpEmitter = &maEmitterPool[lnIndex];
            mFreeEmitters.Push(lpEmitter);
            lpEmitter->mrTimeLastSegmentAdded = -1.0f;
            lpEmitter->mx8Flags               = 0;
            lpEmitter->mu8TrailTypeID         = 0;
            lpEmitter->mn8NumSegments         = 0;
            lpEmitter->mpOwner                = 0;
            lpEmitter->mpCurrentSegments      = &maSegments[lnIndex];
            lpEmitter->mpOldSegments          = &maSegments[KN_TRAIL_EMITTER_POOL_SIZE + lnIndex];
        }
        for (s32 lnTrailType = 0; lnTrailType < KI_MAX_NUM_TRAIL_TYPES; ++lnTrailType)
        {
            maActiveEmitters[lnTrailType].Prepare();
        }
        mnCurrentBuffer = 0;
        return true;
    }

    // =========================================================================================
    // TrailSystem::Update -- inlined into ParticleModule::BuildLionVertexBuffers @0x8228AC20
    //   (`*(this+102644) = renderData->mfCurrentTimeStep; *(this+102640) = mfCurrentTime;`
    //    then the renderer's time + view-projection).
    // =========================================================================================
    void TrailSystem::Update(f32 lfCurrentTimeStep, f32 lfCurrentTime, Matrix44::InParam lViewProjMatrix)
    {
        mfCurrentTimeStep = lfCurrentTimeStep;
        mfCurrentTime     = lfCurrentTime;
        mRenderer.Update(lfCurrentTime, lViewProjMatrix);
    }

    // =========================================================================================
    // TrailSystem::AttachTrailEmitter  @0x8228BF50
    //   Pop a free emitter (NULL when the pool is dry), reset it, point it at the type's
    //   colour pair, register it in the type's active array.
    // =========================================================================================
    TrailEmitter* TrailSystem::AttachTrailEmitter(s8 lu8TrailTypeID)
    {
        // The X360 inlines IsEmpty (with the constructed-check at CgsStack.h:177) before Peek/Pop.
        if (mFreeEmitters.IsEmpty())
        {
            return 0;
        }

        TrailEmitter* lpTrailEmitter = mFreeEmitters.Peek();
        mFreeEmitters.Pop();
        CGS_ASSERT(lpTrailEmitter != 0, "lpTrailEmitter != NULL");

        lpTrailEmitter->mrTimeLastSegmentAdded = -1.0f;
        lpTrailEmitter->mx8Flags               = 0;
        lpTrailEmitter->mu8TrailTypeID         = 0;
        lpTrailEmitter->mn8NumSegments         = 0;
        lpTrailEmitter->mpOwner                = 0;
        lpTrailEmitter->mpParams               = &mgaDefaultParams[lu8TrailTypeID];
        maActiveEmitters[lu8TrailTypeID].AddEntry(lpTrailEmitter);
        lpTrailEmitter->mu8TrailTypeID = lu8TrailTypeID;
        return lpTrailEmitter;
    }

    // =========================================================================================
    // TrailSystem::AddTrailSegment  @0x8228C310
    //   r3 = this, r4 = lpTrailEmitterData, r5 = lu8TrailTypeID, v1 = lContactPoint,
    //   v2 = lContactNormal, f1 = lfSkidStrength, f2 = lrCurrentTime.
    // =========================================================================================
    void TrailSystem::AddTrailSegment(TrailEmitterData* lpTrailEmitterData, Vector3 lContactPoint,
                                      Vector3 lContactNormal, s8 lu8TrailTypeID, f32 lfSkidStrength,
                                      f32 lrCurrentTime)
    {
        CGS_ASSERT(lpTrailEmitterData != 0, "lpTrailEmitterData != NULL");

        TrailEmitter* const lpExisting          = lpTrailEmitterData->mpTrailEmitter;
        const bool          lbAlreadyHasEmitter = lpExisting != 0;
        bool                lbChangedTrailType  = true;
        bool                lbTooMuchTimePassedSinceLastTrail = true;
        bool                lbRunOutOfSegments  = true;
        bool                lbNeedANewEmitter   = !lbAlreadyHasEmitter;

        if (lbAlreadyHasEmitter)
        {
            lbChangedTrailType = lu8TrailTypeID != lpExisting->mu8TrailTypeID;
            lbTooMuchTimePassedSinceLastTrail =
                lrCurrentTime > (mfCurrentTimeStep * KF_TRAIL_TIMEOUT_TIMESTEPS + lpTrailEmitterData->mrLastTrailTime);
            lbRunOutOfSegments = lpExisting->mn8NumSegments == KN_MAX_TRAIL_SIZE;
            if (lbChangedTrailType || lbTooMuchTimePassedSinceLastTrail || lbRunOutOfSegments)
            {
                lbNeedANewEmitter = true;
            }
        }

        if (lbNeedANewEmitter)
        {
            TrailEmitter* const lpTrailEmitter = AttachTrailEmitter(lu8TrailTypeID);

            if (lpTrailEmitter != 0 && lbAlreadyHasEmitter
                && lbRunOutOfSegments && !lbChangedTrailType && !lbTooMuchTimePassedSinceLastTrail)
            {
                // The strip simply ran out of segments: seed the new emitter with the old one's
                // last segment so the mark continues unbroken, and flag the continuance so the
                // direction test can use that seed.
                const TrailSegmentCollection* const lpSegmentCollection = lpExisting->mpCurrentSegments;
                const s32                           lnIndex             = lpExisting->mn8NumSegments - 1;

                CGS_ASSERT(lpTrailEmitter->mn8NumSegments == 0, "lpTrailEmitter->mn8NumSegments == 0");
                lpTrailEmitter->mpCurrentSegments->CopySegmentFromCollection(
                    lpTrailEmitter->mn8NumSegments, lpSegmentCollection, lnIndex);
                ++lpTrailEmitter->mn8NumSegments;
                lpTrailEmitter->mx8Flags |= KX_TRAIL_EMITTER_CONTINUANCE;
                lpTrailEmitter->mrTimeLastSegmentAdded = lpSegmentCollection->ReadSegmentTime(lnIndex);
            }

            if (lbAlreadyHasEmitter)
            {
                lpTrailEmitterData->Detatch();
            }
            if (lpTrailEmitter != 0)
            {
                lpTrailEmitterData->mpTrailEmitter = lpTrailEmitter;
                lpTrailEmitter->mpOwner            = lpTrailEmitterData;
            }
        }

        TrailEmitter* const lpEmitter = lpTrailEmitterData->mpTrailEmitter;
        if (lpEmitter != 0)
        {
            lpTrailEmitterData->mrLastTrailTime = lrCurrentTime;
            lpEmitter->AddTrailSegment(lrCurrentTime, lContactPoint, lContactNormal, lfSkidStrength);
        }
    }

    // =========================================================================================
    // TrailSystem::EndOfFrame  @0x8228C058
    //   Flip every emitter's current/old collections, copy the frame's buffer over the other
    //   one and make it current; then release every active emitter idle for > 10 s.
    // =========================================================================================
    void TrailSystem::EndOfFrame()
    {
        for (s32 lnEmitterIndex = 0; lnEmitterIndex < KN_TRAIL_EMITTER_POOL_SIZE; ++lnEmitterIndex)
        {
            maEmitterPool[lnEmitterIndex].PostRender();
        }

        const s32 lnOldBuffer = mnCurrentBuffer;
        memcpy(&maSegments[(1 - lnOldBuffer) * KN_TRAIL_EMITTER_POOL_SIZE],
               &maSegments[lnOldBuffer * KN_TRAIL_EMITTER_POOL_SIZE],
               KN_TRAIL_EMITTER_POOL_SIZE * sizeof(TrailSegmentCollection));
        mnCurrentBuffer = 1 - lnOldBuffer;

        for (s32 lnTrailType = 0; lnTrailType < KI_MAX_NUM_TRAIL_TYPES; ++lnTrailType)
        {
            EmitterArray& lrActive = maActiveEmitters[lnTrailType];
            for (s32 lnIndex = 0; lnIndex < lrActive.GetSize(); ++lnIndex)
            {
                if ((mfCurrentTime - lrActive[lnIndex]->mrTimeLastSegmentAdded) > KF_TRAIL_EMITTER_LIFE)
                {
                    TrailEmitter* const lpEmitter = lrActive[lnIndex];
                    CGS_ASSERT(lpEmitter != 0, "lpEmitter != NULL");

                    if (lpEmitter->mpOwner != 0)
                    {
                        lpEmitter->mpOwner->Detatch();
                    }
                    lpEmitter->mrTimeLastSegmentAdded = -1.0f;
                    lpEmitter->mx8Flags               = 0;
                    lpEmitter->mu8TrailTypeID         = 0;
                    lpEmitter->mn8NumSegments         = 0;
                    lpEmitter->mpOwner                = 0;

                    lrActive.RemoveEntry(lnIndex);
                    mFreeEmitters.Push(lpEmitter);
                    --lnIndex;   // re-examine the entry swapped into this slot
                }
            }
        }
    }

    // =========================================================================================
    // TrailSystem::UpdateTrailType  @0x8228C248
    //   Overwrite a trail type's colour pair (the caller hands the visualfxsurface's
    //   SkidMarkStartColour / SkidMarkEndColour: EffectsModule::Update @0x8229EC28 case 2 and
    //   PostWorldPreparePrepare @0x822902F0 walk the surface list doing exactly this).
    // =========================================================================================
    void TrailSystem::UpdateTrailType(s16 liSkidMarkTypeID, Vector4 lSkidMarkStartColour,
                                      Vector4 lSkidMarkEndColour)
    {
        CGS_ASSERT(liSkidMarkTypeID < KI_MAX_NUM_TRAIL_TYPES, "liSkidMarkTypeID < KI_MAX_NUM_TRAIL_TYPES");

        TrailParams& lrParams = mgaDefaultParams[liSkidMarkTypeID];
        lrParams.mStartColour.red   = lSkidMarkStartColour.x;
        lrParams.mStartColour.green = lSkidMarkStartColour.y;
        lrParams.mStartColour.blue  = lSkidMarkStartColour.z;
        lrParams.mStartColour.alpha = lSkidMarkStartColour.w;
        lrParams.mEndColour.red     = lSkidMarkEndColour.x;
        lrParams.mEndColour.green   = lSkidMarkEndColour.y;
        lrParams.mEndColour.blue    = lSkidMarkEndColour.z;
        lrParams.mEndColour.alpha   = lSkidMarkEndColour.w;
    }

    // =========================================================================================
    // TrailSystem::Render  @0x82295C58
    //   Called by ParticleModule::RenderFullResParticles @0x8229AFD0 when the frame's
    //   ParticleRenderData carries eRenderDataFlagRenderTrails (0x20).
    // =========================================================================================
    void TrailSystem::Render(const f32 lfWhiteLevel)
    {
        if (!mbIsReady)
        {
            return;
        }

        renderengine::Texture* const lpTexture = mTrailTexture;
        mRenderer.BeginRender(lpTexture);

        for (s32 lnTrailType = 0; lnTrailType < KI_MAX_NUM_TRAIL_TYPES; ++lnTrailType)
        {
            const s32 lnEmitterCount = maActiveEmitters[lnTrailType].GetSize();
            if (lnEmitterCount > 0)
            {
                mRenderer.Render(maActiveEmitters[lnTrailType], lnEmitterCount,
                                 &mgaDefaultParams[lnTrailType], static_cast<s8>(lnTrailType),
                                 lfWhiteLevel);
            }
        }

        mRenderer.EndRender();
    }
}
}
