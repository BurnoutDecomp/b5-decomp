#pragma once

// CgsSceneManager::OverlappingIntervalPair — a broadphase overlap between two object
// intervals (their indices). Reconstructed from the DecFIGS DWARF.
//
// CgsSceneManager::Interval — one object's swept-axis interval used by the sweep-and-prune
// broadphase (CgsIntervalList / CgsIntervalStack). Field set + offsets recovered from the
// DecFIGS DWARF (CgsInterval.h:155-161). The five interval floats are the per-axis min/max
// bounds packed for the SoA sweep; the trailing u16 object index + flags identify the owner
// and the min/max sentinel role.
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"  // CgsModule::EventQueue (OverlapPairQueue)

namespace CgsSceneManager
{
    struct OverlappingIntervalPair
    {
        u16 muObjectIndexA;
        u16 muObjectIndexB;
    };

    // The sweep's output queue type. DWARF CgsInterval.h:61 (the dumpfile reproduces this
    // typedef verbatim inside every scope that names it -- IntervalStack::
    // CheckOverlappingAndAddToQueue's local scope in
    // references/DecFIGS/dwarfdump/.../CgsIntervalStack.h:44/86, and again at :75 -- so
    // CgsInterval.h is its attested home, not CgsIntervalList.h/CgsSceneSweeper.h):
    //     typedef CgsModule::EventQueue<CgsSceneManager::OverlappingIntervalPair,131072>
    //         OverlapPairQueue;
    // X360 CAPACITY ATTESTATION (independent of the DWARF): SceneSweeper::Prepare @0x828C2000
    // puts maObjectToIntervalMapMem at this+0x80024 while the queue's inline maEvents starts
    // at this+0x24 -- 0x80000 bytes == 131072 * sizeof(OverlappingIntervalPair).
    typedef CgsModule::EventQueue<OverlappingIntervalPair, 131072> OverlapPairQueue;

    // Interval — DWARF CgsInterval.h:72. 24 bytes (0x18): five f32 axis bounds + a u16
    // object index + a u16 flags word. The IntervalStack::Push @0x828AA158 reads these by
    // the byte offsets the asm attests:
    //   +0x00  mfXInterval          (lfs 0x00 — not touched by Push; the SORT KEY, see below)
    //   +0x04  mfZMinInterval       (lfs 0x04(a2) -> stack lane[1])
    //   +0x08  mfMinusZMaxInterval  (lfs 0x08(a2) -> stack lane[0])
    //   +0x0C  mfYMinInterval       (lfs 0x0C(a2) -> stack lane[3])
    //   +0x10  mfMinusYMaxInterval  (lfs 0x10(a2) -> stack lane[2])
    //   +0x14  mu16ObjectIndex      (lhz 0x14(a2) -> mpuIndexData[len])
    //   +0x16  mu16Flags            (0 == min-role endpoint, 1 == max-role endpoint)
    struct Interval
    {
        f32 mfXInterval;          // +0x00
        f32 mfZMinInterval;       // +0x04
        f32 mfMinusZMaxInterval;  // +0x08
        f32 mfYMinInterval;       // +0x0C
        f32 mfMinusYMaxInterval;  // +0x10
        u16 mu16ObjectIndex;      // +0x14
        u16 mu16Flags;            // +0x16
    };

    // ---- the SORT PREDICATE, RECOVERED (not guessed) ------------------------------------
    // SceneSweeper::SortLists @0x828D5980 sorts each list's interval array with the
    // three-argument MSVC introsort instantiation
    //     std::_Sort<CgsSceneManager::Interval *, int>(first, last, ideal)
    //     (mangled ??$_Sort@PAVInterval@CgsSceneManager@@H@std@@YAXPAVInterval@...@@0H@Z,
    //      body @0x828D4E70).
    // That instantiation takes NO predicate type parameter -- the template argument list is
    // <Interval*, int> only -- so the ordering comes from Interval's own `operator<`.
    //
    // MEASURED, in both leaves the introsort delegates to (do NOT assume "by min value"):
    //   std::_Insertion_sort1<Interval*, Interval> @0x828CD388
    //       0x828CD3D8  lfs   f13, 0(r30)      ; other.mfXInterval      (element +0x00)
    //       0x828CD3E0  lfs   f0,  var_40(r1)  ; candidate.mfXInterval  (element +0x00)
    //       0x828CD3E4  fcmpu cr6, f0, f13
    //       0x828CD3E8  blt   cr6, ...         ; taken iff candidate < other
    //     and the identical pair again at 0x828CD440/0x828CD444.
    //   std::_Unguarded_partition<Interval*> @0x828CCA30 -- EVERY comparison in the median-of-
    //     three + both scan loops is `lfs +0x00` vs `lfs +0x00` (0x828CCA8C/0x828CCA94,
    //     0x828CCAEC/0x828CCAF0, 0x828CCB2C, ...). No second key, no tie-break, no other field.
    // So the predicate is a strict less-than on mfXInterval ALONE.
    //
    // NaN polarity is the PPC `fcmpu; blt` form -- true only when the comparison is ordered
    // AND less-than (an unordered pair takes the fall-through), which is exactly host `<`.
    inline bool operator<(const Interval& lrLhs, const Interval& lrRhs)
    {
        return lrLhs.mfXInterval < lrRhs.mfXInterval;
    }
}
