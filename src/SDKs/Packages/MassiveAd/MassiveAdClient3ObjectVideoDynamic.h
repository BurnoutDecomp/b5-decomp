#pragma once

// MassiveAdClient3::CMassiveAdObjectVideoDynamic -- a dynamic (rotation type 16) video placement
// (vendor middleware): a composite that spawns one CMassiveAdObjectVideo slave per subscriber,
// hands each the next delivered asset and fans the placement operations out to the slaves.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideo.h"

namespace MassiveAdClient3
{

class CMassiveAdObjectVideoDynamic : public CMassiveAdObjectVideo
{
public:
    // The console passes the size / angle thresholds from the not-yet-initialised members (they
    // keep whatever the allocation held); only the distance comes from the caller.
    CMassiveAdObjectVideoDynamic(const char* pcName, int nInvElementID, float fMaxDistance, int nRotationType,
                                 CMassiveZoneManager* pZone)
        : CMassiveAdObjectVideo(pcName, nInvElementID, muMinSize, mfMinAngle, fMaxDistance, nRotationType, pZone)
        , mnCurrentAssetID(0)
        , muActiveAssetCount(0)
    {
    }
    virtual ~CMassiveAdObjectVideoDynamic();

    int Initialize() override;
    int SetAssetExpired(int nAssetID) override;
    int Tick() override;
    int ReportImpressions() override;
    int Suspend() override;
    int Resume() override;
    int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber) override;
    int GetNextAssetID() override;

    CMassiveAdObject* CreateSlaveM(CMassiveAdObjectSubscriber* pSubscriber);

private:
    int            mnCurrentAssetID;   // asset id handed to the newest slave
    unsigned short muActiveAssetCount; // rotation steps since the last wrap
    CMassiveList   mSlaveList;         // spawned slave placements
};

} // namespace MassiveAdClient3
