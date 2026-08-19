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

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"          // EntityManager::GetVolumeInstance, VolumeInstance
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::Start/StopMonitor

#include <cstring>   // std::memset (the console's own memset call)

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
