// ============================================================================
// GameSource/Director/BrnDirectorResourceManager.cpp
//
// BrnDirector::DirectorResourceManager -- the out-of-line bodies.
//
// ⭐ REWRITTEN 2026-07-31 (shot-group wave). What used to be in this file was a
// reconstruction of the CONSTRUCTOR @0x827DEB98 written through raw console byte offsets
// (`*(void**)(base + 552) = vtable; for (off = 568; off <= 1592; off += 16) ...`), over a
// LOCAL `struct DirectorResourceManager { DirectorResourceManager(); };` re-declaration.
// It was never mountable: those are 4-byte-pointer console offsets, so on x64 it would
// have scribbled across the live class and the DirectorModule that embeds it, and its
// ctor collided at link with the header's implicit one (LNK2005 vs BrnGameModule.obj).
//
// That whole TU is now OBSOLETE rather than merely wrong: the header declares all 65
// shot-group slots as real members, so the console's "default-construct 65 sub-objects at
// stride 0x10, one of them with a different element ctor" IS the compiler-generated
// default constructor, member by member, in the same order. There is nothing left for a
// hand-written ctor to do. (The element ctors it called through -- sub_827DC838 and
// sub_827DC8C8 -- are now named: Attrib::Gen::shotgroup(Collection*, owner) and
// Attrib::Gen::cameradefaults(Collection*, owner).)
//
// ⚠️ ONE CONSOLE BEHAVIOUR THE IMPLICIT CTOR DROPS, deliberately: the console ctor also
// clears bytes 1608..1627 (std/std/stw == the whole 20-byte Attrib::RefSpec
// mAfterTouchCam). Attrib::RefSpec's own default ctor zeroes all three of its fields by
// name, so the effect is reproduced -- but by RefSpec, not here.
//
// What lives here now are the manager's genuinely out-of-line members. Two of them are
// X360-attested symbols; the rest are the ones the header cannot define inline because
// BrnDirector::ICEWrapper is only forward-declared there (its home drags the whole ICE
// manager/camera/editor cone and forward-declares this class in return).
//
// ⚠️ NOT MOUNTED YET. GetKeyAnimFromGuid's two leaves -- ICE::ICEAuthor::
// FindEditedTakeFromGuid (SDKs/Packages/ICE/ICEAuthorTakeOps.cpp) and
// BrnResource::ICEList::GetICETakeDataFromGuid (SharedClasses/DataLists/ICEList.cpp) --
// are both real bodies in TUs that are not in the exe source list, and ICEWrapper's own
// accessors are in the same position. Mounting this file before them would ADD unresolved
// externals to the linked set rather than remove them, so it stays out until that group
// can go in together. Its callers (BrnMomentPlayerStunt, ICEWrapper, BrnDirectorDevTools,
// BrnKeyAnimController, BrnBehaviourIceAnim) are all unmounted too.
// DELETE-WHEN: the ICE take-runtime group lands; mount this with it.
// ============================================================================

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"   // ICEWrapper::GetICETakeData / GetShakeGroup / GetAuthor
#include "SDKs/Packages/ICE/ICEAuthor.hpp"               // ICE::ICEAuthor::FindEditedTakeFromGuid
#include "SharedClasses/DataLists/ICEList.h"             // BrnResource::ICEList::GetICETakeDataFromGuid

