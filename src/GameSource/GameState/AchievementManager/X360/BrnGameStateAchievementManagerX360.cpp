// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/X360/
//     BrnGameStateAchievementManagerX360.cpp
// ============================================================================
// BrnGameState::AchievementManagerX360 -- the Xbox 360 concrete achievement
// manager (derives from AchievementManagerBase). This TU reconstructs the four
// self-contained functions:
//
//   AchievementEarnt   (X360 0x82367000, base vtable slot 0)
//   IsAchievementEarnt (X360 0x82367240, base vtable slot 1)
//   Prepare            (X360 0x82372DD0)
//   Release            (X360 0x82372E28)
//
// The two achievement bit sets are 50-entry CgsContainers::BitArray. The X360
// build inlines every BitArray probe/mutate at the call site AND emits the
// container's internal bounds asserts here (the committed CgsBitArray.h stays
// assert-free by design -- see its banner); those bounds guards are restored as
// CGS_ASSERT call-site asserts, mirroring the sibling StreamedVaultAllocator /
// SceneSweeper reconstructions. The dynamic streamed "invalid index : <i> < 50" /
// "Index: <i>, Number of bits: 50" messages collapse to the project's static
// bit-array-bound token "luIndex < NUMBITS" (attested verbatim in this same TU's
// WriteAchievements asm, and used identically by StreamedVaultAllocator).
//
// THREE FUNCTIONS ARE BLOCKED (declaration-only in the header, NOT reconstructed):
//   * GetAchievementsFromX360Api (0x8238C990) and WriteAchievements (0x8238CEA0)
//     both pivot on the X360 rodata table unk_8202B068 -- an achievement-dwId
//     <-> local-bit-index dispatch table walked against the "leEnumIndex <=
//     E_IMAGE_GALLERY_TYPE_COUNT" end-marker string. Neither the table BYTES nor
//     its LENGTH are in the dossier / dwarfdump / data exports. Guessing them would
//     corrupt the Xbox-Live achievement ids handed to XUserWriteAchievements, so
//     both are BLOCKED (a wrong id table is worse than an honest gap).
//   * Update (0x823967C0) depends on the un-homed
//     GameStateModule::GetControllerToGameStateInterface() and its returned
//     interface's +0xD8 signed-in-user-index member, and drives the two blocked
//     pumps above; BLOCKED.
//
// Original home (from the asm assert strings):
//   ..\GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.cpp
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.h"

// Win32/XDK CloseHandle (real prototype lives in <winbase.h>); declared as an
// extern "C" free function, mirroring the CgsGuideIntegration / System glue
// precedent. mhEnumerator is a raw HANDLE (void*).
extern "C" int CloseHandle(void* lhObject);

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// AchievementEarnt  (X360 0x82367000, base vtable slot 0)
//   Queue leAchievement into mAchievementsToWrite the first time it is seen. If it
//   is already flagged as earnt, fire the "written twice" dev assert and DO NOT
//   re-queue it (the X360 returns without touching the queue in that case).
// ----------------------------------------------------------------------------
void AchievementManagerX360::AchievementEarnt(EAchievement leAchievement)
{
    const u32 luIndex = static_cast<u32>(leAchievement);

    // BitArray<50>::IsBitSet's inlined bounds guard (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");

    if (mAchievementsEarnt.IsBitSet(luIndex))
    {
        // X360 line 544 -- streamed "Trying to write achievement index <i> more
        // than once."; collapsed to the project's static string.
        CGS_ASSERT(false, "Trying to write achievement index more than once.");
    }
    else
    {
        // BitArray<50>::SetBit's inlined bounds guard (CgsBitArray.h:222).
        CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
        mAchievementsToWrite.SetBit(luIndex);
    }
}

// ----------------------------------------------------------------------------
// IsAchievementEarnt  (X360 0x82367240, base vtable slot 1)
//   True iff leAchievement's bit is set in EITHER tracking set: mAchievementsEarnt
//   (already flushed to Xbox Live) OR mAchievementsToWrite (queued this frame).
//   Callers use this to avoid double-queuing an id via AchievementEarnt.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::IsAchievementEarnt(EAchievement leAchievement)
{
    const u32 luIndex = static_cast<u32>(leAchievement);

    // BitArray<50>::IsBitSet's inlined bounds guard (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsEarnt.GetCapacity(), "luIndex < NUMBITS");
    if (mAchievementsEarnt.IsBitSet(luIndex))
    {
        return true;
    }

    // Second probe -- same inlined bounds guard on the write queue (CgsBitArray.h:203).
    CGS_ASSERT(luIndex < mAchievementsToWrite.GetCapacity(), "luIndex < NUMBITS");
    if (mAchievementsToWrite.IsBitSet(luIndex))
    {
        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// Prepare  (X360 0x82372DD0, called by GameStateModule::Prepare)
//   Reset the manager to its idle pre-enumeration state and clear both bit sets.
//   Returns true.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::Prepare()
{
    miUserIndex   = -1;    // no signed-in user yet
    miState       = 0;     // idle
    mhEnumerator  = 0;     // no achievement enumerator open

    mOverLapped.Construct();

    mAchievementsEarnt.UnSetAll();
    mAchievementsToWrite.UnSetAll();

    return true;
}

// ----------------------------------------------------------------------------
// Release  (X360 0x82372E28, called by GameStateModule::Release)
//   Clear both bit sets, reset the user/state fields, close the achievement
//   enumerator handle if one is open, and re-construct the overlapped. Returns true.
// ----------------------------------------------------------------------------
bool AchievementManagerX360::Release()
{
    mAchievementsEarnt.UnSetAll();
    mAchievementsToWrite.UnSetAll();

    void* lhEnumerator = mhEnumerator;

    miUserIndex = -1;
    miState     = 0;

    if (lhEnumerator)
    {
        CloseHandle(lhEnumerator);
    }
    mhEnumerator = 0;

    mOverLapped.Construct();

    return true;
}

}
