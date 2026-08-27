// ChallengeList.cpp
// BrnResource::ChallengeList -- Construct / Destruct.
//
// Reconstructed from the X360 ARTIST build:
//   ChallengeList::Construct @ 0x82677D00  (executed in the boot trace)
//   ChallengeList::Destruct  @ 0x82677D78
//
// Both bodies in the binary are pointer-walk loops (the compiler strength-reduced
// the indexed member access into a marching pointer); they are re-rolled here into
// clean indexed loops over named members, which is semantic parity. The X360
// "return" of each function is the last assignment result -- a fastcall register
// artifact, not a real return value; both methods are void.
//
// The X360 inlined BaseResourcePtr::CreateFromHandle(&maStaticDataLists[i],
// &sentinel) at each iteration; the DecFIGS DWARF for both bodies shows the
// pre-inline call as ResourcePtr<ChallengeListResource>::operator=(...), i.e. the
// source was `maStaticDataLists[i] = skInvalidHandle;` (assign-from-ResourceHandle,
// which resets the ResourcePtr to that handle). That public assignment is the
// faithful source-level form and exactly the observable operation; reconstructed
// as such rather than calling the protected CreateFromHandle.

#include "SharedClasses/DataLists/ChallengeList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (used by value below)
#include "SharedClasses/DataLists/ChallengeListResourceType.h"          // ChallengeListResource (complete: operator-> + GetEntry)
#include "SharedClasses/DataLists/ChallengeListEntry.h"                 // ChallengeListEntry (complete: mChallengeID, content-bought type)
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT + BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (AddListResource's streamed overflow message)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint + Message::gxMessageFilterFlags (the two post-load notices)

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB25C -- the invalid/default resource-handle sentinel used to
    // reset each ResourcePtr to "no resource". PS3 DecFIGS resolves this sentinel:
    // ChallengeList::Construct/Destruct (0x811BF8 / 0x811AE4) call
    // BaseResourcePtr::CreateFromHandle(slot, &CgsResource::NULLResourcePtr.mHandle),
    // i.e. the X360 dword_82FFB25C IS CgsResource::NULLResourcePtr.mHandle -- the
    // engine's canonical null/invalid ResourceHandle. Modeled here as a file-local
    // default-constructed (zero) ResourceHandle, which is that null handle's value.
    // (CgsResource::NULLResourcePtr is not yet a defined global in this port; once it
    //  lands it should be referenced directly instead of this local.)
    const CgsResource::ResourceHandle skInvalidHandle = {};

    // The serialised entry stride the X360 multiplies by inside GetChallengeData
    // (`mulli r11, r11, 0xD8`), == sizeof(ChallengeListEntry).
    const u32 KU_CHALLENGE_LIST_ENTRY_STRIDE = 216;

    // The console's baked assert file for this TU (it ships as part of the `unity` build,
    // hence the path shape). Used only by the ONE streamed message in AddListResource; the
    // plain-string asserts go through CGS_ASSERT, which supplies __FILE__/__LINE__ per this
    // project's convention.
    const char* const KAC_CHALLENGELIST_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
        "../../SharedClasses/DataLists/ChallengeList.cpp";

    // ⛔ X360/DWARF ENUM DRIFT -- BINARY AUTHORITATIVE, and the committed
    // ChallengeListEntryAction::ECombineActionType is deliberately NOT redefined (the same
    // call this family already made for EChallengeActionType; see ChallengeListEntry.h's
    // GetActionType note). AddListResource's post-load pass is the ONLY writer of the
    // combine byte in the whole image, and the values it tests and stores do not fit the
    // committed (PS3-DWARF) enum, which ends at E_COMBINE_ACTION_INDEPENDENT == 4 /
    // E_COMBINE_ACTION_COUNT == 5:
    //   * it logs "CHAIN ON LAST ACTION" for the byte value 0   -> CHAIN really is 0 in both;
    //   * it logs "SIMULTANEOUS ON LAST ACTION" for the value 4 -> SIMULTANEOUS is 4 on the
    //     X360, not the committed 3, so the X360 enum has one extra enumerator below it;
    //   * it propagates the value 5 across every action of a challenge whose FIRST action
    //     carries 5, and
    //   * it stores 6 into the LAST action in three separate arms.
    // MEASURED over the shipped build/game/ONLINECHALLENGES.BNDL the authored combine bytes
    // are {0: 197, 1: 3, 3: 14, 4: 22, 5: 275} -- so 5 is a live authored value and the
    // committed COUNT of 5 is simply wrong for this build. Named by their X360 role and
    // used as raw byte values; RE-EXPRESS them as enumerators the moment the X360 enum is
    // decoded (its owner is the challenge-manager TU family, not this one).
    const u8 KU_COMBINE_CHAIN_X360        = 0;
    const u8 KU_COMBINE_SIMULTANEOUS_X360 = 4;
    const u8 KU_COMBINE_PROPAGATE_X360    = 5;
    const u8 KU_COMBINE_TERMINATOR_X360   = 6;
}

