// MassiveAdClient3::CMassiveAdObjectVideoDynamic (vendor middleware): slave spawning, asset
// rotation and the fan-out of the placement operations to the slaves.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideoDynamic.h"

#include <cstring>
#include <new>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"

namespace MassiveAdClient3
{

CMassiveAdObjectVideoDynamic::~CMassiveAdObjectVideoDynamic()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        CMassiveAdObject* lpSlave = static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData());
        if (lpSlave)
            delete lpSlave;
        mSlaveList.GoToNext();
    }
    mSlaveList.RemoveAll();
}

// Spawns a slave carrying this placement's name, id and thresholds, gives it the next
// delivered asset and, when one is passed, the subscriber it serves.
CMassiveAdObject* CMassiveAdObjectVideoDynamic::CreateSlaveM(CMassiveAdObjectSubscriber* pSubscriber)
{
    void* lpSlaveMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectVideo));
    CMassiveAdObjectVideo* lpSlave =
        lpSlaveMemory ? ::new (lpSlaveMemory) CMassiveAdObjectVideo(mpcAdObjectName, mnInvElementID, muMinSize, mfMinAngle, mfMaxDistance, mnRotationType, 0) : 0;
    if (!lpSlave)
    {
        MassiveLog(2, GetName(), "ALLOCATION Failed for CMassiveAdObjectVideo.  MAO: %s", mpcAdObjectName);
        SetValid(E_STATE_INVALID);
        return 0;
    }

    lpSlave->mpMaster = this;
    mnCurrentAssetID = GetNextAssetID();
    lpSlave->AssetIDAdd(mnCurrentAssetID);
    if (pSubscriber)
        lpSlave->SubscriberAdd(pSubscriber);

    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    CMassiveListNode* lpNode = lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(lpSlave) : 0;
    mSlaveList.Append(lpNode);
    return lpSlave;
}

// The delivered asset after the one handed out last (wrapping to the first, which also
// restarts the step count), bounded by the primary match list size.
int CMassiveAdObjectVideoDynamic::GetNextAssetID()
{
    if (!muAssetIDCount || !mAssetIDList.GetCount())
        return 0;

    mAssetIDList.GoToStart();
    if (mnCurrentAssetID && muAssetIDCount >= 2)
    {
        bool lbWrap = true;
        unsigned short luStep = 1;
        while (mAssetIDList.GetCurrent())
        {
            if (luStep++ >= muAssetIDCount)
                break;

            if (*static_cast<int*>(mAssetIDList.GetCurrData()) == mnCurrentAssetID)
            {
                mAssetIDList.GoToNext();
                ++muActiveAssetCount;
                lbWrap = !mAssetIDList.GetCurrent();
                break;
            }
            mAssetIDList.GoToNext();
        }

        if (lbWrap)
        {
            mAssetIDList.GoToStart();
            muActiveAssetCount = 0;
        }
    }

    mnCurrentAssetID = *static_cast<int*>(mAssetIDList.GetCurrData());
    return mnCurrentAssetID;
}

int CMassiveAdObjectVideoDynamic::SetAssetExpired(int nAssetID)
{
    if (!CMassiveAdObject::SetAssetExpired(nAssetID))
        return 0;

    if (muActiveAssetCount)
        --muActiveAssetCount;
    else
        muActiveAssetCount = 0;
    if (mnCurrentAssetID == nAssetID)
        mnCurrentAssetID = 0;
    return 1;
}

// Ticks every slave; the first non-zero result stops the walk and is returned.
int CMassiveAdObjectVideoDynamic::Tick()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        int lnResult = static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData())->Tick();
        if (lnResult)
            return lnResult;
        mSlaveList.GoToNext();
    }
    return 0;
}

int CMassiveAdObjectVideoDynamic::ReportImpressions()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        int lnResult = static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData())->ReportImpressions();
        if (lnResult)
            return lnResult;
        mSlaveList.GoToNext();
    }
    return 0;
}

int CMassiveAdObjectVideoDynamic::Suspend()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData())->Suspend();
        mSlaveList.GoToNext();
    }
    // The console returns the list cursor's last value; no caller reads it.
    return 0;
}

int CMassiveAdObjectVideoDynamic::Resume()
{
    mSlaveList.GoToStart();
    while (mSlaveList.GetCurrent())
    {
        static_cast<CMassiveAdObject*>(mSlaveList.GetCurrData())->Resume();
        mSlaveList.GoToNext();
    }
    // The console returns the list cursor's last value; no caller reads it.
    return 0;
}

// Each subscriber gets its own slave.
int CMassiveAdObjectVideoDynamic::SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, "");
    CreateSlaveM(pSubscriber);
    return 0;
}

// Adopts the current zone and spawns a slave for every subscriber queued there under this
// placement's name, then starts reporting.
int CMassiveAdObjectVideoDynamic::Initialize()
{
    mpZone = CMassiveClientCore::Instance()->GetCurrentZone();
    if (mpZone)
    {
        mpZone->mPreSubscriberList.GoToStart();
        (void)std::strlen(mpcAdObjectName);
        while (mpZone->mPreSubscriberList.GetCurrent())
        {
            CMassiveAdObjectSubscriber* lpSubscriber =
                static_cast<CMassiveAdObjectSubscriber*>(mpZone->mPreSubscriberList.GetCurrData());
            if (!CompareStrings(mpcAdObjectName, lpSubscriber->mpcName))
                CreateSlaveM(lpSubscriber);
            mpZone->mPreSubscriberList.GoToNext();
        }
    }
    SetValid(E_STATE_REPORTING);
    // The console returns the list cursor's last value; no caller reads it.
    return 0;
}

} // namespace MassiveAdClient3
