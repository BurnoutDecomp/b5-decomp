#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestEnterZone -- the "enter zone" protocol request
// (vendor middleware). A concrete CRequestObject: when the game enters an ad
// zone the MassiveAd zone layer (CMassiveZoneManager::Tick @ 0x82BD3130, state
// 1) builds one through CreateRequest, WriteEnterZoneRequest seals the outgoing
// block (zone name + client player/session IDs + optional bandwidth-usage
// triple), Parse() consumes the server response into the request's own three
// result lists (ReadIEBlock / ReadAssetBlock / ReadOrderBlock), and the zone
// manager's HandleResponse @ 0x82BD3410 drains those lists into its own
// mAdObjectList / mAssetList / mOrderList through GetMAOs / GetAssets /
// GetOrders. Sibling of CRequestExitZone / CRequestImpressionUpdate /
// CRequestObject / CRequestBuilder (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE is reconstructed from the X360 ARTIST.XEX pseudocode + disassembly.
// This header is the class's owning home, created for the CMassiveZoneManager
// slice; the BODIES stay in the CRequestEnterZone TU (not reconstructed here).
// Per-function X360 addresses:
//     MassiveAdClient3::CRequestEnterZone::CRequestEnterZone       @ 0x82BDAE20
//     MassiveAdClient3::CRequestEnterZone::GetRequestURL           @ 0x82BDAEA8
//     MassiveAdClient3::CRequestEnterZone::~CRequestEnterZone      @ 0x82BDAEB8
//     MassiveAdClient3::CRequestEnterZone::WriteEnterZoneRequest   @ 0x82BDAF10
//     MassiveAdClient3::CRequestEnterZone::ReadIEBlock             @ 0x82BDB0D8
//     MassiveAdClient3::CRequestEnterZone::ReadAssetBlock          @ 0x82BDB758
//     MassiveAdClient3::CRequestEnterZone::GetAssets               @ 0x82BDBB18
//                       (NOT in the ledger/export set -- recover via idat; walks
//                        the own list at +0x64, cursor +0x6C)
//     MassiveAdClient3::CRequestEnterZone::GetOrders               @ 0x82BDBBA0
//     MassiveAdClient3::CRequestEnterZone::GetMAOs                 @ 0x82BDBC28
//     MassiveAdClient3::CRequestEnterZone::`vector deleting destructor' @ 0x82BDBE08
//     MassiveAdClient3::CRequestEnterZone::CreateRequest           @ 0x82BDBE58
//     MassiveAdClient3::CRequestEnterZone::ReadOrderBlock          @ 0x82BDBEE0
//     MassiveAdClient3::CRequestEnterZone::Parse                   @ 0x82BDC1A8
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestEnterZone class / its methods) are PRESERVED
// VERBATIM -- external middleware API, not project-owned code.
//
// Layout over the committed CRequestObject base (members BY NAME; the X360
// allocation in CMassiveZoneManager::Tick is `CMassiveListNode::operator
// new(144)` = 0x90 bytes; the ctor @ 0x82BDAE20 chains CRequestObject(51,
// "RequestEnterZone"), installs vftable off_821872DC, and zeroes +0x50..+0x84
// -- exactly the five own members below):
//   +0x50  mMAOList     (CMassiveList of CMassiveAdObject; filled by ReadIEBlock
//                        via Parse; drained by GetMAOs, which also stamps each
//                        copied MAO's owner-zone pointer at MAO+0x18)
//   +0x60  mnField60    (0 at ctor; role recovered in the CRequestEnterZone TU)
//   +0x64  mAssetList   (CMassiveList of CMassiveAsset; filled by ReadAssetBlock
//                        via ReadOrderBlock/Parse; drained by GetAssets)
//   +0x74  mnField74    (0 at ctor; role recovered in the CRequestEnterZone TU)
//   +0x78  mOrderList   (CMassiveList of CMassiveOrder; filled by ReadOrderBlock
//                        via Parse; drained by GetOrders)
// The trailing X360 bytes +0x88..+0x8F of the 0x90-byte allocation are not
// touched by the ctor or any recovered caller and are NOT modelled -- the
// CRequestEnterZone TU adds them if its bodies attest them.
// ===========================================================================

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
    // @ 0x82BDAE20. Chains CRequestObject(51, "RequestEnterZone"), installs
    // this class's vftable (off_821872DC -- modelled by the virtuals), and
    // zeroes the three result lists + the two trailing fields.
    CRequestEnterZone();

    // @ 0x82BDAEB8 (slot 0; the vector deleting destructor @ 0x82BDBE08 is the
    // compiler-emitted thunk around it). Body in the CRequestEnterZone TU.
    virtual ~CRequestEnterZone();

    // @ 0x82BDAEA8. Returns the enter-zone server endpoint (the .data pointer
    // to the request URL string). FLAG: introduced as a virtual on this class
    // (the per-request-type endpoint getter, same slot pattern as the committed
    // sibling CRequestImpressionUpdate::GetRequestURL); the base CRequestObject
    // declaration is not in this ledger slice, so it is not marked `override`.
    virtual const char* GetRequestURL();

    // @ 0x82BDC1A8. Vftable Parse override the response path
    // (CRequestObject::Complete) dispatches: reads the enter-zone reply blocks
    // into the three result lists. Body in the CRequestEnterZone TU.
    int Parse() override;

    // @ 0x82BDBE58. Called by CMassiveZoneManager::Tick (state 1). Rejects a
    // null builder or a null zone name (returns -1100); otherwise chains the
    // base CreateRequest(pBuilder, 256, 1024), writes the enter-zone block, sets
    // status 2 (ready to submit) and logs "Created Request successfully.".
    // Returns 0 on success.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcZoneName,
                      int nBandwidthTotalSize, int nBandwidthTotalTime,
                      unsigned short sBandwidthTotalItems);

    // @ 0x82BDAF10. Builds the outgoing enter-zone block: zone name (tag 71),
    // the client player ID (tag 42) and session ID (tag 43), and -- only when
    // all three bandwidth values are non-zero -- the bandwidth-usage triple
    // (total size tag 16, total time tag 17, total items tag 18). Seals it with
    // FinishBaseBlock(205, 1, 1). Returns 1.
    int WriteEnterZoneRequest(const char* pcZoneName, int nBandwidthTotalSize,
                              int nBandwidthTotalTime,
                              unsigned short sBandwidthTotalItems);

    // @ 0x82BDBC28. Called by CMassiveZoneManager::HandleResponse (server type
    // 0x33): appends every parsed ad object on mMAOList into pMAOList (each in a
    // fresh CMassiveListNode) and stamps the copied MAO's owner-zone pointer
    // (MAO+0x18) with pZoneManager. Body in the CRequestEnterZone TU.
    int GetMAOs(CMassiveList* pMAOList, CMassiveZoneManager* pZoneManager);

    // @ 0x82BDBBA0. Appends every parsed CMassiveOrder on mOrderList into
    // pOrderList (each in a fresh CMassiveListNode). Body in the
    // CRequestEnterZone TU.
    int GetOrders(CMassiveList* pOrderList);

    // @ 0x82BDBB18 (NOT in the ledger/export set -- see the file header).
    // Appends every parsed CMassiveAsset on mAssetList into pAssetList (each in
    // a fresh CMassiveListNode). Body in the CRequestEnterZone TU.
    int GetAssets(CMassiveList* pAssetList);

    // @ 0x82BDB0D8. Reads one inventory-element block out of the response into
    // a freshly-allocated ad object. Body in the CRequestEnterZone TU.
    CMassiveAdObject* ReadIEBlock();

    // @ 0x82BDB758. Reads one asset block out of the response into a freshly-
    // allocated CMassiveAsset. Body in the CRequestEnterZone TU.
    CMassiveAsset* ReadAssetBlock();

    // @ 0x82BDBEE0. Reads one order block out of the response: the order id
    // (tag 19) + frequency cap (tag 3) + contained asset blocks (tag 222),
    // builds the CMassiveOrder, and attaches it to every asset read out of the
    // block (`asset->mpOrder = order`). Body in the CRequestEnterZone TU.
    CMassiveOrder* ReadOrderBlock();

private:
    CMassiveList mMAOList;    // +0x50 (parsed CMassiveAdObjects)
    int          mnField60;   // +0x60 (0 at ctor)
    CMassiveList mAssetList;  // +0x64 (parsed CMassiveAssets)
    int          mnField74;   // +0x74 (0 at ctor)
    CMassiveList mOrderList;  // +0x78 (parsed CMassiveOrders)
};

} // namespace MassiveAdClient3