// ChallengeListResource::GetNumChallenges -- the count word at +0x00 (X360 reads it through
// the truncated accessor BrnResource::ChallengeListRes(a2) inside AddListResource).
u32 ChallengeListResource::GetNumChallenges() const
{
    return muNumChallenges;
}

// ChallengeListResource::GetEntry -- inlined inside ChallengeList::GetChallengeData on the
// X360 (no standalone symbol; the body is the `*(resource+4) + 216*index` tail). The entry
// array base is the FixUp-rebased 32-bit slot at +0x04, resolved through the project's
// low-4 GB PointerFromU32 convention. No bounds check here -- the caller (GetChallengeData)
// owns the index assert. Identical shape to the VehicleList / WheelList siblings.
const ChallengeListEntry* ChallengeListResource::GetEntry(s32 liEntryIndex) const
{
    const u8* lpBase = reinterpret_cast<const u8*>(static_cast<uintptr_t>(muEntriesOffset));
    return reinterpret_cast<const ChallengeListEntry*>(
        lpBase + KU_CHALLENGE_LIST_ENTRY_STRIDE * static_cast<u32>(liEntryIndex));
}

// ChallengeList::Construct @ 0x82677D00
void ChallengeList::Construct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle.
    for ( s32 liIndex = 0; liIndex < KI_MAX_CHALLENGE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }

    // X360: each slot's bought-flag <- 0, both indices <- -1.
    for ( s32 liIndex = 0; liIndex < KI_MAX_FREEBURN_CHALLENGES; ++liIndex )
    {
        maSlots[ liIndex ].mbBought     = false;
        maSlots[ liIndex ].miListIndex  = -1;
        maSlots[ liIndex ].miEntryIndex = -1;
    }

    // X360: a1[3256] = 0; a1[3257] = 0;
    miCount     = 0;
    miListCount = 0;
}

// ChallengeList::Destruct @ 0x82677D78
void ChallengeList::Destruct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle.
    for ( s32 liIndex = 0; liIndex < KI_MAX_CHALLENGE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }
}

