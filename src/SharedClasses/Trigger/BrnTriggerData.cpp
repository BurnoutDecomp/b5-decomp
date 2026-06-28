#include "SharedClasses/Trigger/BrnTriggerData.h"
#include "SharedClasses/Trigger/BrnLandmark.h"      // complete Landmark (GetOnlineLandmark walk, GetLandmarkFromRegionIndex cast)
#include "SharedClasses/Trigger/BrnTriggerBase.h"    // TriggerRegion::GetType() / E_TYPE_LANDMARK
#include "SharedClasses/Trigger/BrnKillzone.h"       // complete Killzone (GetKillzone stride)
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

// X360 0x8267F800. Load-time pointer relocation "fix DOWN": converts every stored pointer from
// an absolute load address back to a serialised file offset by SUBTRACTING the load-base delta
// (liDelta). It is the exact inverse of FixUp. The X360 inlined every per-element FixDown into
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
// reinterpret_cast<uintptr_t>(p) - liDelta -- the established BrnProgressionData::FixDown idiom.
// Killzone/SignatureStunt internal pointers are private to their owning types, so they are reached
// through local relocation views laid out to those types' authoritative (DWARF) member offsets.
int
BrnTrigger::TriggerData::FixDown( int liDelta )
{
    // ---- Landmarks: rebase each landmark's starting-grid pointer (offset 0x2C in the 52B record).
    struct LandmarkRelocationView
    {
        u8        _0[0x2C];        // TriggerRegion subobject (BoxRegion + id + region index + type/pad)
        uintptr_t mpaStartingGrids;// 0x2C
    };
    LandmarkRelocationView* lpLandmarks = reinterpret_cast<LandmarkRelocationView*>( mpLandmarks );
    for ( int liLandmarkIndex = 0; liLandmarkIndex < miLandmarkCount; ++liLandmarkIndex )
    {
        CGS_ASSERT( liLandmarkIndex < miLandmarkCount, "liLandmarkIndex < miLandmarkCount" );
        lpLandmarks[liLandmarkIndex].mpaStartingGrids -= liDelta;
    }

    // ---- Signature stunts: rebase the stunt-element array and every element pointer in it.
    struct SignatureStuntRelocationView
    {
        u8        _0[0x10];        // CgsID mId (8) + int64 miCamera (8)
        uintptr_t mppStuntElements;// 0x10
        s32       miStuntElementCount; // 0x14   (24-byte record)
    };
    SignatureStuntRelocationView* lpSignatureStunts =
        reinterpret_cast<SignatureStuntRelocationView*>( mpSignatureStunts );
    for ( int liSignatureStuntIndex = 0; liSignatureStuntIndex < miSignatureStuntCount; ++liSignatureStuntIndex )
    {
        CGS_ASSERT( liSignatureStuntIndex < miSignatureStuntCount, "liSignatureStuntIndex < miSignatureStuntCount" );
        SignatureStuntRelocationView& lrStunt = lpSignatureStunts[liSignatureStuntIndex];
        uintptr_t* lpElements = reinterpret_cast<uintptr_t*>( lrStunt.mppStuntElements );
        for ( int liElementIndex = 0; liElementIndex < lrStunt.miStuntElementCount; ++liElementIndex )
            lpElements[liElementIndex] -= liDelta;
        lrStunt.mppStuntElements -= liDelta;
    }

    // ---- Generic regions: bounds-checked count walk only (no relocatable inner pointers here).
    for ( int liGenericRegionIndex = 0; liGenericRegionIndex < miGenericRegionCount; ++liGenericRegionIndex )
    {
        CGS_ASSERT( liGenericRegionIndex < miGenericRegionCount, "liGenericRegionIndex < miGenericRegionCount" );
    }

    // ---- Killzones: rebase the trigger array, every trigger pointer in it, and the region-id array.
    struct KillzoneRelocationView
    {
        uintptr_t mppTriggers;    // 0x00
        s32       miTriggerCount; // 0x04
        uintptr_t mpRegionIds;    // 0x08
        s32       miRegionIdCount;// 0x0C   (16-byte record)
    };
    KillzoneRelocationView* lpKillzones = reinterpret_cast<KillzoneRelocationView*>( mpKillzones );
    for ( int liKillzoneIndex = 0; liKillzoneIndex < miKillzoneCount; ++liKillzoneIndex )
    {
        CGS_ASSERT( liKillzoneIndex < miKillzoneCount, "liKillzoneIndex < miKillzoneCount" );
        KillzoneRelocationView& lrKillzone = lpKillzones[liKillzoneIndex];
        uintptr_t* lpTriggers = reinterpret_cast<uintptr_t*>( lrKillzone.mppTriggers );
        for ( int liTriggerIndex = 0; liTriggerIndex < lrKillzone.miTriggerCount; ++liTriggerIndex )
            lpTriggers[liTriggerIndex] -= liDelta;
        lrKillzone.mppTriggers -= liDelta;
        lrKillzone.mpRegionIds -= liDelta;
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
            lpRegions[liRegionIndex] -= liDelta;
    }

    // ---- Nine top-level array base pointers (reached by name).
    mpLandmarks        = reinterpret_cast<Landmark*>( reinterpret_cast<uintptr_t>( mpLandmarks ) - liDelta );
    mpSignatureStunts  = reinterpret_cast<SignatureStunt*>( reinterpret_cast<uintptr_t>( mpSignatureStunts ) - liDelta );
    mpGenericRegions   = reinterpret_cast<GenericRegion*>( reinterpret_cast<uintptr_t>( mpGenericRegions ) - liDelta );
    mpKillzones        = reinterpret_cast<Killzone*>( reinterpret_cast<uintptr_t>( mpKillzones ) - liDelta );
    mpBlackspots       = reinterpret_cast<Blackspot*>( reinterpret_cast<uintptr_t>( mpBlackspots ) - liDelta );
    mpVFXBoxRegions    = reinterpret_cast<VFXBoxRegion*>( reinterpret_cast<uintptr_t>( mpVFXBoxRegions ) - liDelta );
    mpRoamingLocations = reinterpret_cast<RoamingLocation*>( reinterpret_cast<uintptr_t>( mpRoamingLocations ) - liDelta );
    mpSpawnLocations   = reinterpret_cast<SpawnLocation*>( reinterpret_cast<uintptr_t>( mpSpawnLocations ) - liDelta );
    mppRegions         = reinterpret_cast<TriggerRegion**>( reinterpret_cast<uintptr_t>( mppRegions ) - liDelta );

    return liDelta;
}