namespace BrnDirector
{

// @0x821F6948 -- resolve a world-signature key-anim by its numeric id. The console
// formats the take name and hashes it; the name form below is the one the X360 builds.
ICE::ICETakeData* DirectorResourceManager::GetKeyAnim(int64_t liKeyAnimID) const
{
    char lacICEName[16];
    snprintf(lacICEName, 16, "ICE_WLDSIG%d", (int32_t)liKeyAnimID);
    return GetKeyAnim(lacICEName);
}

ICE::ICETakeData* DirectorResourceManager::GetKeyAnim(const char* lpacKeyAnimName) const
{
    return mpICEWrapper->GetICETakeData(BrnResource::MakeICEMovieId(lpacKeyAnimName));
}

ICE::ICEGroup* DirectorResourceManager::GetShakeTakes() const
{
    return mpICEWrapper->GetShakeGroup();
}

// The in-game ICE editor's author/edit store. The console reaches it at manager +560 --
// i.e. through mpICEWrapper -- which is why this is not a plain member read.
ICE::ICEAuthor& DirectorResourceManager::GetICEAuthor() const
{
    return mpICEWrapper->GetAuthor();
}

// @0x821F69A8 -- resolve an ICE take GUID to its take data. The editor's edited-take list
// wins over the on-disk list, so an in-editor edit is what plays back.
ICE::ICETakeData* DirectorResourceManager::GetKeyAnimFromGuid(s32 liGuid) const
{
    ICE::ICETakeData* lpTakeData = mpICEWrapper->GetAuthor().FindEditedTakeFromGuid(liGuid);
    if (lpTakeData == 0)
    {
        lpTakeData = const_cast<ICE::ICETakeData*>(
            mpICEDictionaryList->GetICETakeDataFromGuid(liGuid));
    }
    return lpTakeData;
}

// ----------------------------------------------------------------------------
// @0x821F6AB8 -- BrnDirector::DirectorResourceManager::GetEventIntroShots.
//
// Reconstructed 2026-07-31 from the export, jump table and all: 18 cases over
// jpt_821F6AF0, r4 = the mode, r5 = lbCarInFront, every arm an `addi r3,r31,<offset>`.
// It is one of only SEVEN out-of-line DirectorResourceManager symbols in the image.
//
// ⭐ IT IS ALSO AN INDEPENDENT CONFIRMATION OF NINE MEMBER OFFSETS: 0x238/0x248/0x268/
// 0x278/0x2B8/0x2D8/0x2E8 and 0x258 land exactly on mRaceStartGroup /
// mRoadRageStartGroup / mOnlineRaceStart / mBurningRouteStartGroup /
// mOnlineLobbyStartGroup / mStuntRaceStartGroup / mMarkedManStartGroup /
// mRaceStartRivalInFrontGroup, which is the head of the DWARF declaration order the
// 65-slot table is built on.
//
// ⚠️ TWO CONSOLE ODDITIES, REPRODUCED, DO NOT "FIX":
//   * E_MODE_ELIMINATOR (6) is NOT a case -- it falls into the default arm and fires the
//     "Invalid Event Requested" assert (cpp:615) before returning the race group. The
//     manager has an mEliminatorStartGroup (+664) and this function never returns it.
//   * mSurvivorStartGroup (+648), mTrafficAttackStartGroup (+680) and mPursuitStartGroup
//     (+712) are likewise never returned here: E_MODE_PURSUIT (4) and
//     E_MODE_TRAFFIC_ATTACK (9) both route to the plain race pair instead.
// (Both are consistent with those four groups being driven from somewhere else, or with
// them being dead vault entries; either way this function's behaviour is what it is.)
//
// ⚠️ The default arm falls THROUGH the assert into the same tail as 0/1/4/9 -- it does
// not return early. That is the console's `goto LABEL_9`.
// ----------------------------------------------------------------------------
const Attrib::Gen::shotgroup& DirectorResourceManager::GetEventIntroShots(
    s32 liEventMode, bool lbCarInFront) const
{
    switch (liEventMode)
    {
    case 2:     // E_MODE_OFFLINE_SHOWTIME
    case 16:    // E_MODE_ONLINE_SHOWTIME
        return mRaceStartGroup;                       // +568  (0x238)

    case 3:     // E_MODE_ROAD_RAGE
        return mRoadRageStartGroup;                   // +584  (0x248)

    case 5:     // E_MODE_BURNING_ROUTE
        return mBurningRouteStartGroup;               // +632  (0x278)

    case 7:     // E_MODE_STUNT_ATTACK
        return mStuntRaceStartGroup;                  // +728  (0x2D8)

    case 8:     // E_MODE_MARKED_MAN
        return mMarkedManStartGroup;                  // +744  (0x2E8)

    case 10:    // E_MODE_ONLINE_RACE
    case 11:    // E_MODE_ONLINE_ROAD_RAGE
    case 12:    // E_MODE_ONLINE_FUGITIVE
    case 13:    // E_MODE_ONLINE_BURNING_HOME_RUN
    case 14:    // E_MODE_ONLINE_FREE_BURN
    case 17:    // E_MODE_ONLINE_MODE_END / E_MODE_COUNT
        return mOnlineRaceStart;                      // +616  (0x268)

    case 15:    // E_MODE_ONLINE_FREE_BURN_LOBBY
        return mOnlineLobbyStartGroup;                // +696  (0x2B8)

    case 0:     // E_MODE_OFFLINE_RACE
    case 1:     // E_MODE_FACE_OFF
    case 4:     // E_MODE_PURSUIT
    case 9:     // E_MODE_TRAFFIC_ATTACK
        break;

    default:    // includes E_MODE_ELIMINATOR (6) -- see the note above
        CGS_ASSERT(false, "Invalid Event Requested");
        break;
    }

    return lbCarInFront ? mRaceStartRivalInFrontGroup   // +600  (0x258)
                        : mRaceStartGroup;              // +568  (0x238)
}

}