// ============================================================================================
// ⭐ ChallengeList::AddListResource @ 0x8267B598 -- THE ONE FUNCTION THAT MAKES THE FREEBURN
// CHALLENGE TABLE REAL. Its only caller is GameDataModule::PrepareFreeburnChallengeList
// @0x8266C088 (Prepare stage 10). Two halves:
//
//   (1) REGISTRATION -- the same three steps the VehicleList/WheelList siblings do: bounds,
//       store the resource in the next list slot, then register one slot per challenge
//       (entry index = challenge ordinal, list index = this list) and bump the counts.
//       X360 offsets: `12*(miCount+86) + this` == &maSlots[miCount].miEntryIndex (because
//       12*(miCount+86) == 0x400 + 12*miCount + 8) and `12*miCount + this + 0x404` ==
//       &maSlots[miCount].miListIndex. mbBought is left untouched (only Construct zeroes it).
//
//   (2) POST-LOAD FIXUP -- what the two siblings do NOT have, and what makes landing the
//       reply handler without this function a lie: a pass over EVERY registered challenge
//       that (a) republishes muNumPlayers into both nibbles and (b) normalises the per-action
//       combine bytes, terminating the last action. The console runs it inside
//       AddListResource, so the data every consumer reads is ALREADY normalised; a table
//       registered without it would be subtly different data wearing the same shape.
//
// ⚠️ THE PASS WRITES INTO THE LOADED RESOURCE. That is the console's own behaviour -- it
// reads each record back through the CONST GetChallengeData and stores through the returned
// pointer -- so the const is stripped here at the one site, deliberately and visibly, rather
// than forking a non-const accessor the DWARF does not declare.
//
// ⚠️ The X360 bumps miCount BEFORE reading a challenge back through GetChallengeData (that
// accessor asserts index < miCount), exactly as VehicleList::AddListResource does.
// ============================================================================================
void ChallengeList::AddListResource( CgsResource::ResourcePtr<ChallengeListResource>& lrResource )
{
    // X360 @0x8267B5B0: if (miListCount >= 32) fire assert (ChallengeList.cpp:797).
    CGS_ASSERT( miListCount < KI_MAX_CHALLENGE_LISTS, "No space for more challenge lists\n" );

    const u32 luNumChallenges = lrResource->GetNumChallenges();

    // X360 @0x8267B60C: if (numChallenges + miCount > 1000) fire assert (ChallengeList.cpp:798).
    // This one is STREAMED, so it is built into a local assert buffer and fired directly --
    // the BrnGuiWorldDataController.cpp precedent for a streamed console message (the X360
    // streams into the global CgsDev::Assert::gpcMessageBuffer; a stack buffer is
    // behaviourally identical).
    if ( static_cast<s32>( luNumChallenges ) + miCount > KI_MAX_FREEBURN_CHALLENGES )
    {
        char lacMessageBuffer[ CgsDev::Assert::KI_MESSAGEBUFFERSIZE ];
        CgsDev::StrStream lStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStream << "Not enough space for " << static_cast<s32>( luNumChallenges )
                << " more challenges. Already have " << miCount << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert( lacMessageBuffer, KAC_CHALLENGELIST_FILE, 798 );
        CgsDev::Assert::EndAssert();
    }

    // X360 @0x8267B6E0: CreateFromHandle(&maStaticDataLists[miListCount], a2 + 0x14) -- the
    // inlined ResourcePtr copy-assign. Restored as the source-level operator=.
    maStaticDataLists[ miListCount ] = lrResource;

    // X360 @0x8267B6F4..0x8267B72C: one slot per challenge in this resource.
    for ( u32 luChallenge = 0; luChallenge < luNumChallenges; ++luChallenge )
    {
        maSlots[ miCount ].miEntryIndex = static_cast<s32>( luChallenge );
        maSlots[ miCount ].miListIndex  = miListCount;
        ++miCount;
    }

    // X360 @0x8267B738: ++miListCount, BEFORE the post-load pass (which resolves every slot
    // through maStaticDataLists, so the list must already be registered).
    ++miListCount;

    // ---- (2) the post-load fixup pass, over every registered challenge -------------------
    for ( s32 liChallenge = 0; liChallenge < miCount; ++liChallenge )
    {
        // X360: `result = GetChallengeData(this, liChallenge); if (!result) continue;`
        ChallengeListEntry* lpEntry =
            const_cast<ChallengeListEntry*>( GetChallengeData( liChallenge ) );
        if ( lpEntry == 0 )
        {
            continue;
        }

        // X360: read the LOW nibble, range-check it, store 17*n back -- i.e.
        // SetNumPlayers(GetNumPlayers()), which publishes the authored count into BOTH
        // nibbles. Both guards live in ChallengeListEntry.h (lines 874 / 876) and are
        // carried by the SetNumPlayers inline.
        const s32 liNumActions = lpEntry->GetNumActions();
        lpEntry->SetNumPlayers( lpEntry->GetNumPlayers() );

        // X360 `if (v24 == 1)`: a single-action challenge has nothing to chain, so its one
        // action is terminated outright.
        if ( liNumActions == 1 )
        {
            lpEntry->GetAction( 0 )->SetCombineAction(
                static_cast<ChallengeListEntryAction::ECombineActionType>(
                    KU_COMBINE_TERMINATOR_X360 ) );
            continue;
        }

        // X360 `if (*(entry + 3) == 5)`: when the FIRST action carries the propagate value,
        // every action of the challenge is forced to it.
        if ( static_cast<u8>( lpEntry->GetAction( 0 )->GetCombineAction() )
                 == KU_COMBINE_PROPAGATE_X360 )
        {
            for ( s32 liAction = 0; liAction < liNumActions; ++liAction )
            {
                lpEntry->GetAction( liAction )->SetCombineAction(
                    static_cast<ChallengeListEntryAction::ECombineActionType>(
                        KU_COMBINE_PROPAGATE_X360 ) );
            }
        }

        // X360: the last action may not be left CHAIN or SIMULTANEOUS -- there is nothing
        // after it to chain to / run simultaneously with. Each arm reports the offending
        // challenge id and then terminates the action. Both notices are gated on the
        // console's own `CgsDev::Message::gxMessageFilterFlags & 1`.
        const s32 liLastAction = liNumActions - 1;

        if ( static_cast<u8>( lpEntry->GetAction( liLastAction )->GetCombineAction() )
                 == KU_COMBINE_CHAIN_X360 )
        {
            if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "CHAIN ON LAST ACTION: Challenge ID: " << lpEntry->GetChallengeID() << "\n";
            }
            lpEntry->GetAction( liLastAction )->SetCombineAction(
                static_cast<ChallengeListEntryAction::ECombineActionType>(
                    KU_COMBINE_TERMINATOR_X360 ) );
        }

        if ( static_cast<u8>( lpEntry->GetAction( liLastAction )->GetCombineAction() )
                 == KU_COMBINE_SIMULTANEOUS_X360 )
        {
            if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "SIMULTANEOUS ON LAST ACTION: Challenge ID: " << lpEntry->GetChallengeID()
                    << "\n";
            }
            lpEntry->GetAction( liLastAction )->SetCombineAction(
                static_cast<ChallengeListEntryAction::ECombineActionType>(
                    KU_COMBINE_TERMINATOR_X360 ) );
        }
    }
}

