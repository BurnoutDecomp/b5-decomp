#pragma once

// MassiveAdClient3::CMassiveAdObject -- one in-game ad placement ("MAO", inventory element) of
// the MassiveAd client (vendor middleware): the delivered-asset rotation, the media download
// state machine and the impression bookkeeping its subscribers feed. The media-specific
// leaves (MassiveAdClient3Object{Texture,Audio,Video,Model}.h) supply GetBestImpression; the
// *Dynamic composites spawn one leaf "slave" per subscriber.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"  // CMassiveBaseObject, CMassiveList

namespace MassiveAdClient3
{

class CMassiveAdObjectSubscriber;
class CMassiveZoneManager;
class CMassiveAsset;
class CMassiveRecord;

// The 32-byte impression record a subscriber hands to the client (copied whole by
// CMassiveAdObjectSubscriber::SetImpression / GetImpression).
struct SMassiveImpression
{
    int           mnSizeType;     // +0x00 0 linear size, 1 on-screen area
    int           mnAngleType;    // +0x04 0 dot product, 1 radians, 2 degrees
    unsigned char mbInView;       // +0x08
    short         mnScreenWidth;  // +0x0A reference screen the size was measured on
    short         mnScreenHeight; // +0x0C
    unsigned int  muSize;         // +0x10
    float         mfAngle;        // +0x14
    unsigned char mbAudible;      // +0x18
    float         mfDistance;     // +0x1C
};
static_assert(sizeof(SMassiveImpression) == 32, "the impression record is 32 bytes");

class CMassiveAdObject : public CMassiveBaseObject
{
    // The enter-zone parser sets the rotation bound and checks the state of the placements
    // it builds; the zone hands its placements over by stamping their zone.
    friend class CRequestEnterZone;

public:
    // Download / reporting states (the CMassiveBaseObject state word at +0x10).
    enum E_State
    {
        E_STATE_INVALID           = 0,
        E_STATE_INIT              = 1,
        E_STATE_REPORTING         = 2,
        E_STATE_REQUEST_MEDIA     = 18,
        E_STATE_DOWNLOADING       = 19,
        E_STATE_DOWNLOAD_COMPLETE = 20,
        E_STATE_CLEAR_BUFFER      = 21,
        E_STATE_DOWNLOAD_FAILED   = 23,
        E_STATE_ROTATE            = 24
    };

    // pZone 0 = the client core's current zone.
    CMassiveAdObject(const char* pcName, int nInvElementID, int nRotationType, CMassiveZoneManager* pZone);
    virtual ~CMassiveAdObject();

    virtual int Initialize();
    virtual int SetAssetExpired(int nAssetID);
    virtual int Tick();
    virtual int ReportImpressions();
    virtual int Suspend();
    virtual int Resume();
    virtual int SubscriberAdd(CMassiveAdObjectSubscriber* pSubscriber);
    virtual int GetNextAssetID();

    // Folds the impression of the subscriber on pSubscriberNode into pBest when it beats it;
    // non-zero when the impression meets this placement's size/angle thresholds. The base
    // placement scores nothing.
    virtual int GetBestImpression(CMassiveListNode* pSubscriberNode, SMassiveImpression* pBest);

    int BaseFindPreSubscribers(CMassiveAdObject* pAdObject);
    int AssetIDRemove(int nAssetID);
    int AssetIDAdd(int nAssetID);
    int SubscriberRemove(CMassiveAdObjectSubscriber* pSubscriber);
    CMassiveAdObjectSubscriber* SubscriberFind(CMassiveAdObjectSubscriber* pSubscriber);
    int GetCrexID();
    int ChangeViewState(int nViewState);
    float GetScreenRatio(unsigned short nScreenWidth, unsigned short nScreenHeight);

    char* mpcAdObjectName;      // +0x14 (read by the zone manager's name lookup)
    CMassiveAdObject* mpMaster; // +0x34 spawning composite of a slave, or 0
    int mnInvElementID;         // +0x48 inventory-element id

protected:
    int StateInit();
    int StateDownload();
    int StateClearBuffer();
    int StateRotate();
    int StateRequestMedia();
    int StateDownloadComplet();
    int SetCurrentRecord(int bDefaultAsset);
    int CheckRotation();
    int MediaDownload(int nAssetID);
    int MediaDownloadComplete(const void* pData, int nSize, unsigned int nMediaType, int nAssetID);
    int SaveImpressionRecord(SMassiveImpression* pImpression);
    int UpdateImpressions(int bOutOfView);

    CMassiveZoneManager* mpZone;            // +0x18
    CMassiveList         mSubscriberList;   // +0x1C
    CMassiveAsset*       mpCurrentAsset;    // +0x2C asset being reported on
    CMassiveAsset*       mpDownloadAsset;   // +0x30 asset whose media is downloading
    int                  mnRotationType;    // +0x38 1 fixed, 16 dynamic composite
    int                  mnViewState;       // +0x3C 1 impression, 2 view, 3 none
    CMassiveRecord*      mpCurrentRecord;   // +0x40
    unsigned char        mbSuspended;       // +0x44
    unsigned char        mbMediaDownloaded; // +0x45
    unsigned short       muAssetIDCount;    // +0x4C primary match list size (rotation bound)
    CMassiveList         mAssetIDList;      // +0x50 delivered asset ids (MassiveMalloc'd ints)
};

} // namespace MassiveAdClient3