// X360 0x8267FC08. Load-time pointer relocation "fix UP": converts every stored pointer from a
// serialised file offset to its absolute load address by ADDING the load-base delta (liDelta).
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
int
BrnTrigger::TriggerData::FixUp( int liDelta )
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
    mpLandmarks        = reinterpret_cast<Landmark*>( reinterpret_cast<uintptr_t>( mpLandmarks ) + liDelta );
    mpSignatureStunts  = reinterpret_cast<SignatureStunt*>( reinterpret_cast<uintptr_t>( mpSignatureStunts ) + liDelta );
    mpGenericRegions   = reinterpret_cast<GenericRegion*>( reinterpret_cast<uintptr_t>( mpGenericRegions ) + liDelta );
    mpKillzones        = reinterpret_cast<Killzone*>( reinterpret_cast<uintptr_t>( mpKillzones ) + liDelta );
    mpBlackspots       = reinterpret_cast<Blackspot*>( reinterpret_cast<uintptr_t>( mpBlackspots ) + liDelta );
    mpVFXBoxRegions    = reinterpret_cast<VFXBoxRegion*>( reinterpret_cast<uintptr_t>( mpVFXBoxRegions ) + liDelta );
    mpRoamingLocations = reinterpret_cast<RoamingLocation*>( reinterpret_cast<uintptr_t>( mpRoamingLocations ) + liDelta );
    mpSpawnLocations   = reinterpret_cast<SpawnLocation*>( reinterpret_cast<uintptr_t>( mpSpawnLocations ) + liDelta );
    mppRegions         = reinterpret_cast<TriggerRegion**>( reinterpret_cast<uintptr_t>( mppRegions ) + liDelta );

    // ---- Landmarks: rebase each landmark's starting-grid pointer (offset 0x2C in the 52B record).
    struct LandmarkRelocationView
    {
        u8        _0[0x2C];
        uintptr_t mpaStartingGrids;// 0x2C
    };
    LandmarkRelocationView* lpLandmarks = reinterpret_cast<LandmarkRelocationView*>( mpLandmarks );
    for ( int liLandmarkIndex = 0; liLandmarkIndex < miLandmarkCount; ++liLandmarkIndex )
    {
        CGS_ASSERT( liLandmarkIndex < miLandmarkCount, "liLandmarkIndex < miLandmarkCount" );
        lpLandmarks[liLandmarkIndex].mpaStartingGrids += liDelta;
    }

    // ---- Signature stunts: rebase the stunt-element array, then every element pointer in it.
    struct SignatureStuntRelocationView
    {
        u8        _0[0x10];
        uintptr_t mppStuntElements;// 0x10
        s32       miStuntElementCount; // 0x14
    };
    SignatureStuntRelocationView* lpSignatureStunts =
        reinterpret_cast<SignatureStuntRelocationView*>( mpSignatureStunts );
    for ( int liSignatureStuntIndex = 0; liSignatureStuntIndex < miSignatureStuntCount; ++liSignatureStuntIndex )
    {
        CGS_ASSERT( liSignatureStuntIndex < miSignatureStuntCount, "liSignatureStuntIndex < miSignatureStuntCount" );
        SignatureStuntRelocationView& lrStunt = lpSignatureStunts[liSignatureStuntIndex];
        lrStunt.mppStuntElements += liDelta;
        uintptr_t* lpElements = reinterpret_cast<uintptr_t*>( lrStunt.mppStuntElements );
        for ( int liElementIndex = 0; liElementIndex < lrStunt.miStuntElementCount; ++liElementIndex )
            lpElements[liElementIndex] += liDelta;
    }

    // ---- Generic regions: bounds-checked count walk only.
    for ( int liGenericRegionIndex = 0; liGenericRegionIndex < miGenericRegionCount; ++liGenericRegionIndex )
    {
        CGS_ASSERT( liGenericRegionIndex < miGenericRegionCount, "liGenericRegionIndex < miGenericRegionCount" );
    }

    // ---- Killzones: rebase the trigger array + region-id array, then every trigger pointer.
    struct KillzoneRelocationView
    {
        uintptr_t mppTriggers;    // 0x00
        s32       miTriggerCount; // 0x04
        uintptr_t mpRegionIds;    // 0x08
        s32       miRegionIdCount;// 0x0C
    };
    KillzoneRelocationView* lpKillzones = reinterpret_cast<KillzoneRelocationView*>( mpKillzones );
    for ( int liKillzoneIndex = 0; liKillzoneIndex < miKillzoneCount; ++liKillzoneIndex )
    {
        CGS_ASSERT( liKillzoneIndex < miKillzoneCount, "liKillzoneIndex < miKillzoneCount" );
        KillzoneRelocationView& lrKillzone = lpKillzones[liKillzoneIndex];
        lrKillzone.mppTriggers += liDelta;
        lrKillzone.mpRegionIds += liDelta;
        uintptr_t* lpTriggers = reinterpret_cast<uintptr_t*>( lrKillzone.mppTriggers );
        for ( int liTriggerIndex = 0; liTriggerIndex < lrKillzone.miTriggerCount; ++liTriggerIndex )
            lpTriggers[liTriggerIndex] += liDelta;
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
            lpRegions[liRegionIndex] += liDelta;
    }

    return liDelta;
}
