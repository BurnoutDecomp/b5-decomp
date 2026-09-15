// ============================================================================
// CgsSceneManager::SceneSweeper -- broadphase body-home TU.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SceneSweeper::Prepare()        @ 0x828C2000  (60 insns)
//   SceneSweeper::Clear()          @ 0x828B57A0  (110 insns)
//   SceneSweeper::ForceNoPadding() @ 0x828B0578  (71 insns)
//   SceneSweeper::Construct()      -- no out-of-line X360 symbol; the console carries it
//                                     inlined in OverlapGenerationModule::Construct
//                                     @0x828D0460 (de-inlined here per the project's
//                                     inlining-reversal rule).
//
// 2026-08-18 (wave Q5, cluster B1 keystone). Until today this file held ONE function
// (ForceNoPadding) and addressed the sweeper through `(u8*)this + 0xDC230` -- a raw
// X360 byte offset that does not survive the x64 compile. CgsSceneSweeper.h now carries
// the full DWARF member set with every offset asm-attested, so every access below is by
// NAME and the offset hack is gone.
//
// The remaining five sweeper bodies (SortLists / SweepLists / BuildCollidingPairs /
// Update, and the Add/Remove/UpdateObject mutators) are round-2 partfile work; their
// declaration surface is complete in the header and the plan is in
// scratchpad/waveQ5/sweeper.owner.md.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // std::memset (the console's own memset calls)

// includes folded in from the CgsSceneSweeper_w*.cpp partfiles (2026-09-15)
#include "rw/math/vpu/vector3_operation.h"                // Min / Max / operator+ (vminfp/vmaxfp/vaddfp)
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"          // EntityManager::GetVolumeInstance, VolumeInstance
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::Start/StopMonitor

namespace CgsSceneManager
{
    // ---- perf-monitor ids (DWARF CgsSceneSweeper.h:174-176) --------------------------
    //
    // These are the console's dword_82F33F54 / _58 / _5C. Each has EXACTLY FOUR xrefs in the
    // whole image, and they name both ends of the contract:
    //
    //   READER   SceneSweeper::Update @0x828D5A88 -- 0x828D5AC8 + 0x828D5AE0 (_54),
    //            0x828D5AF4 + 0x828D5B0C (_58), 0x828D5B20 + 0x828D5B3C (_5C). Each phase is
    //            bracketed `lwz; cmpwi cr6,r3,-1; ble skip; bl PerfMonCpu::Start/StopMonitor`
    //            -- i.e. run the monitor iff the id is > -1.
    //   REGISTRAR ⭐ SceneManagerModule::Construct @0x828D09A0 -- NOT the SceneSweeperDebug-
    //            Component. 0x828D0C24 reads _54, `cmpwi cr6,r11,-1; bne` SKIPS registration
    //            unless the id is still -1, then AddMonitor("      SortLists", 0x10, ...) and
    //            0x828D0C4C stores the handle back; 0x828D0C58-0C70 asserts
    //            "SceneSweeper::siUpdate_SortListsPerfMon >= 0" (CgsSceneManagerModule.cpp
    //            line 0x88 == 136). The same shape at 0x828D0C78/0CA0/0CB8 for _58
    //            ("      SweepLists", line 137) and 0x828D0CCC/0CF4/0D0C for _5C
    //            ("      BuildColPairs", line 138). Nothing else in .text writes them.
    //
    // ⚠️ THE INITIALISER -1 IS FAITHFUL, NOT A DIVERGENCE. Re-measured 2026-08-18 by headless
    // idat on a private copy of the .i64: all three live in .data (0x82CD0000..0x832BE424) and
    // the image dwords at 0x82F33F54/58/5C are 0xFFFFFFFF 0xFFFFFFFF 0xFFFFFFFF == -1, -1, -1.
    // (An earlier note in this file and in scratchpad/waveQ5/sweeper.owner.md claimed the .data
    // image was 0 and that -1 was therefore a deliberate PC divergence; both were wrong.) The
    // registrar's own guard proves it independently: it registers ONLY when the id equals -1,
    // so a zero image would (a) mean the monitors never register at all and (b) make Update's
    // `> -1` guard pass with id 0 and time whatever monitor 0 happens to be. -1 is the source
    // initialiser the console compiled.
    //
    // ⚠️ HAND-OFF (a file this owner does not own -- reported, not edited): these three statics
    // are FORKED by a MOUNTED TU. CgsSceneManagerModule.cpp:62-64 declares file-local
    // `static s32 siSceneSweeper_{SortLists,SweepLists,BuildCollidingPairs}PerfMon = -1;` and
    // :222-227 registers into THOSE -- while carrying the console's own
    // "SceneSweeper::siUpdate_*PerfMon >= 0" assert strings, which is the tell that they are
    // this class's statics. Until :62-64 are deleted and :222-227 point at
    // SceneSweeper::siUpdate_SortListsPerfMon / _SweepListsPerfMon / _BuildCollidingPairsPerfMon,
    // the three below stay -1 forever and round 2's Update brackets are permanently dead while
    // the console's are live. Not a link duplicate (different symbols), so no gate to retire.
    s32 SceneSweeper::siUpdate_SortListsPerfMon           = -1;
    s32 SceneSweeper::siUpdate_SweepListsPerfMon          = -1;
    s32 SceneSweeper::siUpdate_BuildCollidingPairsPerfMon = -1;

    // ================================================================================
    // SceneSweeper::Construct  (DWARF CgsSceneSweeper.h:109)
    //
    // No out-of-line X360 symbol. OverlapGenerationModule::Construct @0x828D0460 does the
    // whole thing on the embedded sweeper (this+560 == the module's mSweeper):
    //     EventQueue<OverlappingIntervalPair,131072>::Construct(this+584)  ; sweeper +0x18
    //     *(this+572) = this+560   ; mDebugComponent.mpSceneSweeper       = &sweeper (+0x0C)
    //     *(this+576) = 50.0f      ; mDebugComponent.mfDrawDistance                  (+0x10)
    //     *(this+580) = 0          ; mDebugComponent.mbRenderDynamicBoxes            (+0x14)
    //     *(this+581) = 0          ; mDebugComponent.mbRenderInactiveBoxes           (+0x15)
    //     CgsDev::DebugComponent::Register(this+560)
    //     SceneSweeper::Clear(this+560)
    // The four debug-component stores and the Register() call are NOT reproduced: the
    // component is the parked opaque slice (header FLAG -- its real type has no compiling
    // header and its virtuals have no bodies, so touching it here would be an invention
    // AND a link hole). They are debug-overlay-only side effects; no gameplay path reads
    // them. Everything else lands.
    void SceneSweeper::Construct()
    {
        mOverlappingPairQueue.Construct();
        mDebugComponent.Construct(this);
        mDebugComponent.Register();
        Clear();
    }

    // ================================================================================
    // SceneSweeper::Prepare  @ 0x828C2000  (CgsSceneSweeper.cpp:113-121)
    //
    // Binds the scene's per-volume-instance culling-group array + the culling BitTable, wipes
    // the sweeper state, then hands each interval list its backing storage.
    //
    // ⭐ ALL THREE LISTS SHARE ONE ObjectToIntervalMap ARRAY. The console passes the SAME
    // third argument (`r30` == this+0x80024 == maObjectToIntervalMapMem) to all three
    // IntervalList::Prepare calls -- 0x828C2088/0x828C208C set it once and 0x828C20A4 /
    // 0x828C20B8 / 0x828C20D4 re-pass it. That is what lets SceneSweeper::Update move an
    // object's two intervals from the dynamic list to the inactive list without touching the
    // map: the object's slot indices are list-agnostic because only one list ever owns a
    // given object at a time.
    //
    // Returns true unconditionally (`li r3,1` at 0x828C20E4); the two NULL checks are
    // non-gating tripwires and the three IntervalList::Prepare results are discarded.
    bool SceneSweeper::Prepare(u8* lpaVolumeInstanceCullingGroup,
                               rw::BitTable::Storage* lpCullingTable)
    {
        CGS_ASSERT(lpaVolumeInstanceCullingGroup != NULL,
                   "lpaVolumeInstanceCullingGroup != NULL");           // CgsSceneSweeper.cpp:115
        CGS_ASSERT(lpCullingTable != NULL, "lpCullingTable != NULL");  // CgsSceneSweeper.cpp:116

        mpaVolumeInstanceCullingGroup = lpaVolumeInstanceCullingGroup;  // stwx r29,r31,0xDC4A8
        mpCullingTable                = lpCullingTable;                 // stwx r28,r31,0xDC4AC

        Clear();

        mDynamicIntervalList.Prepare(maDynamicIntervalMem, maObjectToIntervalMapMem,
                                     KI_MAX_NUM_DYNAMIC_INTERVALS);     // li r6,0xFA1
        mInactiveIntervalList.Prepare(maInactiveIntervalMem, maObjectToIntervalMapMem,
                                      KI_MAX_NUM_INACTIVE_INTERVALS);   // li r6,0x17D5
        mStaticIntervalList.Prepare(maStaticIntervalMem, maObjectToIntervalMapMem,
                                    KI_MAX_NUM_STATIC_INTERVALS);       // li r6,3
        return true;
    }

