#ifndef GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H
#define GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<s32,50> (the per-group time lists)
#include "GameSource/Director/Camera/Camera.h"            // Camera::Camera::ShotSelectionInfo / ShotReference

// ============================================================================
// GameSource/Director/Utils/BrnShotSelector.h
//
// BrnDirector::ShotSelector - picks the crash-camera shot for an event: it scores
// every shot in the chosen group's attrib ShotList (event-flag suitability first,
// then property suitability, then least-recently-used) and keeps per-shot use-time
// stamps. Class shape + member names from the DecFIGS DWARF (BrnShotSelector.h),
// gated on the X360 ledger.
//
// Members reconciled to the DWARF names when GetCrashShot landed (the earlier
// Construct-only home spelled them maShotLists / <alignment gap> / void* mpOwner):
//   +0x000/+0x0CC/+0x198  maaShotTimes[3]   (Array<s32,50>; count word @+0xC8 each --
//                          Construct @0x8221B410 sets each full(50) then zeroes slots)
//   +0x264                miCurrentTimeID   ("now"; GetCrashShot seeds its
//                                            least-recent compare with it)
//   +0x268                mpResourceManager (const DirectorResourceManager* -- the
//                                            DWARF Construct parameter type)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    class DirectorResourceManager;   // GameSource/Director/BrnDirectorResourceManager.h

class ShotSelector
{
public:
    // DWARF BrnShotSelector.h -- the crash-energy shot groups.
    enum EGroup
    {
        E_GROUP_LOW_ENERGY_CRASH  = 0,
        E_GROUP_NORMAL_CRASH      = 1,
        E_GROUP_HIGH_ENERGY_CRASH = 2,
        E_NUM_GROUPS              = 3,
    };

    static const u32 KU_NUM_SHOT_LISTS = 3;
    static const u32 KU_SHOT_LIST_SIZE = 50;

    // Initialise the selector: record the resource manager, then bring all three
    // time lists to their full (50-element) state with every slot zeroed. @0x8221B410.
    // (Parameter reconciled to the DWARF's const DirectorResourceManager*; the body
    // is the same stw r4, 0x268.)
    void Construct(const DirectorResourceManager* lpResourceManager);

    // DWARF cpp:73. PS3 @0x15000 (`li 0 ; stw 0x264 ; li r3, 1`); inlined on the X360 by
    // MainDirector::Prepare stage 1 @0x8224FBDC..0x8224FBE4 (`stwx 0` -> director + 0x12454, i.e.
    // this + 0x264). Body in BrnShotSelector.cpp.
    bool Prepare();
    // DWARF cpp:89. PS3 @0x2E4AC; inlined on the X360 by MainDirector::Update @0x82274FB0..0x82274FE4.
    // Body in BrnShotSelector.cpp.
    void Update(const Camera::Camera& lrSelectedCamera);

    // @0x822396F8 (this TU, DWARF cpp:121) -- score the group's ShotList and return
    // the best shot (Camera::ShotReference* == const Attrib::RefSpec*); the winning
    // {group, index} pair lands in lpShotSelectionInfoOut.
    Camera::Camera::ShotReference* GetCrashShot(u32 lxCrashEventFlags,
                                                Camera::Camera::ShotSelectionInfo* lpShotSelectionInfoOut,
                                                u32 lxPreferredShotProperties,
                                                u32 lxExcludedShotProperties,
                                                u32 lxRequiredShotProperties,
                                                EGroup leShotGroup) const;

private:
    // DWARF cpp:313. PS3 @0x2E1D8, called only by GetCrashShot (whose X360 copy reads the list
    // directly). NOT-CALLED here: declaration-only.
    s32 GetLastUsedIndex(const Camera::Camera::ShotSelectionInfo& lrShotSelectionInfo) const;
    // DWARF cpp:322. PS3 @0x2E33C; tail-called by Update. Body in BrnShotSelector.cpp.
    void SetLastUsedToNow(const Camera::Camera::ShotSelectionInfo& lrShotSelectionInfo);

    // DWARF: Array<int32_t,50u>[3] maaShotTimes -- per-group per-shot "time last
    // used" stamps, indexed [group][shot index].
    Array<s32, KU_SHOT_LIST_SIZE>  maaShotTimes[KU_NUM_SHOT_LISTS];   // +0x000 / +0x0CC / +0x198
    s32                            miCurrentTimeID;                    // +0x264
    const DirectorResourceManager* mpResourceManager;                  // +0x268
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_UTILS_BRN_SHOT_SELECTOR_H
