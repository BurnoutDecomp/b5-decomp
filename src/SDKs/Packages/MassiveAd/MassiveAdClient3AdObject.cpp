// MassiveAdClient3::CMassiveAdObject (vendor middleware): the ad placement's asset rotation,
// media download state machine and impression accounting, plus the media leaves' impression
// scoring (GetBestImpression).

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"

#include <cmath>
#include <cstring>
#include <new>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Asset.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectAudio.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModel.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTexture.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideo.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Record.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Subscriber.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ZoneManager.h"

namespace MassiveAdClient3
{

namespace
{

// Impression counters roll over into a fresh record past this many samples.
const unsigned short KU_MAX_IMPRESSION_SAMPLES = 40000;

// The subscriber's per-tick impression verdict (+0x54).
const int KI_IMPRESSION_NOT_SEEN  = 3;
const int KI_IMPRESSION_SEEN      = 1;
const int KI_IMPRESSION_QUALIFIED = 2;

// The diagonal of the reference screen the size thresholds are authored against.
const float KF_REFERENCE_DIAGONAL = 1280.0f;

} // anonymous namespace

// ===========================================================================
// CMassiveAdObject
// ===========================================================================

CMassiveAdObject::CMassiveAdObject(const char* pcName, int nInvElementID, int nRotationType,
                                   CMassiveZoneManager* pZone)
    : CMassiveBaseObject("CMassiveAdObject")
    , mpcAdObjectName(0)
    , mpMaster(0)
    , mnInvElementID(nInvElementID)
    , mpZone(pZone)
    , mpCurrentAsset(0)
    , mpDownloadAsset(0)
    , mnRotationType(nRotationType)
    , mnViewState(0)
    , mpCurrentRecord(0)
    , mbSuspended(0)
    , mbMediaDownloaded(0)
    , muAssetIDCount(0)
{
    if (!IsValidString(pcName))
    {
        SetLastError(-400, "");
        return;
    }

    unsigned int luLength = static_cast<unsigned int>(std::strlen(pcName)) + 1;
    mpcAdObjectName = static_cast<char*>(MassiveMalloc(luLength));
    if (!mpcAdObjectName)
    {
        MassiveLog(2, GetName(), "ALLOCATION Failed in Asset() for m_pName");
        SetValid(E_STATE_INVALID);
        return;
    }

    std::strncpy(mpcAdObjectName, pcName, luLength);
    if (!pZone)
        mpZone = CMassiveClientCore::Instance()->GetCurrentZone();
    MassiveLog(5, GetName(), "*ADDED MAO: %s", pcName);
}

CMassiveAdObject::~CMassiveAdObject()
{
    MassiveLog(5, GetName(), "*REMOVED MAO: %s", mpcAdObjectName);
    if (mpcAdObjectName)
    {
        MassiveFree(mpcAdObjectName);
        mpcAdObjectName = 0;
    }

    mAssetIDList.GoToStart();
    while (mAssetIDList.GetCurrent())
    {
        MassiveFree(mAssetIDList.GetCurrData());
        mAssetIDList.GoToNext();
    }
    mAssetIDList.RemoveAll();

    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mpAdObject = 0;
        mSubscriberList.GoToNext();
    }
    mSubscriberList.RemoveAll();
}

// Hands every subscriber queued on the current zone under this placement's name to pAdObject.
int CMassiveAdObject::BaseFindPreSubscribers(CMassiveAdObject* pAdObject)
{
    if (!CMassiveClientCore::Instance()->GetCurrentZone())
        return 0;
    return CMassiveClientCore::Instance()->GetCurrentZone()->PreSubscriberAssignT(pAdObject, mpcAdObjectName);
}

int CMassiveAdObject::Initialize()
{
    return BaseFindPreSubscribers(this);
}

// Drops a delivered asset id and releases this placement's hold on the asset's media.
int CMassiveAdObject::AssetIDRemove(int nAssetID)
{
    if (!nAssetID)
    {
        SetLastError(-600, "");
        return 0;
    }

    mAssetIDList.GoToStart();
    while (mAssetIDList.GetCurrent())
    {
        if (*static_cast<int*>(mAssetIDList.GetCurrData()) == nAssetID)
        {
            MassiveFree(mAssetIDList.GetCurrData());
            mAssetIDList.Remove(mAssetIDList.GetCurrent(), 1);

            CMassiveAsset* lpAsset = mpZone->AssetFind(nAssetID);
            if (lpAsset)
                lpAsset->mnRefCount = lpAsset->mnRefCount ? lpAsset->mnRefCount - 1 : 0;
            return 1;
        }
        mAssetIDList.GoToNext();
    }
    return 0;
}

int CMassiveAdObject::AssetIDAdd(int nAssetID)
{
    if (!nAssetID)
    {
        SetLastError(-600, "");
        return 0;
    }

    int* lpAssetID = static_cast<int*>(MassiveMalloc(sizeof(int)));
    if (!lpAssetID)
    {
        MassiveLog(2, GetName(), "ALLOCATION Failed in AssetIDAdd");
        return 0;
    }
    *lpAssetID = nAssetID;

    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    CMassiveListNode* lpNode = lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(lpAssetID) : 0;
    mAssetIDList.Append(lpNode);
    return 1;
}

