#pragma once

// =============================================================================================
// BrnParticleModuleIO_EventTypes.h
//
// The record types carried by the particle module's inter-thread event queue --
// CgsModule::VariableEventQueue<16384,16>, declared here (as the DWARF does) through
// BrnParticle::InterThreadEventQueue<16384>. The queue is written on the UPDATE thread
// (BrnEffects::EffectsModule::HandleSparkContacts and the debris/junkyard publishers),
// bulk-appended into BrnGame::DispatchThreadInputBuffer by ParticleModule::PreRenderUpdate
// @0x82294760, and drained on the DISPATCH thread by ParticleModule::ProcessEventQueue
// @0x8229C418.
//
// LAYOUTS ARE THE DecFIGS DWARF's (BrnParticleModuleIO_EventTypes.h:48..154), and every one
// of them is cross-checked against a console SIZE IMMEDIATE or a console STRIDE:
//
//   type 0  (no payload)                     ProcessEventQueue case 0: clear all debris buckets
//   type 1  SpawnSparksFromPointEvent        -> HandleSpawnSparksFromPointEvent      @0x82299840
//   type 2  SpawnSparkShowerFromPointEvent   -> HandleSpawnSparkShowerFromPointEvent @0x82299CC8
//   type 3  SpawnSparksAlongLineEvent        -> HandleSpawnSparksAlongLineEvent      @0x8229A138
//   type 4  DebrisBatchSpawnEvent            -> the inline BrnDebrisArray::SpawnDebris loop
//   type 5  FireDebrisBurstEvent             -> HandleFireDebrisBurstEvent           @0x8229A660
//
// ⭐ SpawnSparksAlongLineEvent's size is pinned TWICE, from opposite ends:
//   * the producer bakes it -- EffectsModule::HandleSparkContacts @0x82290A24 is
//         li r6, 0x50 ; li r5, 3 ; addi r4, r1, var_F0 ; AddEventSafe(queue, &rec, 3, 80)
//     and its own stores run var_F0(+0x00) .. var_A8(+0x48), i.e. 0x49 bytes rounded to 0x50;
//   * the consumer reads exactly those offsets -- HandleSpawnSparksAlongLineEvent's
//     lvx128 (r29), lvx128 (r29+0x10), lvx128 (r29+0x20), lfs 0x30/0x34/0x38/0x3C/0x40,
//     lwz 0x44 and lbz 0x48.
//
// ⭐ DebrisBatchSpawnEvent's stride is pinned the same way: PreRenderUpdate's publish computes
//   `count*5 << 4` + 0x10 == count*80 + 16 for the AllocateEventSafe size, and ProcessEventQueue
//   case 4 walks `r26 = event + 0x10` with `(n + n*4) << 4` == n*80 and reads meType at +0x40.
//
// ⚠️ FLAG -- NOT USED BY THIS BUILD YET, AND SAID OUT LOUD: SparkBatchSpawnEvent /
//   SparkSpawnData below are the DWARF's, but NOTHING in the ARTIST image publishes them.
//   PreRenderUpdate's single AllocateEventSafe publishes the record pair at ParticleModule
//   +0x2B7A0 / +0x2B7B0 as type 4 with an 80-byte stride, which is the DEBRIS shape
//   (SparkSpawnData is 49 bytes -> 64 aligned, and could not produce that stride). Either the
//   X360 lays those two pairs out in the other order or it dropped the spark batch in the merge
//   window; this header does not guess -- see the note on the members in ParticleModule.h.
// =============================================================================================

#include "BrnCommonTypes.h"                                  // Vector3 / Vector3Plus / Vector4
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event, VariableEventQueue

#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"  // Native::ESparkArrayID
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"    // Native::EDebrisArrayID

#include "GameSource/AttribSys/Generated/classes/debrisparams.h"  // Attrib::Gen::debrisparams (by value)

namespace BrnParticle
{
    // BrnParticleModuleIO_EventTypes.h:25.
    const s32 KI_PARTICLE_MODULE_INTERTHREAD_COMMAND_QUEUE_MEMSIZE = 16384;

    // BrnParticleModuleIO_EventTypes.h:154 -- `struct InterThreadEventQueue<MEMSIZE> :
    // public VariableEventQueue<MEMSIZE,16>`. The only instantiation in the game is <16384>,
    // and BrnGame::DispatchThreadInputBuffer already types its own copy as
    // CgsModule::VariableEventQueue<16384,16>; the alias keeps the two spellings one type.
    template <s32 MEMSIZE>
    struct InterThreadEventQueue : public CgsModule::VariableEventQueue<MEMSIZE, 16>
    {
    };

    // The event ids ProcessEventQueue @0x8229C418 switches on (its jump table has exactly six
    // cases and a "Unhandled Event" default at ParticleModule.cpp:938).
    enum EParticleEventType : s32
    {
        eParticleEvent_ClearAllDebrisBuckets  = 0,
        eParticleEvent_SpawnSparksFromPoint   = 1,
        eParticleEvent_SpawnSparkShowerFromPoint = 2,
        eParticleEvent_SpawnSparksAlongLine   = 3,
        eParticleEvent_DebrisBatchSpawn       = 4,
        eParticleEvent_FireDebrisBurst        = 5
    };

    // BrnParticleModuleIO_EventTypes.h:48 / :52.
    struct SparkBatchSpawnEvent : public CgsModule::Event
    {
        struct SparkSpawnData
        {
            Vector3                 mvPosition;           // :53
            Vector3                 mvVelocity;           // :54
            f32                     mfSize;               // :55
            f32                     mfSpawnTime;          // :56
            f32                     mfHeightAboveGround;  // :57
            Native::ESparkArrayID   meType;               // :58
            bool                    mbUseCrashBank;       // :59
        };

