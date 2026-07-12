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
// The X360 byte offsets are 32-bit-pointer/Xenon facts; on a 64-bit host the base,
// the embedded CgsXOverlapped, and the scratch buffers widen/re-pad, so members are
// accessed BY NAME and absolute offsets are NOT static_asserted (same rule as the
// sibling BrnNetwork::*ManagerX360 homes).
//
// BLOCKED FRONTIER (declaration-only here; NOT reconstructed -- see the .cpp banner):
//   * GetAchievementsFromX360Api / WriteAchievements pivot on the X360 rodata table
//     unk_8202B068, an achievement-dwId <-> local-bit-index dispatch table whose
//     bytes AND length (bounded by the "leEnumIndex <= E_IMAGE_GALLERY_TYPE_COUNT"
//     end-marker string) are unrecoverable. Guessing that table would corrupt the
//     Xbox-Live achievement ids, so both are BLOCKED, not fabricated.
//   * Update needs the un-homed GameStateModule::GetControllerToGameStateInterface()
//     and its returned interface's +0xD8 signed-in-user-index member; BLOCKED.
// ----------------------------------------------------------------------------

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"            // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/System/CgsXOverlapped.h"            // CgsSystem::CgsXOverlapped
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // base + EAchievement

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

    // ===== X360 lifecycle (hide the base's platform-neutral Prepare/Release) =====

    // X360 0x82372DD0 (called by GameStateModule::Prepare). Reset the manager to
    // idle: miUserIndex = -1, clear the state/handle, construct the overlapped, and
    // zero both tracking bit sets. Returns true.
    bool Prepare();

    // X360 0x82372E28 (called by GameStateModule::Release). Zero both bit sets,
    // reset miUserIndex/miState, close the enumerator handle if open, and
    // re-construct the overlapped. Returns true.
    bool Release();

    // ===== BLOCKED (declaration-only; see .cpp banner + header BLOCKED note) =====

    // X360 0x823967C0 (called by GameStateModule::PreWorldUpdate). Per-frame pump.
    // BLOCKED: depends on the un-homed GameStateModule::GetControllerToGameStateInterface()
    // + its +0xD8 user-index member, and drives the two table-gated pumps below.
    u32 Update(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
               GameStateModuleIO::OutputBuffer* lpOutput);

    // X360 0x8238C990. BLOCKED: parses the XEnumerate result against the unrecoverable
    // unk_8202B068 achievement-id dispatch table.
    bool GetAchievementsFromX360Api(GameStateModuleIO::OutputBuffer* lpOutput);

    // X360 0x8238CEA0. BLOCKED: fills XUSER_ACHIEVEMENT.dwAchievementId from the
    // unrecoverable unk_8202B068 table before XUserWriteAchievements.
    u32 WriteAchievements(GameStateModuleIO::OutputBuffer* lpOutput);

protected:
    // ===== the two base pure virtuals, realised over the bit sets (X360-attested) =====

    // X360 0x82367000 (vtable slot 0). Queue leAchievement into mAchievementsToWrite
    // the first time it is seen; assert if it is already flagged earnt.
    void AchievementEarnt(EAchievement leAchievement) override;

    // X360 0x82367240 (vtable slot 1). True iff leAchievement's bit is set in EITHER
    // mAchievementsEarnt or mAchievementsToWrite (i.e. earnt or already queued).
    bool IsAchievementEarnt(EAchievement leAchievement) override;

private:
    CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS> mAchievementsEarnt;   // +0x18
    CgsContainers::BitArray<KU_NUM_ACHIEVEMENTS> mAchievementsToWrite; // +0x20
    s32                                          miUserIndex;          // +0x28 (-1 == none)
    s32                                          miState;              // +0x2C (0/1/2)
    u32                                          mcbBuffer;            // +0x30 (bytes)
    void*                                        mhEnumerator;         // +0x34 (HANDLE)
    CgsSystem::CgsXOverlapped                    mOverLapped;          // +0x38 (28 bytes)

    // FLAG: scratch buffers touched ONLY by the BLOCKED enumerate/write pumps. Sizes
    // are X360-asm-attested (details stride 36 == 9 dwords from +0x54; the write buffer
    // begins at +0x75C == +0x54 + 50*36). Modelled as opaque storage so the layout is
    // documented; do NOT access these until GetAchievementsFromX360Api / WriteAchievements
    // are unblocked (the unk_8202B068 id table is recovered).
    u8 maAchievementDetails[KU_NUM_ACHIEVEMENTS * 36]; // +0x54  (XACHIEVEMENT_DETAILS[50])
    u8 maAchievementsToWrite[KU_NUM_ACHIEVEMENTS * 8]; // +0x75C (XUSER_ACHIEVEMENT[50])
};

}
