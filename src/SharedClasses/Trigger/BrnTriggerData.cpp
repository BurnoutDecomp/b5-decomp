#include "SharedClasses/Trigger/BrnTriggerData.h"
#include "SharedClasses/Trigger/BrnLandmark.h"      // complete Landmark (GetOnlineLandmark walk, GetLandmarkFromRegionIndex cast)
#include "SharedClasses/Trigger/BrnTriggerBase.h"    // TriggerRegion::GetType() / E_TYPE_LANDMARK
#include "SharedClasses/Trigger/BrnKillzone.h"       // complete Killzone (GetKillzone stride)
#include "SharedClasses/Trigger/BrnSpawnLocation.h"  // complete SpawnLocation (GetSpawnLocation stride)
#include "SharedClasses/Trigger/BrnGenericRegion.h"  // complete GenericRegion (GetGenericRegion stride == 0x38)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (GetOnlineLandmark / FixUp message build)
#include <cstdint>                                   // uintptr_t (load-time pointer relocation arithmetic)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTrigger::TriggerData::GetRegion                 @ 0x8230E490
//   BrnTrigger::TriggerData::GetLandmarkFromRegionIndex @ 0x8231B648
//   BrnTrigger::TriggerData::GetKillzone               @ 0x82354820
//   BrnTrigger::TriggerData::GetOnlineLandmark         @ 0x824EAA00
//   BrnTrigger::TriggerData::FindLandmark              @ 0x82675738
//   BrnTrigger::TriggerData::FixDown                   @ 0x8267F800
//   BrnTrigger::TriggerData::FixUp                     @ 0x8267FC08

// X360 0x8230E490. Returns the liRegionIndex'th entry of the region table (mppRegions @0x74).
const BrnTrigger::TriggerRegion*
BrnTrigger::TriggerData::GetRegion( int liRegionIndex ) const
{
    CGS_ASSERT( liRegionIndex < miRegionCount, "liRegionIndex < miRegionCount" );
    return mppRegions[liRegionIndex];
}

// X360 0x8231B648. Looks up the region-table entry at liRegionIndex and re-types it as a
// Landmark, asserting that the entry's type really is E_TYPE_LANDMARK.
const BrnTrigger::Landmark*
BrnTrigger::TriggerData::GetLandmarkFromRegionIndex( int liRegionIndex ) const
{
    CGS_ASSERT( liRegionIndex < miRegionCount, "liRegionIndex < miRegionCount" );

    const TriggerRegion* lpTriggerRegion = mppRegions[liRegionIndex];
    CGS_ASSERT( lpTriggerRegion->GetType() == TriggerRegion::E_TYPE_LANDMARK,
                "lpTriggerRegion->GetType() == TriggerRegion::E_TYPE_LANDMARK" );

    return static_cast<const Landmark*>( lpTriggerRegion );
}

// X360 0x82354820. &mpKillzones[liKillzoneIndex] (Killzone is 16 bytes; see BrnKillzone.h).
const BrnTrigger::Killzone*
BrnTrigger::TriggerData::GetKillzone( int liKillzoneIndex ) const
{
    CGS_ASSERT( liKillzoneIndex < miKillzoneCount, "liKillzoneIndex < miKillzoneCount" );
    return &mpKillzones[liKillzoneIndex];
}

// GetSpawnLocation. No standalone symbol exists in the X360 image -- like every other
// &mp<Table>[i] accessor on this struct it is inlined at the call site (CarSelectManager::
// SetupSpawnLocations walks [0, GetSpawnLocationCount()) through it to file each junkyard's
// spawn points). Reconstructed as the exact sibling of GetKillzone @0x82354820, whose
// range-guard idiom (`index < count`) is X360-attested, over the mpSpawnLocations table @0x6C
// with its count at 0x70. The elements are records, not pointers (contrast mppRegions).
const BrnTrigger::SpawnLocation*
BrnTrigger::TriggerData::GetSpawnLocation( int liIndex ) const
{
    CGS_ASSERT( liIndex < miSpawnLocationCount, "liIndex < miSpawnLocationCount" );
    return &mpSpawnLocations[liIndex];
}

