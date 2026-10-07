#pragma once

// MassiveAdClient3::CMassiveAdObjectAudioDynamic -- a dynamic (rotation type 16) audio placement
// (vendor middleware): a composite that spawns one CMassiveAdObjectAudio slave per subscriber,
// hands each the next delivered asset and fans the placement operations out to the slaves.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectAudio.h"

namespace MassiveAdClient3
{

class CMassiveAdObjectAudioDynamic : public CMassiveAdObjectAudio
{
public:
    // fMaxDistance is not used: the console passes the leaf threshold from the not-yet-initialised
    // member (it keeps whatever the allocation held).
    CMassiveAdObjectAudioDynamic(const char* pcName, int nInvElementID, float fMaxDistance, int nRotationType,
                                 CMassiveZoneManager* pZone);
    virtual ~CMassiveAdObjectAudioDynamic();

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
