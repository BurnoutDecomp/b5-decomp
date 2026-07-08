#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestLocateService.h"

#include <new>
#include <cstring>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestBuilder.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3Objects.h"

// ===========================================================================
// MassiveAdClient3::CRequestLocateService -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order. The vftable installs the X360 ctor / dtors
// perform (off_82185A70 for this class, off_82185A5C for CPortIndexPair) are
// modelled by the class virtuals, so no vftable store is written by hand. The
// verbose-trace call sites the X360 renders as Hex-Rays `STUB(level, mpcName,
// fmt, ...)` are the MassiveLog hook (r3 = level, r4 = the base name at +0x0C,
// r5 = format, then the args); the `free' indirect (off_82F91C18) is MassiveFree.
//
// Every allocation follows the vendor idiom the sibling classes use: the raw
// block comes from CMassiveListNode::operator new (the MassiveAd heap hook) and
// the object is placement-constructed into it, so a null allocation leaves a
// null object exactly as the X360's `if (ptr) ctor(ptr,...) else 0`.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestLocateService::CRequestLocateService @ 0x82BD3C58
//
//   bl CRequestObject::CRequestObject(this, 0x11=17, "RequestLocateService")
//   stw off_82185A70, 0(this)          -> install vftable (modelled by vtable)
//   the two CMassiveList members and every trailing response field are zeroed
//   (stw/std/sth/stb 0 at +0x50..+0x84); reproduced by the members' own default
//   initialisation below.
// ---------------------------------------------------------------------------
CRequestLocateService::CRequestLocateService()
    : CRequestObject(17, "RequestLocateService")
    , mAddressPairs()             // stw 0 @ +0x50..+0x5C
    , mPortPairs()                // stw 0 @ +0x60..+0x6C
    , mnServerTime(0)             // std 0 @ +0x70
    , mnServerConfigOptions(0)    // sth 0 @ +0x78
    , mbAutoReportingTime(0)      // stb 0 @ +0x7A
    , mbHasAutoReportingTime(0)   // stw 0 @ +0x7C
    , mbHeartbeatPeriod(0)        // stb 0 @ +0x80
    , mbHasHeartbeatPeriod(0)     // stw 0 @ +0x84
{
}

// ---------------------------------------------------------------------------
// CRequestLocateService::~CRequestLocateService @ 0x82BD3CE0
//                                               (vector deleting destructor @ 0x82BD3D40)
//
//   stw off_82185A70, 0(this)          -> rewrite vftable (modelled by vtable)
//   CMassiveList::~CMassiveList(this+0x60)   -> destroy mPortPairs
//   CMassiveList::~CMassiveList(this+0x50)   -> destroy mAddressPairs
//   CRequestObject::~CRequestObject(this)    -> chain base dtor
// The two list members destruct in reverse declaration order (ports then
// addresses), exactly matching the X360; the base dtor chains automatically.
// ---------------------------------------------------------------------------
CRequestLocateService::~CRequestLocateService()
{
}

// ---------------------------------------------------------------------------
// CRequestLocateService::GetRequestURL @ 0x82BD3D30
//
//   lis r11, off_82F91C44@ha ; lwz r3, off_82F91C44@l(r11)  # "/adsrv/4/locateService"
//   blr
//
// off_82F91C44 is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestLocateService::GetRequestURL()
{
    return "/adsrv/4/locateService";
}