// The asset id after the current one in delivery order (wrapping), bounded by the primary
// match list size; the first delivered id when nothing is current yet.
int CMassiveAdObject::GetNextAssetID()
{
    if (!mAssetIDList.GetCount())
    {
        SetLastError(-397, "No Assets for MAO: %s", mpcAdObjectName);
        SetValid(E_STATE_INVALID);
        return 0;
    }

    unsigned short luStep = 1;
    int lnNextID = 0;
    CMassiveAsset* lpAsset = mpDownloadAsset ? mpDownloadAsset : mpCurrentAsset;
    int lnCurrentID = lpAsset ? lpAsset->mnAssetId : 0;

    mAssetIDList.GoToStart();
    bool lbTakeCursor = false;
    if (!mpCurrentAsset && !mpDownloadAsset)
    {
        lbTakeCursor = mAssetIDList.GetCount() != 0;
    }
    else
    {
        bool lbFound = false;
        while (mAssetIDList.GetCurrent())
        {
            if (luStep++ >= muAssetIDCount)
                break;

            if (*static_cast<int*>(mAssetIDList.GetCurrData()) == lnCurrentID)
            {
                if (mAssetIDList.GetCurrent()->GetNext())
                    mAssetIDList.GoToNext();
                else
                    mAssetIDList.GoToStart();
                lnNextID = *static_cast<int*>(mAssetIDList.GetCurrData());
                lbFound = lnNextID != 0;
                break;
            }
            mAssetIDList.GoToNext();
        }

        if (!lbFound && mAssetIDList.GetCount())
        {
            mAssetIDList.GoToStart();
            lbTakeCursor = true;
        }
    }

    if (lbTakeCursor)
        lnNextID = *static_cast<int*>(mAssetIDList.GetCurrData());

    MassiveLog(5, GetName(), "MAO: %s -> Next AssetID: %d", mpcAdObjectName, lnNextID);
    return lnNextID;
}

int CMassiveAdObject::StateInit()
{
    if (!mpCurrentRecord && mpZone)
    {
        CMassiveAsset* lpDefaultAsset = mpZone->AssetFind(0);
        if (lpDefaultAsset)
            mpCurrentRecord = lpDefaultAsset->RecordFind(mnInvElementID);
    }

    if ((CMassiveClientCore::Instance()->IsFlagSet(2) || mSubscriberList.GetCount()) && !mpCurrentAsset &&
        !mpDownloadAsset)
    {
        SetValid(E_STATE_REQUEST_MEDIA);
    }
    return 0;
}

int CMassiveAdObject::StateDownload()
{
    if (!mpDownloadAsset)
    {
        SetValid(E_STATE_INIT);
        return SetLastError(-397, "Invalid Download Asset");
    }

    switch (mpDownloadAsset->GetValid())
    {
    case E_STATE_DOWNLOADING:
        break;

    case E_STATE_DOWNLOAD_COMPLETE:
        SetValid(E_STATE_DOWNLOAD_COMPLETE);
        break;

    case E_STATE_DOWNLOAD_FAILED:
        if (mnRotationType == 1)
        {
            SetLastError(-396, "");
            mpDownloadAsset = 0;
            SetValid(E_STATE_DOWNLOAD_FAILED);
        }
        else
        {
            SetLastError(-396, "");
            SetValid(E_STATE_ROTATE);
        }
        break;

    default:
        MassiveLog(5, GetName(), "Generic StateDownload Error. State: %d", mpDownloadAsset->mnField48);
        break;
    }
    return 0;
}

int CMassiveAdObject::StateClearBuffer()
{
    if (mpCurrentAsset)
        mpCurrentAsset->ClearMediaBuffer(0);
    SetValid(E_STATE_REPORTING);
    return 0;
}

// Picks the record impressions are reported into: the current asset's, or the zone's
// default (asset id 0) one when there is no current asset, bDefaultAsset is set or nobody
// subscribes.
int CMassiveAdObject::SetCurrentRecord(int bDefaultAsset)
{
    CMassiveAsset* lpAsset = mpCurrentAsset;
    if (!lpAsset || bDefaultAsset || !mSubscriberList.GetCount())
    {
        lpAsset = mpZone->AssetFind(0);
        if (!lpAsset)
            return 0;
    }
    mpCurrentRecord = lpAsset->RecordFind(mnInvElementID);
    return 1;
}

int CMassiveAdObject::StateRotate()
{
    bool lbRotate = mnRotationType != 1 && mAssetIDList.GetCount() >= 2;
    if (!lbRotate)
    {
        mAssetIDList.GoToStart();
        lbRotate = mAssetIDList.GetCurrent() && mpCurrentAsset &&
                   *static_cast<int*>(mAssetIDList.GetCurrData()) != mpCurrentAsset->mnAssetId;
    }

    if (lbRotate)
    {
        SetValid(E_STATE_REQUEST_MEDIA);
        return 0;
    }

    MassiveLog(5, GetName(), "Rotate failed for MAO: %s Type: %d Assets: %d", mpcAdObjectName, mnRotationType,
               mAssetIDList.GetCount());
    SetValid(E_STATE_REPORTING);
    return -395;
}

