// MassiveAdClient3::XMassiveAdClientSubscriber -- the closure-free members (base callbacks, impression
// record, inventory-element id); the constructor/destructor/GetCrexID/MediaDownloadComplete stay in the parent.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"

#include <cstring>  // memset, memcpy

namespace MassiveAdClient3
{

// Base download callbacks: MediaDownload accepts (returns 1), Tick does nothing.
int XMassiveAdClientSubscriber::MediaDownload(int /*nAdvertId*/)
{
    return 1;
}

void XMassiveAdClientSubscriber::Tick()
{
}

void XMassiveAdClientSubscriber::SetImpression(MAD_Impression* pImpression)
{
    if (pImpression)
    {
        std::memset(&m_CurrentImpression, 0, sizeof(m_CurrentImpression));
        std::memcpy(&m_CurrentImpression, pImpression, sizeof(m_CurrentImpression));
    }
}

// Snapshot the live record and, when bClear is set, clear it; returns the snapshot.
MAD_Impression* XMassiveAdClientSubscriber::GetImpression(int bClear)
{
    std::memcpy(&m_CopyImpression, &m_CurrentImpression, sizeof(m_CopyImpression));
    if (bClear)
        std::memset(&m_CurrentImpression, 0, sizeof(m_CurrentImpression));
    return &m_CopyImpression;
}

// The delivered ad's inventory-element id, or 0 when no ad object is attached.
unsigned int XMassiveAdClientSubscriber::GetInvElementID()
{
    if (mpAdObject)
        return mpAdObject->mnInvElementID;
    return 0;
}

} // namespace MassiveAdClient3