// ---------------------------------------------------------------------------
// CRequestLocateService::WriteLocateServiceRequest @ 0x82BD3D90
//
//   [61,string skuName][62,string skuVersion]
//   Instance(); GetHardwareAddress(&hw);
//   if (hw && strlen(hw)):
//     hash = CalculateMD5Hash(hw, strlen);
//     [29,string hash]; free(hw)
//   else: log "Could not get Hardware ID from system."
//   return FinishBaseBlock(201, 0, 0);
// ---------------------------------------------------------------------------
int CRequestLocateService::WriteLocateServiceRequest(const char* pcSkuName,
                                                     const char* pcSkuVersion)
{
    MassiveLog(5, GetName(), "Writing Request...");

    WriteU8(61, 0);                  // li r4, 0x3D
    WriteString(pcSkuName, 0);
    MassiveLog(5, GetName(), "Writing SKU Name: %s", pcSkuName);

    WriteU8(62, 0);                  // li r4, 0x3E
    WriteString(pcSkuVersion, 0);
    MassiveLog(5, GetName(), "Writing SKU Version: %s", pcSkuVersion);

    // The X360 feeds Instance()'s result into GetHardwareAddress's dead leading
    // argument (nUnused); Instance() is still invoked for that side-effect, and 0
    // is passed for the value the callee never reads (the committed convention,
    // see MassiveAdClient3_embed_check.cpp).
    char* pcHardwareAddress = 0;
    CMassiveSystem::Instance();
    CMassiveSystem::GetHardwareAddress(0, &pcHardwareAddress);

    int lnLength;
    if (pcHardwareAddress && (lnLength = (int)std::strlen(pcHardwareAddress)) != 0)
    {
        char* pcHash = CalculateMD5Hash(pcHardwareAddress, lnLength);
        WriteU8(29, 0);              // li r4, 0x1D
        WriteString(pcHash, 0);
        MassiveLog(5, GetName(), "Writing Hashed Hardware ID: %s", pcHash);
        MassiveFree(pcHardwareAddress);   // off_82F91C18
    }
    else
    {
        MassiveLog(2, GetName(), "Could not get Hardware ID from system.");
    }

    return FinishBaseBlock(201, 0, 0);   // li r4, 0xC9 ; li r5, 0 ; li r6, 0
}

