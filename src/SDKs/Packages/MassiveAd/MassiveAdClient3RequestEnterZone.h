#pragma once

// MassiveAdClient3::CRequestEnterZone -- the "enter zone" protocol request (vendor middleware).
// The zone manager builds one when the game enters an ad zone; Parse reads the response's
// inventory elements (ad placements), assets and orders into the request's own lists, which
// the zone manager then copies out through GetMAOs / GetAssets / GetOrders.

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;
class CMassiveZoneManager;
class CMassiveAdObject;
class CMassiveAsset;
class CMassiveOrder;

class CRequestEnterZone : public CRequestObject
{
public:
    CRequestEnterZone();
    virtual ~CRequestEnterZone();

    const char* GetRequestURL() override;
    int Parse() override;

    // Builds the request block (zone name, player / session ids and, when all three are
    // non-zero, the bandwidth usage since the last zone) and marks it ready to submit.
    // -1100 without a builder or zone name.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcZoneName, int nBandwidthTotalSize,
                      int nBandwidthTotalTime, unsigned short sBandwidthTotalItems);
    int WriteEnterZoneRequest(const char* pcZoneName, int nBandwidthTotalSize, int nBandwidthTotalTime,
                              unsigned short sBandwidthTotalItems);

    // Append the parsed placements (stamped with pZoneManager), assets and orders to the
    // caller's lists.
    void GetMAOs(CMassiveList* pMAOList, CMassiveZoneManager* pZoneManager);
    void GetOrders(CMassiveList* pOrderList);
    void GetAssets(CMassiveList* pAssetList);

    CMassiveAdObject* ReadIEBlock();
    CMassiveAsset* ReadAssetBlock();
    CMassiveOrder* ReadOrderBlock();

private:
    CMassiveList   mMAOList;     // +0x50 parsed placements
    unsigned short mnIECount;    // +0x60 placement count the response announces
    CMassiveList   mAssetList;   // +0x64 parsed assets (every order's)
    int            mnField74;    // +0x74 (0 at construction)
    CMassiveList   mOrderList;   // +0x78 parsed orders
    unsigned short mnOrderCount; // +0x88 order count the response announces (set by Parse)
};

} // namespace MassiveAdClient3
