// MassiveAdClient3::CRequestEnterZone (vendor middleware): the enter-zone request block and
// the response parser that builds the zone's placements, assets and orders.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestEnterZone.h"

#include <new>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Asset.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectAudio.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectAudioDynamic.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModel.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectModelDynamic.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTexture.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTextureDynamic.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideo.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideoDynamic.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"

namespace MassiveAdClient3
{

namespace
{

// Thresholds a placement / asset takes when the response does not carry them.
const unsigned short KU_DEFAULT_SIZE_THRESHOLD = 100;
const float          KF_DEFAULT_ANGLE_THRESHOLD = 0.64278f;
const float          KF_DEFAULT_IE_DISTANCE     = 0.5f;
const float          KF_DEFAULT_ASSET_DISTANCE  = 1.0f;

// The rotation type that makes a placement a per-subscriber composite.
const int KI_ROTATION_DYNAMIC = 16;

// Appends pData to pList on a freshly allocated node (a failed node allocation appends nothing).
void AppendNode(CMassiveList* pList, void* pData)
{
    void* lpNodeMemory = CMassiveListNode::operator new(sizeof(CMassiveListNode));
    CMassiveListNode* lpNode = lpNodeMemory ? ::new (lpNodeMemory) CMassiveListNode(pData) : 0;
    pList->Append(lpNode);
}

} // anonymous namespace

CRequestEnterZone::CRequestEnterZone()
    : CRequestObject(51, "RequestEnterZone")
    , mnIECount(0)
    , mnField74(0)
{
}

CRequestEnterZone::~CRequestEnterZone()
{
}

const char* CRequestEnterZone::GetRequestURL()
{
    return "/adsrv/4/enterZone";
}

int CRequestEnterZone::WriteEnterZoneRequest(const char* pcZoneName, int nBandwidthTotalSize,
                                             int nBandwidthTotalTime, unsigned short sBandwidthTotalItems)
{
    MassiveLog(5, GetName(), "Writing Request...");

    WriteU8(71, 0);
    WriteString(pcZoneName, 0);
    MassiveLog(5, GetName(), "Writing Zone Name: %s", pcZoneName);

    WriteU8(42, 0);
    WriteU32(gnMassivePlayerID, 0);
    MassiveLog(5, GetName(), "Writing Massive Player ID: %d", gnMassivePlayerID);

    WriteU8(43, 0);
    WriteU32(gnMassiveSessionID, 0);
    MassiveLog(5, GetName(), "Writing Massive Session ID: %d", gnMassiveSessionID);

    if (nBandwidthTotalSize && nBandwidthTotalTime && sBandwidthTotalItems)
    {
        WriteU8(16, 0);
        WriteU32(nBandwidthTotalSize, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Size: %d", nBandwidthTotalSize);

        WriteU8(17, 0);
        WriteU32(nBandwidthTotalTime, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Time: %d", nBandwidthTotalTime);

        WriteU8(18, 0);
        WriteU16(sBandwidthTotalItems, 0);
        MassiveLog(5, GetName(), "Writing Bandwidth Total Items: %d", sBandwidthTotalItems);
    }

    FinishBaseBlock(205, 1, 1);
    return 1;
}

int CRequestEnterZone::CreateRequest(CRequestBuilder* pBuilder, const char* pcZoneName, int nBandwidthTotalSize,
                                     int nBandwidthTotalTime, unsigned short sBandwidthTotalItems)
{
    if (!pBuilder || !pcZoneName)
        return -1100;

    CRequestObject::CreateRequest(pBuilder, 256, 1024);
    WriteEnterZoneRequest(pcZoneName, nBandwidthTotalSize, nBandwidthTotalTime, sBandwidthTotalItems);
    SetStatus(2);
    MassiveLog(5, GetName(), "Created Request successfully.");
    return 0;
}

void CRequestEnterZone::GetAssets(CMassiveList* pAssetList)
{
    if (!pAssetList)
        return;

    pAssetList->GoToStart();
    mAssetList.GoToStart();
    while (mAssetList.GetCurrent())
    {
        AppendNode(pAssetList, mAssetList.GetCurrData());
        mAssetList.GoToNext();
    }
}

void CRequestEnterZone::GetOrders(CMassiveList* pOrderList)
{
    if (!pOrderList)
        return;

    pOrderList->GoToStart();
    mOrderList.GoToStart();
    while (mOrderList.GetCurrent())
    {
        AppendNode(pOrderList, mOrderList.GetCurrData());
        mOrderList.GoToNext();
    }
}

void CRequestEnterZone::GetMAOs(CMassiveList* pMAOList, CMassiveZoneManager* pZoneManager)
{
    if (!pMAOList)
        return;

    pMAOList->GoToStart();
    mMAOList.GoToStart();
    while (mMAOList.GetCurrent())
    {
        AppendNode(pMAOList, mMAOList.GetCurrData());
        static_cast<CMassiveAdObject*>(pMAOList->GetCurrData())->mpZone = pZoneManager;
        mMAOList.GoToNext();
    }
}

// One inventory element: the placement's id, name, media type, rotation type, thresholds and
// delivered asset ids. The media type picks the placement class (texture below 0x400, video
// 0x401-0x7FF, audio 0x801-0xFFF, model above 0x1000); rotation type 16 makes it a dynamic
// composite.
CMassiveAdObject* CRequestEnterZone::ReadIEBlock()
{
    MassiveLog(5, GetName(), "Reading Inventory Element Block...");

    unsigned short luSizeThreshold = KU_DEFAULT_SIZE_THRESHOLD;
    unsigned short luRotationType = 0;
    char* lpcName = 0;
    float lfAngleThreshold = KF_DEFAULT_ANGLE_THRESHOLD;
    float lfDistanceThreshold = KF_DEFAULT_IE_DISTANCE;
    unsigned short luAssetCount = 0;
    unsigned int* lpAssetIDs = 0;
    unsigned char lbPrimaryMatchListSize = 0;
    unsigned char lnRequiredFields = 0;
    int lnIEID = 0;
    unsigned int luMediaType = 0;

    unsigned int luBlockLength = ReadU32();
    MassiveLog(5, GetName(), "Block Length: %d", luBlockLength);
    int lnBlockStart = mnPosition;
    while (static_cast<unsigned int>(mnPosition - lnBlockStart) < luBlockLength)
    {
        unsigned char lbTag = static_cast<unsigned char>(ReadU8());
        switch (lbTag)
        {
        case 0x08:
            lfAngleThreshold = ReadFloat();
            MassiveLog(5, GetName(), "IE Angle Threshold: %.2f", lfAngleThreshold);
            break;

        case 0x09:
            lfDistanceThreshold = ReadFloat();
            MassiveLog(5, GetName(), "IE Distance Threshold: %.2f", lfDistanceThreshold);
            break;

        case 0x0A:
            luSizeThreshold = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "IE Size Threshold: %d", luSizeThreshold);
            break;

        case 0x0B:
            luMediaType = ReadU32();
            MassiveLog(5, GetName(), "IE Media Type: %d", luMediaType);
            ++lnRequiredFields;
            break;

        case 0x23:
        {
            float lfAdjustment = ReadFloat();
            MassiveLog(5, GetName(), "IE Adjustment: %.2f", lfAdjustment);
            break;
        }

        case 0x24:
            MassiveLog(5, GetName(), "IE Asset Array:");
            ReadU32Array(&lpAssetIDs, &luAssetCount);
            if (!lpAssetIDs && luAssetCount)
                break;
            // (the array is hex-dumped to the compiled-out logger)
            ++lnRequiredFields;
            break;

        case 0x26:
            lnIEID = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "IE ID: %d", lnIEID);
            ++lnRequiredFields;
            break;

        case 0x27:
            lpcName = ReadString();
            MassiveLog(5, GetName(), "IE Name: %s", lpcName);
            ++lnRequiredFields;
            break;

        case 0x28:
            luRotationType = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "IE Rotation Type: %d", luRotationType);
            ++lnRequiredFields;
            break;

        case 0x2E:
            lbPrimaryMatchListSize = static_cast<unsigned char>(ReadU8());
            MassiveLog(5, GetName(), "IE Primary Match List Size: %d", lbPrimaryMatchListSize);
            ++lnRequiredFields;
            break;

        default:
            if (!SkipField(lbTag))
                return 0;
            break;
        }
    }