    // ================================================================================
    // SceneSweeper::Clear  @ 0x828B57A0
    //
    // Full state wipe. Store-for-store against the asm; the console's memset sizes are quoted
    // as provenance only -- every call below sizes itself with sizeof(array), which is the
    // SAME number on the host for all of them (Interval 24 B, ObjectToIntervalMap 4 B,
    // IntervalStackEntry 16 B, IntervalObjectData 1 B, BitArray<5051> 632 B all survive the
    // x64 compile; the static_asserts at the bottom of this file pin that).
    //
    //   0x828B57DC..5804  three IntervalList::Clear()s  (dynamic, static, inactive)
    //   0x828B5808        memset(mUseInternalCollision, 0, 0x278 == 632)
    //   0x828B5824        muNumCollidingPairs = 0                      (stwx .., 0xDC4B4)
    //   0x828B5828        memset(mCollidingBodies,   0, 632)
    //   0x828B583C        memset(mabForceNoPadding,  0, 632)
    //   0x828B5858        muNumObjects = 0                             (stwx .., 0xDC4B0)
    //   0x828B585C        memset(mabMovedThisFrame,  0, 0x13BB == 5051)
    //   0x828B5860..589C  the culling-table word wipe (guarded on mpCullingTable != NULL and
    //                     on muWordCount != 0; walks muWordCount words from +0x0C)
    //   0x828B58A4        mOverlappingPairQueue length = 0             (stw r30,0x20(r31))
    //   0x828B58A8..58C0  5051 x `stb 3` == IntervalObjectData::Construct (E_INVALID_BODY)
    //   0x828B58D4        memset(maObjectToIntervalMapMem, 0, 0x4EEC  == 20204)
    //   0x828B58EC        memset(maDynamicIntervalMem,     0, 0x17718 == 96024)
    //   0x828B5904..590C  9 x `std 0` at maStaticIntervalMem           == 72 bytes
    //   0x828B5924        memset(maInactiveIntervalMem,    0, 0x23BF8 == 146424)
    //   0x828B5938/593C   mbSortStaticObjects = mbSortFrozenObjects = false
    //
    // ⚠️ NOT touched by Clear (deliberate, reproduced): the four interval-stack scratch arrays
    // (they are rebuilt from scratch by SweepLists every frame), muMaxNumIntervals inside each
    // list, mpaVolumeInstanceCullingGroup / mpCullingTable (Prepare owns those and calls Clear
    // AFTER storing them), and miTracerID.
    void SceneSweeper::Clear()
    {
        mDynamicIntervalList.Clear();
        mStaticIntervalList.Clear();
        mInactiveIntervalList.Clear();

        mUseInternalCollision.UnSetAll();
        muNumCollidingPairs = 0;
        mCollidingBodies.UnSetAll();
        mabForceNoPadding.UnSetAll();
        muNumObjects = 0;

        std::memset(mabMovedThisFrame, 0, sizeof(mabMovedThisFrame));

        // The culling table's packed bit words. muWordCount == 0 skips the loop, which is why
        // the console tests it separately from the NULL check.
        if (mpCullingTable != NULL)
        {
            for (u32 luWord = 0; luWord < mpCullingTable->muWordCount; ++luWord)
            {
                mpCullingTable->maBits[luWord] = 0;
            }
        }

        mOverlappingPairQueue.Clear();

        for (u32 luObject = 0; luObject < KU_MAX_NUM_OBJECTS; ++luObject)
        {
            maObjectData[luObject].Construct();
        }

        std::memset(maObjectToIntervalMapMem, 0, sizeof(maObjectToIntervalMapMem));
        std::memset(maDynamicIntervalMem,     0, sizeof(maDynamicIntervalMem));
        std::memset(maStaticIntervalMem,      0, sizeof(maStaticIntervalMem));
        std::memset(maInactiveIntervalMem,    0, sizeof(maInactiveIntervalMem));

        mbSortStaticObjects = false;
        mbSortFrozenObjects = false;
    }

    // ================================================================================
    // SceneSweeper::ForceNoPadding  @ 0x828B0578
    //
    // Marks one volume instance as "no padding" in the sweeper's force-no-padding bit set.
    // Called by OverlapGenerationModule::ProcessForceNoPaddingQueue per queued request; not
    // executed before the boot-trace milestone.
    //
    // 2026-08-18: rewritten off the raw offset hack. The body used to reach the bit set as
    // `*(BitArray<5051>*)((u8*)this + 0xDC230)`; that console byte offset is now the named
    // member mabForceNoPadding. Offset re-derived and re-attested in the header's layout
    // table: the console forms the bit-array base at 0x828B05B4 `addis r27,r31,0xE` +
    // 0x828B05BC `addi r27,r27,-0x3DD0` == this + 0xE0000 - 0x3DD0 == this + 0xDC230
    // (== `this + 225420` dwords == 901680 bytes), which is exactly where the member set puts
    // it. (0x828B0598, cited here on first landing, is `lis r11, aDP4B5MainBurno_430@ha` --
    // the assert's file-name string, not the base.)
    //
    // Two guard asserts, faithful to the X360:
    //   1. the KI_MAX_NUM_VOLUME_INSTANCES (0x13B8 == 5048) range check, string + home baked
    //      by the console (CgsSceneSweeper.h:328);
    //   2. BitArray<5051>::SetBit's own bounds assert (CgsBitArray.h:222). The X360 streams a
    //      dynamic "Index: <i>, Number of bits: 5051" message; per the project convention for
    //      this exact streamed BitArray:222 assert (see the committed sibling
    //      CgsGui::EventInterpreterModule.cpp) it is reduced to the static string.
    void SceneSweeper::ForceNoPadding(u32 luObjectIndex)
    {
        CGS_ASSERT(luObjectIndex < static_cast<u32>(KI_MAX_NUM_VOLUME_INSTANCES),
                   "luObjectIndex < (uint32_t)KI_MAX_NUM_VOLUME_INSTANCES");

        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "Index out of range\n");