// GetGenericRegion. Same story as GetSpawnLocation above: no standalone X360 symbol, because it
// is inlined at every call site. Recovered from GameStateModule::FindNearestJunkyardID
// @0x8236BB48..0x8236BB80, which open-codes it as
//     assert(liGenericRegionIndex < *(triggerData + 0x48));   // BrnTriggerData.h:495 (0x1EF)
//     region = *(triggerData + 0x44) + liGenericRegionIndex * 0x38;
// The 0x38 stride is exactly sizeof(GenericRegion) (36-byte BoxRegion + 8-byte TriggerRegion tail
// + 12-byte GenericRegion tail), and 0x44/0x48 are mpGenericRegions/miGenericRegionCount. The
// assert TEXT and LINE below are the X360's verbatim.
const BrnTrigger::GenericRegion*
BrnTrigger::TriggerData::GetGenericRegion( int liIndex ) const
{
    // (X360 assert line BrnTriggerData.h:495; plain CGS_ASSERT to match the committed
    // GetKillzone / GetSpawnLocation siblings in this TU.)
    CGS_ASSERT( liIndex < miGenericRegionCount, "liGenericRegionIndex < miGenericRegionCount" );
    return &mpGenericRegions[liIndex];
}

// X360 0x824EAA00. Returns the liLandmarkIndex'th ONLINE landmark: walks the full landmark
// list, counting only the ones whose IsOnline() flag is set, and returns the one whose running
// online-index equals liLandmarkIndex. On miss it builds a diagnostic string and fires the
// assert, then returns the first landmark (the X360 `result = mpLandmarks`).
//
// NOTE: the X360 streams the miss diagnostic into the global CgsDev::Assert::gpcMessageBuffer.
// That global has no committed home, so -- matching the committed CgsID.cpp / BrnGameStateSharedIO.cpp
// precedent -- the message is built into a local stack buffer via CgsDev::StrStream instead;
// behaviorally identical.
const BrnTrigger::Landmark*
BrnTrigger::TriggerData::GetOnlineLandmark( int liLandmarkIndex ) const
{
    CGS_ASSERT( liLandmarkIndex < miLandmarkCount, "liLandmarkIndex < miLandmarkCount" );
    CGS_ASSERT( liLandmarkIndex < miOnlineLandmarkCount, "liLandmarkIndex < miOnlineLandmarkCount" );

    int liOnlineIndex = 0;
    for ( int liIndex = 0; liIndex < miLandmarkCount; ++liIndex )
    {
        const Landmark* lpLandmark = &mpLandmarks[liIndex];
        if ( lpLandmark->IsOnline() )
        {
            if ( liOnlineIndex == liLandmarkIndex )
                return lpLandmark;
            ++liOnlineIndex;
        }
    }

    // Not found: build the diagnostic into a local assert buffer and fire.
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStream << "Couldn't find online landmark " << liLandmarkIndex
                << " out of " << miOnlineLandmarkCount << "\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            425 );
        CgsDev::Assert::EndAssert();
    }

    return &mpLandmarks[0];
}

// X360 0x82675738. Linear scan of the landmark table for one whose id (TriggerRegion::mId,
// at landmark offset 0x24) equals lId. The X360 walks raw landmark records with a 52-byte
// stride starting at the mId field (mpLandmarks + 0x24); modelling it through the typed
// accessor &mpLandmarks[i].GetId() is identical. On an empty table OR a miss the X360 returns
// the BASE pointer mpLandmarks (== &mpLandmarks[0]), NOT null -- both fall-through paths load
// `*(a1 + 0x30)` (mpLandmarks). On a hit it returns &mpLandmarks[index].
//
// id compare: TriggerRegion stores mId as int32_t and the X360 sign-extends it (extsw) before
// an unsigned-64 compare (cmpld) against the CgsID argument. GetId() returns
// static_cast<CgsID>(mId), which is exactly that int32_t -> u64 sign-extend, so the typed
// compare reproduces the console semantics.
const BrnTrigger::Landmark*
BrnTrigger::TriggerData::FindLandmark( CgsID lId ) const
{
    if ( miLandmarkCount <= 0 )
        return mpLandmarks;

    for ( int liLandmarkIndex = 0; liLandmarkIndex < miLandmarkCount; ++liLandmarkIndex )
    {
        if ( mpLandmarks[liLandmarkIndex].GetId() == lId )
            return &mpLandmarks[liLandmarkIndex];
    }

    return mpLandmarks;
}