    if (lnRequiredFields != 6)
    {
        MassiveLog(2, GetName(), "IE Block does not contain all of the required fields.");
        return 0;
    }

    MassiveLog(5, GetName(), "IE Block contains all of the required fields.");
    MassiveLog(5, GetName(), "Done Reading Inventory Element Block...");

    CMassiveAdObject* lpAdObject = 0;
    void* lpMemory = 0;
    if (luMediaType < 0x400)
    {
        if (luRotationType == KI_ROTATION_DYNAMIC)
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectTextureDynamic));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectTextureDynamic(lpcName, lnIEID, KI_ROTATION_DYNAMIC, 0);
        }
        else
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectTexture));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectTexture(lpcName, lnIEID, luSizeThreshold,
                                                                      lfAngleThreshold, luRotationType, 0);
        }
    }
    else if (luMediaType - 0x401 <= 0x3FE)
    {
        if (luRotationType == KI_ROTATION_DYNAMIC)
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectVideoDynamic));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectVideoDynamic(lpcName, lnIEID, lfDistanceThreshold,
                                                                           KI_ROTATION_DYNAMIC, 0);
        }
        else
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectVideo));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectVideo(lpcName, lnIEID, luSizeThreshold, lfAngleThreshold,
                                                                    lfDistanceThreshold, luRotationType, 0);
        }
    }
    else if (luMediaType - 0x801 <= 0x7FE)
    {
        if (luRotationType == KI_ROTATION_DYNAMIC)
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectAudioDynamic));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectAudioDynamic(lpcName, lnIEID, lfDistanceThreshold,
                                                                           KI_ROTATION_DYNAMIC, 0);
        }
        else
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectAudio));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectAudio(lpcName, lnIEID, lfDistanceThreshold,
                                                                    luRotationType, 0);
        }
    }
    else if (luMediaType > 0x1000)
    {
        if (luRotationType == KI_ROTATION_DYNAMIC)
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectModelDynamic));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectModelDynamic(lpcName, lnIEID, luSizeThreshold,
                                                                           KI_ROTATION_DYNAMIC, 0);
        }
        else
        {
            lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAdObjectModel));
            if (lpMemory)
                lpAdObject = ::new (lpMemory) CMassiveAdObjectModel(lpcName, lnIEID, luSizeThreshold,
                                                                    luRotationType, 0);
        }
    }
    else
    {
        MassiveLog(2, GetName(), "MAO media type not recognized.  Can not create MAO.");
    }

    if (lpAdObject)
    {
        for (unsigned short i = 0; i < luAssetCount; ++i)
            lpAdObject->AssetIDAdd(static_cast<int>(lpAssetIDs[i]));
        lpAdObject->muAssetIDCount = lbPrimaryMatchListSize;
        lpAdObject->Initialize();
    }
    else
    {
        MassiveLog(2, GetName(), "MAO creation failed.");
    }

    if (lpAssetIDs)
        MassiveFree(lpAssetIDs);
    if (lpcName)
        MassiveFree(lpcName);
    return lpAdObject;
}