        mabForceNoPadding.SetBit(luObjectIndex);
    }

    // ================================================================================
    // LAYOUT PINS.
    //
    // Only POINTER-INVARIANT facts are asserted: element sizes, and the byte deltas between
    // members with no pointer (and no pointer-bearing member) in between. The whole-object
    // size and every absolute console offset are DELIBERATELY not asserted -- five members
    // widen on LLP64 (mpaVolumeInstanceCullingGroup, mpCullingTable, and each of the three
    // IntervalLists, two pointers apiece) and so does the EventQueue base, so the console's
    // 902336 (== 0xDC4C0, sweeper-relative) is not a host number. Asserting it would be the
    // exact bug this rewrite removed.
    namespace
    {
        // Element strides the console's memsets / index arithmetic depend on.
        static_assert(sizeof(Interval) == 24,
                      "Interval must be 24 bytes (SortLists divides by 24; Clear memsets 4001*24)");
        static_assert(sizeof(ObjectToIntervalMap) == 4,
                      "ObjectToIntervalMap must be 4 bytes (Clear memsets 5051*4 == 20204)");
        static_assert(sizeof(IntervalStackEntry) == 16,
                      "IntervalStackEntry must be 16 bytes (IntervalStack::Push strides by 16)");
        static_assert(sizeof(CollidingPair) == 12,
                      "CollidingPair must be 12 bytes (BuildCollidingPairs strides by 12)");
        static_assert(sizeof(SceneSweeper::IntervalObjectData) == 1,
                      "IntervalObjectData must be 1 byte (Clear stb's 5051 of them)");
        static_assert(sizeof(SceneSweeper::ObjectBitArray) == 632,
                      "BitArray<5051> must be 632 bytes (Clear memsets 0x278 three times)");

    }

    // The pointer-free spans between consecutive members -- the deltas the console's Prepare /
    // SweepLists / Clear / BuildCollidingPairs arguments encode, re-derived on the HOST with
    // offsetof. Each right-hand comment is the console pair the delta reproduces. Every one of
    // these spans is pointer-free, so the host number must equal the console number; the three
    // spans that ARE NOT (the three IntervalLists, and the two trailing pointers) are excluded
    // by construction, and the whole-object size is deliberately never asserted.
    //
    // ⚠️ SweepLists' two IntervalStack::Init calls are the reason the two "index" -> "interval"
    // stack deltas below carry padding: mauDynamicStackIndexMem ends at an 8-mod-16 address on
    // the console (0xC0268 + 4000 == 0xC1208) and the 16-byte-aligned IntervalStackEntry array
    // starts 8 bytes later at 0xC1210; the secondary pair pads by 12 (0xC8F10 + 6100 ==
    // 0xCA6E4 -> 0xCA6F0). The host alignment rules reproduce both, which is exactly what
    // these asserts check.
    void SceneSweeper::AssertLayout()
    {
        static_assert(offsetof(SceneSweeper, maDynamicIntervalMem)
                      - offsetof(SceneSweeper, maObjectToIntervalMapMem) == 20204,
                      "maObjectToIntervalMapMem span (0x80024 -> 0x84F10, 5051*4)");
        static_assert(offsetof(SceneSweeper, maStaticIntervalMem)
                      - offsetof(SceneSweeper, maDynamicIntervalMem) == 96024,
                      "maDynamicIntervalMem span (0x84F10 -> 0x9C628, 4001*24)");
        static_assert(offsetof(SceneSweeper, maInactiveIntervalMem)
                      - offsetof(SceneSweeper, maStaticIntervalMem) == 72,
                      "maStaticIntervalMem span (0x9C628 -> 0x9C670, 3*24)");
        static_assert(offsetof(SceneSweeper, mauDynamicStackIndexMem)
                      - offsetof(SceneSweeper, maInactiveIntervalMem) == 146424,
                      "maInactiveIntervalMem span (0x9C670 -> 0xC0268, 6101*24)");
        // ⚠️ The two u16-index -> IntervalStackEntry spans are the ONLY two that are NOT
        // reproducible as a console number: the entry array is 16-aligned, and the amount of
        // alignment padding depends on where the index array happens to START -- which the
        // widened members ahead of it move. (Console: 0xC0268 + 4000 == 0xC1208 -> 8 bytes of
        // pad -> 0xC1210; and 0xC8F10 + 6100 == 0xCA6E4 -> 12 bytes -> 0xCA6F0.) Asserting the
        // console's 4008 / 6112 here is exactly the console-value-on-the-host bug; what IS
        // invariant is that the entry array follows the index array immediately, separated by
        // nothing but alignment padding.
        static_assert(offsetof(SceneSweeper, maDynamicStackIntervalMem)
                      - offsetof(SceneSweeper, mauDynamicStackIndexMem) >= KI_DYNAMIC_STACK_SIZE * 2
                      && offsetof(SceneSweeper, maDynamicStackIntervalMem)
                      - offsetof(SceneSweeper, mauDynamicStackIndexMem) < KI_DYNAMIC_STACK_SIZE * 2 + 16,
                      "maDynamicStackIntervalMem must directly follow mauDynamicStackIndexMem (pad only)");
        static_assert(offsetof(SceneSweeper, mauSecondaryStackIndexMem)
                      - offsetof(SceneSweeper, maDynamicStackIntervalMem) == 32000,
                      "maDynamicStackIntervalMem span (0xC1210 -> 0xC8F10, 2000*16)");
        static_assert(offsetof(SceneSweeper, maSecondaryStackIntervalMem)
                      - offsetof(SceneSweeper, mauSecondaryStackIndexMem) >= KI_SECONDARY_STACK_SIZE * 2
                      && offsetof(SceneSweeper, maSecondaryStackIntervalMem)
                      - offsetof(SceneSweeper, mauSecondaryStackIndexMem) < KI_SECONDARY_STACK_SIZE * 2 + 16,
                      "maSecondaryStackIntervalMem must directly follow mauSecondaryStackIndexMem (pad only)");
        static_assert(offsetof(SceneSweeper, mDynamicIntervalList)
                      - offsetof(SceneSweeper, maSecondaryStackIntervalMem) == 48800,
                      "maSecondaryStackIntervalMem span (0xCA6F0 -> 0xD6590, 3050*16)");

        // The three lists are consecutive: console stride 16, host stride sizeof(IntervalList)
        // (24 on LLP64 -- two widened pointers). Pin the STRIDE RELATION, never the 16.
        static_assert(offsetof(SceneSweeper, mInactiveIntervalList)
                      - offsetof(SceneSweeper, mDynamicIntervalList) == sizeof(IntervalList),
                      "mDynamicIntervalList -> mInactiveIntervalList must be one IntervalList");
        static_assert(offsetof(SceneSweeper, mStaticIntervalList)
                      - offsetof(SceneSweeper, mInactiveIntervalList) == sizeof(IntervalList),
                      "mInactiveIntervalList -> mStaticIntervalList must be one IntervalList");

        static_assert(offsetof(SceneSweeper, maObjectData)
                      - offsetof(SceneSweeper, maCollidingPairs) == 12288,
                      "maCollidingPairs span (0xD65C0 -> 0xD95C0, 1024*12)");
        static_assert(offsetof(SceneSweeper, mUseInternalCollision)
                      - offsetof(SceneSweeper, maObjectData) == 5056,
                      "maObjectData span (0xD95C0 -> 0xDA980, 5051 + 5 pad)");
        static_assert(offsetof(SceneSweeper, mCollidingBodies)
                      - offsetof(SceneSweeper, mUseInternalCollision) == 632,
                      "mUseInternalCollision span (0xDA980 -> 0xDABF8)");
        static_assert(offsetof(SceneSweeper, mabMovedThisFrame)
                      - offsetof(SceneSweeper, mCollidingBodies) == 632,
                      "mCollidingBodies span (0xDABF8 -> 0xDAE70)");
        static_assert(offsetof(SceneSweeper, mabForceNoPadding)
                      - offsetof(SceneSweeper, mabMovedThisFrame) == 5056,
                      "mabMovedThisFrame span (0xDAE70 -> 0xDC230, 5051 + 5 pad)");
        static_assert(offsetof(SceneSweeper, mpaVolumeInstanceCullingGroup)
                      - offsetof(SceneSweeper, mabForceNoPadding) == 632,
                      "mabForceNoPadding span (0xDC230 -> 0xDC4A8)");
    }
}

// ============================================================================
// FOLDED FROM CgsSceneSweeper_wQ5_01.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// CgsSceneManager::SceneSweeper::AddObject
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.cpp
//   Partfile: CgsSceneSweeper_wQ5_01.cpp   (wave Q5 round 2, cluster "sweeper (i)")
//
// X360: @ 0x828B5958, 316 instructions, store-for-store.
//
// This is the sweeper's ENTRY DOOR. Everything the broadphase later does -- SortLists,
// SweepIntervals, SweepAgainstList, BuildCollidingPairs -- operates on the endpoint
// records this function manufactures, so nothing collides with anything until a body
// gets in here. The console reaches it from
// OverlapGenerationModule::ProcessAddBodyQueue @0x828C1D18, one call per queued
// InAddBodyEvent.
//
// LEDGER NOTE (measured, gotcha 10): progress/ledger marks this `reviewed` with its TU
// set to the `CgsStrStream.h` catch-all bucket. There was NO body anywhere in the tree.
// ============================================================================