int CMassiveAdObject::GetCrexID()
{
    return mpCurrentAsset ? mpCurrentAsset->mnCrex : 0;
}

// Decides whether the asset on show should rotate out: its order's minimum display time has
// passed, or the order's frequency cap is exhausted (which also expires the order's assets).
// Moves to the rotate state and returns 1 when so.
int CMassiveAdObject::CheckRotation()
{
    if (mnRotationType == 1 || !mpCurrentAsset || !mpCurrentRecord || !mpCurrentRecord->mImpression1.msField58)
        return 0;

    int lbRotate = 0;
    CMassiveOrder* lpOrder = mpCurrentAsset->mpOrder;
    if (GetValid() == E_STATE_REPORTING && mAssetIDList.GetCount() > 1 &&
        static_cast<unsigned int>(lpOrder->mnField1C) > static_cast<unsigned int>(mpCurrentAsset->mnField4C))
    {
        MassiveLog(5, GetName(), "Rotate: Minimum Time Reached for Mao: %s IE: %d Crex: %d", mpcAdObjectName,
                   mnInvElementID, mpCurrentAsset->mnCrex);
        lbRotate = 1;
    }

    int lnState = GetValid();
    if ((lnState == E_STATE_REPORTING || lnState == E_STATE_DOWNLOADING || lnState == E_STATE_DOWNLOAD_COMPLETE ||
         lnState == E_STATE_CLEAR_BUFFER) &&
        (mAssetIDList.GetCount() > 1 || mnRotationType == 16) && lpOrder && lpOrder->mnType &&
        static_cast<unsigned int>(lpOrder->mnField20) > static_cast<unsigned int>(lpOrder->mnType))
    {
        MassiveLog(6, GetName(), "Rotate: Frequency Cap Reached for Crex: %d", GetCrexID());
        mpZone->SetAssetExpiredByOrderID(lpOrder->mnNumber);
        if (mpMaster && mpMaster->SetAssetExpired(mpCurrentAsset->mnAssetId))
        {
            MassiveLog(5, GetName(), "Rotate: Frequency Cap Reached for Mao: %s IE: %d Crex: %d", mpcAdObjectName,
                       mnInvElementID, GetCrexID());
        }
        if (GetValid() == E_STATE_REPORTING)
            lbRotate = 1;
    }

    if (lbRotate)
        SetValid(E_STATE_ROTATE);
    return lbRotate;
}

// Expires a delivered asset id from the rotation, never the one currently shown or the
// placement's only asset.
int CMassiveAdObject::SetAssetExpired(int nAssetID)
{
    if (mAssetIDList.GetCount() == 1)
    {
        MassiveLog(6, GetName(), "%s - FAILED: Only Asset for MAO, NOT removing", mpcAdObjectName);
        return 0;
    }

    unsigned short luStep = 0;
    mAssetIDList.GoToStart();
    while (mAssetIDList.GetCurrent())
    {
        if (luStep++ >= muAssetIDCount)
            break;

        int lnCurrentID = mpCurrentAsset ? mpCurrentAsset->mnAssetId : 0;
        if (lnCurrentID == nAssetID)
        {
            MassiveLog(6, GetName(), "Current Asset, Dont remove");
            break;
        }

        if (*static_cast<int*>(mAssetIDList.GetCurrData()) == nAssetID)
        {
            AssetIDRemove(nAssetID);
            MassiveLog(6, GetName(), "%s - Asset Expired: %d Inv ID: %d", mpcAdObjectName, nAssetID,
                       mnInvElementID);
            return 1;
        }
        mAssetIDList.GoToNext();
    }
    return 0;
}

// Switches the record's active impression accumulator: 1 the impression one, 2 the view one,
// anything else none; each switch counts as a transition on the newly active accumulator.
int CMassiveAdObject::ChangeViewState(int nViewState)
{
    if (!mpCurrentRecord)
    {
        SetValid(E_STATE_INVALID);
        return SetLastError(-393, "No Impression Record for mao: %s", mpcAdObjectName);
    }

    mnViewState = nViewState;
    if (nViewState == 1)
    {
        mpCurrentRecord->mpCurrentImpression = &mpCurrentRecord->mImpression0;
    }
    else if (nViewState == 2)
    {
        mpCurrentRecord->mpCurrentImpression = &mpCurrentRecord->mImpression1;
    }
    else
    {
        mpCurrentRecord->mpCurrentImpression = 0;
        return -393;
    }
    ++mpCurrentRecord->mpCurrentImpression->msField58;
    return 0;
}