// -----------------------------------------------------------------------------
// RELOCATION VIEWS (FixUp / FixDown).
//
// Landmark / SignatureStunt / Killzone keep their pointer members private in their own
// owning headers, so the relocation loops reach them through these local views -- the same
// idiom the committed BrnProgression::ProgressionData::FixDown uses.
//
// ⚠️ THESE ARE HOST OFFSETS, NOT CONSOLE OFFSETS. The lane-data port widens every
// serialised pointer slot to 64 bits, so Landmark grew 52 -> 64 bytes (mpaStartingGrids
// 0x2C -> 0x30), SignatureStunt 24 -> 32 (mppStuntElements 0x10, count 0x14 -> 0x18) and
// Killzone 16 -> 32 (mppTriggers 0x00, count 0x04 -> 0x08; mpRegionIds 0x08 -> 0x10, count
// 0x0C -> 0x18). Leaving the old console offsets here would have walked every one of these
// records at the wrong stride against the widened TRIGGERS.DAT -- bug class (c). The sizeof
// asserts below are the contract with tools/assets/bundles/lane_transcode.py's emitter.
// -----------------------------------------------------------------------------
namespace
{
    struct LandmarkRelocationView
    {
        u8        _0[0x30];          // TriggerRegion subobject (44 B) + pad to the pointer
        uintptr_t mpaStartingGrids;  // host +0x30 (console +0x2C)
        s8        miStartingGridCount;
        u8        muDesignIndex;
        u8        muDistrict;
        u8        mu8Flags;
        u8        _pad[4];
    };
    static_assert(sizeof(LandmarkRelocationView) == 0x40, "Landmark host stride");

    struct SignatureStuntRelocationView
    {
        u8        _0[0x10];             // CgsID mId (8) + int64 miCamera (8)
        uintptr_t mppStuntElements;     // host +0x10
        s32       miStuntElementCount;  // host +0x18 (console +0x14)
        s32       _pad;
    };
    static_assert(sizeof(SignatureStuntRelocationView) == 0x20, "SignatureStunt host stride");

    struct KillzoneRelocationView
    {
        uintptr_t mppTriggers;      // host +0x00
        s32       miTriggerCount;   // host +0x08 (console +0x04)
        s32       _pad0;
        uintptr_t mpRegionIds;      // host +0x10 (console +0x08)
        s32       miRegionIdCount;  // host +0x18 (console +0x0C)
        s32       _pad1;
    };
    static_assert(sizeof(KillzoneRelocationView) == 0x20, "Killzone host stride");
}

