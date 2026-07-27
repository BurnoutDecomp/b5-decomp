#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestImpressionUpdate.h"

// ===========================================================================
// MassiveAdClient3::CRequestImpressionUpdate -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// Every Add*Report body is a flat sequence of base CRequestObject::Write*
// appends (bPrepend = 0) building one impression-report block, followed by a
// single increment of the matching u16 counter; the return is a constant 0.
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order. The vftable install the X360 ctor / vector
// deleting destructor perform (off_82186F84) is modelled by the class virtuals,
// so no vftable store is written by hand.
//
// Parse @ 0x82BD9DA8 is intentionally NOT defined here (BLOCKED): its body calls
// an un-homed function (Hex-Rays `STUB(this, mpSignature, 20)`) and CRequest
// Object::ReadRemoveSignature (un-attested in the base). See the header.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::CRequestImpressionUpdate @ 0x82BD9538
//
//   bl CRequestObject::CRequestObject(this, 0x65=101, "RequestImpressionUpdate")
//   sth 0, 0x50(this)                     -> mnReportCount = 0
//   stw off_82186F84, 0(this)             -> install vftable (modelled by vtable)
//   sth 0, 0x52(this)                     -> mnInteractionCount = 0
// ---------------------------------------------------------------------------
CRequestImpressionUpdate::CRequestImpressionUpdate()
    : CRequestObject(101, "RequestImpressionUpdate")
    , mnReportCount(0)       // sth 0, 0x50
    , mnInteractionCount(0)  // sth 0, 0x52
{
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::~CRequestImpressionUpdate @ 0x82BD9E88
//                                                     (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestImpressionUpdate::~CRequestImpressionUpdate()
{
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::GetRequestURL @ 0x82BD9590
//
//   lis r11, off_82F91D34@ha ; lwz r3, off_82F91D34@l(r11)  # "/impsrv/4/activity"
//   blr
//
// off_82F91D34 is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestImpressionUpdate::GetRequestURL()
{
    return "/impsrv/4/activity";
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::CreateRequest @ 0x82BD95A0
//
// NOT in the ledger/export set (IDA exported no JSON for this address; the body
// was recovered from the shared .i64 via idat -- see the CMassiveZoneManager
// unblock spec). Called by CMassiveZoneManager::CreateImpUpdateReque
// @ 0x82BD2E00 (`bl CRequestImpressionUpdate::CreateRequest` with r4 = the zone
// manager). Rejects a null builder (-1100), chains the base
// CreateRequest(pBuilder, 512, 512), pre-writes the client player ID (tag 42)
// and session ID (tag 43) in PREPEND mode (bPrepend = 1, the `li r5, 1`
// operands -- value first, then its tag, mirroring the committed prepend
// convention in CRequestCloseSession/CRequestHeartbeat), and sets status 1
// (building -- the zone layer keeps appending impression reports before
// Finish() seals it to status 2). Returns 0.
//
//   cmplwi r4, 0 ; beq -> li r3, -0x44C (-1100)
//   bl CRequestObject::CreateRequest(this, pBuilder, 0x200, 0x200)
//   bl WriteU32(this, gnMassivePlayerID, 1) ; bl WriteU8(this, 42, 1)
//   bl WriteU32(this, gnMassiveSessionID, 1) ; bl WriteU8(this, 43, 1)
//   bl SetStatus(this, 1) ; li r3, 0
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::CreateRequest(CRequestBuilder* pBuilder)
{
    if (!pBuilder)
        return -1100;

    CRequestObject::CreateRequest(pBuilder, 512, 512);
    WriteU32(gnMassivePlayerID, 1);  // dword_8327F2D0 ; li r5, 1
    WriteU8(42, 1);
    WriteU32(gnMassiveSessionID, 1); // dword_8327F2CC ; li r5, 1
    WriteU8(43, 1);
    SetStatus(1);
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::AddModelReport @ 0x82BD9AC8
//
// Block: 223, U32 41, [54,u8][38,u32][21,u32][50,u32][51,u64][52,u64][53,u16]
//        [70,u16]; then ++mnReportCount.
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::AddModelReport(unsigned char uTag54, unsigned int nTag38,
                                             unsigned int nTag21, unsigned int nTag50,
                                             unsigned long long nTag51, unsigned long long nTag52,
                                             unsigned short sTag53, unsigned short sTag70)
{
    WriteU8(223, 0);       // li r4, 0xDF
    WriteU32(41, 0);       // li r4, 0x29
    WriteU8(54, 0);        // li r4, 0x36
    WriteU8(uTag54, 0);    // clrlwi r4, r30, 24
    WriteU8(38, 0);        // li r4, 0x26
    WriteU32(nTag38, 0);
    WriteU8(21, 0);        // li r4, 0x15
    WriteU32(nTag21, 0);
    WriteU8(50, 0);        // li r4, 0x32
    WriteU32(nTag50, 0);
    WriteU8(51, 0);        // li r4, 0x33
    WriteU64(nTag51, 0);
    WriteU8(52, 0);        // li r4, 0x34
    WriteU64(nTag52, 0);
    WriteU8(53, 0);        // li r4, 0x35
    WriteU16(sTag53, 0);
    WriteU8(70, 0);        // li r4, 0x46
    WriteU16(sTag70, 0);
    ++mnReportCount;       // lhz/addi/sth 0x50
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::AddTextureRepor @ 0x82BD9640
//
// Block: 223, U32 46, [54,u8][38,u32][21,u32][50,u32][51,u64][52,u64][53,u16]
//        [48,float]; then [70,u16]; then ++mnReportCount.
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::AddTextureRepor(unsigned char uTag54, unsigned int nTag38,
                                              unsigned int nTag21, unsigned int nTag50,
                                              unsigned long long nTag51, unsigned long long nTag52,
                                              unsigned short sTag53, float fTag48,
                                              unsigned short sTag70)
{
    WriteU8(223, 0);       // li r4, 0xDF
    WriteU32(46, 0);       // li r4, 0x2E
    WriteU8(54, 0);        // li r4, 0x36
    WriteU8(uTag54, 0);    // clrlwi r4, r30, 24
    WriteU8(38, 0);        // li r4, 0x26
    WriteU32(nTag38, 0);
    WriteU8(21, 0);        // li r4, 0x15
    WriteU32(nTag21, 0);
    WriteU8(50, 0);        // li r4, 0x32
    WriteU32(nTag50, 0);
    WriteU8(51, 0);        // li r4, 0x33
    WriteU64(nTag51, 0);
    WriteU8(52, 0);        // li r4, 0x34
    WriteU64(nTag52, 0);
    WriteU8(53, 0);        // li r4, 0x35
    WriteU16(sTag53, 0);
    WriteU8(48, 0);        // li r4, 0x30
    WriteFloat(fTag48, 0); // fmr f1, f31
    WriteU8(70, 0);        // li r4, 0x46
    WriteU16(sTag70, 0);
    ++mnReportCount;       // lhz/addi/sth 0x50
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::AddVideoReport @ 0x82BD97D0
//
// Byte-identical wire shape to AddTextureRepor (block type 46, float under tag
// 48); a separate X360 symbol, reconstructed store-for-store.
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::AddVideoReport(unsigned char uTag54, unsigned int nTag38,
                                             unsigned int nTag21, unsigned int nTag50,
                                             unsigned long long nTag51, unsigned long long nTag52,
                                             unsigned short sTag53, float fTag48,
                                             unsigned short sTag70)
{
    WriteU8(223, 0);       // li r4, 0xDF
    WriteU32(46, 0);       // li r4, 0x2E
    WriteU8(54, 0);        // li r4, 0x36
    WriteU8(uTag54, 0);    // clrlwi r4, r30, 24
    WriteU8(38, 0);        // li r4, 0x26
    WriteU32(nTag38, 0);
    WriteU8(21, 0);        // li r4, 0x15
    WriteU32(nTag21, 0);
    WriteU8(50, 0);        // li r4, 0x32
    WriteU32(nTag50, 0);
    WriteU8(51, 0);        // li r4, 0x33
    WriteU64(nTag51, 0);
    WriteU8(52, 0);        // li r4, 0x34
    WriteU64(nTag52, 0);
    WriteU8(53, 0);        // li r4, 0x35
    WriteU16(sTag53, 0);
    WriteU8(48, 0);        // li r4, 0x30
    WriteFloat(fTag48, 0); // fmr f1, f31
    WriteU8(70, 0);        // li r4, 0x46
    WriteU16(sTag70, 0);
    ++mnReportCount;       // lhz/addi/sth 0x50
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::AddAudioReport @ 0x82BD9960
//
// Block: 223, U32 43, [54,u8][38,u32][21,u32][50,u32][51,u64][52,u64]
//        [49,float][70,u16]; then ++mnReportCount. (No tag-53 field; the float
//        rides tag 49 instead of tag 48.)
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::AddAudioReport(unsigned char uTag54, unsigned int nTag38,
                                             unsigned int nTag21, unsigned int nTag50,
                                             unsigned long long nTag51, unsigned long long nTag52,
                                             float fTag49, unsigned short sTag70)
{
    WriteU8(223, 0);       // li r4, 0xDF
    WriteU32(43, 0);       // li r4, 0x2B
    WriteU8(54, 0);        // li r4, 0x36
    WriteU8(uTag54, 0);    // clrlwi r4, r30, 24
    WriteU8(38, 0);        // li r4, 0x26
    WriteU32(nTag38, 0);
    WriteU8(21, 0);        // li r4, 0x15
    WriteU32(nTag21, 0);
    WriteU8(50, 0);        // li r4, 0x32
    WriteU32(nTag50, 0);
    WriteU8(51, 0);        // li r4, 0x33
    WriteU64(nTag51, 0);
    WriteU8(52, 0);        // li r4, 0x34
    WriteU64(nTag52, 0);
    WriteU8(49, 0);        // li r4, 0x31
    WriteFloat(fTag49, 0); // fmr f1, f31
    WriteU8(70, 0);        // li r4, 0x46
    WriteU16(sTag70, 0);
    ++mnReportCount;       // lhz/addi/sth 0x50
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::AddInteractionR @ 0x82BD9C28
//
// Block: 224, U32 24, [38,u32][21,u32][51,u64][31,u8][32,u16]; then
//        ++mnInteractionCount.
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::AddInteractionR(unsigned char uTag31, unsigned int nTag38,
                                              unsigned int nTag21, unsigned long long nTag51,
                                              unsigned short sTag32)
{
    WriteU8(224, 0);        // li r4, 0xE0
    WriteU32(24, 0);        // li r4, 0x18
    WriteU8(38, 0);         // li r4, 0x26
    WriteU32(nTag38, 0);
    WriteU8(21, 0);         // li r4, 0x15
    WriteU32(nTag21, 0);
    WriteU8(51, 0);         // li r4, 0x33
    WriteU64(nTag51, 0);
    WriteU8(31, 0);         // li r4, 0x1F
    WriteU8(uTag31, 0);     // clrlwi r4, r30, 24
    WriteU8(32, 0);         // li r4, 0x20
    WriteU16(sTag32, 0);
    ++mnInteractionCount;   // lhz/addi/sth 0x52
    return 0;
}

// ---------------------------------------------------------------------------
// CRequestImpressionUpdate::Finish @ 0x82BD9D20
//
//   WriteU8(33);  WriteU16(mnReportCount);
//   WriteU8(41);  WriteU16(mnInteractionCount);
//   FinishBaseBlock(211, 1, 1);   SetStatus(2);   return 0;
// ---------------------------------------------------------------------------
int CRequestImpressionUpdate::Finish()
{
    WriteU8(33, 0);                 // li r4, 0x21
    WriteU16(mnReportCount, 0);     // lhz 0x50
    WriteU8(41, 0);                 // li r4, 0x29
    WriteU16(mnInteractionCount, 0);// lhz 0x52
    FinishBaseBlock(211, 1, 1);     // li r4, 0xD3 ; li r5, 1 ; li r6, 1
    SetStatus(2);                   // li r4, 2
    return 0;
}

} // namespace MassiveAdClient3