// The factor that rescales a size measured on a nScreenWidth x nScreenHeight screen to the
// reference screen (cached per width).
float CMassiveAdObject::GetScreenRatio(unsigned short nScreenWidth, unsigned short nScreenHeight)
{
    static unsigned short suCachedWidth = 0;
    static float sfCachedRatio = 0.0f;

    if (nScreenWidth <= 1 || nScreenHeight <= 1)
    {
        SetLastError(-390, "Invalid ScreenSize width: %d  height %d", nScreenWidth, nScreenHeight);
        return 0.0f;
    }

    if (nScreenWidth != suCachedWidth)
    {
        suCachedWidth = nScreenWidth;
        int lnDiagonalSquared = nScreenHeight * nScreenHeight + nScreenWidth * nScreenWidth;
        sfCachedRatio = KF_REFERENCE_DIAGONAL / static_cast<float>(std::sqrt(static_cast<double>(lnDiagonalSquared)));
    }
    return sfCachedRatio;
}

CMassiveAdObjectSubscriber* CMassiveAdObject::SubscriberFind(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
    {
        SetLastError(-500, "");
        return 0;
    }

    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        if (pSubscriber == mSubscriberList.GetCurrData())
            return static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData());
        mSubscriberList.GoToNext();
    }
    return 0;
}

int CMassiveAdObject::SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, "");
    if (SubscriberFind(pSubscriber))
        return SetLastError(-499, "Name: %s", pSubscriber->mpcName);

    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    CMassiveListNode* lpNode = lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(pSubscriber) : 0;
    mSubscriberList.Append(lpNode);
    pSubscriber->mpAdObject = this;

    if (static_cast<unsigned int>(GetValid()) >= E_STATE_REPORTING)
    {
        SetValid(E_STATE_REQUEST_MEDIA);
        MassiveLog(4, GetName(), "New Subscriber Added To MAO, Forcing a Media Download State Change");
    }
    return 0;
}

// Unlinks a subscriber (releasing its hold on the downloading asset); the last one leaving
// resets the placement to re-initialise.
int CMassiveAdObject::SubscriberRemove(CMassiveAdObjectSubscriber* pSubscriber)
{
    if (!pSubscriber)
        return SetLastError(-500, "Subscriber %s", 0);

    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        if (pSubscriber == mSubscriberList.GetCurrData())
        {
            if (mpDownloadAsset)
                mpDownloadAsset->mnRefCount = mpDownloadAsset->mnRefCount ? mpDownloadAsset->mnRefCount - 1 : 0;

            mSubscriberList.Remove(mSubscriberList.GetCurrent(), 1);
            if (!mSubscriberList.GetCount())
            {
                mpDownloadAsset = 0;
                mpCurrentAsset = 0;
                mpCurrentRecord = 0;
                mnViewState = 0;
                SetValid(E_STATE_INIT);
            }
            break;
        }
        mSubscriberList.GoToNext();
    }
    return 0;
}

// Asks every subscriber whether it wants asset nAssetID's media; 1 only when all do.
int CMassiveAdObject::MediaDownload(int nAssetID)
{
    int lbAllAccept = 1;
    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        CMassiveAdObjectSubscriber* lpSubscriber =
            static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData());
        if (!lpSubscriber->MediaDownload(nAssetID))
            lbAllAccept = 0;
        mSubscriberList.GoToNext();
    }
    return lbAllAccept;
}

// Hands the downloaded media to every subscriber; 1 only when all accept it.
int CMassiveAdObject::MediaDownloadComplete(const void* pData, int nSize, unsigned int nMediaType, int nAssetID)
{
    int lbAllAccept = 1;
    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        CMassiveAdObjectSubscriber* lpSubscriber =
            static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData());
        if (!lpSubscriber->MediaDownloadComplete(pData, nSize, nMediaType, nAssetID))
            lbAllAccept = 0;
        mSubscriberList.GoToNext();
    }
    return lbAllAccept;
}

// Clears the stale "last sample" stamps of every record of every delivered asset, then
// leaves the suspended state.
int CMassiveAdObject::Resume()
{
    if (mbSuspended)
    {
        mAssetIDList.GoToStart();
        while (mAssetIDList.GetCurrent())
        {
            CMassiveAsset* lpAsset = mpZone->AssetFind(*static_cast<int*>(mAssetIDList.GetCurrData()));
            if (lpAsset)
            {
                lpAsset->mRecordList.GoToStart();
                while (lpAsset->mRecordList.GetCurrent())
                {
                    static_cast<CMassiveRecord*>(lpAsset->mRecordList.GetCurrData())->mImpression1.mnField50 = 0;
                    static_cast<CMassiveRecord*>(lpAsset->mRecordList.GetCurrData())->mImpression0.mnField50 = 0;
                    lpAsset->mRecordList.GoToNext();
                }
            }
            mAssetIDList.GoToNext();
        }
    }

    MassiveLog(5, GetName(), "MAO Resume: %s", mpcAdObjectName);
    mbSuspended = 0;
    // The console leaves the logger's scratch in the return register; no caller reads it.
    return 0;
}