namespace CgsSceneManager
{
namespace
{
    // ------------------------------------------------------------------------
    // rw::collision::AABBox is only FORWARD-DECLARED in CgsSceneSweeper.h, deliberately:
    // its real home (vendor/renderware/collision/AABBox.hpp) pulls the SDKs/EATech
    // rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD via
    // BrnCommonTypes.h, and the two cannot coexist -- MEASURED here, not inherited:
    // scratchpad/waveQ5/probe_sweep1/p_aabb.cpp includes exactly this header plus
    // AABBox.hpp and dies with `C2011: "rw::math::vpu::Vector3": struct redefinition`
    // (SDKs/EATech/include/rw/math/vpu/vector3.h:26 vs
    // vendor/renderware/include/rw/math/vpu/types.h:24) followed by a >100-error cascade.
    // That double definition is a PRE-EXISTING FORK owned by neither this cluster nor the
    // sweeper keystone; it is REPORTED, not worked around (see sweep1.owner.md).
    //
    // What this body needs from the box is only its byte image: two 16-byte corner rows,
    // mMin @+0x00 and mMax @+0x10 (the console reads them as `lvx128 v, r0, box` and
    // `lvx128 v, r0, box+0x10` -- 0x828B5B3C/0x828B5B48 and again at 0x828B5BD0/0x828B5BD8
    // and 0x828B5C5C/0x828B5C64). That image is PINNED by the already-mounted layout oracle
    // GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp (bat line 1748),
    // which CAN include AABBox.hpp and static_asserts sizeof==32 / offsetof(mMin)==0 /
    // offsetof(mMax)==16 -- so a change to AABBox breaks THAT gate rather than this reader.
    // Same solution, same wording, as the two committed precedents that hit the identical
    // wall: CgsVolumeManager.cpp:54-77 and PropManager_wQ4_03.cpp:289-301.
    // ------------------------------------------------------------------------
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;   // AABBox +0x00
        Vector3 mMax;   // AABBox +0x10
    };
    static_assert(sizeof(AABBoxRows) == 32, "AABBox image is two 16-byte rows");
}

    // ------------------------------------------------------------------------
    // CgsSceneSweeper.cpp:270-306 @ 0x828B5958
    //
    // Registers one swept body. The three-way dispatch on the RenderWare body state is the
    // whole point of the function -- it decides WHICH of the sweeper's three interval lists
    // owns the object, and therefore which sweep leg will ever see it:
    //
    //   leBodyState & rw::physics::ACTIVE_BODY (4)  -> mDynamicIntervalList,  E_DYNAMIC_BODY
    //     (`rlwinm r11,r14,0,29,29` @0x828B5CE8 -- PPC bit 29 == mask 0x4)
    //   leBodyState & rw::physics::FROZEN_BODY (2)  -> mInactiveIntervalList, E_INACTIVE_BODY
    //     (`rlwinm r11,r14,0,30,30` @0x828B5D14 -- mask 0x2) ... and mbSortFrozenObjects = true
    //     (`lis 0xD ; ori 0xC4B9 ; stbx` @0x828B5D38-0x828B5D44 == this+0xDC4B8+1)
    //   otherwise                                   -> assert STATIC_BODY (1) and RETURN
    //     (`clrlwi r11,r14,31` @0x828B5E18 -- mask 0x1)
    //
    // ⚠️ FAITHFUL ODDITY -- REPRODUCED, NOT "FIXED". A STATIC_BODY is added to NO list, gets
    // NO maObjectData state (it stays E_INVALID_BODY), does NOT get its mCollidingBodies bit
    // and does NOT bump muNumObjects. The console's static arm is `assert; return` and
    // nothing else -- 0x828B5E18..0x828B5E44 contains exactly the assert block and the
    // epilogue. mStaticIntervalList is bound by Prepare and then swept by nobody (SweepLists
    // @0x828C20F0 touches only the dynamic and inactive lists). Do not "complete" this arm.
    //
    // ⭐ WHICH BIT ARRAY. The duplicate-add test and the tail SetBit both address
    // `this + 0xDABF8 + 8*(idx>>6)` (read: 0x828B5A68 `addis r11,r11,2 ; addi r11,r11,-0x4A81
    // ; slwi 3 ; ldx r11,r11,r15`; write: 0x828B5D48 `addis r24,r15,0xE ; addi r24,r24,-0x5408`
    // then `ldx/or/stdx` at 0x828B5DF8-0x828B5E00). 0xDABF8 is mCollidingBodies -- the SECOND
    // of the two BitArray<5051> members -- once maObjectData's 5051-byte span and
    // mUseInternalCollision's 632 are walked forward from the asm-attested maObjectData base
    // 0xD95C0 (`add r11,r21,r15 ; addis r18,r11,0xE ; addi r18,r18,-0x6A40` @0x828B5B04).
    // It is NOT mUseInternalCollision, and it is NOT mabForceNoPadding (0xDC230, attested
    // separately by ForceNoPadding @0x828B0578). See sweep1.owner.md s5 for the three header
    // COMMENT offsets this measurement corrects; the member ORDER and every host-side
    // static_assert span in CgsSceneSweeper.cpp are unaffected, and this body addresses
    // everything by name.
    //
    // The 64-bit-word bit math (`srwi r11,r21,6` field, `clrldi r10,r21,58` bit) is exactly
    // CgsContainers::BitArray<5051>'s inlined IsBitSet/SetBit, which is why the two range
    // tripwires below bake CgsBitArray.h rather than CgsSceneSweeper.cpp.
    //
    // ASSERTS -- lines and messages recovered in full from a private .i64 copy (IDA's inline
    // listing truncates at 39 chars), file strings dumped byte-for-byte:
    //   CgsSceneSweeper.cpp:274  "luObjectIndex < KU_MAX_NUM_OBJECTS"
    //   CgsBitArray.h:203        streamed "invalid index : " << idx << " < " << 5051
    //                            (IsBitSet's own bounds check; the console CSEs it into the
    //                             SAME `blt` as :274 because the condition is identical --
    //                             0x828B59A8 skips both blocks)
    //   CgsSceneSweeper.cpp:275  streamed "object being added to the scene sweeper twice ID="
    //                            << lVolumeInstanceId
    //   CgsSceneSweeper.cpp:276  "maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY"
    //   CgsSceneSweeper.cpp:278/:279/:280  streamed "Bad AABBox " << lVolumeInstanceId,
    //                            one per axis, tested `vcmpgefp max >= min` on lanes
    //                            0 / 2 / 1 -- i.e. X, then Z, then Y, in that order
    //   CgsSceneSweeper.cpp:304  "(leBodyState & rw::physics::STATIC_BODY) != 0"
    //   CgsBitArray.h:222        streamed "Index: " << idx << ", Number of bits: " << 5051
    //                            (SetBit's own bounds check)
    // The four streamed messages are reduced to their literal prefix, the tree's standing
    // convention for CgsDev StrStream asserts (CgsVolumeManager.cpp, CgsTriangleCacheManager_
    // Events.cpp), except the CgsBitArray.h:222 one which is spelled the way this same class
    // already spells it in ForceNoPadding (CgsSceneSweeper.cpp:200).
    //
    // ⚠️ lVolumeInstanceId IS CONSUMED ONLY BY THOSE STREAMED MESSAGES on the console -- its
    // four uses are all `mr r4,r20 ; bl sub_82203EE8` feeding the StrStream (0x828B5AE0,
    // 0x828B5BAC, 0x828B5C38, 0x828B5CC4). It reaches no list, no map and no member. The
    // parameter is kept (the DWARF declares it and a future StrStream restoration needs it);
    // it is genuinely unreferenced in the reduced form, which is not a defect.
    // ------------------------------------------------------------------------
    void SceneSweeper::AddObject(u32 luObjectIndex, const rw::collision::AABBox* lpAABBox,
                                 rw::physics::BodyState leBodyState,
                                 VolumeInstanceId lVolumeInstanceId)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS,
                   "luObjectIndex < KU_MAX_NUM_OBJECTS");                    // :274
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");  // CgsBitArray.h:203

        CGS_ASSERT(!mCollidingBodies.IsBitSet(luObjectIndex),
                   "object being added to the scene sweeper twice ID=");     // :275

        CGS_ASSERT(maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY,
                   "maObjectData[luObjectIndex].mu8BodyState == E_INVALID_BODY");  // :276

        // The world box, read as its two corner rows (see the AABBoxRows note above).
        const AABBoxRows& lrBox = *reinterpret_cast<const AABBoxRows*>(lpAABBox);

        CGS_ASSERT(lrBox.mMax.x >= lrBox.mMin.x, "Bad AABBox ");   // :278  (lane 0)
        CGS_ASSERT(lrBox.mMax.z >= lrBox.mMin.z, "Bad AABBox ");   // :279  (lane 2)
        CGS_ASSERT(lrBox.mMax.y >= lrBox.mMin.y, "Bad AABBox ");   // :280  (lane 1)

        if ((leBodyState & rw::physics::ACTIVE_BODY) != 0)
        {
            mDynamicIntervalList.AddObject(static_cast<u16>(luObjectIndex),
                                           lrBox.mMin, lrBox.mMax);
            maObjectData[luObjectIndex].mu8BodyState = E_DYNAMIC_BODY;
        }
        else if ((leBodyState & rw::physics::FROZEN_BODY) != 0)
        {
            mInactiveIntervalList.AddObject(static_cast<u16>(luObjectIndex),
                                            lrBox.mMin, lrBox.mMax);
            maObjectData[luObjectIndex].mu8BodyState = E_INACTIVE_BODY;

            // The inactive list is only re-sorted on demand; adding to it dirties it.
            mbSortFrozenObjects = true;
        }
        else
        {
            CGS_ASSERT((leBodyState & rw::physics::STATIC_BODY) != 0,
                       "(leBodyState & rw::physics::STATIC_BODY) != 0");     // :304
            return;   // faithful: a static body joins no list and is not counted
        }

        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "Index out of range\n");  // CgsBitArray.h:222
        mCollidingBodies.SetBit(luObjectIndex);
        ++muNumObjects;
    }
}

// ============================================================================
// FOLDED FROM CgsSceneSweeper_wQ5_02.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// CgsSceneManager::SceneSweeper -- round-2 partfile (ii): the two per-frame body
// mutators the broadphase drives every tick.
//
// Home of record: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.cpp
// (the console bakes THAT path into every assert in both bodies). This partfile is a
// wave-Q5 ownership split only -- the class, the member set and the layout pins all live
// in CgsSceneSweeper.{h,cpp}; nothing here re-declares anything.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   SceneSweeper::UpdateObject @ 0x828B6160  (318 insns)
//       ⚠️ ABSENT FROM progress/identity.json AND FROM THE PER-ADDRESS EXPORT. Recovered by
//       targeted headless IDA on a private .i64 copy; the dump this body was written from is
//       scratchpad/waveQ5/q5_sweeper_holes.json (entry "0x828b6160").
//   SceneSweeper::RemoveObject @ 0x828B5E48  (197 insns)
//       .ida-exports/BURNOUT_X360_ARTIST.XEX/0x828B5E48.json
//
// Callers: OverlapGenerationModule::ProcessUpdateBodyQueue @0x828C1DE8 and
// ProcessRemoveBodyQueue -- i.e. these two run once per queued body event per frame, and
// UpdateObject is the leg that keeps a MOVING car in the dynamic interval list (a body that
// stops being updated is auto-frozen into the inactive list by SceneSweeper::Update, which is
// how a parked smash gate ends up in the list the car is swept against).
//
// ---------------------------------------------------------------------------------------
// ⭐ THE THIRD PARAMETER OF UpdateObject IS A **PADDING** VECTOR, NOT A POSITION -- and the
// DecFIGS DWARF now settles it by NAME. The header currently spells it `lvPosition` (and the
// producer event field is called mvPosition); both names are DEFECTS, reported by the
// cluster-B1 keystone. FOUR independent witnesses:
//   * DWARF (rung 2, declaration shape -- dwarfdump/.../CgsSceneSweeper.cpp:152) declares
//         SceneSweeper::UpdateObject(uint32_t luObjectIndex, const AABBox* lpBox,
//                                    const rw::math::vpu::Vector3 lPadding,
//                                    VolumeInstanceId lVolumeInstanceID)
//     -- the original source's own parameter names;
//   * the X360 body never uses the vector as a point -- it is a per-lane clamp on the box
//     inflation (`vminfp128` against {-0.1,-0.1,-0.1,0}, `vmaxfp128` against {0.1,0.1,0.1,0});
//   * the body's own assert message prints it as " Padding: [" (0x828B647C aPadding_1);
//   * the producer OverlapGenerationIO::InputBuffer::UpdateBody @0x828BA430 names it lvPadding.
// This definition therefore uses the DWARF's names (lpBox / lPadding / lVolumeInstanceID),
// exactly as the sibling bodies in CgsIntervalList.cpp and CgsVolumeManager.cpp use theirs. A
// definition may name a parameter differently from its declaration; the types and order are
// identical, which is what the ABI and the caller see. The header rename is the fixer's.
// ---------------------------------------------------------------------------------------
// ⚠️ FAITHFUL ODDITY IN UpdateObject -- REPRODUCED, DO NOT "FIX": the final else arm asserts
// `leBodyState == E_STATIC_BODY` and then does the static-list update ANYWAY, so an object in
// E_INVALID_BODY (the state Clear() seeds and RemoveObject() restores) is pushed through
// mStaticIntervalList.UpdateObject with a stale object->interval mapping. The console does
// exactly that: 0x828B65E4 `cmpwi 2 / beq loc_828B6618` falls THROUGH the assert block into
// the same loc_828B6618 the E_STATIC_BODY case branches to.
// ============================================================================


