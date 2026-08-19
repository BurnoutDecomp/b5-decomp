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