// ---------------------------------------------------------------------------
// CRequestLocateService::CreateRequest @ 0x82BD45F8
//
// Rejects a null builder / SKU name / SKU version (-1100). Otherwise chains the
// base CreateRequest(pBuilder, 256, 256) (returning -1097 on its failure),
// writes the block, marks the request ready to submit (SetStatus 2), logs, and
// returns 0.
// ---------------------------------------------------------------------------
int CRequestLocateService::CreateRequest(CRequestBuilder* pBuilder, const char* pcSkuName,
                                         const char* pcSkuVersion)
{
    if (!pBuilder || !pcSkuName || !pcSkuVersion)
        return -1100;                // li r3, -0x44C

    if (CRequestObject::CreateRequest(pBuilder, 256, 256))   // li r5/r6, 0x100
        return -1097;                // li r3, -0x449

    WriteLocateServiceRequest(pcSkuName, pcSkuVersion);
    SetStatus(2);                    // li r4, 2
    MassiveLog(5, GetName(), "Created request successfully.");
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestLocateService::Parse @ 0x82BD3EE8
//
// Rewinds the read cursor, verifies the protocol version and the block id (202),
// then walks the block: address pairs (tags 'H'/0x22/0x2D), port pairs
// (tags 'I'/'J'/'K'), the server clock (tag 59, the single REQUIRED field), the
// config options (tag 58), the auto-reporting period (tag 13) and the heartbeat
// period (tag 84 'T'); anything else is SkipField'd. Returns 1 only when the
// server-time field was present, else 0.
// ---------------------------------------------------------------------------
int CRequestLocateService::Parse()
{
    int lnServerTimeCount = 0;      // v38 -- the required server-time field counter

    mnPosition = 0;                 // stw 0, 0x20(this) -- rewind the read cursor
    MassiveLog(5, GetName(), "Reading Response...");

    if (!ReadRemoveVerifyProtocolVersion())
        return 0;

    unsigned char lbBlockId = (unsigned char)ReadU8();
    MassiveLog(5, GetName(), "Block ID: %d", lbBlockId);
    if (lbBlockId != 202)           // 0xCA
    {
        MassiveLog(2, GetName(),
                   "Block ID of %d is not correct. Assuming its an error block.", lbBlockId);
        return 0;
    }

    unsigned int lnBlockLength = ReadU32();
    MassiveLog(5, GetName(), "Block Length: %d", lnBlockLength);

    int lnStartPosition = mnPosition;   // v39 -- snapshot before the field loop
    if (lnBlockLength)
    {
        do
        {
            unsigned char lbFieldTag = (unsigned char)ReadU8();
            switch (lbFieldTag)
            {
            case 0x48:  // 'H' -- Zone Server address
            {
                char* pcAddress = ReadString();
                if (!pcAddress)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for Zone Server String");
                    return 0;
                }
                CAddressIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CAddressIndexPair));
                if (lpMem)
                    lpPair = ::new (lpMem) CAddressIndexPair(pcAddress, 3);
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CAddressIndexPair for Zone Server");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mAddressPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Zone Server:%s", lpPair->GetAddress());
                MassiveFree(pcAddress);   // LABEL_42 -- off_82F91C18
                break;
            }

            case 0x22:  // Impression Server address
            {
                char* pcAddress = ReadString();
                if (!pcAddress)
                {
                    // The X360 shares the "Zone Server String" rodata here (both
                    // paths branch to the same message); reproduced verbatim.
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for Zone Server String");
                    return 0;
                }
                CAddressIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CAddressIndexPair));
                if (lpMem)
                    lpPair = ::new (lpMem) CAddressIndexPair(pcAddress, 5);
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CAddressIndexPair for Impression Server");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mAddressPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Impression Server:%s", lpPair->GetAddress());
                MassiveFree(pcAddress);   // LABEL_42
                break;
            }

            case 0x2D:  // '-' -- Media Server address
            {
                char* pcAddress = ReadString();
                if (!pcAddress)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for Media Server String");
                    return 0;
                }
                CAddressIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CAddressIndexPair));
                if (lpMem)
                    lpPair = ::new (lpMem) CAddressIndexPair(pcAddress, 4);
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CAddressIndexPair for Media Server");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mAddressPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Media Server:%s", lpPair->GetAddress());
                MassiveFree(pcAddress);   // LABEL_42
                break;
            }

            case 0x49:  // 'I' -- Zone Server port
            {
                CPortIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CPortIndexPair));
                if (lpMem)
                {
                    unsigned short lnPort = (unsigned short)ReadU16();
                    lpPair = ::new (lpMem) CPortIndexPair(lnPort, 3);
                }
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CPortIndexPair for Zone Port");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mPortPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Zone Server Port:%u", lpPair->GetPort());
                break;
            }

            case 0x4A:  // 'J' -- Media Server port
            {
                CPortIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CPortIndexPair));
                if (lpMem)
                {
                    unsigned short lnPort = (unsigned short)ReadU16();
                    lpPair = ::new (lpMem) CPortIndexPair(lnPort, 4);
                }
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CPortIndexPair for Media Port");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mPortPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Media Server Port:%u", lpPair->GetPort());
                break;
            }

            case 0x4B:  // 'K' -- Impression Server port
            {
                CPortIndexPair* lpPair = 0;
                void* lpMem = CMassiveListNode::operator new(sizeof(CPortIndexPair));
                if (lpMem)
                {
                    unsigned short lnPort = (unsigned short)ReadU16();
                    lpPair = ::new (lpMem) CPortIndexPair(lnPort, 5);
                }
                if (!lpPair)
                {
                    MassiveLog(2, GetName(),
                               "ALLOCATION Failed, could not allocate memory for CPortIndexPair for Impression Port");
                    return 0;
                }
                void* lpNodeMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
                CMassiveListNode* lpNode = lpNodeMem ? ::new (lpNodeMem) CMassiveListNode(lpPair) : 0;
                mPortPairs.Append(lpNode);
                MassiveLog(5, GetName(), "Reading Impression Server Port:%u", lpPair->GetPort());
                break;
            }

            case 0x0D:  // Auto Reporting Time
            {
                unsigned char lbValue = (unsigned char)ReadU8();
                mbHasAutoReportingTime = 1;         // stw 1, 0x7C
                mbAutoReportingTime = lbValue;      // stb, 0x7A
                MassiveLog(5, GetName(), "Reading Auto Reporting Time:%u", lbValue);
                break;
            }

            case 0x54:  // 'T' -- Heartbeat Period
            {
                unsigned char lbValue = (unsigned char)ReadU8();
                mbHasHeartbeatPeriod = 1;           // stw 1, 0x84
                mbHeartbeatPeriod = lbValue;        // stb, 0x80
                MassiveLog(5, GetName(), "Reading Heartbeat Period:%u", lbValue);
                break;
            }

            case 0x3A:  // ':' -- Server Configurable Options
            {
                unsigned short lnOptions = (unsigned short)ReadU16();
                mnServerConfigOptions = lnOptions;  // sth, 0x78
                MassiveLog(5, GetName(), "Reading Server Configurable Options:%u", lnOptions);
                break;
            }

            case 0x3B:  // ';' -- Server Time (the single REQUIRED field)
            {
                unsigned long long lnServerTime = ReadU64();
                mnServerTime = lnServerTime;        // std, 0x70
                MassiveLog(5, GetName(), "Reading Server Time:%lu", lnServerTime);
                ++lnServerTimeCount;
                break;
            }

            default:
                if (!SkipField(lbFieldTag))
                    return 0;
                break;
            }
        }
        while ((unsigned int)(mnPosition - lnStartPosition) < lnBlockLength);
    }

    if (lnServerTimeCount != 1)
    {
        MassiveLog(2, GetName(), "Response does not contain all of the required fields.");
        return 0;
    }

    MassiveLog(5, GetName(), "Response contains all of the required fields.");
    MassiveLog(5, GetName(), "Response successfully read and parsed.");
    return 1;
}

