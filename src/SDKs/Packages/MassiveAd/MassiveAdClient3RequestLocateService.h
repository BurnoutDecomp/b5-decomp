#pragma once

// ===========================================================================
// MassiveAdClient3::CRequestLocateService -- the "/adsrv/4/locateService"
// service-locate request (vendor middleware). A concrete CRequestObject: the
// MassiveAd client core (CMassiveClientCore::RequestLocateService) builds one
// of these through CreateRequest, WriteLocateServiceRequest seals the outgoing
// block (SKU name/version + the MD5-hashed hardware id), and Parse() consumes
// the server response -- a directory of zone/media/impression server addresses
// (CAddressIndexPair) and ports (CPortIndexPair) plus the server clock, config
// options, auto-reporting period and heartbeat period. GetResponse*IndexPairs
// hand the parsed address/port lists back to the caller. Sibling of
// CRequestObject / CRequestBuilder / CRequestManager / CRequestImpressionUpdate
// / CRequestExitZone (all committed).
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CRequestLocateService::CRequestLocateService       @ 0x82BD3C58
//     MassiveAdClient3::CRequestLocateService::~CRequestLocateService      @ 0x82BD3CE0
//     MassiveAdClient3::CRequestLocateService::`vector deleting destructor'@ 0x82BD3D40
//     MassiveAdClient3::CRequestLocateService::GetRequestURL              @ 0x82BD3D30
//     MassiveAdClient3::CRequestLocateService::WriteLocateServiceRequest  @ 0x82BD3D90
//     MassiveAdClient3::CRequestLocateService::Parse                      @ 0x82BD3EE8
//     MassiveAdClient3::CRequestLocateService::GetResponseAddressIndexPairs @ 0x82BD44E8
//     MassiveAdClient3::CRequestLocateService::GetResponsePortIndexPairs  @ 0x82BD4570
//     MassiveAdClient3::CRequestLocateService::CreateRequest              @ 0x82BD45F8
//
// Unlike the sibling CRequestExitZone / CRequestImpressionUpdate, this TU's
// Parse @ 0x82BD3EE8 is FULLY reconstructed: it does NOT call the un-homed
// ReadRemoveSignature/STUB signature path -- every `bl STUB` in its asm is the
// MassiveLog trace hook (r3 = level, r4 = the base name at +0x0C, r5 = format,
// then the args), and every collaborator it touches is homed (or homed here for
// CPortIndexPair below).
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CRequestLocateService / CPortIndexPair class names / their
// methods) are PRESERVED VERBATIM -- external middleware API, not project-owned
// code.
//
// Layout over the committed CRequestObject base (members BY NAME; X360-absolute
// offsets are reproduced by member NAME because the base subobject and the
// embedded list pointers are wider on the 64-bit host):
//   +0x00  vftable            (off_82185A70; slot 0 = vector deleting destructor,
//                              the Parse override, GetRequestURL)
//   ...    CRequestObject base body
//   +0x50  mAddressPairs       (CMassiveList of CAddressIndexPair; ctor zeroes it,
//                              Parse Appends zone/media/impression address pairs)
//   +0x60  mPortPairs          (CMassiveList of CPortIndexPair; ctor zeroes it,
//                              Parse Appends zone/media/impression port pairs)
//   +0x70  mnServerTime        (int64 std/ld; server clock, wire tag 59, REQUIRED)
//   +0x78  mnServerConfigOptions (u16; server configurable options, wire tag 58)
//   +0x7A  mbAutoReportingTime (u8; auto-reporting period value, wire tag 13)
//   +0x7C  mbHasAutoReportingTime (int; set to 1 when tag 13 was present)
//   +0x80  mbHeartbeatPeriod   (u8; heartbeat period value, wire tag 84 'T')
//   +0x84  mbHasHeartbeatPeriod (int; set to 1 when tag 84 was present)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Request.h"

namespace MassiveAdClient3
{

class CRequestBuilder;

// ---------------------------------------------------------------------------
// MassiveAd MD5 helper (separate crypto TU).
//
// CalculateMD5Hash(data, dataLength) returns a freshly MassiveMalloc'd string
// holding the MD5 digest of the input (freed elsewhere / owned by the wire
// buffer copy WriteString makes), or null on failure. Attested by the named
// `bl CalculateMD5Hash` in WriteLocateServiceRequest's asm (r3 = the hardware-id
// string, r4 = its strlen). The X360 symbol demangles WITHOUT the
// MassiveAdClient3 namespace, but it is declared inside the namespace here so the
// whole vendor package stays self-contained (link-name fidelity is not a gate).
// Body lives in the MassiveAd crypto TU (sibling of CalculateSHA1HMac).
// ---------------------------------------------------------------------------
char* CalculateMD5Hash(const void* pData, int nDataLength);

// ---------------------------------------------------------------------------
// CPortIndexPair -- an (index/type byte, server port u16) pair (vendor
// middleware). A CMassiveBaseObject subclass Parse allocates through
// CMassiveListNode::operator new and appends to CRequestLocateService's port
// list, one per zone / media / impression server port in the response.
//
// Reconstructed from the X360 ARTIST.XEX: its constructor is INLINED into
// CRequestLocateService::Parse (there is no standalone ctor symbol in this TU),
// so every store is attested there:
//     CMassiveBaseObject::CMassiveBaseObject(this, "CPortIndexPair");
//     *this        = &off_82185A5C;   // install CPortIndexPair vftable
//     *(this+0x16) = <u16 port>;      // sth
//     *(this+0x14) = <u8 type>;       // stb  (3 = zone, 4 = media, 5 = impression)
// The vftable install is modelled by the virtual destructor. Layout over the
// 0x14-byte CMassiveBaseObject base (members BY NAME):
//   +0x14  mbType   (u8; 3 zone / 4 media / 5 impression)
//   +0x16  mnPort   (u16; the server port)
// ---------------------------------------------------------------------------
class CPortIndexPair : public CMassiveBaseObject
{
public:
    // Inlined into Parse @ 0x82BD3EE8. Chains the base ctor ("CPortIndexPair"),
    // installs this class's vftable (modelled by the virtual dtor), and stores
    // the port and the index/type byte.
    CPortIndexPair(unsigned short nPort, char bType)
        : CMassiveBaseObject("CPortIndexPair")
        , mbType(bType)
        , mnPort(nPort)
    {
    }

