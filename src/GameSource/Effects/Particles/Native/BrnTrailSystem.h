#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailSystem.h
//
// BrnParticle::Native::TrailSystem -- the skid / tyre-mark system. A pool of 96
// TrailEmitters, each owning a 16-segment strip (TrailSegmentCollection, double
// buffered: 192 collections), handed out per wheel by EffectsModule::
// HandleWheels @0x82296C80 whenever a wheel's skid factor beats its contact
// surface's SkidMarkThreshold, aged out 10 s after the last segment was laid,
// and drawn by TrailRenderer as strips of BrnGraphics::SkidVertex.
//
// X360 ARTIST bodies (BrnTrailSystem.cpp):
//   TrailSystem::Prepare            @0x8228BE78   ::AttachTrailEmitter @0x8228BF50
//   TrailSystem::AddTrailSegment    @0x8228C310   ::EndOfFrame         @0x8228C058
//   TrailSystem::UpdateTrailType    @0x8228C248   ::Render             @0x82295C58
//   TrailEmitter::AddTrailSegment   @0x8227A9E0
//   EmitterArray::AddEntry @0x82277D50  ::RemoveEntry(int) @0x82277DC8
//   Stack<TrailEmitter*,96>::Push/Pop/Peek own their StackTrailEmitter96_*.cpp TUs.
//   TrailSystem::Construct is inlined into ParticleModule::Prepare @0x8229BEA0,
//   TrailSystem::Update into ParticleModule::BuildLionVertexBuffers @0x8228AC20,
//   TrailEmitterData::Prepare into ActiveRaceCarData::Construct @0x82287E08 and
//   TrailEmitterData::Detatch / TrailEmitter::PostRender into the bodies above.
//
// DWARF AUTHORITY (DecFIGS BrnTrailSystem.h) for every name below; console
// offsets pinned by the asm are given per member. The whole object is
// 102652 bytes on the console and lives at ParticleModule +0x9710.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // Vector3 / Vector4 / Matrix44
#include "GameSource/Effects/Particles/Native/BrnTrailDataStructures.h"  // TrailSegmentCollection
#include "GameSource/Effects/Particles/Native/BrnTrailRender.h"          // TrailRenderer
#include "GameShared/GameClasses/Containers/CgsStack.h"                  // CgsContainers::Stack
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // CgsResource::SafeResourceHandle
#include "rw/core/base/ostypes.h"                                        // RwRGBAReal

namespace renderengine { class Texture; }
namespace CgsMemory    { class HeapMalloc; }
namespace BrnGraphics  { struct Im3dSkidsRenderer; }

namespace BrnParticle
{
namespace Native
{
    // BrnTrailSystem.h:46 (DWARF).
    enum EB4EmitterState
    {
        eEMITTER_NORMAL        = 0,
        eEMITTER_READYTODELETE = 1,
    };

    // BrnTrailSystem.h:61-64 (DWARF).
    const u8  KX_TRAIL_EMITTER_CONTINUANCE = 2;    // kxTrailEmitterContinuance: mx8Flags bit -- this emitter was seeded with its predecessor's last segment
    const u32 KU_TRAIL_BLEND_NORMAL        = 1;    // kuTrailBlendNormal
    const s32 KN_TRAIL_EMITTER_POOL_SIZE   = 96;   // knTrailEmitterPoolSize
    const s32 KI_MAX_NUM_TRAIL_TYPES       = 4;    // KI_MAX_NUM_TRAIL_TYPES

    struct TrailEmitter;

    // ------------------------------------------------------------------------
    // BrnTrailSystem.h:81 -- the per-wheel handle onto a live emitter. One per
    // wheel inside BrnEffects::ActiveRaceCarData (mTrailEmitters[4]).
    // ------------------------------------------------------------------------
    struct TrailEmitterData
    {
        Vector3       mLastPosition;     // +0x00 (:100)
        TrailEmitter* mpTrailEmitter;    // +0x10 (:103)
        f32           mrLastTrailTime;   // +0x14 (:104)  -1.0 == no trail this frame

        // :85 -- inlined into ActiveRaceCarData::Construct @0x82287E08 (`+144 = 0; +148 = -1.0`).
        void Prepare()
        {
            mpTrailEmitter  = 0;
            mrLastTrailTime = -1.0f;
        }
        // :92 -- inlined into TrailSystem::AddTrailSegment / EndOfFrame: unlink both ways.
        void Detatch();
    };

    // ------------------------------------------------------------------------
    // BrnTrailSystem.h:120 -- a fixed-capacity unordered array of the emitters
    // active for one trail type. AddEntry appends; RemoveEntry(int) swap-removes.
    // ------------------------------------------------------------------------
    struct EmitterArray
    {
        // :124 -- inlined into TrailSystem::Prepare @0x8228BE78 (memset 384 + size 0).
        bool Prepare();
        // :132 -- @0x82277D50.
        void AddEntry(TrailEmitter* lpEmitter);
        // :163 -- @0x82277DC8.
        void RemoveEntry(s32 liIndex);
        // :180 / :187 / :193 -- inlined everywhere (EndOfFrame asserts "lnIndex < mnCurrentSize").
        TrailEmitter* operator[](s32 lnIndex);
        s32 GetSize() const { return mnCurrentSize; }
        operator TrailEmitter**() { return mapEmitters; }

    private:
        TrailEmitter* mapEmitters[KN_TRAIL_EMITTER_POOL_SIZE];   // +0x000 (:200)
        s32           mnCurrentSize;                              // +0x180 (:201)
    };