// ---------------------------------------------------------------------------
// CRequestLocateService::GetResponseAddressIndexPairs @ 0x82BD44E8
//
// Shallow-copies the parsed address-pair list into pOutList: for each entry a
// fresh CMassiveListNode (allocated through the MassiveAd heap hook) wrapping the
// same CAddressIndexPair payload is appended. A null pOutList is a no-op.
// ---------------------------------------------------------------------------
void CRequestLocateService::GetResponseAddressIndexPairs(CMassiveList* pOutList)
{
    if (!pOutList)
        return;

    pOutList->GoToStart();
    mAddressPairs.GoToStart();
    while (mAddressPairs.GetCurrent())
    {
        void* lpMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
        CMassiveListNode* lpNode =
            lpMem ? ::new (lpMem) CMassiveListNode(mAddressPairs.GetCurrData()) : 0;
        pOutList->Append(lpNode);
        mAddressPairs.GoToNext();
    }
}

// ---------------------------------------------------------------------------
// CRequestLocateService::GetResponsePortIndexPairs @ 0x82BD4570
//
// As GetResponseAddressIndexPairs, for the port-pair list (walked from +0x60).
// ---------------------------------------------------------------------------
void CRequestLocateService::GetResponsePortIndexPairs(CMassiveList* pOutList)
{
    if (!pOutList)
        return;

    pOutList->GoToStart();
    mPortPairs.GoToStart();
    while (mPortPairs.GetCurrent())
    {
        void* lpMem = CMassiveListNode::operator new(sizeof(CMassiveListNode));
        CMassiveListNode* lpNode =
            lpMem ? ::new (lpMem) CMassiveListNode(mPortPairs.GetCurrData()) : 0;
        pOutList->Append(lpNode);
        mPortPairs.GoToNext();
    }
}

} // namespace MassiveAdClient3