// Picks the asset to show next and starts its media download (or falls back to reporting on
// the default record when downloads are disabled or nothing is deliverable).
int CMassiveAdObject::StateRequestMedia()
{
    if (CMassiveClientCore::Instance()->IsFlagSetInternal(2))
    {
        SetCurrentRecord(1);
        SetValid(E_STATE_REPORTING);
        MassiveLog(4, GetName(), "Server Configuration: Media Download Disabled");
        return 0;
    }

    int lnAssetID = GetNextAssetID();
    if (!lnAssetID)
    {
        SetCurrentRecord(1);
        SetValid(E_STATE_REPORTING);
        MassiveLog(2, GetName(), "No Assets for Mao reporting on Default Crex 0: %s", mpcAdObjectName);
        return SetLastError(-397, "");
    }

    if (!mpCurrentAsset && !mpCurrentRecord)
        SetCurrentRecord(1);

    if (mpZone)
        mpDownloadAsset = mpZone->AssetFind(lnAssetID);

    if (!mpDownloadAsset)
    {
        SetValid(E_STATE_REPORTING);
        return SetLastError(-397, "No Assets with ID: %d for Mao: %s", lnAssetID, mpcAdObjectName);
    }

    if (MediaDownload(mpDownloadAsset->mnAssetId))
    {
        mpDownloadAsset->RequestDownloadBinary();
        SetValid(E_STATE_DOWNLOADING);
    }
    else
    {
        MassiveLog(5, GetName(), "Asset will not download media %s, subscriber handling: %s", mpDownloadAsset->mpcUrl,
                   mpcAdObjectName);
        SetValid(E_STATE_DOWNLOAD_COMPLETE);
    }
    return 0;
}

// The downloaded asset becomes the one on show: subscribers get its media, the record and
// view state follow it.
int CMassiveAdObject::StateDownloadComplet()
{
    mbMediaDownloaded = 1;
    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mnField10 = 1;
        mSubscriberList.GoToNext();
    }

    if (mnRotationType == 4 || mnRotationType == 32)
    {
        // Hold the swap while any subscriber still has the placement in view.
        mSubscriberList.GoToStart();
        while (mSubscriberList.GetCurrent())
        {
            CMassiveAdObjectSubscriber* lpSubscriber =
                static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData());
            if (static_cast<SMassiveImpression*>(lpSubscriber->GetImpression(0))->mbInView)
                return 0;
            mSubscriberList.GoToNext();
        }
    }

    mpCurrentAsset = mpDownloadAsset;
    mpDownloadAsset = 0;
    if (!mpCurrentAsset || !mpCurrentAsset->mpMediaBuffer)
        MassiveLog(5, GetName(), "Invalid Reporting Asset for MAO: %s", mpcAdObjectName);

    if (mpCurrentAsset->mpOrder)
        mpCurrentAsset->mpOrder->mnField1C = 0;

    if (!MediaDownloadComplete(mpCurrentAsset->mpMediaBuffer, mpCurrentAsset->mnField48, mpCurrentAsset->mnMediaType,
                               mpCurrentAsset->mnAssetId))
    {
        SetLastError(-396, "");
        MassiveLog(5, GetName(), "Subscriber (%s) Failed MediaDownloadComplete(), defaulting to Crex 0 URL: %s",
                   mpcAdObjectName, mpCurrentAsset ? mpCurrentAsset->mpcUrl : "NULL");
        mpCurrentAsset = 0;
        mpDownloadAsset = 0;
        SetValid(E_STATE_REPORTING);
        return 0;
    }

    CMassiveAsset* lpRecordAsset = mpCurrentAsset;
    if (!lpRecordAsset || !mSubscriberList.GetCount())
        lpRecordAsset = mpZone->AssetFind(0);
    if (lpRecordAsset)
        mpCurrentRecord = lpRecordAsset->RecordFind(mnInvElementID);

    ChangeViewState(3);
    SetValid(CMassiveClientCore::Instance()->IsFlagSet(1) ? E_STATE_REPORTING : E_STATE_CLEAR_BUFFER);
    MassiveLog(5, GetName(), "Media Downloaded for MAO: %s Asset: %d", mpcAdObjectName, GetCrexID());
    return 0;
}

int CMassiveAdObject::ReportImpressions()
{
    ChangeViewState(3);
    return 0;
}

