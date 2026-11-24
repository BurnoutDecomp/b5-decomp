// XDK size optimization matches ARTIST at 0x82BD4E00, 0x82BD53C8 (2 functions).
// MassiveAd was built for size: /Os turns 63 more of its functions exact across 31 files, none worse.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestOpenSession.h"

#include <cstring>

// ===========================================================================
// MassiveAdClient3::CRequestOpenSession -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no reference / DecFIGS).
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
// Parse @ 0x82BD4E58 is not yet defined here (body pending). Its two
// once-blocking collaborators are RESOLVED: ReadRemoveSignature @ 0x82BD02D0 is
// declared+defined in the committed base slice (MassiveAdClient3Request.cpp),
// and the post-signature `bl STUB` (`STUB(this, mpSignature, 20)`) is the
// measured single-`blr` ICF-folded trace hook @ 0x82AD5078 (documented, not
// modelled -- the CRequestImpressionUpdate::Parse precedent). The remaining
// compile dependency is a CMassiveClientCore::ZoneNameAdd declaration in the
// shared MassiveAdClient3ClientCore.h. See the header.
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
// ---------------------------------------------------------------------------
char* CRequestOpenSession::m_szRequestURL = "/adsrv/4/openSession"; // ARTIST 0x82F91CAC

char* CRequestOpenSession::GetRequestURL()
{
    return m_szRequestURL;
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

    if (m_szThirdPartyID)            // dword_8327F2C4
    {
        WriteU8(65, 0);              // li r4, 0x41
        WriteString(m_szThirdPartyID, 0);
        MassiveLog(5, GetName(), "Writing UnHashed Third Party ID: %s", m_szThirdPartyID);
    }
    else
    {
        MassiveLog(5, GetName(), "No Third Party ID Set");
    }

    if (m_szThirdPartyService)       // dword_8327F2C8
    {
        WriteU8(66, 0);              // li r4, 0x42
        WriteString(m_szThirdPartyService, 0);
        MassiveLog(5, GetName(), "Writing Third Party Service Name: %s", m_szThirdPartyService);
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
        char* pcHardwareID = 0;
        CMassiveSystem::Instance()->GetHardwareAddress(&pcHardwareID);

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

// FOLDED FROM MassiveAdClient3RequestOpenSession_wO_01.cpp

#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestOpenSession.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestOpenSession::Parse @ 0x82BD4E58
//
// Open-session response walker (vftable slot 1). Rewinds the cursor, verifies
// the protocol version, demands response block id 204, reads the block's
// remaining-byte total and then walks the field list until the cursor has
// consumed that many bytes. Three fields are REQUIRED (the HMAC signature block
// wire tag 30, the Massive player id tag 42 and the Massive session id tag 43);
// the zone-name field (tag 71) is optional and every other tag must SkipField
// cleanly. Success additionally requires the closing HMAC to verify.
//
// Register map (MEASURED, 0x82BD4E58..0x82BD509C; frame 0xB0, __savegprlr_22):
//   r31 = this
//   r28 = required-field counter, kept as a u8 (`clrlwi r11, r28, 24` around
//         every increment at 0x82BD501C..0x82BD5024)
//   r30 = the block-id byte, later reused for the tag-71 zone string
//   r29 = the remaining-byte total from ReadU32 (also the tag-30 -= 22 target)
//   r22 = the post-header cursor snapshot (`lwz r22, 0x20(r31)` @0x82BD4F08)
//   r27/r26/r25/r24 = the four loop-body format strings, hoisted out of the
//         loop by the compiler; r23 = off_82F91C18 (the MassiveFree hook)
//   loop test @0x82BD5028: `lwz r11, 0x20(r31) ; subf r11, r22, r11 ;
//         cmplw cr6, r11, r29 ; blt` -- consumed is recomputed from the LIVE
//         cursor at the BOTTOM of every iteration and compared UNSIGNED.
//
// Every integer literal below is a WIRE protocol value (field tags, the 204
// response-type byte, the 22-byte serialised signature-field size, the
// required-field count 3) -- none of them is a console offset, stride or object
// size, so nothing here is layout-dependent. Both response members and the base
// cursor / signature pointer are reached BY NAME (X360 +0x50 / +0x54 / +0x20 /
// +0x30 appear in comments only, since the base subobject and every pointer are
// wider on the 64-bit host).
// ---------------------------------------------------------------------------
int CRequestOpenSession::Parse()
{
    unsigned char lnRequiredFields = 0;   // li r28, 0
    mnPosition = 0;                       // stw r28, 0x20(r31) -- rewind cursor

    MassiveLog(5, GetName(), "Reading Open Session Response...");

    if (!ReadRemoveVerifyProtocolVersion())   // cmpwi r3, 0 ; beq -> return 0
        return 0;

    unsigned char lnBlockID = static_cast<unsigned char>(ReadU8());  // clrlwi r30, r11, 24
    MassiveLog(5, GetName(), "Block ID: %d", lnBlockID);             // r6 = r30
    if (lnBlockID != 204)                     // cmplwi cr6, r30, 0xCC
    {
        MassiveLog(2, GetName(),
                   "Block ID of %d is not correct. Assuming its an error block.", lnBlockID);
        return 0;
    }

    unsigned int lnRemaining = ReadU32();     // r29 = block length
    MassiveLog(5, GetName(), "Block Length: %d", lnRemaining);       // r6 = r29

    int lnBase = mnPosition;                  // r22 -- post-header cursor

    // `subf r11, r22, r22` @0x82BD4F0C makes the first consumed value 0, and the
    // entry test (`cmplw cr6, r11, r29 ; bge -> loc_82BD5038`) is the same
    // UNSIGNED compare the bottom test uses -- a zero-length block skips the
    // loop entirely and lands straight on the required-field check.
    for (unsigned int lnConsumed = 0; lnConsumed < lnRemaining;
         lnConsumed = static_cast<unsigned int>(mnPosition - lnBase))
    {
        unsigned char lnTag = static_cast<unsigned char>(ReadU8());  // clrlwi r11, r4, 24

        if (lnTag == 30)                      // cmpwi cr6, r11, 0x1E -- HMAC signature
        {
            ReadRemoveSignature();            // r3 = this, already loaded
            MassiveLog(5, GetName(), "Reading HMAC Signature:");
            // bl 0x82AD5078 with (r3 = this, r4 = mpSignature [lwz r4, 0x30(r31)],
            // r5 = 20 [li r5, 0x14, the SHA1-HMAC digest length]). MEASURED: the
            // callee's whole body is a single `blr` (4 bytes, 150 xrefs across
            // unrelated subsystems) -- the ICF-folded, compiled-out debug/trace
            // hook. Attested no-op: there is no behaviour to reproduce and the
            // fold destroyed the original name, so the call is DOCUMENTED here
            // rather than modelled (inventing a hook name would be fabrication).
            // Same treatment as the sibling CRequestImpressionUpdate::Parse.
            lnRemaining -= 22;                // addi r29, r29, -0x16 (the 2-byte
                                              // length + 20-byte digest that
                                              // ReadRemoveSignature pulled back
                                              // out of the buffer)
            ++lnRequiredFields;               // falls through into loc_82BD501C
        }
        else if (lnTag == 42)                 // cmpwi cr6, r11, 0x2A -- player id
        {
            mnMassivePlayerID = ReadU32();    // mr r6, r3 ; stw r6, 0x50(r31)
            // r6 still carries the freshly-read value into the log call (the
            // Hex-Rays pseudocode DROPS this vararg; the asm at 0x82BD4FE4 has it).
            MassiveLog(5, GetName(), "Reading Massive Player ID: %d", mnMassivePlayerID);
            ++lnRequiredFields;               // b loc_82BD4FD0 -> loc_82BD501C
        }
        else if (lnTag == 43)                 // cmpwi cr6, r11, 0x2B -- session id
        {
            mnMassiveSessionID = ReadU32();   // mr r6, r3 ; stw r6, 0x54(r31)
            MassiveLog(5, GetName(), "Reading Massive Session ID: %d", mnMassiveSessionID);
            ++lnRequiredFields;               // -> loc_82BD501C
        }
        else if (lnTag == 71)                 // cmpwi cr6, r11, 0x47 -- zone name
        {
            // NOTE: this branch does NOT touch the required-field counter -- it
            // rejoins the loop at loc_82BD5028, past the increment.
            char* lpcZoneName = ReadString();
            if (lpcZoneName)                  // mr. r30, r3 ; beq loc_82BD5028
            {
                // bl Instance ; mr r4, r30 ; bl ZoneNameAdd -- r3 falls straight
                // through from the singleton getter into the member call.
                CMassiveClientCore::Instance()->ZoneNameAdd(lpcZoneName);
                MassiveLog(5, GetName(), "Reading Zone Name: %s", lpcZoneName);
                MassiveFree(lpcZoneName);     // off_82F91C18 indirect, AFTER the log
            }
        }
        else if (!SkipField(lnTag))           // cmpwi r3, 0 ; beq -> return 0
        {
            // (The X360 hands SkipField the unmasked ReadU8 return in r4; the
            // callee's parameter is the same u8, so passing the masked tag is
            // byte-identical.)
            return 0;
        }
    }

    if (lnRequiredFields != 3)                // clrlwi r11, r28, 24 ; cmplwi r11, 3 ; bne
    {
        MassiveLog(2, GetName(), "Response does not contain all of the required fields.");
        return 0;                             // falls into li r3, 0 @loc_82BD5094
    }

    MassiveLog(5, GetName(), "Response contains all of the required fields.");

    if (!VerifyHMACSignature())               // cmpwi r3, 0 ; beq loc_82BD5094
        return 0;

    MassiveLog(5, GetName(), "Response successfully read and parsed.");
    return 1;                                 // li r3, 1
}

} // namespace MassiveAdClient3