// One asset: id, creative id, URL, media type, size, expiry, content hash and minimum display
// time (all required), plus optional size / angle / distance thresholds.
CMassiveAsset* CRequestEnterZone::ReadAssetBlock()
{
    MassiveLog(5, GetName(), "Reading Asset Block...");

    unsigned char lnRequiredFields = 0;
    int lnAssetID = 0;
    int lnCrexID = 0;
    int lnMediaType = 0;
    int lnSize = 0;
    long long lnExpiration = 0;
    int lnRotationDuration = 0;
    char* lpcURL = 0;
    float lfAngleThreshold = KF_DEFAULT_ANGLE_THRESHOLD;
    float lfDistanceThreshold = KF_DEFAULT_ASSET_DISTANCE;
    unsigned short luSizeThreshold = KU_DEFAULT_SIZE_THRESHOLD;
    unsigned char* lpHash = 0;
    unsigned short luHashLength = 0;

    unsigned int luBlockLength = ReadU32();
    MassiveLog(5, GetName(), "Block Length: %d", luBlockLength);
    int lnBlockStart = mnPosition;
    while (static_cast<unsigned int>(mnPosition - lnBlockStart) < luBlockLength)
    {
        unsigned char lbTag = static_cast<unsigned char>(ReadU8());
        switch (lbTag)
        {
        case 0x02:
            lnExpiration = static_cast<long long>(ReadU64());
            MassiveLog(5, GetName(), "Asset Expiration: %I64d", lnExpiration);
            ++lnRequiredFields;
            break;

        case 0x04:
            ReadByteArray(&lpHash, &luHashLength);
            if (!lpHash)
                break;
            MassiveLog(5, GetName(), "Asset Hash:");
            // (the hash is hex-dumped to the compiled-out logger)
            ++lnRequiredFields;
            break;

        case 0x05:
            lnAssetID = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Asset ID: %d", lnAssetID);
            ++lnRequiredFields;
            break;

        case 0x06:
            lnRotationDuration = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Asset Rotation Duration: %d", lnRotationDuration);
            ++lnRequiredFields;
            break;

        case 0x07:
            lnSize = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Asset Size: %d", lnSize);
            ++lnRequiredFields;
            break;

        case 0x08:
            lfAngleThreshold = ReadFloat();
            MassiveLog(5, GetName(), "Asset Angle Threshold: %f", lfAngleThreshold);
            break;

        case 0x09:
            lfDistanceThreshold = ReadFloat();
            MassiveLog(5, GetName(), "Asset Distance Threshold: %f", lfDistanceThreshold);
            break;

        case 0x0A:
            luSizeThreshold = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "Asset Size Threshold: %d", luSizeThreshold);
            break;

        case 0x0B:
            lnMediaType = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Asset Type: %d", lnMediaType);
            ++lnRequiredFields;
            break;

        case 0x0C:
            lpcURL = ReadString();
            MassiveLog(5, GetName(), "Asset URL: %s", lpcURL);
            ++lnRequiredFields;
            break;

        case 0x15:
            lnCrexID = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Crex ID: %d", lnCrexID);
            ++lnRequiredFields;
            break;

        default:
            if (!SkipField(lbTag))
                return 0;
            break;
        }
    }

    if (lnRequiredFields != 8)
    {
        MassiveLog(2, GetName(), "Asset Block does not contain all of the required fields.");
        return 0;
    }
    MassiveLog(5, GetName(), "Asset Block contains all of the required fields.");

    void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveAsset));
    CMassiveAsset* lpAsset =
        lpMemory ? ::new (lpMemory) CMassiveAsset(lnAssetID, lnCrexID, lnMediaType, lnSize, lnRotationDuration,
                                                  lnExpiration, 0, lfAngleThreshold, lfDistanceThreshold, 0,
                                                  luSizeThreshold, lpcURL, lpHash)
                 : 0;

    if (lpcURL)
        MassiveFree(lpcURL);
    if (lpHash)
        MassiveFree(lpHash);
    return lpAsset;
}