// Folds one tick's best impression into the active accumulator of the current record:
// running sums of size, angle and distance, the displayed time since the last sample (also
// charged to the asset's order for an impression), and a record rollover once a counter
// passes the sample cap.
int CMassiveAdObject::SaveImpressionRecord(SMassiveImpression* pImpression)
{
    if (!pImpression)
        return SetLastError(-393, "");

    if (!mpCurrentRecord || !mpCurrentRecord->mpCurrentImpression)
        return -393;

    long long lnNow = CMassiveClientCore::Instance()->GetTimeTickStart();
    CMassiveRecordImpression* lpImpression = mpCurrentRecord->mpCurrentImpression;

    if (CMassiveClientCore::Instance()->IsFlagSetInternal(0x400))
    {
        lpImpression->mnField18 = 0;
        lpImpression->mfField20 = 0.0f;
        lpImpression->mfField28 = 0.0f;
        lpImpression->msField24 = 0;
        lpImpression->msField1C = 0;
        lpImpression->msField2C = 0;
        lpImpression->mnField30 = 0;
        lpImpression->mnField50 = 0;
        return 0;
    }

    lpImpression->mfField20 = pImpression->mfAngle + lpImpression->mfField20;
    lpImpression->mnField18 = static_cast<int>(pImpression->muSize) + lpImpression->mnField18;
    lpImpression->mfField28 = pImpression->mfDistance + lpImpression->mfField28;
    ++lpImpression->msField24;
    ++lpImpression->msField1C;
    ++lpImpression->msField2C;

    if (!lpImpression->mnField40)
        lpImpression->mnField40 = lnNow;

    long long lnDuration = 0;
    if (lpImpression->mnField50)
        lnDuration = lnNow - lpImpression->mnField50;

    int lnDurationMs = static_cast<int>(lnDuration);
    lpImpression->mnField30 += lnDurationMs;
    lpImpression->mnField34 += lnDurationMs;
    lpImpression->mnField38 += lnDurationMs;

    if (lpImpression->mnParentRecord == 1)
    {
        CMassiveOrder* lpOrder = mpCurrentRecord->mpAsset->mpOrder;
        if (lpOrder)
        {
            lpOrder->mnField1C += lnDurationMs;
            lpOrder->mnField20 += lnDurationMs;
        }
    }

    MassiveLog(7, GetName(), "Record Duration: %d", lnDuration);
    lpImpression->mnField50 = lnNow;
    lpImpression->mnField48 = lnNow;

    const char* lpcKind = "?";
    if (lpImpression->mnParentRecord == 1)
        lpcKind = "IMPR";
    else if (lpImpression->mnParentRecord == 2)
        lpcKind = "VIEW";

    MassiveLog(6, GetName(),
               "%s %s ID: %d Crex: %d Ang: %1.2f Size: %d Fall: %1.2f Tran: %d Dur: [C: %d T: %d S: %d] - %I64d\n",
               lpcKind, mpcAdObjectName, mnInvElementID, GetCrexID(),
               lpImpression->mfField20 / static_cast<float>(lpImpression->msField24),
               static_cast<unsigned int>(lpImpression->mnField18) / lpImpression->msField1C,
               lpImpression->mfField28 / static_cast<float>(lpImpression->msField2C), lpImpression->msField58,
               lpImpression->mnField30, lpImpression->mnField34, lpImpression->mnField38, lnDuration);

    if (lpImpression->msField1C > KU_MAX_IMPRESSION_SAMPLES || lpImpression->msField24 > KU_MAX_IMPRESSION_SAMPLES ||
        lpImpression->msField2C > KU_MAX_IMPRESSION_SAMPLES)
    {
        if (mpCurrentAsset)
        {
            mpCurrentAsset->RecordCreate(mnInvElementID);
            ChangeViewState(mnViewState);
        }
    }
    return 0;
}

// Scores every subscriber's latest impression and records the best one; bOutOfView forces
// the "not in view" state (suspend).
int CMassiveAdObject::UpdateImpressions(int bOutOfView)
{
    if (!mSubscriberList.GetCount())
        return 0;

    unsigned long long luLastTick = static_cast<unsigned long long>(CMassiveClientCore::Instance()->GetTimeTickLast());
    if (mpCurrentRecord)
    {
        // A sample stamp older than the last client tick is stale (the placement was not
        // ticked meanwhile): restart the duration count.
        CMassiveRecordImpression* lpImpression = &mpCurrentRecord->mImpression0;
        if (static_cast<unsigned long long>(lpImpression->mnField50) > 0 &&
            static_cast<unsigned long long>(lpImpression->mnField50) != luLastTick)
        {
            lpImpression->mnField50 = 0;
        }
        lpImpression = &mpCurrentRecord->mImpression1;
        if (static_cast<unsigned long long>(lpImpression->mnField50) > 0 &&
            static_cast<unsigned long long>(lpImpression->mnField50) != luLastTick)
        {
            lpImpression->mnField50 = 0;
        }
    }

    SMassiveImpression lBest;
    std::memset(&lBest, 0, sizeof(lBest));

    int lbQualified = 0;
    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        if (!mbMediaDownloaded ||
            static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mnField10)
        {
            if (GetBestImpression(mSubscriberList.GetCurrent(), &lBest))
                lbQualified = 1;
        }
        mSubscriberList.GoToNext();
    }

    int lnViewState = 3;
    if (!bOutOfView && (lBest.mbInView || lBest.mbAudible))
        lnViewState = lbQualified ? 2 : 1;

    if (lnViewState != mnViewState)
        ChangeViewState(lnViewState);
    return SaveImpressionRecord(&lBest);
}

int CMassiveAdObject::Suspend()
{
    if (!mbSuspended)
        UpdateImpressions(1);
    MassiveLog(5, GetName(), "MAO Suspend: %s", mpcAdObjectName);
    mbSuspended = 1;
    // The console leaves the logger's scratch in the return register; no caller reads it.
    return 0;
}

