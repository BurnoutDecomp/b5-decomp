// MassiveAdClient3::CMassiveAdObjectVideo construction (vendor middleware); its impression
// scoring lives with the CMassiveAdObject bodies.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideo.h"

namespace MassiveAdClient3
{

CMassiveAdObjectVideo::CMassiveAdObjectVideo(const char* pcName, int nInvElementID, unsigned short uMinSize,
                                             float fMinAngle, float fMaxDistance, int nRotationType,
                                             CMassiveZoneManager* pZone)
    : CMassiveAdObject(pcName, nInvElementID, nRotationType, pZone)
    , mfMaxDistance(fMaxDistance)
    , muMinSize(uMinSize)
    , mfMinAngle(fMinAngle)
{
}

CMassiveAdObjectVideo::~CMassiveAdObjectVideo()
{
}

} // namespace MassiveAdClient3
