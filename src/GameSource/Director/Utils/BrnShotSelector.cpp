// ============================================================================
// GameSource/Director/Utils/BrnShotSelector.cpp
//
// Compilation home for BrnDirector::ShotSelector. Bodies (reconstructed from
// BURNOUT_X360_ARTIST.XEX):
//   ShotSelector::Construct        @0x8221B410  (the original slice)
//   BrnDirector::PrintShotEventFlags  @0x821F6EA0 \  (this batch's ledger TU
//   BrnDirector::PrintShotProperties  @0x821F6DF0  }  GameSource/Director/
//   ShotSelector::GetCrashShot        @0x822396F8 /   BrnShotSelector.cpp)
//
// The two Print helpers walk .data name tables whose relocated pointers were read
// from the decrypted XEX: properties @0x82CDA4F8 { "None", "LeftOfActionLine",
// "RightOfActionLine", "TopDown", "Stationary", "SlowMo", "SlowMoOut", "SlowMoIn" }
// and event flags @0x82CDA518 { "None", "HardStop", "LeftImpact", "RightImpact",
// "WorldImpact", "CarImpact", "CrashStart", "FrontImpact", "RearImpact" }; the
// loops start at entry [1] (bit 0), entry [0] being the zero-mask name.
// ============================================================================

#include "GameSource/Director/Utils/BrnShotSelector.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Numeric/CgsBitOperations.h"        // GetNumberOfSetBits
#include "GameSource/AttribSys/Generated/classes/iceanim.h"         // Attrib::Gen::iceanim
#include "GameSource/AttribSys/Generated/classes/proceduralshot.h"  // Attrib::Gen::proceduralshot
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"       // Attrib::Gen::shotgroup
#include "GameSource/Director/BrnDirectorICEWrapper.h"              // ICEWrapper (the manager header's inlines dereference it)
#include "GameSource/Director/BrnDirectorResourceManager.h"         // the crash-shot banks

namespace BrnDirector
{
namespace
{
    // The bit -> name tables (XEX .data @0x82CDA4FC / @0x82CDA51C, i.e. entry [1] of
    // each block -- see the file banner). Sized to the named bits; the X360 walk
    // trusts the flag width and never leaves the named span for real flag values.
    const char* const KAPC_SHOT_PROPERTY_NAMES[] =
    {
        "LeftOfActionLine", "RightOfActionLine", "TopDown", "Stationary",
        "SlowMo", "SlowMoOut", "SlowMoIn",
    };
    const char* const KAPC_SHOT_EVENT_FLAG_NAMES[] =
    {
        "HardStop", "LeftImpact", "RightImpact", "WorldImpact",
        "CarImpact", "CrashStart", "FrontImpact", "RearImpact",
    };