// rw::collision::AABBox is only NAMED by the declaration this file defines; the body needs
// its two corner ROWS. Its real home, vendor/renderware/collision/AABBox.hpp, pulls the SDK
// rw::math::vpu::Vector3 CLASS into a TU that already has the vendor 4-lane POD through
// CgsSceneSweeper.h -> BrnCommonTypes.h, and the two cannot coexist (measured:
// scratchpad/waveQ5/probe_sweeper/probe_incl.cpp, and the same clash is solved the same way in
// GameShared/GameClasses/SceneManager/CgsVolumeManager.cpp:54-77 and
// GameSource/Physics/PropManager/PropManager_wQ4_03.cpp:289-301). What this TU needs is only
// AABBox's byte image: two 16-byte float4 rows, mMin @+0x00 and mMax @+0x10 -- which is
// exactly what the console reads (`lvx128 v0,r0,r29` and `lvx128 v12,r0,r29+0x10`).
// That image is PINNED by the mounted oracle TU
// GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp, which CAN include
// AABBox.hpp and static_asserts sizeof==32 / offsetof(mMin)==0 / offsetof(mMax)==16, so a
// change to AABBox breaks that gate rather than this reader.
namespace rw { namespace collision { class AABBox; } }

namespace CgsSceneManager
{

namespace
{
    // The byte image of rw::collision::AABBox over the vendor POD Vector3 this TU speaks:
    // AABBoxRows, defined ONCE in this TU's anonymous namespace (the wave-Q5 section formerly
    // _wQ5_01); the identical definition this section carried went with the partfile fold
    // (issue #20).

    // The two per-lane clamps UpdateObject applies to the caller's padding vector before it
    // inflates the box. Both are .rdata singles the body loads and splats into lanes 0..2 with
    // lane 3 stored as literal zero (0x828B633C/0x828B6374 `stw r21`):
    //   flt_8200D530 == -0.1f  (0x828B6348)  -- value corroborated by three independent users
    //                                           (0x825C7568 `if ( v53 < -0.1 )`, 0x827249F8,
    //                                           0x8253E200), not inferred from this body.
    //   flt_82004014 == +0.1f  (0x828B6364)  -- likewise (0x82715A18, 0x828ABF48).
    // Semantics: the box is inflated by AT LEAST 0.1 in every direction, and by more where the
    // caller's padding asks for more.
    const f32 KF_MIN_PADDING = -0.1f;
    const f32 KF_MAX_PADDING =  0.1f;
}

// ================================================================================
// SceneSweeper::UpdateObject  @ 0x828B6160   (CgsSceneSweeper.cpp:388-431)
//
// Re-fits one already-registered body's interval pair to a fresh world box, inflated by the
// caller's padding vector (clamped to at least 0.1 outward per lane), and migrates the body
// back into the DYNAMIC list if the auto-freeze had moved it to the inactive one. Every arm
// stamps mabMovedThisFrame, which is what stops SceneSweeper::Update re-freezing the body at
// the end of the same frame.
//
// Store-for-store map (console -> here):
//   0x828B61A8  cmplwi 0x13BB / blt          -> the merged bound check; the compiler folded the
//               body's OWN `luObjectIndex < KU_MAX_NUM_OBJECTS` assert (:392, streamed
//               "Object index is invalid: " << i << "\n") and BitArray<5051>::IsBitSet's
//               inlined bounds assert (CgsBitArray.h:203, streamed "invalid index : " << i
//               << " < " << 5051) into ONE branch, because both test the same predicate.
//   0x828B62D4  the inlined IsBitSet + assert :393 "mCollidingBodies.IsBitSet( luObjectIndex )"
//   0x828B6330  lvx128 box.mMin / box.mMax, the two literal clamp vectors, vminfp128/vmaxfp128
//               against the padding, two vaddfp -> the padded min/max
//   0x828B63B0  THREE per-lane `vcmpgtfp.` tests, lanes 0/1/2 == x/y/z, each one SKIPPING the
//               assert when that lane is ordered -- i.e. ONE assert whose condition is the OR
//               of the three lanes (:403). NaN polarity is host `<`/`>`: an unordered lane
//               leaves the CR6 all-true bit clear and falls through to the next lane.
//   0x828B6404  the assert's StrStream message, which reports the RAW box and the RAW padding
//               (not the padded values it just tested):
//                 "\n Bad AABBox \n" " Padding: [" px ", " py ", " pz "] \n "
//                 " Min: [" minx ", " miny ", " minz "] \n"
//                 " Max: [" maxx ", " maxy ", " maxz "] \n" " ID: " <VolumeInstanceId> "\n"
//               CGS_ASSERT in this tree takes a plain string (CgsAssert.h), so as in every
//               other reconstructed TU the message stops at the leading static string and
//               lVolumeInstanceID -- whose ONLY use in this body is that stream -- is unread.
//   0x828B656C  lbz maObjectData[i].mu8BodyState + the three-way dispatch
//   0x828B6634  stbx 1 -> mabMovedThisFrame[i]   (base 0xDAE70, all three arms)
//
// DWARF CORROBORATION (dwarfdump/.../CgsSceneSweeper.cpp:152-252) -- the local names and the
// call list below are the ORIGINAL source's, not this author's:
//   locals   lPaddedMin (:388), lPaddedMax (:389), leBodyState (:390, typed eObjectBodyState)
//   calls    Vector3::Vector3 x2 (the two literal clamp vectors), rw::math::vpu::Min,
//            rw::math::vpu::Max, AABBoxTemplate<...>::Min / ::Max (the box's corner
//            accessors -- our reconstructed AABBox exposes the same two rows as mMin/mMax),
//            rw::math::vpu::operator+ x2, then EXACTLY THREE per-axis comparisons
//            `rw::math::vpu::operator< <VectorAxisX,VectorAxisX>` / Y / Z -- which is why the
//            assert below is spelled min < max per axis, and why it is an OR of three lanes
//            and not three separate asserts.
void SceneSweeper::UpdateObject(u32 luObjectIndex, const rw::collision::AABBox* lpBox,
                                Vector3 lPadding, VolumeInstanceId lVolumeInstanceID)
{
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "Object index is invalid: ");   // :392
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");            // CgsBitArray.h:203
    CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
               "mCollidingBodies.IsBitSet( luObjectIndex )");                      // :393

    (void)lVolumeInstanceID;   // console: streamed into the :403 assert message only

    const AABBoxRows& lrBox = *reinterpret_cast<const AABBoxRows*>(lpBox);

    // The two literal clamp vectors, lane 3 zero exactly as the console builds them
    // (DWARF: the two rw::math::vpu::Vector3::Vector3 constructions).
    const Vector3 lMinPaddingLimit = { KF_MIN_PADDING, KF_MIN_PADDING, KF_MIN_PADDING, 0.0f };
    const Vector3 lMaxPaddingLimit = { KF_MAX_PADDING, KF_MAX_PADDING, KF_MAX_PADDING, 0.0f };

    // vminfp128 v13,v13,v127 / vmaxfp128 v11,v11,v127 -- ARGUMENT ORDER PRESERVED: the clamp
    // vector is the first operand, so an unordered (NaN) padding lane yields the padding lane,
    // which is what `(a < b) ? a : b` gives for a == the limit. Then vaddfp against the box.
    const Vector3 lPaddedMin = lrBox.mMin + rw::math::vpu::Min(lMinPaddingLimit, lPadding);
    const Vector3 lPaddedMax = lrBox.mMax + rw::math::vpu::Max(lMaxPaddingLimit, lPadding);

    CGS_ASSERT(lPaddedMin.x < lPaddedMax.x
               || lPaddedMin.y < lPaddedMax.y
               || lPaddedMin.z < lPaddedMax.z, "\n Bad AABBox \n");                // :403

    const eObjectBodyState leBodyState =
        static_cast<eObjectBodyState>(maObjectData[luObjectIndex].mu8BodyState);
    if (leBodyState == E_DYNAMIC_BODY)
    {
        // Already swept as dynamic: rewrite both endpoints in place, list membership unchanged.
        mDynamicIntervalList.UpdateObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
    }
    else if (leBodyState == E_INACTIVE_BODY)
    {
        // ⭐ THE THAW. The body was auto-frozen into the inactive list by a previous
        // SceneSweeper::Update; it moved again, so it goes back to the dynamic list. Both
        // lists share one ObjectToIntervalMap array (SceneSweeper::Prepare passes the same
        // third argument to all three IntervalList::Prepare calls), so the remove/add pair
        // needs no map fix-up of its own.
        mInactiveIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
        mDynamicIntervalList.AddObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
        maObjectData[luObjectIndex].mu8BodyState = E_DYNAMIC_BODY;   // stb 0
        // The inactive list lost an entry, so it must be re-sorted before the next sweep.
        mbSortFrozenObjects = true;                                  // stbx 1 -> +0xDC4B9
    }
    else
    {
        // E_STATIC_BODY -- and, per the oddity in this file's banner, E_INVALID_BODY too:
        // the console asserts and then takes this arm regardless.
        CGS_ASSERT(leBodyState == E_STATIC_BODY, "leBodyState == E_STATIC_BODY");   // :426
        mStaticIntervalList.UpdateObject(static_cast<u16>(luObjectIndex), lPaddedMin, lPaddedMax);
        mbSortStaticObjects = true;                                  // stbx 1 -> +0xDC4B8
    }

    mabMovedThisFrame[luObjectIndex] = true;
}