// One order: its id and asset count (required), optional frequency cap, and its asset
// blocks; every asset joins this request's asset list and is attached to the order.
CMassiveOrder* CRequestEnterZone::ReadOrderBlock()
{
    MassiveLog(5, GetName(), "Reading Order Block...");

    unsigned char lnRequiredFields = 0;
    int lnOrderID = 0;
    int lnFrequencyCap = 0;
    unsigned short luAssetCount = 0;
    unsigned short luAssetsRead = 0;
    CMassiveList lOrderAssets;

    unsigned int luBlockLength = ReadU32();
    MassiveLog(5, GetName(), "Block Length: %d", luBlockLength);
    int lnBlockStart = mnPosition;
    while (static_cast<unsigned int>(mnPosition - lnBlockStart) < luBlockLength)
    {
        unsigned char lbTag = static_cast<unsigned char>(ReadU8());
        if (lbTag == 0x01)
        {
            luAssetCount = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "Asset Count: %d", luAssetCount);
            ++lnRequiredFields;
        }
        else if (lbTag == 0x03)
        {
            lnFrequencyCap = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Order Frequency Cap: %d", lnFrequencyCap);
        }
        else if (lbTag == 0x13)
        {
            lnOrderID = static_cast<int>(ReadU32());
            MassiveLog(5, GetName(), "Order ID: %d", lnOrderID);
            ++lnRequiredFields;
        }
        else if (lbTag == 0xDE)
        {
            CMassiveAsset* lpAsset = ReadAssetBlock();
            if (!lpAsset || !lpAsset->GetValid())
                return 0;
            AppendNode(&mAssetList, lpAsset);
            AppendNode(&lOrderAssets, lpAsset);
            ++luAssetsRead;
        }
        else if (!SkipField(lbTag))
        {
            return 0;
        }
    }

    if (lnRequiredFields != 2 || luAssetCount != luAssetsRead)
    {
        MassiveLog(2, GetName(), "Order Block does not contain all of the required fields.");
        return 0;
    }
    MassiveLog(5, GetName(), "Order Block contains all of the required fields.");

    void* lpMemory = CMassiveListNode::operator new(sizeof(CMassiveOrder));
    CMassiveOrder* lpOrder = lpMemory ? ::new (lpMemory) CMassiveOrder(lnOrderID, lnFrequencyCap) : 0;
    if (!lpOrder)
    {
        MassiveLog(1, GetName(), "Allocation failed for Order");
        return 0;
    }

    lOrderAssets.GoToStart();
    while (lOrderAssets.GetCurrent())
    {
        CMassiveAsset* lpAsset = static_cast<CMassiveAsset*>(lOrderAssets.GetCurrData());
        if (lpAsset)
        {
            lpAsset->mpOrder = lpOrder;
            MassiveLog(5, GetName(), "Attached Asset(%d) to Order(%d)", lpAsset->mnAssetId, lpOrder->mnNumber);
        }
        lOrderAssets.GoToNext();
    }
    return lpOrder;
}

