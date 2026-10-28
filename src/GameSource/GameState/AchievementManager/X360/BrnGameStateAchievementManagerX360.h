#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/X360/
//     BrnGameStateAchievementManagerX360.{h,cpp}
// ============================================================================
// Canonical home for BrnGameState::AchievementManagerX360 -- the Xbox 360
// concrete achievement manager. It is the X360 sibling of the (data-table-gated)
// AchievementManagerPS3: it derives from BrnGameState::AchievementManagerBase and
// supplies the two protected virtuals (AchievementEarnt / IsAchievementEarnt)
// over its OWN pair of 50-bit CgsContainers::BitArray tracking sets, plus the
// X360-specific lifecycle (Prepare / Release) and the XDK achievement-enumerate /
// -write pump (Update -> GetAchievementsFromX360Api -> WriteAchievements).
//
// The base owns the vptr + four manager back-pointers (this+0x04..0x13); the X360
// members below begin at this+0x18 (every offset a store/load immediate in the
// BrnGameStateAchievementManagerX360.cpp ARTIST asm):
//
//   +0x18  mAchievementsEarnt    BitArray<50>  (IsAchievementEarnt reads (index/64)+3 qwords)
//   +0x20  mAchievementsToWrite  BitArray<50>  (AchievementEarnt sets (index/64)+4 qwords)
//   +0x28  miUserIndex           s32           (signed-in user index; -1 == none)
//   +0x2C  miState               s32           (0 idle / 1 enumerating / 2 done)
//   +0x30  mcbBuffer             u32           (enumeration buffer size, bytes)
//   +0x34  mhEnumerator          HANDLE        (XUserCreateAchievementEnumerator handle)
//   +0x38  mOverLapped           CgsXOverlapped (28-byte XOVERLAPPED async I/O)
//   +0x54  maAchievementDetails  (XACHIEVEMENT_DETAILS[50], 36B each -- enumerate scratch)
//   +0x75C maAchievementsToWrite (XUSER_ACHIEVEMENT[50], 8B each  -- write scratch)
//
// Native offsets are checked against the actual SDK records below.
//
// The per-frame pump: Update tracks the active controller's user index, and while that
// user is signed in and no asynchronous call is in flight, first downloads the user's
// earnt achievements (GetAchievementsFromX360Api: enumerate, then map each earnt
// achievement id back to its local bit through KAU_ACHIEVEMENT_IDS), then writes up to
// three queued achievements per call (WriteAchievements). The system calls go through the
// Xbox system-software APIs declared by the XDK.
// ----------------------------------------------------------------------------

#include "types.hpp"
#include <xtl.h>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"            // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/System/CgsXOverlapped.h"            // CgsSystem::CgsXOverlapped
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // base + EAchievement

// Use the original SDK record definitions and calling conventions.
CGS_STATIC_ASSERT((sizeof(XACHIEVEMENT_DETAILS) == 36), "X360 achievement details");
CGS_STATIC_ASSERT((offsetof(XACHIEVEMENT_DETAILS, ftAchieved) == 0x18), "ARTIST completion time");
CGS_STATIC_ASSERT((sizeof(XUSER_ACHIEVEMENT) == 8), "X360 achievement write record");

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// AchievementManagerX360 : public AchievementManagerBase
//
// The concrete X360 manager. The base dispatches every gameplay-event hook
// through the two protected virtuals overridden below; this class realises them
// as two 50-entry bit sets:
//   - mAchievementsEarnt   : achievements already flushed to Xbox Live.
//   - mAchievementsToWrite  : achievements queued (this frame) to be flushed.
// IsAchievementEarnt(id) reports "earnt OR queued" (either bit set); AchievementEarnt
// queues an id into mAchievementsToWrite (asserting if it is already flagged earnt).
// ---------------------------------------------------------------------------
class AchievementManagerX360 : public AchievementManagerBase
{
public:
    // The number of achievements the X360 SKU tracks (every bounds compare is `>= 0x32`).
    static const u32 KU_NUM_ACHIEVEMENTS = 50;
    // At most this many achievements go into one system write (the write-record count).
    static const u32 KU_MAX_ACHIEVEMENTS_PER_WRITE = 3;