// Runs one step of the download state machine, scores impressions while the placement is
// reporting or downloading, then ticks every subscriber (handing on this placement's error).
int CMassiveAdObject::Tick()
{
    if (mbSuspended)
        return -392;

    ClearLastError();
    switch (GetValid())
    {
    case E_STATE_INIT:
        StateInit();
        break;
    case E_STATE_REQUEST_MEDIA:
        StateRequestMedia();
        break;
    case E_STATE_DOWNLOADING:
        StateDownload();
        break;
    case E_STATE_DOWNLOAD_COMPLETE:
        StateDownloadComplet();
        break;
    case E_STATE_CLEAR_BUFFER:
        StateClearBuffer();
        break;
    case E_STATE_ROTATE:
        StateRotate();
        break;
    default:
        break;
    }

    int lnState = GetValid();
    if (lnState == E_STATE_REPORTING || lnState > E_STATE_REQUEST_MEDIA)
    {
        UpdateImpressions(0);
        CheckRotation();
    }

    mSubscriberList.GoToStart();
    while (mSubscriberList.GetCurrent())
    {
        static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mnLastError = 0;
        static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->Tick();
        if (!static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mnLastError && GetLastError())
            static_cast<CMassiveAdObjectSubscriber*>(mSubscriberList.GetCurrData())->mnLastError = GetLastError();
        mSubscriberList.GoToNext();
    }
    return 0;
}

int CMassiveAdObject::GetBestImpression(CMassiveListNode* /*pSubscriberNode*/, SMassiveImpression* /*pBest*/)
{
    return 0;
}

// ===========================================================================
// Leaf placements: impression scoring
// ===========================================================================

// Normalises the subscriber's impression (area -> linear size, angle -> cosine) and scales its
// size to the reference screen; qualifies it against the minimum size / angle and keeps it in
// pBest when its size x facing score is at least the best seen this tick.
int CMassiveAdObjectTexture::GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest)
{
    unsigned short luMinSize = mpCurrentAsset ? mpCurrentAsset->msField5E : muMinSize;
    float lfMinAngle = mpCurrentAsset ? mpCurrentAsset->mfField60 : mfMinAngle;

    CMassiveAdObjectSubscriber* lpSubscriber = static_cast<CMassiveAdObjectSubscriber*>(pSubscriberNode->GetOwner());
    if (!lpSubscriber)
    {
        MassiveLog(2, GetName(), "Invalid Subscriber: %d", 0);
        return 0;
    }

    SMassiveImpression* lpImpression = static_cast<SMassiveImpression*>(lpSubscriber->GetImpression(1));
    if (!lpImpression)
    {
        MassiveLog(2, GetName(), "Invalid Subscriber Impression Data: %d", lpSubscriber);
        return 0;
    }

    lpSubscriber->mnField54 = KI_IMPRESSION_NOT_SEEN;
    if (lpImpression->muSize > static_cast<unsigned int>(lpImpression->mnScreenWidth * lpImpression->mnScreenHeight))
    {
        SetLastError(-390, "Invalid Impression Data: Size: (%d) Screen Size: (%d x %d)", lpImpression->muSize,
                     lpImpression->mnScreenHeight, lpImpression->mnScreenWidth);
        return 0;
    }

    if (lpImpression->mnSizeType == 1)
    {
        float lfSide = static_cast<float>(std::sqrt(static_cast<double>(lpImpression->muSize)));
        lpImpression->muSize = static_cast<unsigned int>(static_cast<long long>(
            static_cast<float>(lfSide * 1.4142135623730951)));
    }

    unsigned int luAngleType = static_cast<unsigned int>(lpImpression->mnAngleType);
    if (luAngleType < 1)
    {
        if (lpImpression->mfAngle < -1.0f || lpImpression->mfAngle > 1.0f)
        {
            SetLastError(-390, "Invalid Angle Data: Dot Product: %3.2f", lpImpression->mfAngle);
            return 0;
        }
    }
    else if (luAngleType < 3)
    {
        if (luAngleType == 1)
        {
            if (lpImpression->mfAngle < 0.0f || lpImpression->mfAngle > 6.2831802f)
            {
                SetLastError(-390, "Invalid Angle Data: Radian: %3.4f", lpImpression->mfAngle);
                return 0;
            }
        }
        else
        {
            if (lpImpression->mfAngle < 0.0f || lpImpression->mfAngle > 360.0f)
            {
                SetLastError(-390, "Invalid Angle Data: Degrees: %d", lpImpression->mfAngle);
                return 0;
            }
            lpImpression->mfAngle = lpImpression->mfAngle * 0.01745f;
        }
        lpImpression->mfAngle = static_cast<float>(std::cos(static_cast<double>(lpImpression->mfAngle)));
    }

    MassiveLog(7, GetName(), "TYPE: %d : %d  SIZE: %d  ANGLE: %f ", lpImpression->mnSizeType,
               lpImpression->mnAngleType, lpImpression->muSize, lpImpression->mfAngle);
    if (!lpImpression->mbInView)
        return 0;

    int lbQualified = 0;
    lpSubscriber->mnField54 = KI_IMPRESSION_SEEN;
    float lfRatio = GetScreenRatio(static_cast<unsigned short>(lpImpression->mnScreenWidth),
                                   static_cast<unsigned short>(lpImpression->mnScreenHeight));
    lpImpression->muSize =
        static_cast<unsigned int>(static_cast<long long>(static_cast<float>(lpImpression->muSize) * lfRatio));
    MassiveLog(7, GetName(), "Valid Impression State for MAO: %s %d", mpcAdObjectName, GetCrexID());

    if (!(lpImpression->mfAngle < lfMinAngle) && lpImpression->muSize >= luMinSize && lpImpression->mfAngle < 1.001f)
    {
        lbQualified = 1;
        lpSubscriber->mnField54 = KI_IMPRESSION_QUALIFIED;
    }

    float lfScore = static_cast<float>(lpImpression->muSize) * lpImpression->mfAngle;
    float lfBestScore = static_cast<float>(pBest->muSize) * pBest->mfAngle;
    if (lfScore < lfBestScore)
        return 0;

    std::memcpy(pBest, lpImpression, sizeof(SMassiveImpression));
    return lbQualified;
}

