#pragma once

// MassiveAdClient3::CMassiveAdObjectVideo -- a video ad placement (vendor middleware): a
// CMassiveAdObject with minimum-size / minimum-angle impression thresholds plus a maximum
// viewing distance.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

class CMassiveAdObjectVideo : public CMassiveAdObject
{
public:
    CMassiveAdObjectVideo(const char* pcName, int nInvElementID, unsigned short uMinSize, float fMinAngle,
                          float fMaxDistance, int nRotationType, CMassiveZoneManager* pZone);
    virtual ~CMassiveAdObjectVideo();

    int GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest) override;

protected:
    float          mfMaxDistance; // +0x60
    unsigned short muMinSize;     // +0x64
    float          mfMinAngle;    // +0x68
};

} // namespace MassiveAdClient3
