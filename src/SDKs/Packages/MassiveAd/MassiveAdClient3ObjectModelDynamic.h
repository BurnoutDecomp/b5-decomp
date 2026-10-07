#pragma once

// MassiveAdClient3::CMassiveAdObjectModelDynamic -- a dynamic (rotation type 16) 3D-model
// placement (vendor middleware): a composite that spawns one CMassiveAdObjectModel slave per
// subscriber, hands each the next delivered asset and fans the placement operations out to
// the slaves.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModel.h"  // CMassiveAdObjectModel

namespace MassiveAdClient3
{

class CMassiveAdObjectSubscriber;

class CMassiveAdObjectModelDynamic : public CMassiveAdObjectModel
{
public:
    // uMinSize is not used: the console passes the leaf threshold from the not-yet-initialised
    // member (it keeps whatever the allocation held).
    CMassiveAdObjectModelDynamic(const char* pcName, int nInvElementID, unsigned short uMinSize,
                                 int nRotationType, CMassiveZoneManager* pZone);
    virtual ~CMassiveAdObjectModelDynamic();

    int Tick() override;
    int Resume() override;
    int Suspend() override;
    int SetAssetExpired(int nAssetId) override;
    int ReportImpressions() override;
    int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber) override;
    int GetNextAssetID() override;
    int Initialize() override;

    CMassiveAdObject* CreateSlaveM(CMassiveAdObjectSubscriber* pSubscriber);

private:
    int            mnCurrentAssetID;   // +0x64 asset id handed to the newest slave
    unsigned short muActiveAssetCount; // +0x68 rotation steps since the last wrap
    CMassiveList   mSlaveList;         // +0x6C spawned slave placements
};

} // namespace MassiveAdClient3