    // BrnTrailSystem.h:213 -- the colour a trail type fades between.
    struct TrailParams
    {
        RwRGBAReal mStartColour;   // +0x00 (:216)
        RwRGBAReal mEndColour;     // +0x10 (:217)
    };

    // ------------------------------------------------------------------------
    // BrnTrailSystem.h:221 -- one live skid mark: up to 16 segments in
    // mpCurrentSegments (mpOldSegments is last frame's copy).
    // Console layout: 24 bytes -- +0 mrTimeLastSegmentAdded, +4 mpParams,
    // +8 mpCurrentSegments, +12 mpOldSegments, +16 mn8NumSegments,
    // +17 mu8TrailTypeID, +18 mx8Flags, +19 mn8TextureIndex, +20 mpOwner.
    // ------------------------------------------------------------------------
    struct TrailEmitter
    {
        f32                     mrTimeLastSegmentAdded;   // (:260)
        TrailParams*            mpParams;                 // (:261)
        TrailSegmentCollection* mpCurrentSegments;        // (:266)
        TrailSegmentCollection* mpOldSegments;            // (:271)
        s8                      mn8NumSegments;           // (:273)
        s8                      mu8TrailTypeID;           // (:274)
        u8                      mx8Flags;                 // (:275)
        s8                      mn8TextureIndex;          // (:276)
        TrailEmitterData*       mpOwner;                  // (:279)

        // :241 -- @0x8227A9E0. Lay one segment at the contact point (lifted by
        // kTrailHeightAdjustment): refused when < 0.3 m from the previous one; MOVES
        // the previous one instead when the direction is within cos 0.9995 of the
        // previous direction; otherwise appends. Returns whether a segment was written.
        bool AddTrailSegment(f32 lfCurrentTime, Vector3 lContactPoint, Vector3 lContactNormal,
                             f32 lfSkidStrength);

        // :244 -- inlined into TrailSystem::EndOfFrame @0x8228C058 (the 96-iteration
        // swap of +8 / +12): flip the current and old segment collections.
        void PostRender()
        {
            TrailSegmentCollection* lpOld = mpOldSegments;
            mpOldSegments     = mpCurrentSegments;
            mpCurrentSegments = lpOld;
        }

        // :253
        s8 GetTrailTypeID() const { return mu8TrailTypeID; }
    };

    // ------------------------------------------------------------------------
    // BrnTrailSystem.h:293 -- the system. Console offsets in brackets.
    // ------------------------------------------------------------------------
    struct TrailSystem
    {
        // :339 -- the per-type start/end colours (X360 .data flt_82CDAE78: every type
        // starts at {0,0,0,0.77} and ends at {0,0,0,0}); UpdateTrailType overwrites a
        // type's pair from the surface's visualfxsurface SkidMarkStart/EndColour.
        static TrailParams mgaDefaultParams[KI_MAX_NUM_TRAIL_TYPES];

        // The X360 ParticleModule ctor @0x827E2218 stamps the Stack's unconstructed
        // sentinel at +0x22190 (== this Stack's miLength) -- the inlined
        // Stack<TrailEmitter*,96> constructor. Reproduced here since the tree's Stack
        // has no constructor of its own.
        TrailSystem() { mFreeEmitters.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED; }

        // :297 -- inlined into ParticleModule::Prepare @0x8229BEA0.
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, BrnGraphics::Im3dSkidsRenderer* lpRenderer);
        // :303 -- @0x8228BE78.
        bool Prepare();
        // :312 -- inlined into ParticleModule::BuildLionVertexBuffers @0x8228AC20.
        void Update(f32 lfCurrentTimeStep, f32 lfCurrentTime, Matrix44::InParam lViewProjMatrix);
        // :315 -- @0x82295C58.
        void Render(const f32 lfWhiteLevel);
        // :324 -- @0x8228C310. Called per wheel by EffectsModule::HandleWheels.
        void AddTrailSegment(TrailEmitterData* lpTrailEmitterData, Vector3 lContactPoint,
                             Vector3 lContactNormal, s8 lu8TrailTypeID, f32 lfSkidStrength,
                             f32 lrCurrentTime);
        // :328 -- @0x8228BF50.
        TrailEmitter* AttachTrailEmitter(s8 lu8TrailTypeID);
        // :333 -- @0x8228C058.
        void EndOfFrame();
        // :337 -- @0x8228C248.
        void UpdateTrailType(s16 liSkidMarkTypeID, Vector4 lSkidMarkStartColour, Vector4 lSkidMarkEndColour);

        bool IsReady() const { return mbIsReady; }

    private:
        TrailSegmentCollection                          maSegments[2 * KN_TRAIL_EMITTER_POOL_SIZE]; // [+0]      (:346) two 96-collection buffers
        TrailEmitter                                    maEmitterPool[KN_TRAIL_EMITTER_POOL_SIZE];  // [+98304]  (:348)
        CgsContainers::Stack<TrailEmitter*, KN_TRAIL_EMITTER_POOL_SIZE> mFreeEmitters;             // [+100608] (:350)
        CgsResource::SafeResourceHandle<renderengine::Texture> mTrailTexture;                       // [+100996] (:352)
        EmitterArray                                    maActiveEmitters[KI_MAX_NUM_TRAIL_TYPES];   // [+101004] (:354)
        s32                                             mnCurrentBuffer;                            // [+102556] (:355)
        TrailRenderer                                   mRenderer;                                  // [+102560] (:357)
        f32                                             mfCurrentTime;                              // [+102640] (:358)
        f32                                             mfCurrentTimeStep;                          // [+102644] (:359)
        bool                                            mbIsReady;                                  // [+102648] (:360)
    };
}
}
