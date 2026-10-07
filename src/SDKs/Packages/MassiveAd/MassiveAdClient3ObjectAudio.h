#pragma once

// MassiveAdClient3::CMassiveAdObjectAudio -- an audio ad placement (vendor middleware): a
// CMassiveAdObject that accepts an impression once the listener is audible and within the
// maximum distance. Its constructor is inline (the console folds it into every allocation
// site).

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

class CMassiveAdObjectAudio : public CMassiveAdObject
{
public:
    CMassiveAdObjectAudio(const char* pcName, int nInvElementID, float fMaxDistance, int nRotationType,
                          CMassiveZoneManager* pZone)
        : CMassiveAdObject(pcName, nInvElementID, nRotationType, pZone)
        , mfMaxDistance(fMaxDistance)
    {
    }
    virtual ~CMassiveAdObjectAudio();

    int GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest) override;

protected:
    float mfMaxDistance; // +0x60
};

} // namespace MassiveAdClient3