// ChallengeList::GetChallengeCount -- X360-INLINED everywhere (it has no ledger row of its
// own; every caller open-codes the `lwz` of miCount, e.g. the loop bound GetChallengeIndex
// @0x82326168 re-reads each iteration). Declared at ChallengeList.h:95.
//
// ⭐ REPLACES the return-0 [[silent-drop-stub]] that lived in BrnFriendsListLinkGates.cpp
// (deleted in the same change). That gate's own DELETE-WHEN said "Land the body with that
// mount" -- this wave is that mount: GameDataModule Prepare stage 10 now fills the table, so
// answering 0 would no longer be "no challenges exist", it would be a lie about 458 of them.
s32 ChallengeList::GetChallengeCount() const
{
    return miCount;
}

// ---------------------------------------------------------------------------
// File-scope DLC entitlement state (FLAG -- foreign home).
//
// IsChallengeContentBought reads a small block of downloadable-content state that the
// X360 binary lays out from base byte_82FFA7F0:
//   dword_82FFA7F4   -- the owned/entitlement bitmask          (+0x04)
//   dword_82FFA7F8[] -- per-content required-bit values        (+0x08, indexed)
//   dword_82FFA80C   -- index into the array for content-type 1 (+0x1C)
//   dword_82FFA810   -- index into the array for content-type 2 (+0x20)
//   byte_82FFA870    -- "type-1 content bought" flag            (+0x80)
//   byte_82FFA871    -- "type-2 content bought" flag            (+0x81)
// These belong to the DLC subsystem (BrnResource::DLCManager / its translation unit),
// NOT to ChallengeList.
//
// ⭐ [gateui r4] THEY ARE NOW REAL ZERO-INITIALISED DEFINITIONS, NOT `extern` DECLARATIONS.
// The round-3 form left six UNDEF externals in ChallengeList.obj that NOTHING in the tree
// defines -- survivable only under `/Gy + /OPT:REF` and only while every caller stayed
// unmounted, which this wave changes. ChallengeList.cpp takes the HOST SEAT for them until
// the DLC manager is reconstructed; the moment it lands, it becomes the owner and these
// definitions move there (they stay non-static precisely so that hand-over is an
// `extern` away and the linker catches a duplicate).
//
// ⛔ FLAG -- THE CONSOLE'S RUNTIME WRITER IS THE DLC MANAGER, AND THERE IS NO DLC MANAGER
// IN THIS BUILD. Nothing here writes any of these six; on the X360 they are .data that the
// downloadable-content subsystem fills after the entitlement query. So they hold zero for
// the whole process.
//
// POLARITY SANITY-CHECKED AGAINST THE EXPORT, not assumed. IsChallengeContentBought
// @0x8267BC08, verbatim:
//     if ( v2 == 1 ) { if ( (dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA80C])
//                            != dword_82FFA7F8[dword_82FFA80C] ) return 0;
//                      v4 = byte_82FFA870; }
//     else           { if ( v2 != 2 ) return 1; ... v4 = byte_82FFA871; }
//     return v4 != 0;
// With every global zero: the required-bits value is 0, so `(0 & 0) != 0` is FALSE and the
// mask test does NOT early-out; the answer is then `byte_82FFA870/871 != 0` == FALSE.
// So zero yields "DLC-A and DLC-B challenges are NOT bought", while type 0 (the base-game
// challenges) still returns true unconditionally. That IS the asm's no-DLC behaviour, and
// it is the safe direction -- a build with no entitlement data must not unlock paid content.
//
// Extents of dword_82FFA7F8[]: the next named global in the block sits at +0x1C
// (dword_82FFA80C), so the array occupies +0x08..+0x1B == FIVE dwords. It is indexed only
// by the two index globals, both of which are zero here, so only element 0 is ever read in
// this build. The size is documentation of the console block, not a load-bearing bound.
u32  gauChallengeDlcOwnedMask       = 0;        // byte_82FFA7F4  (+0x04)
u32  gauChallengeDlcRequiredBits[5] = { 0, 0, 0, 0, 0 };  // byte_82FFA7F8[] (+0x08..+0x1B)
u32  guChallengeDlcIndexTypeA       = 0;        // byte_82FFA80C  (+0x1C)
u32  guChallengeDlcIndexTypeB       = 0;        // byte_82FFA810  (+0x20)
u8   gbChallengeContentBoughtTypeA  = 0;        // byte_82FFA870  (+0x80)
u8   gbChallengeContentBoughtTypeB  = 0;        // byte_82FFA871  (+0x81)

