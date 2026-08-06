
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