// X360 0x8267F800. Load-time pointer relocation "fix DOWN": converts every stored pointer from
// an absolute load address back to a serialised file offset by SUBTRACTING the load-base delta
// (luDelta). It is the exact inverse of FixUp. The X360 inlined every per-element FixDown into
// this routine (xrefs_from shows no element-FixDown calls), so the inner rebasing is open-coded
// here over the element arrays, matching the console shape.
//
// Order (mirrors the asm): per-landmark starting-grid pointer; per-signature-stunt element
// array + each element pointer; the genericregion / blackspot / vfxbox / roaming / spawn count
// loops (bounds asserts only -- those element types carry no relocatable pointers reachable
// here); per-killzone trigger array + region-id array + each trigger pointer; the region table
// (each TriggerRegion* entry); finally the nine top-level array base pointers.
//
// Pointer relocation is intrinsically address arithmetic, so each rebased pointer is handled via
// reinterpret_cast<uintptr_t>(p) - luDelta -- the established BrnProgressionData::FixDown idiom.
// Killzone/SignatureStunt internal pointers are private to their owning types, so they are reached
// through local relocation views laid out to those types' authoritative (DWARF) member offsets.
uintptr_t
BrnTrigger::TriggerData::FixDown( uintptr_t luDelta )
{
    // ---- Landmarks: rebase each landmark's starting-grid pointer.
    LandmarkRelocationView* lpLandmarks = reinterpret_cast<LandmarkRelocationView*>( mpLandmarks );
    for ( int liLandmarkIndex = 0; liLandmarkIndex < miLandmarkCount; ++liLandmarkIndex )
    {
        CGS_ASSERT( liLandmarkIndex < miLandmarkCount, "liLandmarkIndex < miLandmarkCount" );
        lpLandmarks[liLandmarkIndex].mpaStartingGrids -= luDelta;
    }

    // ---- Signature stunts: rebase the stunt-element array and every element pointer in it.
    SignatureStuntRelocationView* lpSignatureStunts =
        reinterpret_cast<SignatureStuntRelocationView*>( mpSignatureStunts );
    for ( int liSignatureStuntIndex = 0; liSignatureStuntIndex < miSignatureStuntCount; ++liSignatureStuntIndex )
    {
        CGS_ASSERT( liSignatureStuntIndex < miSignatureStuntCount, "liSignatureStuntIndex < miSignatureStuntCount" );
        SignatureStuntRelocationView& lrStunt = lpSignatureStunts[liSignatureStuntIndex];
        uintptr_t* lpElements = reinterpret_cast<uintptr_t*>( lrStunt.mppStuntElements );
        for ( int liElementIndex = 0; liElementIndex < lrStunt.miStuntElementCount; ++liElementIndex )
            lpElements[liElementIndex] -= luDelta;
        lrStunt.mppStuntElements -= luDelta;
    }

    // ---- Generic regions: bounds-checked count walk only (no relocatable inner pointers here).
    for ( int liGenericRegionIndex = 0; liGenericRegionIndex < miGenericRegionCount; ++liGenericRegionIndex )
    {
        CGS_ASSERT( liGenericRegionIndex < miGenericRegionCount, "liGenericRegionIndex < miGenericRegionCount" );
    }

    // ---- Killzones: rebase the trigger array, every trigger pointer in it, and the region-id array.
    KillzoneRelocationView* lpKillzones = reinterpret_cast<KillzoneRelocationView*>( mpKillzones );
    for ( int liKillzoneIndex = 0; liKillzoneIndex < miKillzoneCount; ++liKillzoneIndex )
    {
        CGS_ASSERT( liKillzoneIndex < miKillzoneCount, "liKillzoneIndex < miKillzoneCount" );
        KillzoneRelocationView& lrKillzone = lpKillzones[liKillzoneIndex];
        uintptr_t* lpTriggers = reinterpret_cast<uintptr_t*>( lrKillzone.mppTriggers );
        for ( int liTriggerIndex = 0; liTriggerIndex < lrKillzone.miTriggerCount; ++liTriggerIndex )
            lpTriggers[liTriggerIndex] -= luDelta;
        lrKillzone.mppTriggers -= luDelta;
        lrKillzone.mpRegionIds -= luDelta;
    }

    // ---- Blackspots / VFX-box regions / roaming locations / spawn locations: count walks only.
    for ( int liBlackspotIndex = 0; liBlackspotIndex < miBlackspotCount; ++liBlackspotIndex )
    {
        CGS_ASSERT( liBlackspotIndex < miBlackspotCount, "liBlackspotIndex < miBlackspotCount" );
    }
    for ( int liVFXBoxRegionIndex = 0; liVFXBoxRegionIndex < miVFXBoxRegionCount; ++liVFXBoxRegionIndex )
    {
        CGS_ASSERT( liVFXBoxRegionIndex < miVFXBoxRegionCount, "liVFXBoxRegionIndex < miVFXBoxRegionCount" );
    }
    for ( int liRoamingLocationIndex = 0; liRoamingLocationIndex < miRoamingLocationCount; ++liRoamingLocationIndex )
    {
        CGS_ASSERT( liRoamingLocationIndex < miRoamingLocationCount, "liRoamingLocationIndex < miRoamingLocationCount" );
    }
    for ( int liSpawnLocationIndex = 0; liSpawnLocationIndex < miSpawnLocationCount; ++liSpawnLocationIndex )
    {
        CGS_ASSERT( liSpawnLocationIndex < miSpawnLocationCount, "liSpawnLocationIndex < miSpawnLocationCount" );
    }

    // ---- Region table: rebase each TriggerRegion* entry (mppRegions[i]).
    {
        uintptr_t* lpRegions = reinterpret_cast<uintptr_t*>( mppRegions );
        for ( int liRegionIndex = 0; liRegionIndex < miRegionCount; ++liRegionIndex )
            lpRegions[liRegionIndex] -= luDelta;
    }

    // ---- Nine top-level array base pointers (reached by name).
    mpLandmarks        = reinterpret_cast<Landmark*>( reinterpret_cast<uintptr_t>( mpLandmarks ) - luDelta );
    mpSignatureStunts  = reinterpret_cast<SignatureStunt*>( reinterpret_cast<uintptr_t>( mpSignatureStunts ) - luDelta );
    mpGenericRegions   = reinterpret_cast<GenericRegion*>( reinterpret_cast<uintptr_t>( mpGenericRegions ) - luDelta );
    mpKillzones        = reinterpret_cast<Killzone*>( reinterpret_cast<uintptr_t>( mpKillzones ) - luDelta );
    mpBlackspots       = reinterpret_cast<Blackspot*>( reinterpret_cast<uintptr_t>( mpBlackspots ) - luDelta );
    mpVFXBoxRegions    = reinterpret_cast<VFXBoxRegion*>( reinterpret_cast<uintptr_t>( mpVFXBoxRegions ) - luDelta );
    mpRoamingLocations = reinterpret_cast<RoamingLocation*>( reinterpret_cast<uintptr_t>( mpRoamingLocations ) - luDelta );
    mpSpawnLocations   = reinterpret_cast<SpawnLocation*>( reinterpret_cast<uintptr_t>( mpSpawnLocations ) - luDelta );
    mppRegions         = reinterpret_cast<TriggerRegion**>( reinterpret_cast<uintptr_t>( mppRegions ) - luDelta );

    return luDelta;
}