// ChallengeList::GetChallengeData(s32) @ 0x82326080
// X360: range-guard the index against miCount ("Index out of range", ChallengeList.h:162,
// non-fatal), then resolve the entry as
//   maStaticDataLists[ maSlots[liIndex].miListIndex ]->GetEntry( maSlots[liIndex].miEntryIndex )
// The list ResourcePtr is dereferenced through operator-> (the X360 BrnResource::ChallengeL
// helper @0x82324D20 == ResourcePtr<ChallengeListResource>::operator-> const, baked assert
// CgsResourcePtr.h:563), and the entry is reached by indexing mpEntries (216-byte stride).
const ChallengeListEntry* ChallengeList::GetChallengeData( s32 liIndex ) const
{
    CGS_ASSERT( liIndex >= 0 && liIndex < miCount, "Index out of range\n" );

    // [marked deviation] the console assert is log-and-continue and then indexes maSlots
    // anyway; on the PC host that is an out-of-bounds read of the owning module object.
    // Guard, exactly as the VehicleList sibling does.
    if ( liIndex < 0 || liIndex >= miCount )
    {
        return 0;
    }

    const ChallengeSlot& lrSlot = maSlots[ liIndex ];
    if ( lrSlot.miListIndex < 0 || lrSlot.miListIndex >= KI_MAX_CHALLENGE_LISTS )
    {
        return 0;   // [marked deviation] unregistered slot (Construct seeds -1)
    }

    // The entry is reached through the resource's own 32-bit entry-array slot (216-byte
    // stride) -- see ChallengeListResource::GetEntry. It used to be spelled
    // `&lpResource->mpEntries[...]` against an 8-byte host pointer the FixUp never wrote;
    // that read a garbage base. See the ⚠️ CORRECTED note in ChallengeListResourceType.h.
    return maStaticDataLists[ lrSlot.miListIndex ]->GetEntry( lrSlot.miEntryIndex );
}

