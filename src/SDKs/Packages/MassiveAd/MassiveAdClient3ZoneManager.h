#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveZoneManager -- minimal owning home (vendor
// middleware).
//
// The full CMassiveZoneManager body (Tick / HandleResponse / HandleError /
// AssetFind / ReportImpressions / PreSubscriber{AssignT,Remove} / Resume /
// SetAssetExpired* / ...) is a SEPARATE ledger TU. This header only pins the two
// members CMassiveAdObjectSubscriber's constructor is attested to call on the
// client's current zone (same "surface pin" pattern as
// MassiveAdClient3ClientCore.h):
//
//   - MAOFind(name): @ 0x82BCEA30. Called as `currentZone->MAOFind(name)`
//     (this = the CMassiveZoneManager returned by
//     CMassiveClientCore::GetCurrentZone, r4 = the subscriber name). Returns the
//     matching ad object, or null.
//   - PreSubscriberAdd(subscriber): @ 0x82BCEA60. Called as
//     `currentZone->PreSubscriberAdd(subscriber)` when no ad object matched yet,
//     to queue the subscriber for a future ad.
//
// This is vendor/SDK code reconstructed in its canonical vendor home; the
// MassiveAdClient3 namespace and CMassiveZoneManager class name are external
// middleware identifiers PRESERVED VERBATIM.
// ===========================================================================

namespace MassiveAdClient3
{

class CMassiveAdObject;
class CMassiveAdObjectSubscriber;

class CMassiveZoneManager
{
public:
    // @ 0x82BCEA30. Finds the ad object named pcName within this zone. Body in
    // the CMassiveZoneManager TU.
    CMassiveAdObject* MAOFind(const char* pcName);

    // @ 0x82BCEA60. Queues a subscriber for an ad object that has not been
    // located yet. Body in the CMassiveZoneManager TU.
    int PreSubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber);
};

} // namespace MassiveAdClient3