// The enter-zone response: protocol version, block 206, then the IE count, order count and
// signature fields plus the inventory-element and order blocks they announce.
int CRequestEnterZone::Parse()
{
    unsigned char lnRequiredFields = 0;
    mnPosition = 0;
    MassiveLog(5, GetName(), "Reading Response...");

    if (!ReadRemoveVerifyProtocolVersion())
        return 0;

    unsigned char lbBlockID = static_cast<unsigned char>(ReadU8());
    MassiveLog(5, GetName(), "Block ID: %d", lbBlockID);
    if (lbBlockID != 206)
    {
        MassiveLog(2, GetName(), "Block ID of %d is not correct. Assuming its an error block.", lbBlockID);
        return 0;
    }

    unsigned int luBlockLength = ReadU32();
    MassiveLog(5, GetName(), "Block Length: %d", luBlockLength);
    int lnBlockStart = mnPosition;
    unsigned short luIEsRead = 0;
    unsigned short luOrdersRead = 0;
    while (static_cast<unsigned int>(mnPosition - lnBlockStart) < luBlockLength)
    {
        unsigned char lbTag = static_cast<unsigned char>(ReadU8());
        switch (lbTag)
        {
        case 0x1E:
            ReadRemoveSignature();
            MassiveLog(5, GetName(), "Reading HMAC Signature:");
            // (the signature is hex-dumped to the compiled-out logger)
            luBlockLength -= 22;
            ++lnRequiredFields;
            break;

        case 0x25:
            mnIECount = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "IE Count: %d", mnIECount);
            ++lnRequiredFields;
            break;

        case 0x55:
            mnOrderCount = static_cast<unsigned short>(ReadU16());
            MassiveLog(5, GetName(), "Order Count: %d", mnOrderCount);
            ++lnRequiredFields;
            break;

        case 0xDD:
        {
            CMassiveAdObject* lpAdObject = ReadIEBlock();
            if (!lpAdObject || !lpAdObject->GetValid())
                return 0;
            AppendNode(&mMAOList, lpAdObject);
            ++luIEsRead;
            break;
        }

        case 0xE3:
        {
            CMassiveOrder* lpOrder = ReadOrderBlock();
            if (!lpOrder || !lpOrder->GetValid())
                return 0;
            AppendNode(&mOrderList, lpOrder);
            ++luOrdersRead;
            break;
        }

        default:
            if (!SkipField(lbTag))
                return 0;
            break;
        }
    }

    if (lnRequiredFields != 3 || mnIECount != luIEsRead || mnOrderCount != luOrdersRead)
    {
        MassiveLog(2, GetName(), "Response does not contain all of the required fields.");
        return 0;
    }
    MassiveLog(5, GetName(), "Response contains all of the required fields.");

    if (!VerifyHMACSignature())
        return 0;

    MassiveLog(5, GetName(), "Response successfully read and parsed.");
    return 1;
}

} // namespace MassiveAdClient3
