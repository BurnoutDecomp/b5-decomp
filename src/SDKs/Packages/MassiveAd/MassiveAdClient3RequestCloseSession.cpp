#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestCloseSession.h"

// ===========================================================================
// MassiveAdClient3::CRequestCloseSession -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order, and every scalar write here carries bPrepend=1
// (r5 = 1) -- the close-session block prepends its session/player fields onto the
// front of the request buffer. The vftable install the X360 ctor / vector
// deleting destructor perform (off_82185A24) is modelled by the class virtuals,
// so no vftable store is written by hand.
//
// Parse @ 0x82BD3A40 is intentionally NOT defined here (BLOCKED): inside its
// signature branch its body calls CRequestObject::ReadRemoveSignature (un-attested
// in the committed base header) and then an un-homed, un-named function (Hex-Rays
// `STUB(this, mpSignature, 20)`). See the header.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestCloseSession::CRequestCloseSession @ 0x82BD3990
//
//   li r4, 0x73=115 ; addi r5, "RequestCloseSession"
//   bl CRequestObject::CRequestObject(this, 115, "RequestCloseSession")
//   stw off_82185A24, 0(this)             -> install vftable (modelled by vtable)
// ---------------------------------------------------------------------------
CRequestCloseSession::CRequestCloseSession()
    : CRequestObject(115, "RequestCloseSession")
{
}

// ---------------------------------------------------------------------------
// CRequestCloseSession::~CRequestCloseSession @ 0x82BD39E8
//                                             (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestCloseSession::~CRequestCloseSession()
{
}

// ---------------------------------------------------------------------------
// CRequestCloseSession::GetRequestURL @ 0x82BD39D8
//
//   lis r11, off_82F91C30@ha ; lwz r3, off_82F91C30@l(r11)  # "/adsrv/4/closeSession"
//   blr
//
// off_82F91C30 is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestCloseSession::GetRequestURL()
{
    return "/adsrv/4/closeSession";
}

// ---------------------------------------------------------------------------
// CRequestCloseSession::WriteCloseSessionRequest @ 0x82BD3B20
//
// All writes bPrepend=1:
//   WriteU32(gnMassiveSessionID);  WriteU8(43);
//   WriteU32(gnMassivePlayerID);   WriteU8(42);
//   return FinishBaseBlock(209, 1, 1);
// ---------------------------------------------------------------------------
int CRequestCloseSession::WriteCloseSessionRequest()
{
    WriteU32(gnMassiveSessionID, 1); // dword_8327F2CC ; li r5, 1
    WriteU8(43, 1);                  // li r4, 0x2B    ; li r5, 1
    WriteU32(gnMassivePlayerID, 1);  // dword_8327F2D0 ; li r5, 1
    WriteU8(42, 1);                  // li r4, 0x2A    ; li r5, 1
    return FinishBaseBlock(209, 1, 1); // li r4, 0xD1 ; li r5, 1 ; li r6, 1
}

// ---------------------------------------------------------------------------
// CRequestCloseSession::CreateRequest @ 0x82BD3BA0
//
// Rejects a null builder (-1100). Otherwise chains the base CreateRequest(
// pBuilder, 256, 256), writes the block, marks the request ready to submit
// (SetStatus 2), and returns 0.
// ---------------------------------------------------------------------------
int CRequestCloseSession::CreateRequest(CRequestBuilder* pBuilder)
{
    if (!pBuilder)
        return -1100;                // li r3, -0x44C

    CRequestObject::CreateRequest(pBuilder, 256, 256); // li r5, 0x100 ; li r6, 0x100
    WriteCloseSessionRequest();
    SetStatus(2);                    // li r4, 2
    return 0;
}

} // namespace MassiveAdClient3
