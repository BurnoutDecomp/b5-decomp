#pragma once

// ===========================================================================
// CgsSceneManager::SceneSweeper
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.{h,cpp}
//
// The sweep-and-prune broadphase the OverlapGenerationModule drives. The
// OverlapGenerationModule embeds it BY VALUE (mSweeper, X360 +0x230).
//
// ===========================================================================
// LAYOUT -- RECOVERED IN FULL, 2026-08-18 (wave Q5, cluster B1 keystone).
//
// Until this rewrite the class was a 24-byte debug slice + a queue + one
// `u8 maIntervalListState[902336 - ...]` opaque tail, and every body that
// touched it (ForceNoPadding) poked a console byte offset through a u8* view of
// `this`. That is the exact failure mode the project's X360-console-values rule
// exists to stop, and it made Prepare/Clear/SortLists/SweepLists/BuildCollidingPairs/
// Update unwritable. The member set below is the DecFIGS DWARF's
// (references/DecFIGS/dwarfdump/.../CgsSceneSweeper.h:203-248, declaration order
// preserved) and EVERY offset in the right-hand column is ATTESTED by the X360 asm,
// not inferred:
//
//  console  member                                    attested by
//  -------  ----------------------------------------  ---------------------------------
//        0  mDebugComponent            (see FLAG)     OverlapGenerationModule::Construct
//                                                     @0x828D0460 (self-ptr +0x0C, draw
//                                                     distance 50.0f +0x10, flags +0x14/+0x15)
//    0x018  mOverlappingPairQueue                     SweepLists 0x828C2110 (`addi r30,r31,0x18`)
//                                                     ... its miLength at +0x20 (Clear/SweepLists
//                                                     `stw r,0x20(r31)`); capacity 131072 is
//                                                     (0x80024-0x24)/4 == 0x80000/4
//   0x80024  maObjectToIntervalMapMem[5051]           Prepare 0x828C2088-0x828C2094 (the
//                                                     shared 3rd arg to IntervalList::Prepare);
//                                                     Clear memset(+0x80024, 0, 0x4EEC==20204)
//   0x84F10  maDynamicIntervalMem[4001]               Prepare arg2 #1 (`addis 8; addi 0x4F10`),
//                                                     max 0xFA1==4001; Clear memset 0x17718
//   0x9C628  maStaticIntervalMem[3]                   Prepare arg2 #3 (`addis 0xA; addi -0x39D8`),
//                                                     max 3; Clear 9 x std == 72 bytes
//   0x9C670  maInactiveIntervalMem[6101]              Prepare arg2 #2 (`addis 0xA; addi -0x3990`),
//                                                     max 0x17D5==6101; Clear memset 0x23BF8
//   0xC02E8  mauDynamicStackIndexMem[2000]            SweepLists 0x828C2104 (`addis 0xC; addi 0x268`)
//   0xC1290  maDynamicStackIntervalMem[2000]          SweepLists 0x828C2130 (`addis 0xC; addi 0x1210`),
//                                                     with muMaxLen 0x7D0==2000
//   0xC8F90  mauSecondaryStackIndexMem[3050]          SweepLists 0x828C2148 (`addis 0xD; addi -0x70F0`)
//   0xCA6B0  maSecondaryStackIntervalMem[3050]        SweepLists 0x828C2154 (`addis 0xD; addi -0x5910`),
//                                                     with muMaxLen 0xBEA==3050
//   0xD6590  mDynamicIntervalList                     Prepare/Clear/SortLists/SweepLists/Update
//   0xD65A0  mInactiveIntervalList                    ditto  (stride 16 => console
//   0xD65B0  mStaticIntervalList                      ditto   sizeof(IntervalList)==16)
//   0xD65C0  maCollidingPairs[1024]                   BuildCollidingPairs `&this[3*i + 219504]`
//                                                     == 878016 + 12*i (console stride 12)
//   0xD95C0  maObjectData[5051]                       Clear (5051 x `stb 3`), Update, AddObject
//   0xD9A40  mUseInternalCollision                    Clear/Update memset(+0xD9A40, 0, 0x278==632)
//   0xD9CB8  mCollidingBodies                         Clear memset; AddObject's duplicate-add
//                                                     tripwire + SetBit (`this + 223998` dwords)
//   0xD9F30  mabMovedThisFrame[5051]                  Clear/Update memset(..., 0x13BB==5051)
//   0xDC230  mabForceNoPadding                        ForceNoPadding @0x828B0578 (`this + 225420`
//                                                     dwords), Clear memset 632
//   0xDC4A8  mpaVolumeInstanceCullingGroup            Prepare `stwx r29,r31,0xDC4A8`
//   0xDC4AC  mpCullingTable                           Prepare `stwx r28,r31,0xDC4AC`; Clear reads it
//   0xDC4B0  muNumObjects                             Clear `stwx r30,r31,0xDC4B0`; AddObject ++
//   0xDC4B4  muNumCollidingPairs                      Clear `stwx r30,r31,0xDC4B4`; BuildCollidingPairs
//   0xDC4B8  mbSortStaticObjects                      Clear `stbx`; SortLists gate for the STATIC list
//   0xDC4B9  mbSortFrozenObjects                      Clear `stbx`; SortLists gate for the INACTIVE list
//   0xDC4BC  miTracerID                               (DWARF :248; no attested access -- see FLAG)
//   0xDC6F0  <end>                                    == 902336, the OverlapGenerationModule
//                                                     member delta +0x230..+0xDC6F0
//
// The arithmetic closes EXACTLY: every neighbouring pair above differs by that member's
// console size (20204 / 96024 / 72 / 146424 / 4000+8pad / 32000 / 6100+12pad / 48800 /
// 3*16 / 12288 / 5051+5pad / 632 / 632 / 5051+1pad / 632 / 4 / 4 / 4 / 4 / 1 / 1 / 2pad+4),
// and the last one lands on 902336. That closure is the proof the member ORDER is right.
//
// ⚠️ THESE CONSOLE OFFSETS DO NOT SURVIVE THE x64 COMPILE and are comments only. Three
// members widen on the host -- mpaVolumeInstanceCullingGroup, mpCullingTable, and the three
// IntervalLists (two pointers each, so host sizeof(IntervalList)==24, not 16) -- and the
// EventQueue base widens too. Everything is addressed BY NAME; the static_asserts at the
// bottom of CgsSceneSweeper.cpp pin only the POINTER-INVARIANT facts (element sizes and the
// intra-array deltas that contain no pointer), never a whole-object size.
//
// FLAG (park, precise reason) -- mDebugComponent is a sized opaque slice, not the real
// CgsSceneManager::SceneSweeperDebugComponent, for TWO independent reasons, both measured:
//   1. its real header does not compile: CgsSceneSweeperDebugComponent.h names `rw::RGBA`
//      (RenderIntervalList's by-value colour) which has no reconstructed home anywhere in
//      the tree, and `float32_t` which is not a project-wide typedef. (Measured:
//      scratchpad/waveQ5/probe_sweeper/p3.cpp.)
//   2. even with those fixed, embedding it BY VALUE makes SceneSweeper's TU emit the
//      component's vtable, and SceneSweeperDebugComponent::{Construct, Destruct, OnActivate,
//      RenderWorld, RenderIntervalList} have NO body anywhere in b5-decomp/src -- an
//      instant LNK2019 for the exe link that runs right after this wave.
// The sweeper's own reconstructed bodies never touch it (only the console's Construct does,
// to Register() it), so the slice costs nothing today. See sweeper.owner.md.
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3 (UpdateObject position lane), Vector4
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                   // CgsContainers::BitArray
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"         // VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsInterval.h"      // Interval, OverlapPairQueue
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalList.h"  // IntervalList, ObjectToIntervalMap
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsIntervalStack.h" // IntervalStack, IntervalStackEntry
#include "vendor/renderware/collision/BitTable.hpp"                          // rw::BitTable (culling table)
#include "rw/physics/rigidbody.h"                                            // rw::physics::BodyState