// ================================================================================
// SceneSweeper::RemoveObject  @ 0x828B5E48   (CgsSceneSweeper.cpp:338-364)
//
// Unregisters a swept body: drops its two intervals from whichever list owns it, restores the
// per-object state byte to E_INVALID_BODY, clears its colliding-bodies bit and decrements the
// live count.
//
// ⚠️ THE STATIC/INVALID ARM RETURNS EARLY AND TOUCHES NOTHING -- no state reset, no bit clear,
// no counter decrement (0x828B5FC8 `bne cr6, loc_828B6154` jumps straight to the epilogue).
// That is the exact mirror of AddObject's faithful oddity, where a STATIC_BODY is added to no
// list, gets no maObjectData state and does not bump muNumObjects: what was never counted is
// never uncounted. Reproduced, not "fixed".
//
// Store-for-store map (console -> here):
//   0x828B5E60  cmplwi 0x13BB -> assert :340 "luObjectIndex < KU_MAX_NUM_OBJECTS"
//   0x828B5E88  lbz maObjectData[i].mu8BodyState        (base 0xD95C0)
//   0x828B5EA0  E_DYNAMIC arm: CgsBitArray.h:203 bounds assert, inlined IsBitSet, assert :347,
//               IntervalList::RemoveObject on +0xD6590 (mDynamicIntervalList)
//   0x828B5FCC  E_INACTIVE arm: the same pair at :352, RemoveObject on +0xD65A0
//               (mInactiveIntervalList), then stbx 1 -> +0xDC4B9 (mbSortFrozenObjects)
//   0x828B60FC  common tail: stb 3 (E_INVALID_BODY), CgsBitArray.h:241 bounds assert
//               ("luIndex < NUMBITS"), the `andc` UnSetBit on +0xDABF8 (mCollidingBodies),
//               and `lwz/addi -1/stw` on +0xDC4B0 (muNumObjects)
//
// DWARF CORROBORATION (dwarfdump/.../CgsSceneSweeper.cpp:64-87): the body's one local is
// `eObjectBodyState leBodyState` (:341), and its inline-call list is exactly
// BitArray<5051>::IsBitSet, BitArray<5051>::UnSetBit, BitArray<5051>::IsBitSet + TWO
// four-`operator<<` StrStream blocks -- i.e. ONE IsBitSet per swept arm (each carrying the
// "invalid index : " << i << " < " << 5051 bounds message) and ONE UnSetBit in the tail,
// which is precisely the shape below.
void SceneSweeper::RemoveObject(u32 luObjectIndex)
{
    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "luObjectIndex < KU_MAX_NUM_OBJECTS");  // :340

    const eObjectBodyState leBodyState =
        static_cast<eObjectBodyState>(maObjectData[luObjectIndex].mu8BodyState);
    if (leBodyState == E_DYNAMIC_BODY)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");   // CgsBitArray.h:203
        CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
                   "Non existing object being removed from the scene sweeper");            // :347
        mDynamicIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
    }
    else if (leBodyState == E_INACTIVE_BODY)
    {
        CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "invalid index : ");   // CgsBitArray.h:203
        CGS_ASSERT(mCollidingBodies.IsBitSet(luObjectIndex),
                   "Non existing object being removed from the scene sweeper");            // :352
        mInactiveIntervalList.RemoveObject(static_cast<u16>(luObjectIndex));
        // The inactive list lost an entry, so it must be re-sorted before the next sweep.
        mbSortFrozenObjects = true;
    }
    else
    {
        // E_STATIC_BODY / E_INVALID_BODY -- never entered a swept list; nothing to undo.
        return;
    }

    maObjectData[luObjectIndex].mu8BodyState = E_INVALID_BODY;

    CGS_ASSERT(luObjectIndex < KU_MAX_NUM_OBJECTS, "luIndex < NUMBITS");      // CgsBitArray.h:241
    mCollidingBodies.UnSetBit(luObjectIndex);

    --muNumObjects;
}

}

// ============================================================================
// FOLDED FROM CgsSceneSweeper_wQ5_03.cpp (wave Q5) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ============================================================================
// CgsSceneManager::SceneSweeper -- round-2 partfile (iii): THE PER-FRAME SWEEP.
//
// Home: GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.{h,cpp}.
// This partfile carries the four bodies that turn the sweeper's registered bodies into
// broadphase pairs once per frame; the keystone partfile (CgsSceneSweeper.cpp) carries
// Construct / Prepare / Clear / ForceNoPadding + the layout pins.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (per-address JSON, `assembly` array):
//   SceneSweeper::Update              @ 0x828D5A88  (95 insns)
//   SceneSweeper::SortLists           @ 0x828D5980  (65 insns)
//   SceneSweeper::SweepLists          @ 0x828C20F0  (40 insns)
//   SceneSweeper::BuildCollidingPairs @ 0x828C2190  (244 insns)
//
// ⭐ WHY THIS IS THE CAR-vs-SMASH-GATE LEG. Update's tail loop is the AUTO-FREEZE: any
// object still in the DYNAMIC list that did not move this frame is migrated to the
// INACTIVE list. A parked smash gate therefore lives in mInactiveIntervalList, a driving
// car lives in mDynamicIntervalList, and the ONLY sweep leg that crosses those two lists
// is SweepLists' second call, IntervalList::SweepAgainstList. Dropping that call (or
// swapping its two stacks) produces a broadphase that silently never reports car-vs-prop.
//
// ⚠️ CONSOLE OFFSETS BELOW ARE PROVENANCE COMMENTS ONLY. Every access is by member name;
// the host layout differs (three IntervalLists and two trailing pointers widen on LLP64).
// The offsets quoted in the per-body banners are the X360 numbers the asm encodes, used
// here to prove WHICH member each access is, never to compute an address.
//
// PARKED, deliberately, with reasons (do NOT invent these):
//   * SceneSweeper::CanCollide / ShouldBeCulled (DWARF CgsSceneSweeper.h:268/:273) --
//     declared by the DWARF, but they have NO row in progress/identity.json, NO ledger
//     row, and NO X360 address anywhere; they are not callees of any of the four bodies
//     here (BuildCollidingPairs does its own culling-table lookup INLINE -- the console
//     folded them, or they are PS3-only). There is no body to recover.
//   * SceneSweeper::RemoveCollidingPair (:157) / SetToUseInternalCollision (:161) -- same
//     evidence state (no identity.json row, no address), and neither is called from these
//     four bodies. They belong to whoever finds their X360 callers, not to this partfile.
// ============================================================================


namespace CgsSceneManager
{
    // ================================================================================
    // SceneSweeper::SortLists  @ 0x828D5980  (private, DWARF CgsSceneSweeper.h:255)
    //
    // Re-sorts the endpoint arrays the sweep walks. The DYNAMIC list is sorted every frame
    // unconditionally; the other two are sorted lazily, each behind its own dirty flag,
    // because a static/frozen body's endpoints only move when one is added or removed.
    //
    // The console spells each of the three as the SAME inlined trio -- introsort, then
    // append the terminating sentinel, then rebuild the object->slot map:
    //     lwz r11,8(list) / lwz r3,0(list)          ; muNumIntervals, mpaIntervals
    //     r4 = mpaIntervals + muNumIntervals*24 ; r5 = (r4-r3)/24 == muNumIntervals
    //     bl std::_Sort<CgsSceneManager::Interval *,int>   @0x828D4E70
    //     bl CgsSceneManager::IntervalList::AddSentinelInterval  @0x828B5178
    //     bl CgsSceneManager::IntervalList::RepairMappings       @0x828AB3E8
    // That trio is IntervalList::Sort() (already landed in CgsIntervalList.cpp): the middle
    // call is PRIVATE to IntervalList (DWARF CgsIntervalList.h:191) and SceneSweeper is not
    // a friend, so SortLists cannot have been the thing calling it -- the access rule pins
    // the split that the asm alone would leave ambiguous.
    //
    // ⚠️ WHICH FLAG GATES WHICH LIST IS EASY TO SWAP; it is measured here, not assumed:
    //   0x828D59DC lbz  (this + 0xE<<16 - 0x3B48 == +902328 == mbSortStaticObjects)
    //              gates the list at 0xD65B0 == mStaticIntervalList   (0x828D59EC)
    //   0x828D5A30 lbz  (this + 0xE<<16 - 0x3B47 == +902329 == mbSortFrozenObjects)
    //              gates the list at 0xD65A0 == mInactiveIntervalList (0x828D5A40)
    // Each flag is cleared (`stb r27(=0)`) only on the branch that actually sorted.
    void SceneSweeper::SortLists()
    {
        mDynamicIntervalList.Sort();                       // 0x828D5994..0x828D59CC (unconditional)

        if (mbSortStaticObjects)                           // 0x828D59DC/0x828D59E4
        {
            mStaticIntervalList.Sort();                    // 0x828D59E8..0x828D5A20
            mbSortStaticObjects = false;                   // 0x828D5A24
        }

        if (mbSortFrozenObjects)                           // 0x828D5A30/0x828D5A38
        {
            mInactiveIntervalList.Sort();                  // 0x828D5A3C..0x828D5A74
            mbSortFrozenObjects = false;                   // 0x828D5A78
        }
    }

