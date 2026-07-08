#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestOpenSession.h"

#include <cstring>

// ===========================================================================
// MassiveAdClient3::CRequestOpenSession -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order; every scalar write here appends (bPrepend = 0,
// r5 = 0). The vftable install the X360 ctor / scalar deleting destructor perform
// (off_82186064) is modelled by the class virtuals, so no vftable store is
// written by hand. The verbose-trace call sites the X360 renders as Hex-Rays
// `STUB(level, mpcName, fmt, ...)` are the MassiveLog hook (r3 = level, r4 = the
// base name at +0x0C, r5 = format, then the args); the `free' indirect
// (off_82F91C18) is MassiveFree.
//
// Parse @ 0x82BD4E58 is intentionally NOT defined here (BLOCKED): inside its
// signature branch (wire tag 30) its body calls CRequestObject::ReadRemoveSignature
// (un-attested in the committed base header) and then an un-homed, un-named
// function (Hex-Rays `STUB(this, mpSignature, 20)`). See the header.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestOpenSession::CRequestOpenSession @ 0x82BD4D98
//
//   li r4, 0x23=35 ; addi r5, "RequestOpenSession"
//   bl CRequestObject::CRequestObject(this, 35, "RequestOpenSession")
//   stw off_82186064, 0(this)          -> install vftable (modelled by vtable)
//   stw 0, 0x50(this)                  -> mnMassivePlayerID  = 0
//   stw 0, 0x54(this)                  -> mnMassiveSessionID = 0
// ---------------------------------------------------------------------------
CRequestOpenSession::CRequestOpenSession()
    : CRequestObject(35, "RequestOpenSession")
    , mnMassivePlayerID(0)   // stw r10(0), 0x50(r31)
    , mnMassiveSessionID(0)  // stw r10(0), 0x54(r31)
{
}

// ---------------------------------------------------------------------------
// CRequestOpenSession::~CRequestOpenSession @ 0x82BD4E00
//                                            (scalar deleting destructor)
//
// No own teardown -- the X360 scalar deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestOpenSession::~CRequestOpenSession()
{
}

// ---------------------------------------------------------------------------
// CRequestOpenSession::GetRequestURL @ 0x82BD4DF0
//
//   lis r11, off_82F91CAC@ha ; lwz r3, off_82F91CAC@l(r11)  # "/adsrv/4/openSession"
//   blr
//
// off_82F91CAC is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestOpenSession::GetRequestURL()
{
    return "/adsrv/4/openSession";
}