namespace rw
{
namespace collision
{
    // Forward declaration, documented exception: AddObject/UpdateObject take the world box by
    // POINTER only, and vendor/renderware/collision/AABBox.hpp includes the SDKs/EATech
    // rw::math::vpu vector headers, which redefine rw::math::vpu::Vector3 against the vendor
    // definition BrnCommonTypes.h already supplies in this closure. (Measured: including it
    // beside BrnCommonTypes.h is a >100-error cascade -- scratchpad/waveQ5/probe_sweeper/
    // probe_incl.cpp.) Bodies that dereference the box include AABBox.hpp in their own .cpp.
    class AABBox;
}
}

namespace CgsSceneManager
{
    // Pointer-only in this header (Update / BuildCollidingPairs take it by const pointer);
    // its real home is GameShared/GameClasses/SceneManager/CgsEntityManager.h, which the
    // bodies include.
    class EntityManager;

    // CgsSceneSweeper.h:48 (DWARF). One broadphase output pair: the two object indices,
    // the pair's squared centre separation, and the culling-table verdict. 12 bytes on the
    // console AND on the host (u16,u16,f32,bool -> 9 bytes padded to 12); BuildCollidingPairs
    // @0x828C2190 attests the stride directly (`&this[3*i + 219504]` == 878016 + 12*i).
    struct CollidingPair
    {
        // CgsSceneSweeper.h:54.
        void Set(u16 lu16ObjectA, u16 lu16ObjectB, f32 lfPadding, bool lbCull)
        {
            mu16ObjectA = lu16ObjectA;
            mu16ObjectB = lu16ObjectB;
            mfPadding   = lfPadding;
            mbCull      = lbCull;
        }