    // Shared body of the two Print helpers (they differ only in the table; each is
    // its own X360 symbol, so both stay real functions below).
    void PrintFlagNames(u32 lxFlags, const char* const* lppcNames)
    {
        for (u32 lxBits = lxFlags; lxBits != 0; lxBits >>= 1, ++lppcNames)
        {
            if ((lxBits & 1) != 0 && (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                const char* lpcName = *lppcNames;
                if (lpcName == NULL)
                    lpcName = "<NULLSTRING>";
                *CgsDev::Log::gpDebugPrint << lpcName << " ";
            }
        }
    }
}

// @ 0x821F6EA0
void PrintShotEventFlags(u32 lxShotEventFlags)
{
    PrintFlagNames(lxShotEventFlags, KAPC_SHOT_EVENT_FLAG_NAMES);
}

// @ 0x821F6DF0
void PrintShotProperties(u32 lxShotProperties)
{
    PrintFlagNames(lxShotProperties, KAPC_SHOT_PROPERTY_NAMES);
}

// ----------------------------------------------------------------------------
// BrnDirector::ShotSelector::Construct @0x8221B410
//
//   stw   r4, 0x268(this)        ; mpResourceManager = lpResourceManager
//   li    r28, 3                 ; outer count
//   li    r27, 50                ; the count value
//   li    r29, 0                 ; the zero value
//   r30 = this
// loop_outer:                    ; three times, r30 += 0xCC each pass
//   r31 = 0
//   stw   r27, 0xC8(r30)         ; maaShotTimes[k].miCount = 50  (full)
// loop_inner:                    ; i = 0..49
//   r3 = r30; r4 = r31
//   bl    Array<int,50>::GetItem ; &maaShotTimes[k].maElements[i]
//   r31 += 1
//   stw   r29, 0(r3)             ; *element = 0
//   if r31 < 50 goto loop_inner
//   r28 -= 1; r30 += 0xCC
//   if r28 != 0 goto loop_outer
//
// The count word is set to 50 BEFORE the element writes (so the checked GetItem sees a
// fully-constructed, full-length list); each of the 50 ints is then zeroed in turn.
// ----------------------------------------------------------------------------
void
ShotSelector::Construct(const DirectorResourceManager* lpResourceManager)
{
    mpResourceManager = lpResourceManager;           // stw r4, 0x268(this)

    for (u32 luList = 0; luList < KU_NUM_SHOT_LISTS; ++luList)
    {
        Array<s32, KU_SHOT_LIST_SIZE>& lrList = maaShotTimes[luList];
        lrList.SetFullCount();                       // stw 50, 0xC8(base)  -> miCount = 50
        for (u32 luSlot = 0; luSlot < KU_SHOT_LIST_SIZE; ++luSlot)
        {
            lrList.GetItem(luSlot) = 0;              // *(&maElements[slot]) = 0
        }
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ShotSelector::Prepare (DWARF cpp:73)
//   PS3 @0x15000:  li r0, 0 ; stw r0, 0x264(this) ; li r3, 1 ; blr
//   X360: inlined by MainDirector::Prepare stage 1 -- `stwx r28(=0), r31, 0x12454` @0x8224FBE4.
// Restart the use clock; the per-shot stamps Construct zeroed are kept.
// ----------------------------------------------------------------------------
bool
ShotSelector::Prepare()
{
    miCurrentTimeID = 0;
    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::ShotSelector::Update (DWARF cpp:89)
//   PS3 @0x2E4AC: ++miCurrentTimeID; if (camera.mShotSelectionInfo.miType != -1)
//                 tail-call SetLastUsedToNow(camera.mShotSelectionInfo)
//   X360: inlined by MainDirector::Update @0x82274FB0..0x82274FE4 (lwz/addi/stw 0x264, then the
//   `cmpwi var_3BC, -1` test and Array<int,50>::GetItem(type * 0xCC + this, id) = miCurrentTimeID).
// Once per published frame: tick the clock and stamp the shot the published camera came from, so
// GetCrashShot's least-recently-used tie-break prefers the others.
// ----------------------------------------------------------------------------
void
ShotSelector::Update(const Camera::Camera& lrSelectedCamera)
{
    ++miCurrentTimeID;
    if (lrSelectedCamera.mShotSelectionInfo.miType != -1)
    {
        SetLastUsedToNow(lrSelectedCamera.mShotSelectionInfo);
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::ShotSelector::SetLastUsedToNow (DWARF cpp:322)
//   PS3 @0x2E33C: maaShotTimes[info.miType].GetItem(info.miId) = miCurrentTimeID (the two
//   CgsArray.h:548/:549 GetItem tripwires are the Array's own).
// ----------------------------------------------------------------------------
void
ShotSelector::SetLastUsedToNow(const Camera::Camera::ShotSelectionInfo& lrShotSelectionInfo)
{
    maaShotTimes[lrShotSelectionInfo.miType].GetItem(static_cast<u32>(lrShotSelectionInfo.miId)) = miCurrentTimeID;
}

// @ 0x822396F8 -- score the chosen group's attrib ShotList and return the best shot.
//
// Ranking (asm @0x82239A04..0x82239AA4): a candidate that passes the exclusion /
// required property masks wins when its EVENT suitability (set bits of
// shotEventFlags & lxCrashEventFlags) beats the best so far; ties fall to the
// PROPERTY suitability (set bits of shotProperties & lxPreferredShotProperties);
// full ties fall to the least-recently-used stamp.
Camera::Camera::ShotReference* ShotSelector::GetCrashShot(
    u32 lxCrashEventFlags,
    Camera::Camera::ShotSelectionInfo* lpShotSelectionInfoOut,
    u32 lxPreferredShotProperties,
    u32 lxExcludedShotProperties,
    u32 lxRequiredShotProperties,
    EGroup leShotGroup) const
{
    // Non-gating tripwire (cpp:123).
    CGS_ASSERT(lpShotSelectionInfoOut != NULL, "lpShotSelectionInfoOut != NULL");

    u32 luIndex = 0;
    s32 liLeastRecentTime = miCurrentTimeID;
    s32 liBestShotPropertySuitability = -1;
    Camera::Camera::ShotReference* lpBestShot = NULL;
    s32 liBestShotEventSuitability = -1;

    // Pick the group's shotgroup bank (X360: direct manager+1432/+1416/+1400 loads --
    // the inlined GetSlow/GetNormal/GetFastCrashShots accessors).
    const Attrib::Gen::shotgroup* lpCrashShots = NULL;
    switch (leShotGroup)
    {
    case E_GROUP_LOW_ENERGY_CRASH:
        lpCrashShots = &mpResourceManager->GetSlowCrashShots();
        break;
    case E_GROUP_NORMAL_CRASH:
        lpCrashShots = &mpResourceManager->GetNormalCrashShots();
        break;
    case E_GROUP_HIGH_ENERGY_CRASH:
        lpCrashShots = &mpResourceManager->GetFastCrashShots();
        break;
    default:
        // cpp:154 -- the X360 streams "unhandled group:" + the value; folded static.
        CGS_ASSERT(false, "unhandled group:");
        break;
    }
    // Non-gating (cpp:158): the X360 carries on with the null group after firing.
    CGS_ASSERT(lpCrashShots != NULL, "lpCrashShots != NULL");

    while (luIndex < lpCrashShots->Num_ShotList())
    {
        Camera::Camera::ShotReference* lpShot = lpCrashShots->ShotList(luIndex);

        // The candidate's {group, index} pair -- staged up front (the X360 keeps it
        // in a stack qword and stores it wholesale on a win).
        Camera::Camera::ShotSelectionInfo lCameraShotSelectionInfo;
        lCameraShotSelectionInfo.miType = static_cast<s32>(leShotGroup);
        lCameraShotSelectionInfo.miId   = static_cast<s32>(luIndex);

        const s32 liTimeLastUsed = maaShotTimes[leShotGroup].GetItem(luIndex);

        // Pull the shot's event/property masks off the RefSpec'd generated class
        // (the ShotList mixes iceanim and proceduralshot elements; anything else is
        // skipped).
        u32 lxShotEventFlags;
        u32 lxShotProperties;
        if (lpShot->GetClassKey() == static_cast<u64>(Attrib::Gen::iceanim::ClassKey()))
        {
            const Attrib::Gen::iceanim lCandidateShot(*lpShot, NULL);
            lxShotEventFlags = lCandidateShot.SuitableFor();
            lxShotProperties = lCandidateShot.ShotProperties();
        }
        else if (lpShot->GetClassKey() == static_cast<u64>(Attrib::Gen::proceduralshot::ClassKey()))
        {
            const Attrib::Gen::proceduralshot lCandidateShot(*lpShot, NULL);
            lxShotEventFlags = lCandidateShot.SuitableFor();
            lxShotProperties = lCandidateShot.ShotProperties();
        }
        else
        {
            ++luIndex;
            continue;
        }

        // Hard filters: no excluded property, all required properties.
        if ((lxShotProperties & lxExcludedShotProperties) == 0 &&
            (lxShotProperties & lxRequiredShotProperties) == lxRequiredShotProperties)
        {
            const s32 liPropertySuitablity =
                CgsNumeric::BitOperations::GetNumberOfSetBits(lxShotProperties & lxPreferredShotProperties);
            const s32 liEventSuitablity =
                CgsNumeric::BitOperations::GetNumberOfSetBits(lxShotEventFlags & lxCrashEventFlags);

            const bool lbUsedLessRecently       = liTimeLastUsed < liLeastRecentTime;
            const bool lbBestMatchForEvent      = liEventSuitablity > liBestShotEventSuitability;
            const bool lbBestMatchForProperties = liPropertySuitablity > liBestShotPropertySuitability;
            const bool lbGoodMatchForEvent      = liEventSuitablity >= liBestShotEventSuitability;
            const bool lbGoodMatchForProperties = liPropertySuitablity >= liBestShotPropertySuitability;

            if (lbBestMatchForEvent ||
                (lbGoodMatchForEvent && lbBestMatchForProperties) ||
                (lbGoodMatchForEvent && lbGoodMatchForProperties && lbUsedLessRecently))
            {
                lpBestShot                    = lpShot;
                liBestShotPropertySuitability = liPropertySuitablity;
                liBestShotEventSuitability    = liEventSuitablity;
                liLeastRecentTime             = liTimeLastUsed;
                *lpShotSelectionInfoOut       = lCameraShotSelectionInfo;
            }
        }

        ++luIndex;
    }

    if (lpBestShot == NULL)
    {
        // The debug dump of what was asked for, then the tripwire (cpp:305).
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "\nCould not find a suitable shot!\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Event flags:";
        PrintShotEventFlags(lxCrashEventFlags);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Preferred properties:";
        PrintShotProperties(lxPreferredShotProperties);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "Required properties:";
        PrintShotProperties(lxRequiredShotProperties);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "\n\n";

        CGS_ASSERT(false, "Could not find a suitable shot");
    }

    return lpBestShot;
}

} // namespace BrnDirector