    // ================================================================================
    // SceneSweeper::SweepLists  @ 0x828C20F0  (private, DWARF CgsSceneSweeper.h:258)
    //
    // One frame of sweep-and-prune. Empties last frame's overlap queue, builds the two
    // scratch stacks ON THE STACK FRAME over the sweeper's own backing arrays, then runs
    // the two sweep legs.
    //
    // The console builds both IntervalStacks in place -- four stores each, i.e.
    // IntervalStack::Init folded inline (de-inlined here per the project's
    // inlining-reversal rule). Measured:
    //     0x828C2118  stw r11(=0), 0x20(this)     ; mOverlappingPairQueue.miLength = 0
    //                                             ;   (queue base is this+0x18, miLength +8)
    //     var_40 = this + 0xC02E8   ; mauDynamicStackIndexMem        (0x828C2104/0x828C212C)
    //     var_3C = this + 0xC1290   ; maDynamicStackIntervalMem      (0x828C2130/0x828C213C)
    //     var_38 = 0x7D0 == 2000    ; muMaxLen                       (0x828C2140/0x828C2144)
    //     var_34 = 0                ; muLen                          (0x828C2120)
    //     var_30 = this + 0xC8F90   ; mauSecondaryStackIndexMem      (0x828C2148/0x828C2150)
    //     var_2C = this + 0xCA6B0   ; maSecondaryStackIntervalMem    (0x828C2154/0x828C215C)
    //     var_28 = 0xBEA == 3050    ; muMaxLen                       (0x828C2160/0x828C2164)
    //     var_24 = 0                ; muLen                          (0x828C2128)
    //     0x828C2168  SweepIntervals   (r3 = this+0xD6590 == mDynamicIntervalList,
    //                                   r4 = this+0x18, r5 = &var_40)
    //     0x828C2184  SweepAgainstList (r3 = this+0xD6590 == mDynamicIntervalList,
    //                                   r4 = this+0xD65A0 == mInactiveIntervalList,
    //                                   r5 = this+0x18, r6 = &var_40, r7 = &var_30)
    //
    // ⚠️ THREE FACTS A REWRITE WOULD GET WRONG, ALL MEASURED:
    //   1. mStaticIntervalList (this+0xD65B0) is NEVER SWEPT. Prepare binds it and SortLists
    //      sorts it, but no sweep leg touches it. That is what the console does.
    //   2. STACK A IS THE DYNAMIC STACK. SweepAgainstList's lpStaticStackA holds THIS list's
    //      open intervals and lpStaticStackB the other list's; r6 is var_40 (the 2000-entry
    //      dynamic stack) and r7 is var_30 (the 3050-entry secondary stack). Swapping them
    //      compiles, runs, and silently emits wrong pairs.
    //   3. The dynamic stack is passed to BOTH legs and is NOT re-Init'd between them, so
    //      SweepIntervals leaves it in whatever state its walk ended in -- reproduced by
    //      construction here (one local, two uses).
    void SceneSweeper::SweepLists()
    {
        mOverlappingPairQueue.Clear();                                  // 0x828C2118

        IntervalStack lDynamicStack;
        lDynamicStack.Init(mauDynamicStackIndexMem, maDynamicStackIntervalMem,
                           static_cast<u32>(KI_DYNAMIC_STACK_SIZE));    // 2000 == 0x7D0

        IntervalStack lSecondaryStack;
        lSecondaryStack.Init(mauSecondaryStackIndexMem, maSecondaryStackIntervalMem,
                             static_cast<u32>(KI_SECONDARY_STACK_SIZE)); // 3050 == 0xBEA

        // Leg 1 -- dynamic x dynamic (car vs car, car vs any other moving body).
        mDynamicIntervalList.SweepIntervals(&mOverlappingPairQueue, &lDynamicStack);

        // Leg 2 -- dynamic x inactive. ⭐ THE CAR-vs-PARKED-PROP LEG.
        mDynamicIntervalList.SweepAgainstList(mInactiveIntervalList, &mOverlappingPairQueue,
                                              &lDynamicStack, &lSecondaryStack);
    }