        u16  GetObjectA() const { return mu16ObjectA; }  // :58
        u16  GetObjectB() const { return mu16ObjectB; }  // :62
        f32  GetPadding() const { return mfPadding;   }  // :66
        bool GetCull()    const { return mbCull;      }  // :70

    private:
        u16 mu16ObjectA;  // +0x00 :74  (BuildCollidingPairs stores min(A,B) here)
        u16 mu16ObjectB;  // +0x02 :75  (and max(A,B) here)
        f32 mfPadding;    // +0x04 :76
        bool mbCull;      // +0x08 :77
    };

    class SceneSweeper
    {
    public:
        // ---- capacities (DWARF CgsSceneSweeper.h:92-104; every value re-attested by asm) ----
        static const u32 KU_MAX_NUM_COLLIDING_PAIRS   = 1024;  // :92  BuildCollidingPairs `>= 0x400`
        static const u32 KU_MAX_NUM_OBJECTS           = 5051;  // :96  AddObject `>= 0x13BB`
        static const s32 KI_MAX_NUM_DYNAMIC_INTERVALS = 4001;  // :99  Prepare `li r6,0xFA1`
        static const s32 KI_MAX_NUM_STATIC_INTERVALS  = 3;     // :100 Prepare `li r6,3`
        static const s32 KI_MAX_NUM_INACTIVE_INTERVALS= 6101;  // :101 Prepare `li r6,0x17D5`
        static const s32 KI_DYNAMIC_STACK_SIZE        = 2000;  // :103 SweepLists `li r10,0x7D0`
        static const s32 KI_SECONDARY_STACK_SIZE      = 3050;  // :104 SweepLists `li r10,0xBEA`

        // Volume-instance ceiling ForceNoPadding range-checks. Not in the DWARF dump, but the
        // X360 assert bakes both the name and the home: "luObjectIndex <
        // (uint32_t)KI_MAX_NUM_VOLUME_INSTANCES", CgsSceneSweeper.h:328, value 0x13B8 == 5048.
        static const s32 KI_MAX_NUM_VOLUME_INSTANCES = 5048;

        // CgsSceneSweeper.h:182 (DWARF). maObjectData[i].mu8BodyState's value set.
        enum eObjectBodyState
        {
            E_DYNAMIC_BODY          = 0,
            E_INACTIVE_BODY         = 1,
            E_STATIC_BODY           = 2,
            E_INVALID_BODY          = 3,
            E_NUM_OBJECT_BODY_STATES = 4
        };

        // CgsSceneSweeper.h:193 (DWARF). Per-object bookkeeping; one byte each.
        struct IntervalObjectData
        {
            // :196. Clear @0x828B57A0 fills the whole 5051-entry array with 3 == E_INVALID_BODY
            // (0x828B58A8 `li r9,3` / 0x828B58B8 `stb r9,0(r11)` x 0x13BB), which is this
            // Construct de-inlined.
            void Construct() { mu8BodyState = E_INVALID_BODY; }

            u8 mu8BodyState;  // :199
        };

        typedef CgsContainers::BitArray<KU_MAX_NUM_OBJECTS> ObjectBitArray;

