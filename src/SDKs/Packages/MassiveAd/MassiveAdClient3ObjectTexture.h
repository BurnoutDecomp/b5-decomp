#pragma once

// MassiveAdClient3::CMassiveAdObjectTexture -- a texture ad placement (vendor middleware): a
// CMassiveAdObject that accepts an impression once its on-screen size and facing reach the
// placement's (or the delivered asset's) minimum size / angle.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

class CMassiveAdObjectTexture : public CMassiveAdObject
{
public:
    CMassiveAdObjectTexture(const char* pcName, int nInvElementID, unsigned short uMinSize,
                            float fMinAngle, int nRotationType, CMassiveZoneManager* pZone);
    virtual ~CMassiveAdObjectTexture();

    int GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest) override;

protected:
    unsigned short muMinSize;  // +0x60
    float          mfMinAngle; // +0x64
};

} // namespace MassiveAdClient3
