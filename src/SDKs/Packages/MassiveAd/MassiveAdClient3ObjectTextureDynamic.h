#pragma once

// MassiveAdClient3::CMassiveAdObjectTextureDynamic -- a dynamic (rotation type 16) texture placement
// (vendor middleware): a composite that spawns one CMassiveAdObjectTexture slave per subscriber,
// hands each the next delivered asset and fans the placement operations out to the slaves.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTexture.h"

namespace MassiveAdClient3
{

class CMassiveAdObjectTextureDynamic : public CMassiveAdObjectTexture
{
public:
    // The console passes the leaf thresholds from the not-yet-initialised members (they keep
    // whatever the allocation held); the composite never scores impressions itself.
    CMassiveAdObjectTextureDynamic(const char* pcName, int nInvElementID, int nRotationType,
                                   CMassiveZoneManager* pZone)
        : CMassiveAdObjectTexture(pcName, nInvElementID, muMinSize, mfMinAngle, nRotationType, pZone)
        , mnCurrentAssetID(0)
        , muActiveAssetCount(0)
    {
    }
    virtual ~CMassiveAdObjectTextureDynamic();

    int Initialize() ;
    int SetAssetExpired(int nAssetID) ;
    int Tick() ;
    int ReportImpressions() ;
    int Suspend() ;
    int Resume() ;
    int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber) ;
    int GetNextAssetID() ;

    CMassiveAdObject* CreateSlaveM(CMassiveAdObjectSubscriber* pSubscriber);

private:
    int            mnCurrentAssetID;   // asset id handed to the newest slave
    unsigned short muActiveAssetCount; // rotation steps since the last wrap
    CMassiveList   mSlaveList;         // spawned slave placements
};

} // namespace MassiveAdClient3
