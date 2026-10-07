// MassiveAdClient3::CMassiveAdObjectTexture construction (vendor middleware); its impression
// scoring lives with the CMassiveAdObject bodies.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTexture.h"

namespace MassiveAdClient3
{

CMassiveAdObjectTexture::CMassiveAdObjectTexture(const char* pcName, int nInvElementID, unsigned short uMinSize,
                                                 float fMinAngle, int nRotationType, CMassiveZoneManager* pZone)
    : CMassiveAdObject(pcName, nInvElementID, nRotationType, pZone)
    , muMinSize(uMinSize)
    , mfMinAngle(fMinAngle)
{
}

CMassiveAdObjectTexture::~CMassiveAdObjectTexture()
{
}

} // namespace MassiveAdClient3