    // The X360 inlines the derived-member initialization into GameStateModule::Construct
    // immediately after AchievementManagerBase::Construct (ARTIST 0x82380388, 0x82380738..64).
    void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                   StreetManager* lpStreetManager,
                   ScoringSystem* lpScoringSystem,
                   GameStateModule* lpGameStateModule);

    // ===== X360 lifecycle (hide the base's platform-neutral Prepare/Release) =====

    // X360 0x82372DD0 (called by GameStateModule::Prepare). Reset the manager to
    // idle: miUserIndex = -1, clear the state/handle, construct the overlapped, and
    // zero both tracking bit sets. Returns true.
    bool Prepare();

    // X360 0x82372E28 (called by GameStateModule::Release). Zero both bit sets,
    // reset miUserIndex/miState, close the enumerator handle if open, and
    // re-construct the overlapped. Returns true.
    bool Release();

    // ===== the per-frame achievement pump =====

    // Called by GameStateModule::PreWorldUpdate. Adopt the active
    // controller's user index (a change resets the pump to idle); while that user is signed
    // in and nothing is in flight, download the earnt set once, then write queued ids.
    void Update(const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    // Start (state 0) or poll (state 1) the achievement enumeration; when it
    // completes, rebuild mAchievementsEarnt from the achieved entries and report true (state 2).
    bool GetAchievementsFromX360Api(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    // Move up to three queued achievements into the write records, mark
    // them earnt, post their telemetry and the new earnt count, and start the system write.
    void WriteAchievements(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

protected:
    // ===== the two base pure virtuals, realised over the bit sets (X360-attested) =====

    // X360 0x82367000 (vtable slot 0). Queue leAchievement into mAchievementsToWrite
    // the first time it is seen; assert if it is already flagged earnt.
    void AchievementEarnt(EAchievement leAchievement) ;

    // X360 0x82367240 (vtable slot 1). True iff leAchievement's bit is set in EITHER
    // mAchievementsEarnt or mAchievementsToWrite (i.e. earnt or already queued).
    bool IsAchievementEarnt(EAchievement leAchievement) ;

private:
    CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS> mAchievementsEarnt;   // +0x18
    CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS> mAchievementsToWrite; // +0x20
    s32                                          miUserIndex;          // +0x28 (-1 == none)
    s32                                          miState;              // +0x2C (0/1/2)
    DWORD                                        mcbBuffer;            // +0x30 (bytes)
    HANDLE                                       mhEnumerator;         // +0x34 (HANDLE)
    CgsSystem::CgsXOverlapped                    mOverLapped;          // +0x38 (28 bytes)

    // Enumerate / write scratch (console: the details from +0x54 at a 36-byte stride, the write
    // records from +0x75C == +0x54 + 50 * 36; three of them, so the manager ends at +0x774 and
    // the module's road-rules manager follows at the next 8-byte boundary).
    XACHIEVEMENT_DETAILS maAchievementDetails[KU_NUM_ACHIEVEMENTS];            // +0x54
    XUSER_ACHIEVEMENT    maAchievementsToWrite[KU_MAX_ACHIEVEMENTS_PER_WRITE]; // +0x75C
    static void AssertNativeLayout()
    {
        CGS_STATIC_ASSERT((sizeof(AchievementManagerX360) == 0x778), "X360 achievement manager size");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, mAchievementsEarnt) == 0x18), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, mAchievementsToWrite) == 0x20), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, miUserIndex) == 0x28), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, miState) == 0x2C), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, mcbBuffer) == 0x30), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, mhEnumerator) == 0x34), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, mOverLapped) == 0x38), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, maAchievementDetails) == 0x54), "ARTIST achievement member");
        CGS_STATIC_ASSERT((offsetof(AchievementManagerX360, maAchievementsToWrite) == 0x75C), "ARTIST achievement member");
    }
};

}