        u16 mu16SparkCount;                               // :64
    };

    // BrnParticleModuleIO_EventTypes.h:68.
    //   HandleSpawnSparksFromPointEvent @0x82299840 reads: lvx128 (r29+0x00/0x10/0x20),
    //   lwz 0x30, lfs 0x34/0x38/0x3C/0x40/0x44, lbz 0x48.
    struct SpawnSparksFromPointEvent : public CgsModule::Event
    {
        Vector3               mvWorldSpacePoint;    // :71  +0x00
        Vector3               mvVelocity;           // :72  +0x10
        Vector3               mvNormal;             // :73  +0x20
        Native::ESparkArrayID meSparkType;          // :74  +0x30
        f32                   mfCurrentTime;        // :75  +0x34
        f32                   mfNumSparks;          // :76  +0x38
        f32                   mfHeightAboveGround;  // :77  +0x3C
        f32                   mfVelocityInheritMin; // :78  +0x40
        f32                   mfVelocityInheritMax; // :79  +0x44
        bool                  mbIsCrashRelated;     // :80  +0x48
    };

    // BrnParticleModuleIO_EventTypes.h:84.
    struct SpawnSparkShowerFromPointEvent : public CgsModule::Event
    {
        Matrix44Affine        mTransform;                                 // :87  +0x00
        Vector4               mLateralAngleMinMaxForwardAngleMinMax;      // :88  +0x40
        Vector4               mVelocityMinMaxInheritanceMinMax;           // :89  +0x50
        Vector3               mVelocityToInherit;                         // :90  +0x60
        Vector4               mSparkSizeMinMaxSpawnRadiusXSpawnRadiusYZ;  // :91  +0x70
        f32                   mfCurrentTime;                              // :92  +0x80
        f32                   mfGroundPositionY;                          // :93  +0x84
        f32                   mfVelocityScaleSpeedThreshold;              // :94  +0x88
        f32                   mfReflectionAmount;                         // :95  +0x8C
        u32                   muNumToSpawn;                               // :96  +0x90
        Native::ESparkArrayID meSparkType;                                // :97  +0x94
    };

    // BrnParticleModuleIO_EventTypes.h:102 -- THE GRINDING-CONTACT RECORD.
    // Produced by EffectsModule::HandleSparkContacts @0x822906A8 (size immediate 0x50),
    // consumed by ParticleModule::HandleSpawnSparksAlongLineEvent @0x8229A138.
    struct SpawnSparksAlongLineEvent : public CgsModule::Event
    {
        Vector3               mvStartPos;                     // :105 +0x00
        Vector3               mvEndPos;                       // :106 +0x10
        Vector3               mvVelocity;                     // :107 +0x20
        f32                   mfCurrentTime;                  // :108 +0x30
        f32                   mfNumSparks;                    // :109 +0x34
        f32                   mfHeightAboveGroundOfStartPos;  // :110 +0x38
        f32                   mfVelocityInheritanceMin;       // :111 +0x3C
        f32                   mfVelocityInheritanceMax;       // :112 +0x40
        Native::ESparkArrayID meSparkType;                    // :113 +0x44
        bool                  mbIsCrashing;                   // :114 +0x48
    };

    // BrnParticleModuleIO_EventTypes.h:118 / :122.
    struct DebrisBatchSpawnEvent : public CgsModule::Event
    {
        struct DebrisSpawnData
        {
            Vector3Plus            mvPositionPlusSize;                // :123 +0x00
            Vector3Plus            mvVelocityPlusSpawnTime;           // :124 +0x10
            Vector3Plus            mvRotationAxisPlusRotationAmount;  // :125 +0x20
            Vector4                mvColour;                          // :126 +0x30
            Native::EDebrisArrayID meType;                            // :127 +0x40
        };

        u16 mu16DebrisCount;                                          // :133 +0x00
    };

    // BrnParticleModuleIO_EventTypes.h:138.
    struct FireDebrisBurstEvent : public CgsModule::Event
    {
        Vector3                 mSpawnPosition;        // :141
        Vector3                 mvEmitterHalfExtents;  // :142
        Vector3                 mvVelocityToInherit;   // :143
        Vector3                 mCameraPosition;       // :144
        Vector4                 mvCarColour;           // :145
        Attrib::Gen::debrisparams mDebrisParams;       // :146
        f32                     mfCurrentTime;         // :147
        f32                     mfScaleFactor;         // :148
    };

    // The console's own size pins, as static_asserts. Each number is a size immediate or a
    // stride the ARTIST image bakes -- not an inference from the field list.
    //   0x50 : HandleSparkContacts' `li r6, 0x50` at 0x822909D4.
    //   0x50 : ProcessEventQueue case 4's `(n + 4n) << 4` record walk, and PreRenderUpdate's
    //          `count*5 << 4` allocation size.
    static_assert(sizeof(SpawnSparksAlongLineEvent) == 0x50,
                  "SpawnSparksAlongLineEvent must be 80 bytes (AddEventSafe li r6,0x50)");
    static_assert(sizeof(SpawnSparksFromPointEvent) == 0x50,
                  "SpawnSparksFromPointEvent must be 80 bytes (same field tail as the line event)");
    static_assert(sizeof(DebrisBatchSpawnEvent::DebrisSpawnData) == 0x50,
                  "DebrisSpawnData must be 80 bytes (ProcessEventQueue's n*80 record walk)");
}