// X360 0x8267FC08. Load-time pointer relocation "fix UP": converts every stored pointer from a
// serialised file offset to its absolute load address by ADDING the load-base delta (luDelta).
// It is the exact inverse of FixDown.
//
// Version guard: the routine first asserts miVersionNumber == KI_VERSION_NUMBER (34). The X360
// builds the mismatch diagnostic into the global CgsDev::Assert::gpcMessageBuffer; matching the
// committed GetOnlineLandmark precedent in this TU, the message is built into a local stack
// buffer via CgsDev::StrStream instead (behaviorally identical). The version-mismatch assert
// carries the .cpp file/line "..\\unity\\../../SharedClasses/Trigger/BrnTriggerData.cpp":168.
//
// The X360 rebases the nine top-level array base pointers FIRST (before walking the arrays), so
// the per-element loops below dereference already-fixed-up bases. Inner per-element rebasing is
// open-coded over relocation views, same as FixDown.
uintptr_t
BrnTrigger::TriggerData::FixUp( uintptr_t luDelta )
{
    if ( miVersionNumber != KI_VERSION_NUMBER )
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStream << "Trigger data version mismatch. Expected: " << static_cast<s32>( KI_VERSION_NUMBER )
                << ", got: " << static_cast<s32>( miVersionNumber );
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../../SharedClasses/Trigger/BrnTriggerData.cpp",
            168 );
        CgsDev::Assert::EndAssert();
    }

    // ---- Nine top-level array base pointers FIRST (X360 fixes the bases before walking arrays).
    mpLandmarks        = reinterpret_cast<Landmark*>( reinterpret_cast<uintptr_t>( mpLandmarks ) + luDelta );
    mpSignatureStunts  = reinterpret_cast<SignatureStunt*>( reinterpret_cast<uintptr_t>( mpSignatureStunts ) + luDelta );
    mpGenericRegions   = reinterpret_cast<GenericRegion*>( reinterpret_cast<uintptr_t>( mpGenericRegions ) + luDelta );
    mpKillzones        = reinterpret_cast<Killzone*>( reinterpret_cast<uintptr_t>( mpKillzones ) + luDelta );
    mpBlackspots       = reinterpret_cast<Blackspot*>( reinterpret_cast<uintptr_t>( mpBlackspots ) + luDelta );
    mpVFXBoxRegions    = reinterpret_cast<VFXBoxRegion*>( reinterpret_cast<uintptr_t>( mpVFXBoxRegions ) + luDelta );
    mpRoamingLocations = reinterpret_cast<RoamingLocation*>( reinterpret_cast<uintptr_t>( mpRoamingLocations ) + luDelta );
    mpSpawnLocations   = reinterpret_cast<SpawnLocation*>( reinterpret_cast<uintptr_t>( mpSpawnLocations ) + luDelta );
    mppRegions         = reinterpret_cast<TriggerRegion**>( reinterpret_cast<uintptr_t>( mppRegions ) + luDelta );

    // ---- Landmarks: rebase each landmark's starting-grid pointer.
    LandmarkRelocationView* lpLandmarks = reinterpret_cast<LandmarkRelocationView*>( mpLandmarks );
    for ( int liLandmarkIndex = 0; liLandmarkIndex < miLandmarkCount; ++liLandmarkIndex )
    {
        CGS_ASSERT( liLandmarkIndex < miLandmarkCount, "liLandmarkIndex < miLandmarkCount" );
        lpLandmarks[liLandmarkIndex].mpaStartingGrids += luDelta;
    }

    // ---- Signature stunts: rebase the stunt-element array, then every element pointer in it.
    SignatureStuntRelocationView* lpSignatureStunts =
        reinterpret_cast<SignatureStuntRelocationView*>( mpSignatureStunts );
    for ( int liSignatureStuntIndex = 0; liSignatureStuntIndex < miSignatureStuntCount; ++liSignatureStuntIndex )
    {
        CGS_ASSERT( liSignatureStuntIndex < miSignatureStuntCount, "liSignatureStuntIndex < miSignatureStuntCount" );
        SignatureStuntRelocationView& lrStunt = lpSignatureStunts[liSignatureStuntIndex];
        lrStunt.mppStuntElements += luDelta;
        uintptr_t* lpElements = reinterpret_cast<uintptr_t*>( lrStunt.mppStuntElements );
        for ( int liElementIndex = 0; liElementIndex < lrStunt.miStuntElementCount; ++liElementIndex )
            lpElements[liElementIndex] += luDelta;
    }

    // ---- Generic regions: bounds-checked count walk only.
    for ( int liGenericRegionIndex = 0; liGenericRegionIndex < miGenericRegionCount; ++liGenericRegionIndex )
    {
        CGS_ASSERT( liGenericRegionIndex < miGenericRegionCount, "liGenericRegionIndex < miGenericRegionCount" );
    }

    // ---- Killzones: rebase the trigger array + region-id array, then every trigger pointer.
    KillzoneRelocationView* lpKillzones = reinterpret_cast<KillzoneRelocationView*>( mpKillzones );
    for ( int liKillzoneIndex = 0; liKillzoneIndex < miKillzoneCount; ++liKillzoneIndex )
    {
        CGS_ASSERT( liKillzoneIndex < miKillzoneCount, "liKillzoneIndex < miKillzoneCount" );
        KillzoneRelocationView& lrKillzone = lpKillzones[liKillzoneIndex];
        lrKillzone.mppTriggers += luDelta;
        lrKillzone.mpRegionIds += luDelta;
        uintptr_t* lpTriggers = reinterpret_cast<uintptr_t*>( lrKillzone.mppTriggers );
        for ( int liTriggerIndex = 0; liTriggerIndex < lrKillzone.miTriggerCount; ++liTriggerIndex )
            lpTriggers[liTriggerIndex] += luDelta;
    }

    // ---- Blackspots / roaming locations / spawn locations / VFX-box regions: count walks only.
    // (X360 FixUp orders these blackspot, roaming, spawn, vfxbox -- bounds asserts only.)
    for ( int liBlackspotIndex = 0; liBlackspotIndex < miBlackspotCount; ++liBlackspotIndex )
    {
        CGS_ASSERT( liBlackspotIndex < miBlackspotCount, "liBlackspotIndex < miBlackspotCount" );
    }
    for ( int liRoamingLocationIndex = 0; liRoamingLocationIndex < miRoamingLocationCount; ++liRoamingLocationIndex )
    {
        CGS_ASSERT( liRoamingLocationIndex < miRoamingLocationCount, "liRoamingLocationIndex < miRoamingLocationCount" );
    }
    for ( int liSpawnLocationIndex = 0; liSpawnLocationIndex < miSpawnLocationCount; ++liSpawnLocationIndex )
    {
        CGS_ASSERT( liSpawnLocationIndex < miSpawnLocationCount, "liSpawnLocationIndex < miSpawnLocationCount" );
    }
    for ( int liVFXBoxRegionIndex = 0; liVFXBoxRegionIndex < miVFXBoxRegionCount; ++liVFXBoxRegionIndex )
    {
        CGS_ASSERT( liVFXBoxRegionIndex < miVFXBoxRegionCount, "liVFXBoxRegionIndex < miVFXBoxRegionCount" );
    }

    // ---- Region table: rebase each TriggerRegion* entry (mppRegions[i]).
    {
        uintptr_t* lpRegions = reinterpret_cast<uintptr_t*>( mppRegions );
        for ( int liRegionIndex = 0; liRegionIndex < miRegionCount; ++liRegionIndex )
            lpRegions[liRegionIndex] += luDelta;
    }

    return luDelta;
}