// [gateui] ChallengeList::GetChallengeData(CgsID) -- the id-keyed overload.
// LEDGER IDENTITY: the X360 emitted it as the UNNAMED `sub_82337200` (the ledger's only
// `GetChallengeData` row, 0x82326080, is the s32 overload above). Its body is unambiguous:
//     0x82337214  bl    BrnResource__ChallengeList__GetChallengeIndex
//     0x82337218  mr    r4, r3
//     0x8233721C  cmpwi cr6, r4, 0
//     0x82337220  blt   -> li r3, 0 ; return NULL
//     0x82337228  bl    BrnResource__ChallengeList__GetChallengeData   (r3 = this, r4 = index)
// and its seven callers are exactly the id-keyed sites (ChallengeManager::ProcessEvent
// @0x8233D6A8, four FriendsListComponent entries, OnlineGameRoomPlayerInfo
// @0x824A4B60, and HudMessageAnalyzer::TriggerChallengeEndedMessage @0x82520078, whose
// baked assert text is literally "lpChallengeList->GetChallengeData( mCha...").
// Note it does NOT re-range-check: the s32 overload's own "Index out of range" assert is
// what guards the second hop, and a negative index short-circuits before it.
const ChallengeListEntry* ChallengeList::GetChallengeData( CgsID lID ) const
{
    const s32 liIndex = GetChallengeIndex( lID );
    if ( liIndex < 0 )
    {
        return 0;
    }
    return GetChallengeData( liIndex );
}

// ChallengeList::GetChallengeIndex(CgsID) @ 0x82326168
// X360: linear scan [0, miCount); return the first slot whose challenge data has
// GetChallengeData(i)->mChallengeID == lID (the 8-byte CgsID compared at entry +0xC0),
// else -1.
s32 ChallengeList::GetChallengeIndex( CgsID lID ) const
{
    for ( s32 liIndex = 0; liIndex < miCount; ++liIndex )
    {
        // [marked deviation] the console dereferences unconditionally; GetChallengeData now
        // has a range/unregistered-slot guard that can answer NULL (see its body).
        const ChallengeListEntry* lpEntry = GetChallengeData( liIndex );
        if ( lpEntry != 0 && lpEntry->GetChallengeID() == lID )
        {
            return liIndex;
        }
    }
    return -1;
}

// ChallengeList::IsChallengeContentBought(s32) @ 0x8267BC08
// X360: read the challenge's content-bought type (entry byte @0xD6). Type 1 / 2 each
// gate the challenge behind a downloadable-content entitlement: the content counts as
// "bought" only if (a) the owned-mask has ALL the required bits for that content's index
// AND (b) the matching per-type bought flag is set. Any other type value (0, ...) means
// the content is always available (returns true).
bool ChallengeList::IsChallengeContentBought( s32 liIndex ) const
{
    const u8 luType = GetChallengeData( liIndex )->GetContentBoughtType();

    if ( luType == ChallengeListEntry::E_CONTENT_BOUGHT_DLC_A )
    {
        const u32 luRequired = gauChallengeDlcRequiredBits[ guChallengeDlcIndexTypeA ];
        if ( ( gauChallengeDlcOwnedMask & luRequired ) != luRequired )
        {
            return false;
        }
        return gbChallengeContentBoughtTypeA != 0;
    }

    if ( luType == ChallengeListEntry::E_CONTENT_BOUGHT_DLC_B )
    {
        const u32 luRequired = gauChallengeDlcRequiredBits[ guChallengeDlcIndexTypeB ];
        if ( ( gauChallengeDlcOwnedMask & luRequired ) != luRequired )
        {
            return false;
        }
        return gbChallengeContentBoughtTypeB != 0;
    }

    return true;
}

} // namespace BrnResource