        // ---- public surface (DWARF :109-171) ------------------------------------------
        void Construct();                                               // :109
        void Destruct();                                                // :112

        // :117 @0x828C2000. The DWARF spells the second parameter `BitTable *`; the tree's
        // rw::BitTable (vendor/renderware/collision/BitTable.hpp) models the handle class and
        // its backing store separately, and the pointer the console stores + walks is the
        // BACKING STORE (Clear reads muWordCount at +8 and the packed words at +12;
        // BuildCollidingPairs reads muHeight at +4). CgsCullingGroupManager.h already spells
        // that `rw::BitTable::Storage*` -- matched here rather than forked.
        bool Prepare(u8* lpaVolumeInstanceCullingGroup, rw::BitTable::Storage* lpCullingTable);

        bool Release();                                                 // :120
        void Update(const EntityManager* lpEntityManager);              // :126 @0x828D5A88

        // :252 @0x828B57A0. ⚠️ DWARF-vs-X360 DIVERGENCE, resolved in favour of ARTIST (rung 1):
        // the DecFIGS DWARF lists Clear under `private:`, but the X360 build calls it from a
        // NON-MEMBER twice -- OverlapGenerationModule::Construct @0x828D0460 (`bl
        // CgsSceneManager__SceneSweeper__Clear` on this+560) and OverlapGenerationModule::
        // Release @0x828CB798 (same, in its case-0/1 arm). Both are direct calls, not a
        // public wrapper being inlined. So in the ARTIST build Clear is reachable from the
        // owning module and is declared public here. (Merge-window delta; see AGENTS.md BUILD
        // LINEAGE.) The other five private members below have no such counter-evidence.
        void Clear();

        // :133 @0x828B5958. Register a swept body. ARGUMENT ORDER IS ASM-ATTESTED, not
        // guessed: OverlapGenerationModule::ProcessAddBodyQueue @0x828C1D18 loads
        //   r4 = lwz  0x24(event)  -> luObjectIndex
        //   r5 =      event        -> the world AABBox (the queue element's own +0x00)
        //   r6 = lwz  0x28(event)  -> leBodyState   (tested as bits 4/2/1 == ACTIVE/FROZEN/STATIC)
        //   r7 = ld   0x30(event)  -> lVolumeInstanceId (SIXTY-FOUR bits; the same word whose
        //                             high-dword high byte the caller then uses as the culling
        //                             group, i.e. VolumeInstanceId::GetEntityIDOwner()).
        void AddObject(u32 luObjectIndex, const rw::collision::AABBox* lpAABBox,
                       rw::physics::BodyState leBodyState, VolumeInstanceId lVolumeInstanceId);

        void RemoveObject(u32 luObjectIndex);                           // :137 @0x828B5E48

        // :144 @0x828B6160. ProcessUpdateBodyQueue @0x828C1DE8 loads
        //   r4 = lwz    0x30(event) -> luObjectIndex
        //   r5 =        event       -> the world AABBox
        //   v1 = lvx128 0x20(event) -> lvPosition   (a VECTOR arg: it takes a VR, and the next
        //                              GPR slot is NOT consumed by it -- the PPC vector-arg
        //                              analogue of the float-skips-a-GPR rule)
        //   r6 = ld     0x38(event) -> lVolumeInstanceId
        void UpdateObject(u32 luObjectIndex, const rw::collision::AABBox* lpAABBox,
                          Vector3 lvPosition, VolumeInstanceId lVolumeInstanceId);

        u32 GetNumCollidingPairs() const { return muNumCollidingPairs; }         // :147
        const CollidingPair* GetCollidingPair(u32 luIndex) const                 // :152
        {
            return &maCollidingPairs[luIndex];
        }
        void RemoveCollidingPair(u32 luIndex);                                   // :157
        void SetToUseInternalCollision(u32 luObjectIndex);                       // :161
        bool UseInternalCollision(u32 luObjectIndex) const                       // :166
        {
            return mUseInternalCollision.IsBitSet(luObjectIndex);
        }
        void ForceNoPadding(u32 luObjectIndex);                                  // :171 @0x828B0578