// Keeps the closest audible impression in pBest (an unset best distance counts as 1); it
// qualifies within the maximum distance.
int CMassiveAdObjectAudio::GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest)
{
    if (!(pBest->mfDistance >= 0.000001f))
        pBest->mfDistance = 1.0f;

    CMassiveAdObjectSubscriber* lpSubscriber = static_cast<CMassiveAdObjectSubscriber*>(pSubscriberNode->GetOwner());
    SMassiveImpression* lpImpression = static_cast<SMassiveImpression*>(lpSubscriber->GetImpression(1));
    float lfMaxDistance = mpCurrentAsset ? mpCurrentAsset->mfField64 : mfMaxDistance;

    lpSubscriber->mnField54 = KI_IMPRESSION_NOT_SEEN;
    if (!lpImpression->mbAudible || lpImpression->mfDistance > pBest->mfDistance ||
        !(lpImpression->mfDistance > -0.000001f))
    {
        return 0;
    }

    lpSubscriber->mnField54 = KI_IMPRESSION_SEEN;
    std::memcpy(pBest, lpImpression, sizeof(SMassiveImpression));
    if (lpImpression->mfDistance > lfMaxDistance)
        return 0;

    lpSubscriber->mnField54 = KI_IMPRESSION_QUALIFIED;
    return 1;
}

// Keeps the in-view impression with the larger size x facing score in pBest; it qualifies
// at or above the minimum angle and size.
int CMassiveAdObjectVideo::GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest)
{
    unsigned short luMinSize = mpCurrentAsset ? mpCurrentAsset->msField5E : muMinSize;
    float lfMinAngle = mpCurrentAsset ? mpCurrentAsset->mfField60 : mfMinAngle;

    CMassiveAdObjectSubscriber* lpSubscriber = static_cast<CMassiveAdObjectSubscriber*>(pSubscriberNode->GetOwner());
    SMassiveImpression* lpImpression = static_cast<SMassiveImpression*>(lpSubscriber->GetImpression(1));
    lpSubscriber->mnField54 = KI_IMPRESSION_NOT_SEEN;
    if (!lpImpression || !lpImpression->mbInView)
        return 0;

    MassiveLog(7, GetName(), "Valid Impression State for MAO: %s %d", mpcAdObjectName, GetCrexID());
    lpSubscriber->mnField54 = KI_IMPRESSION_SEEN;

    float lfScore = static_cast<float>(lpImpression->muSize) * lpImpression->mfAngle;
    float lfBestScore = static_cast<float>(pBest->muSize) * pBest->mfAngle;
    if (lfScore < lfBestScore)
        return 0;

    std::memcpy(pBest, lpImpression, sizeof(SMassiveImpression));
    if (lpImpression->mfAngle < lfMinAngle || lpImpression->muSize < luMinSize)
        return 0;

    lpSubscriber->mnField54 = KI_IMPRESSION_QUALIFIED;
    return 1;
}

// Keeps the larger in-view impression in pBest; it qualifies at or above the minimum size.
int CMassiveAdObjectModel::GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest)
{
    CMassiveAdObjectSubscriber* lpSubscriber = static_cast<CMassiveAdObjectSubscriber*>(pSubscriberNode->GetOwner());
    SMassiveImpression* lpImpression = static_cast<SMassiveImpression*>(lpSubscriber->GetImpression(1));
    unsigned int luMinSize = mpCurrentAsset ? mpCurrentAsset->msField5E : muMinSize;

    lpSubscriber->mnField54 = KI_IMPRESSION_NOT_SEEN;
    if (!lpImpression || !lpImpression->mbInView)
        return 0;

    lpSubscriber->mnField54 = KI_IMPRESSION_SEEN;
    if (!(lpImpression->muSize > pBest->muSize))
        return 0;

    std::memcpy(pBest, lpImpression, sizeof(SMassiveImpression));
    if (lpImpression->muSize < luMinSize)
        return 0;

    lpSubscriber->mnField54 = KI_IMPRESSION_QUALIFIED;
    return 1;
}

// ===========================================================================
// Leaf placements: construction / destruction
// ===========================================================================

CMassiveAdObjectAudio::~CMassiveAdObjectAudio()
{
}

CMassiveAdObjectModel::~CMassiveAdObjectModel()
{
}

} // namespace MassiveAdClient3
