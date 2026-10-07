// MassiveAdClient3::CMassiveAdObjectSubscriber -- the closure-free members (base callbacks, impression
// record, inventory-element id); the constructor/destructor/GetCrexID/MediaDownloadComplete stay in the parent.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"

#include <cstring>  // memset, memcpy

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"   // CMassiveAdObject::mnInvElementID

namespace MassiveAdClient3
{

// Base download callbacks: MediaDownload accepts (returns 1), Tick does nothing.
int CMassiveAdObjectSubscriber::MediaDownload(int /*nAdvertId*/)
{
    return 1;
}

void CMassiveAdObjectSubscriber::Tick()
{
}

// Record a viewed impression: a null argument leaves the live record untouched (returns
// this); otherwise the 32-byte live record is cleared, then overwritten (returns the record).
void* CMassiveAdObjectSubscriber::SetImpression(void* pImpressionData)
{
    if (pImpressionData)
    {
        std::memset(macImpression, 0, sizeof(macImpression));
        return std::memcpy(macImpression, pImpressionData, sizeof(macImpression));
    }
    return this;
}

// Snapshot the live record and, when bClear is set, clear it; returns the snapshot.
void* CMassiveAdObjectSubscriber::GetImpression(int bClear)
{
    std::memcpy(macImpressionSnapshot, macImpression, sizeof(macImpressionSnapshot));
    if (bClear)
        std::memset(macImpression, 0, sizeof(macImpression));
    return macImpressionSnapshot;
}

// The delivered ad's inventory-element id, or 0 when no ad object is attached.
int CMassiveAdObjectSubscriber::GetInvElementID()
{
    if (mpAdObject)
        return mpAdObject->mnInvElementID;
    return 0;
}

} // namespace MassiveAdClient3