        // ---- perf-monitor ids (DWARF :174-176; Update @0x828D5A88 reads them as
        // dword_82F33F54 / _58 / _5C and brackets each phase with PerfMonCpu Start/StopMonitor
        // when the id is > -1). Statics, defined in CgsSceneSweeper.cpp.
        static s32 siUpdate_SortListsPerfMon;            // :174
        static s32 siUpdate_SweepListsPerfMon;           // :175
        static s32 siUpdate_BuildCollidingPairsPerfMon;  // :176

    private:
        void SortLists();                                                // :255 @0x828D5980
        void SweepLists();                                               // :258 @0x828C20F0
        void BuildCollidingPairs(const EntityManager* lpEntityManager);  // :263 @0x828C2190
        bool CanCollide(u32 luObjectA, u32 luObjectB);                   // :268
        bool ShouldBeCulled(u32 luObjectA, u32 luObjectB);               // :273

        // NOT a console member. Compile-time layout tripwire: a private static so it can take
        // offsetof of the private members below. Its body (CgsSceneSweeper.cpp) is nothing but
        // static_asserts on the POINTER-INVARIANT intra-array deltas the console's Prepare /
        // SweepLists / Clear / BuildCollidingPairs arguments encode. It is never called.
        static void AssertLayout();

        // ---- members, DWARF declaration order :203-248 (console offsets in the LAYOUT table) ----

        // :203  SceneSweeperDebugComponent mDebugComponent -- sized opaque slice, see the FLAG
        // in the banner. 0x18 bytes is the console span (its last member, mbRenderInactiveBoxes,
        // sits at +0x15 per OverlapGenerationModule::Construct, padded to 4).
        u8 maDebugComponentSlice[0x18];                                       // 0x00000

        OverlapPairQueue mOverlappingPairQueue;                               // 0x00018 :207

        ObjectToIntervalMap maObjectToIntervalMapMem[KU_MAX_NUM_OBJECTS];     // 0x80024 :210
        Interval maDynamicIntervalMem[KI_MAX_NUM_DYNAMIC_INTERVALS];          // 0x84F10 :211
        Interval maStaticIntervalMem[KI_MAX_NUM_STATIC_INTERVALS];            // 0x9C628 :212
        Interval maInactiveIntervalMem[KI_MAX_NUM_INACTIVE_INTERVALS];        // 0x9C670 :213

        u16 mauDynamicStackIndexMem[KI_DYNAMIC_STACK_SIZE];                   // 0xC02E8 :217
        IntervalStackEntry maDynamicStackIntervalMem[KI_DYNAMIC_STACK_SIZE];  // 0xC1290 :218
        u16 mauSecondaryStackIndexMem[KI_SECONDARY_STACK_SIZE];               // 0xC8F90 :219
        IntervalStackEntry maSecondaryStackIntervalMem[KI_SECONDARY_STACK_SIZE]; // 0xCA6B0 :220

        IntervalList mDynamicIntervalList;                                    // 0xD6590 :222
        IntervalList mInactiveIntervalList;                                   // 0xD65A0 :223
        IntervalList mStaticIntervalList;                                     // 0xD65B0 :224

        CollidingPair maCollidingPairs[KU_MAX_NUM_COLLIDING_PAIRS];           // 0xD65C0 :226
        IntervalObjectData maObjectData[KU_MAX_NUM_OBJECTS];                  // 0xD95C0 :228

        ObjectBitArray mUseInternalCollision;                                 // 0xD9A40 :230
        ObjectBitArray mCollidingBodies;                                      // 0xD9CB8 :232
        bool mabMovedThisFrame[KU_MAX_NUM_OBJECTS];                           // 0xD9F30 :235
        ObjectBitArray mabForceNoPadding;                                     // 0xDC230 :237

        u8*                    mpaVolumeInstanceCullingGroup;                 // 0xDC4A8 :239
        rw::BitTable::Storage* mpCullingTable;                                // 0xDC4AC :240

        u32 muNumObjects;                                                     // 0xDC4B0 :242
        u32 muNumCollidingPairs;                                              // 0xDC4B4 :243

        bool mbSortStaticObjects;                                             // 0xDC4B8 :245
        bool mbSortFrozenObjects;                                             // 0xDC4B9 :246

        // :248. FLAG: no X360 access is attested anywhere in the sweeper's nine bodies (the
        // debug-tracer id the DebugComponent would own). Declared because the DWARF declares
        // it and dropping it would leave the member set incomplete; nothing reads it yet.
        s32 miTracerID;                                                       // 0xDC4BC
    };
}
