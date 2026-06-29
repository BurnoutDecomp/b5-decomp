#ifndef GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H
#define GAMESOURCE_EFFECTS_PARTICLES_PARTICLEMODULE_H

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h" // CgsModule::ModuleSingleBuffered (the base; vtable + the two RWMutexes the ctor constructs inline via its DataBuffers)

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

        // muFlags bits (DWARF "ePPEFlag*").
        static const u16 EPPE_FLAG_IN_USE           = 1;
        static const u16 EPPE_FLAG_ENABLED          = 2;
        static const u16 EPPE_FLAG_CHANGED          = 4;
        static const u16 EPPE_FLAG_CREATE           = 8;
        static const u16 EPPE_FLAG_KILL             = 16;
        static const u16 EPPE_FLAG_OVERRIDE_VELOCITY= 32;

        const rw::math::vpu::Matrix44Affine& GetTransform() const { return mTransform; }

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

    // The particle / LION effects module -- a full CgsModule::ModuleSingleBuffered.
    struct ParticleModule : public CgsModule::ModuleSingleBuffered
    {
        // GetLionEffect asserts (handle & 0x7F) < this before indexing maPlayingEffects.
        // X360 immediate 0x80 (GetLionEffect 0x82278380 asm: cmplwi r30,0x80).
        static const u32 KU_MAX_PLAYING_EFFECTS = 128;

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
        ContainedInterface mInterfaceC;                // +0x9210 (37392) off_820CEBE4
        u8 maPadIfaceCToD[0x9274 - (0x9210 + sizeof(ContainedInterface))]; // -> +0x9274
        ContainedInterface mInterfaceD;                // +0x9274 (37492) off_820CEBE8
        u8 maPadIfaceDToE[0x92E0 - (0x9274 + sizeof(ContainedInterface))]; // -> +0x92E0
        ContainedInterface mInterfaceE;                // +0x92E0 (37600) off_820CFA1C

        // Gap to the +0x22190 sentinel word.
        u8 maPadIfaceETo22190[0x22190 - (0x92E0 + sizeof(ContainedInterface))]; // -> +0x22190

        // +0x22190 (139664): a 0x7FFFFFFF (s32 max) sentinel the ctor stamps.
        s32 miSentinel22190;                           // +0x22190 == 0x7FFFFFFF

        // Gap to the first job/sentinel cluster at +0x249C4.
        u8 maPad22194To249C4[0x249C4 - (0x22190 + 4)]; // -> +0x249C4

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