// ---------------------------------------------------------------------------
// CRequestOpenSession::WriteOpenSessionRequest @ 0x82BD50A0
//
// All writes append (bPrepend = 0). Block body, in asm order:
//   MassivePRNG(token);                    // fill the session token
//   [61,string skuName]
//   [62,string skuVersion]
//   [60][u8 sessionType]
//   if (thirdPartyID):      [65,string thirdPartyID]      else log "No Third Party ID Set"
//   if (thirdPartyService): [66,string thirdPartyService] else log "No Third Party Service Set"
//   if (sessionType == 2):
//     if (multiplayerGUID): [28,string multiplayerGUID]
//     else                  log "No Multiplayer GUID supplied for Multiplayer Session."
//   [68,string token]
//   if (writeHardwareID):
//     Instance(); GetHardwareAddress(&hw);
//     if (hw && strlen(hw)): hash = CalculateMD5Hash(hw, strlen); [29,string hash]; free(hw)
//     else                   log "Could not get Hardware ID from system."
//   [22,string "3.3.0.22"]
//   return FinishBaseBlock(203, 1, 1);
// ---------------------------------------------------------------------------
int CRequestOpenSession::WriteOpenSessionRequest(const char* pcSKUName,
                                                 const char* pcSKUVersion,
                                                 int nSessionType,
                                                 int bWriteHardwareID,
                                                 const char* pcMultiplayerGUID)
{
    MassivePRNG(gacHMACKey);          // MassivePRNG(byte_8327F2A0) -- fill token
    MassiveLog(5, GetName(), "Writing Request...");

    WriteU8(61, 0);                  // li r4, 0x3D
    WriteString(pcSKUName, 0);
    MassiveLog(5, GetName(), "Writing SKU Name: %s", pcSKUName);

    WriteU8(62, 0);                  // li r4, 0x3E
    WriteString(pcSKUVersion, 0);
    MassiveLog(5, GetName(), "Writing SKU Version: %s", pcSKUVersion);

    WriteU8(60, 0);                  // li r4, 0x3C
    WriteU8((char)nSessionType, 0);  // clrlwi r4, r28, 24 -- session type byte
    MassiveLog(5, GetName(), "Writing Session Type: %d", nSessionType);

    if (gpcThirdPartyID)             // dword_8327F2C4
    {
        WriteU8(65, 0);              // li r4, 0x41
        WriteString(gpcThirdPartyID, 0);
        MassiveLog(5, GetName(), "Writing UnHashed Third Party ID: %s", gpcThirdPartyID);
    }
    else
    {
        MassiveLog(5, GetName(), "No Third Party ID Set");
    }

    if (gpcThirdPartyService)        // dword_8327F2C8
    {
        WriteU8(66, 0);              // li r4, 0x42
        WriteString(gpcThirdPartyService, 0);
        MassiveLog(5, GetName(), "Writing Third Party Service Name: %s", gpcThirdPartyService);
    }
    else
    {
        MassiveLog(5, GetName(), "No Third Party Service Set");
    }

    if (nSessionType == 2)           // cmpwi r28, 2
    {
        if (pcMultiplayerGUID)       // cmplwi r26, 0
        {
            WriteU8(28, 0);          // li r4, 0x1C
            WriteString(pcMultiplayerGUID, 0);
            MassiveLog(5, GetName(), "Writing Multiplayer GUID: %s", pcMultiplayerGUID);
        }
        else
        {
            MassiveLog(2, GetName(), "No Multiplayer GUID supplied for Multiplayer Session.");
        }
    }

    WriteU8(68, 0);                  // li r4, 0x44
    WriteString(gacHMACKey, 0);      // byte_8327F2A0 -- the freshly-generated token
    MassiveLog(5, GetName(), "Writing Token: <Not showing for security reasons>");

    if (bWriteHardwareID)            // cmpwi r25, 0
    {
        // The X360 feeds Instance()'s result into GetHardwareAddress's dead leading
        // argument (nUnused); Instance() is still invoked, and 0 is passed for the
        // value the callee never reads (the committed convention, matching the
        // sibling CRequestLocateService::WriteLocateServiceRequest).
        char* pcHardwareID = 0;
        CMassiveSystem::Instance();
        CMassiveSystem::GetHardwareAddress(0, &pcHardwareID);

        int lnLength;
        if (pcHardwareID && (lnLength = (int)std::strlen(pcHardwareID)) != 0)
        {
            char* pcHash = CalculateMD5Hash(pcHardwareID, lnLength);
            WriteU8(29, 0);          // li r4, 0x1D
            WriteString(pcHash, 0);
            MassiveLog(5, GetName(), "Writing Hashed Hardware ID: %s", pcHash);
            MassiveFree(pcHardwareID);   // off_82F91C18
        }
        else
        {
            MassiveLog(2, GetName(), "Could not get Hardware ID from system.");
        }
    }

    WriteU8(22, 0);                  // li r4, 0x16
    WriteString("3.3.0.22", 0);
    return FinishBaseBlock(203, 1, 1);   // li r4, 0xCB ; li r5, 1 ; li r6, 1
}

// ---------------------------------------------------------------------------
// CRequestOpenSession::CreateRequest @ 0x82BD53C8
//
// Rejects a null builder / SKU name / SKU version (-1100). Otherwise chains the
// base CreateRequest(pBuilder, 512, 512), writes the open-session block, marks
// the request ready to submit (SetStatus 2), logs, and returns 0.
//
// The asm carries the arguments through as: WriteOpenSessionRequest(this, skuName,
// skuVersion, sessionType, writeHardwareID, multiplayerGUID) -- the session-type
// and the two trailing pointers are reordered from CreateRequest's own parameter
// order by the register moves at 0x82BD5410..0x82BD5424, reproduced here.
// ---------------------------------------------------------------------------
int CRequestOpenSession::CreateRequest(CRequestBuilder* pBuilder, const char* pcSKUName,
                                       const char* pcSKUVersion, int nSessionType,
                                       const char* pcMultiplayerGUID, int bWriteHardwareID)
{
    if (!pBuilder || !pcSKUName || !pcSKUVersion)
        return -1100;                // li r3, -0x44C

    CRequestObject::CreateRequest(pBuilder, 512, 512); // li r5, 0x200 ; li r6, 0x200
    WriteOpenSessionRequest(pcSKUName, pcSKUVersion, nSessionType, bWriteHardwareID,
                            pcMultiplayerGUID);
    SetStatus(2);                    // li r4, 2
    MassiveLog(5, GetName(), "Created Request successfully.");
    return 0;
}

} // namespace MassiveAdClient3