    // ================================================================================
    // SceneSweeper::BuildCollidingPairs  @ 0x828C2190  (private, DWARF CgsSceneSweeper.h:263)
    //
    // The sweeper's OUTPUT producer: drains the frame's OverlappingIntervalPair queue into
    // maCollidingPairs, dropping the pairs where neither body moved and stamping each kept
    // pair with the culling-table verdict.
    //
    // Per queued pair (loop 0x828C2254..0x828C2518):
    //   * `CgsSceneManager::Overl(&mOverlappingPairQueue, i)` -- the export's truncated name
    //     for BaseEventQueue<OverlappingIntervalPair>::GetEvent(int); A = lhz +0, B = lhz +2.
    //   * MOVED GATE (0x828C2278..0x828C2290): `mabMovedThisFrame[A] | mabMovedThisFrame[B]`,
    //     base this+0xDAE70 == +896624. Zero -> skip the pair entirely. A pair of two frozen
    //     bodies is not re-reported.
    //   * both indices resolved through EntityManager::GetVolumeInstance @0x828B9F28, each
    //     with a NULL tripwire (CgsSceneSweeper.cpp:561 / :562, streamed
    //     "Bad volume instance with index " << index).
    //     ⚠️ FAITHFUL: the console asserts and then CARRIES ON -- it dereferences the pointer
    //     four instructions later (0x828C242C/0x828C2430) with no branch around it. No
    //     early-out is invented here. (Since wave Q5/A2 landed the real GetVolumeInstance
    //     this only returns NULL for a genuinely unallocated slot; before A2 the
    //     WorldLinkStubs body returned NULL for EVERY index.)
    //   * mfPadding (0x828C241C..0x828C2440): `lvx128` the 16-byte lane at VolumeInstance
    //     +0x40 from BOTH instances, `vsubfp` A-minus-B, `vmsum3fp128` with itself, keep
    //     lane 0. That is the SQUARED LENGTH of the difference of the two instances' 16-byte
    //     +0x40 lanes, over three components.
    //     ⚠️ +0x40 is VolumeInstance::mPadding, NOT the position: the transform occupies
    //     +0x00..+0x3F (four rows) so Pos() is +0x30. Pinned independently by wave Q5/A2
    //     against three X360 bodies (AddVolumeInstance @0x828CD790, SetVolumePadding
    //     @0x828BA088, SceneManagerModule::UpdateCollisionBody @0x828C7528). The scout's
    //     "|posA - posB|^2" reading of this instruction pair is WRONG; it is read here
    //     through the named member so the name cannot drift again.
    //   * CAPACITY (0x828C2424..0x828C2444): break out of the whole loop when
    //     muNumCollidingPairs + 1 >= 1024, after the one-shot warning. `break`, not
    //     `continue` -- the remaining queue entries are discarded.
    //   * CULLING (0x828C245C..0x828C24F8): group bytes from mpaVolumeInstanceCullingGroup
    //     (this+0xDC4A8) indexed by the RAW object indices (not the min/max ones), bit index
    //     = mpCullingTable->muHeight * groupA + groupB, word at maBits[bit >> 5] (the
    //     console's `(bit>>5)+3` dwords is the +0x0C header offset of rw::BitTable::Storage),
    //     mask 1 << (bit & 31).
    //     ⚠️ The console spells the final boolean `(cntlzw(x & mask) & 0x20) == 0`. cntlzw is
    //     32 only when the value is zero, so that is `!= 0` -- NOT an inversion. Getting the
    //     polarity backwards would cull exactly the pairs that should collide.
    //   * ORDERING (0x828C2488..0x828C24AC + the two `sth`): mu16ObjectA is the MIN of the
    //     two indices and mu16ObjectB the MAX. Downstream consumers rely on that canonical
    //     order to de-duplicate.
    //
    // NOT reproduced: `dcbt r11, r30` (0x828C226C), a data-cache prefetch of the queue
    // element 128 bytes ahead. Pure PPC performance hint, no architectural effect.
    void SceneSweeper::BuildCollidingPairs(const EntityManager* lpEntityManager)
    {
        CGS_ASSERT(lpEntityManager != NULL, "lpEntityManager != NULL");  // CgsSceneSweeper.cpp:521

        // The queue length is read ONCE, before the loop (0x828C21DC `lwz r25,0x20(r26)`,
        // cached in var_DC and only ever re-loaded from there) -- nothing inside the loop
        // appends to the queue, but the fixed bound is the console's shape.
        const s32 liNumOverlappingPairs = mOverlappingPairQueue.GetLength();

        muNumCollidingPairs = 0;                                         // 0x828C21F4

        for (s32 liPair = 0; liPair < liNumOverlappingPairs; ++liPair)
        {
            const OverlappingIntervalPair& lrOverlap = mOverlappingPairQueue.GetEvent(liPair);
            const u16 lu16ObjectA = lrOverlap.muObjectIndexA;
            const u16 lu16ObjectB = lrOverlap.muObjectIndexB;

            // Neither body moved this frame -> the pair was already reported; skip it.
            if (!mabMovedThisFrame[lu16ObjectA] && !mabMovedThisFrame[lu16ObjectB])
            {
                continue;
            }

            const VolumeInstance* lpVolumeInstanceA = lpEntityManager->GetVolumeInstance(lu16ObjectA);
            const VolumeInstance* lpVolumeInstanceB = lpEntityManager->GetVolumeInstance(lu16ObjectB);

            CGS_ASSERT(lpVolumeInstanceA != NULL, "Bad volume instance with index ");  // :561
            CGS_ASSERT(lpVolumeInstanceB != NULL, "Bad volume instance with index ");  // :562

            // vsubfp + vmsum3fp128: the squared length of the two padding lanes' difference,
            // over the x/y/z components (vmsum3 sums three products and splats; the console
            // then reads lane 0 back with `lfs`).
            const Vector3 lvPaddingA = lpVolumeInstanceA->mPadding;
            const Vector3 lvPaddingB = lpVolumeInstanceB->mPadding;
            const f32 lfDeltaX = lvPaddingA.x - lvPaddingB.x;
            const f32 lfDeltaY = lvPaddingA.y - lvPaddingB.y;
            const f32 lfDeltaZ = lvPaddingA.z - lvPaddingB.z;
            const f32 lfPadding = (lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY) + (lfDeltaZ * lfDeltaZ);

            if (muNumCollidingPairs + 1 >= KU_MAX_NUM_COLLIDING_PAIRS)
            {
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Warning: Ran out of colliding pairs - discarding contacts\n";
                }
                break;
            }

            const u32 luPairSlot = muNumCollidingPairs;
            ++muNumCollidingPairs;                                       // 0x828C2454

            // Culling-table lookup. Both group bytes are read with the RAW A/B indices, i.e.
            // BEFORE the min/max canonicalisation below -- so the table is addressed
            // [groupOf(A)][groupOf(B)] in queue order, not in sorted-index order.
            const u32 luCullingGroupA = mpaVolumeInstanceCullingGroup[lu16ObjectA];
            const u32 luCullingGroupB = mpaVolumeInstanceCullingGroup[lu16ObjectB];
            const u32 luCullingBit    = (mpCullingTable->muHeight * luCullingGroupA) + luCullingGroupB;
            const bool lbCull =
                (mpCullingTable->maBits[luCullingBit >> 5] & (1u << (luCullingBit & 31))) != 0;

            const u16 lu16MinObject = (lu16ObjectA < lu16ObjectB) ? lu16ObjectA : lu16ObjectB;
            const u16 lu16MaxObject = (lu16ObjectA > lu16ObjectB) ? lu16ObjectA : lu16ObjectB;

            maCollidingPairs[luPairSlot].Set(lu16MinObject, lu16MaxObject, lfPadding, lbCull);
        }
    }

    // ================================================================================
    // SceneSweeper::Update  @ 0x828D5A88  (public, DWARF CgsSceneSweeper.h:126)
    //
    // The whole broadphase frame, driven by OverlapGenerationModule::GenerateOverlaps
    // @0x828D5C08 after that module has replayed its four body queues into the sweeper.
    //
    //   0x828D5AA4  assert lpEntityManager != NULL          (CgsSceneSweeper.cpp:218)
    //   0x828D5AC4  perfmon bracket -> SortLists            (dword_82F33F54)
    //   0x828D5AF0  perfmon bracket -> SweepLists           (dword_82F33F58)
    //   0x828D5B1C  perfmon bracket -> BuildCollidingPairs  (dword_82F33F5C)
    //   0x828D5B7C  the 5051-iteration auto-freeze loop
    //   0x828D5BD8  memset(mabMovedThisFrame, 0, 0x13BB == 5051)
    //   0x828D5BE8  memset(mUseInternalCollision, 0, 0x278 == 632)
    //
    // Each perfmon bracket is `if (id > -1) { Start(id); ... Stop(id); }` -- the console
    // re-loads the global for both halves (`cmpwi r3,-1; ble`). The three ids are the
    // statics defined in CgsSceneSweeper.cpp, whose .data image really is -1/-1/-1 (that
    // file's banner re-measured it) and whose REGISTRAR is SceneManagerModule::Construct
    // @0x828D09A0 -- which registers each monitor only while the id still equals -1.
    // ⚠️ The brackets below are therefore INERT IN THIS TREE, and not because the console
    // left them off: CgsSceneManagerModule.cpp:62-64 FORKS these three statics as file-local
    // copies and :222-227 registers into the fork, so the class statics stay -1 forever.
    // That hand-off is recorded in CgsSceneSweeper.cpp's banner and in sweep3.owner.md;
    // nothing here needs changing when it is repaired. Do NOT "fix" the brackets by
    // dropping the `> -1` guard -- id 0 is a VALID handle and would time monitor 0.
    //
    // ⭐ THE AUTO-FREEZE LOOP (0x828D5B7C..0x828D5BD4) IS THE POINT OF THIS FUNCTION.
    //   lbzx (this+0x   D95C0 == +890304 == maObjectData) [i]  -> must be 0 == E_DYNAMIC_BODY
    //   lbzx (this+0xDAE70 == +896624 == mabMovedThisFrame)[i] -> must be 0 (did NOT move)
    //   -> GetIntervalsAndRemoveObject on the list at 0xD6590 (mDynamicIntervalList),
    //      writing the two endpoint records to the caller's stack (r5 = min, r6 = max),
    //   -> AddObject @0x828B46E8 (the const Interval& overload) on the list at 0xD65A0
    //      (mInactiveIntervalList) with those same two records, in the same order,
    //   -> mbSortFrozenObjects = true  (stbx at +0xDC4B9),
    //   -> maObjectData[i].mu8BodyState = 1 == E_INACTIVE_BODY.
    // A body that stopped moving migrates dynamic -> inactive. That is why a parked smash
    // gate ends up in the inactive list, and therefore why SweepLists' SweepAgainstList leg
    // (dynamic x inactive) is the one that produces car-vs-gate pairs at all.
    //
    // ⚠️ The loop runs the FULL KU_MAX_NUM_OBJECTS == 5051 range every frame, not
    // muNumObjects -- the object indices are volume-instance slots, not a dense range.
    void SceneSweeper::Update(const EntityManager* lpEntityManager)
    {
        CGS_ASSERT(lpEntityManager != NULL, "lpEntityManager != NULL");  // CgsSceneSweeper.cpp:218

        if (siUpdate_SortListsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(siUpdate_SortListsPerfMon);
        }
        SortLists();
        if (siUpdate_SortListsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(siUpdate_SortListsPerfMon);
        }

        if (siUpdate_SweepListsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(siUpdate_SweepListsPerfMon);
        }
        SweepLists();
        if (siUpdate_SweepListsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(siUpdate_SweepListsPerfMon);
        }

        if (siUpdate_BuildCollidingPairsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StartMonitor(siUpdate_BuildCollidingPairsPerfMon);
        }
        BuildCollidingPairs(lpEntityManager);
        if (siUpdate_BuildCollidingPairsPerfMon > -1)
        {
            CgsDev::PerfMonCpu::StopMonitor(siUpdate_BuildCollidingPairsPerfMon);
        }

        // The auto-freeze: everything that stayed put this frame leaves the dynamic list.
        for (u32 luObject = 0; luObject < KU_MAX_NUM_OBJECTS; ++luObject)
        {
            if (maObjectData[luObject].mu8BodyState != E_DYNAMIC_BODY)
            {
                continue;
            }
            if (mabMovedThisFrame[luObject])
            {
                continue;
            }

            Interval lMinInterval;
            Interval lMaxInterval;
            mDynamicIntervalList.GetIntervalsAndRemoveObject(static_cast<u16>(luObject),
                                                             &lMinInterval, &lMaxInterval);
            mInactiveIntervalList.AddObject(static_cast<u16>(luObject),
                                            lMinInterval, lMaxInterval);

            mbSortFrozenObjects = true;
            maObjectData[luObject].mu8BodyState = E_INACTIVE_BODY;
        }

        // Both wipes are whole-array memsets in the console; sized here by the arrays
        // themselves, which reproduce the console's 5051 / 632 exactly (the two spans are
        // pointer-free, pinned by the static_asserts in CgsSceneSweeper.cpp).
        std::memset(mabMovedThisFrame, 0, sizeof(mabMovedThisFrame));
        mUseInternalCollision.UnSetAll();
    }
}
