#include "SharedClasses/Trigger/BrnTriggerData.h"
#include "SharedClasses/Trigger/BrnLandmark.h"      // complete Landmark (GetOnlineLandmark walk, GetLandmarkFromRegionIndex cast)
#include "SharedClasses/Trigger/BrnTriggerBase.h"    // TriggerRegion::GetType() / E_TYPE_LANDMARK
#include "SharedClasses/Trigger/BrnKillzone.h"       // complete Killzone (GetKillzone stride)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (GetOnlineLandmark message build)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTrigger::TriggerData::GetRegion                 @ 0x8230E490
//   BrnTrigger::TriggerData::GetLandmarkFromRegionIndex @ 0x8231B648
//   BrnTrigger::TriggerData::GetKillzone               @ 0x82354820
//   BrnTrigger::TriggerData::GetOnlineLandmark         @ 0x824EAA00

// X360 0x8230E490. Returns the liRegionIndex'th entry of the region table (mppRegions @0x74).
const BrnTrigger::TriggerRegion*
BrnTrigger::TriggerData::GetRegion( int liRegionIndex ) const
{
    if ( liRegionIndex >= miRegionCount )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRegionIndex < miRegionCount",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            624 );
        CgsDev::Assert::EndAssert();
    }
    return mppRegions[liRegionIndex];
}

// X360 0x8231B648. Looks up the region-table entry at liRegionIndex and re-types it as a
// Landmark, asserting that the entry's type really is E_TYPE_LANDMARK.
const BrnTrigger::Landmark*
BrnTrigger::TriggerData::GetLandmarkFromRegionIndex( int liRegionIndex ) const
{
    if ( liRegionIndex >= miRegionCount )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liRegionIndex < miRegionCount",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            624 );
        CgsDev::Assert::EndAssert();
    }

    const TriggerRegion* lpTriggerRegion = mppRegions[liRegionIndex];
    if ( lpTriggerRegion->GetType() != TriggerRegion::E_TYPE_LANDMARK )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpTriggerRegion->GetType() == TriggerRegion::E_TYPE_LANDMARK",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            615 );
        CgsDev::Assert::EndAssert();
    }

    return static_cast<const Landmark*>( lpTriggerRegion );
}

// X360 0x82354820. &mpKillzones[liKillzoneIndex] (Killzone is 16 bytes; see BrnKillzone.h).
const BrnTrigger::Killzone*
BrnTrigger::TriggerData::GetKillzone( int liKillzoneIndex ) const
{
    if ( liKillzoneIndex >= miKillzoneCount )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liKillzoneIndex < miKillzoneCount",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            533 );
        CgsDev::Assert::EndAssert();
    }
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
    if ( liLandmarkIndex >= miLandmarkCount )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liLandmarkIndex < miLandmarkCount",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            409 );
        CgsDev::Assert::EndAssert();
    }
    if ( liLandmarkIndex >= miOnlineLandmarkCount )
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liLandmarkIndex < miOnlineLandmarkCount",
            "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
            410 );
        CgsDev::Assert::EndAssert();
    }

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