    // The X360 installs CPortIndexPair's own vftable (off_82185A5C) over the base
    // slot; that vftable rewrite is the compiler-emitted artifact of a virtual
    // destructor, so it is modelled here rather than stored by hand. No own
    // teardown (the pair holds no owned allocation).
    virtual ~CPortIndexPair() {}

    // The response port value (read from +0x16 for the trace line in Parse).
    unsigned short GetPort() const { return mnPort; }

private:
    char           mbType;  // +0x14 (3 zone / 4 media / 5 impression)
    unsigned short mnPort;  // +0x16 (the server port)
};

// ---------------------------------------------------------------------------
// CRequestLocateService -- the service-locate protocol request.
// ---------------------------------------------------------------------------
class CRequestLocateService : public CRequestObject
{
public:
    // @ 0x82BD3C58. Chains CRequestObject(17, "RequestLocateService"), installs
    // this class's vftable (off_82185A70 -- modelled by the virtuals), zeroes the
    // two embedded response lists and every trailing response field.
    CRequestLocateService();

    // @ 0x82BD3CE0 (chained by the vector deleting destructor @ 0x82BD3D40). No
    // own teardown beyond the two CMassiveList members (destroyed in reverse
    // declaration order -- ports then addresses, matching the X360) and the base
    // dtor; the deleting-destructor thunk conditionally frees through the base
    // operator delete.
    virtual ~CRequestLocateService();

    // @ 0x82BD3D30. Returns the service-locate server endpoint. The X360 loads it
    // through the .data pointer off_82F91C44 -> "/adsrv/4/locateService".
    // FLAG: introduced as a virtual on this class (the per-request-type endpoint
    // getter of vftable off_82185A70); the base CRequestObject declaration is not
    // in this TU's ledger slice, so it is not marked `override`.
    virtual const char* GetRequestURL();

    // @ 0x82BD3EE8. Vftable Parse override the response path (CRequestObject::
    // Complete) dispatches. Verifies the protocol version and block id (202),
    // then walks the block reading address/port pairs and the server clock/config/
    // auto-report/heartbeat fields. Returns 1 only when the required server-time
    // field was present, else 0.
    int Parse() override;

    // @ 0x82BD45F8. Called by CMassiveClientCore::RequestLocateService. Rejects a
    // null builder, SKU name or SKU version (returns -1100); otherwise chains the
    // base CreateRequest(pBuilder, 256, 256) (returning -1097 on its failure),
    // writes the locate-service block, sets status 2 (ready to submit) and logs.
    // Returns 0 on success.
    int CreateRequest(CRequestBuilder* pBuilder, const char* pcSkuName,
                      const char* pcSkuVersion);

    // @ 0x82BD3D90. Builds the outgoing locate-service block: SKU name (tag 61)
    // and SKU version (tag 62), then -- when the platform hardware address is
    // available -- its MD5 hash (tag 29). Seals it with FinishBaseBlock(201, 0, 0)
    // and returns that result.
    int WriteLocateServiceRequest(const char* pcSkuName, const char* pcSkuVersion);

    // @ 0x82BD44E8. Called by CMassiveClientCore::HandleResponse. Shallow-copies
    // the parsed address-pair list into pOutList (one fresh CMassiveListNode per
    // entry, each pointing at the same CAddressIndexPair payload). A null pOutList
    // is a no-op. (The X360 r3 return is the incidental result of the last list
    // walk; modelled as void.)
    void GetResponseAddressIndexPairs(CMassiveList* pOutList);

    // @ 0x82BD4570. As GetResponseAddressIndexPairs, for the port-pair list.
    void GetResponsePortIndexPairs(CMassiveList* pOutList);

private:
    CMassiveList   mAddressPairs;          // +0x50 (CAddressIndexPair entries)
    CMassiveList   mPortPairs;             // +0x60 (CPortIndexPair entries)
    long long      mnServerTime;           // +0x70 (server clock; tag 59; required)
    unsigned short mnServerConfigOptions;  // +0x78 (server config options; tag 58)
    unsigned char  mbAutoReportingTime;    // +0x7A (auto-report period; tag 13)
    int            mbHasAutoReportingTime; // +0x7C (set when tag 13 present)
    unsigned char  mbHeartbeatPeriod;      // +0x80 (heartbeat period; tag 84 'T')
    int            mbHasHeartbeatPeriod;   // +0x84 (set when tag 84 present)
};

} // namespace MassiveAdClient3
