#pragma once

// MassiveAdClient3::CMassiveAdObjectModel -- a 3D-model ad placement (vendor middleware): a
// CMassiveAdObject that accepts an impression once the model's on-screen size reaches the
// minimum. Its constructor is inline (the console folds it into every allocation site).

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

class CMassiveAdObjectModel : public CMassiveAdObject
{
public:
    CMassiveAdObjectModel(const char* pcName, int nInvElementID, unsigned short uMinSize, int nRotationType,
                          CMassiveZoneManager* pZone)
        : CMassiveAdObject(pcName, nInvElementID, nRotationType, pZone)
        , muMinSize(uMinSize)
    {
    }
    virtual ~CMassiveAdObjectModel();

    int GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest) override;

protected:
    unsigned int muMinSize; // +0x60
};

} // namespace MassiveAdClient3
