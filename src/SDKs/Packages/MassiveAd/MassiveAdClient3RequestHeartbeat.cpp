#include "SDKs/Packages/MassiveAd/MassiveAdClient3RequestHeartbeat.h"

// ===========================================================================
// MassiveAdClient3::CRequestHeartbeat -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (no leak / DecFIGS).
//
// The wire tags are reproduced verbatim from the `li r4, <tag>` immediates in
// the disassembly, in asm order, and every scalar write here carries bPrepend=1
// (r5 = 1) -- the heartbeat block prepends its session/player fields onto the
// front of the request buffer. The vftable install the X360 ctor / vector
// deleting destructor perform (off_82186AD4) is modelled by the class virtuals,
// so no vftable store is written by hand.
//
// Parse @ 0x82BD86B8 is intentionally NOT defined here (BLOCKED): inside its
// signature branch its body calls CRequestObject::ReadRemoveSignature (un-attested
// in the committed base header) and then an un-homed, un-named function (Hex-Rays
// `STUB(this, mpSignature, 20)`). See the header.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CRequestHeartbeat::CRequestHeartbeat @ 0x82BD8608
//
//   li r4, 0x85=133 ; addi r5, "RequestHeartbeat"
//   bl CRequestObject::CRequestObject(this, 133, "RequestHeartbeat")
//   stw off_82186AD4, 0(this)             -> install vftable (modelled by vtable)
// ---------------------------------------------------------------------------
CRequestHeartbeat::CRequestHeartbeat()
    : CRequestObject(133, "RequestHeartbeat")
{
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::~CRequestHeartbeat @ 0x82BD8660
//                                        (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor rewrites the vftable,
// chains ~CRequestObject, and conditionally frees through the base operator
// delete. Modelled by the empty virtual destructor.
// ---------------------------------------------------------------------------
CRequestHeartbeat::~CRequestHeartbeat()
{
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::GetRequestURL @ 0x82BD8650
//
//   lis r11, off_82F91D10@ha ; lwz r3, off_82F91D10@l(r11)  # "/impsrv/4/heartbeat"
//   blr
//
// off_82F91D10 is the .data pointer to the rodata endpoint string; returning the
// literal is byte-identical to the indirection the X360 performs.
// ---------------------------------------------------------------------------
const char* CRequestHeartbeat::GetRequestURL()
{
    return "/impsrv/4/heartbeat";
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::WriteHeartbeatRequest @ 0x82BD8798
//
// All writes bPrepend=1:
//   WriteU32(gnMassiveSessionID);  WriteU8(43);
//   WriteU32(gnMassivePlayerID);   WriteU8(42);
//   return FinishBaseBlock(214, 1, 1);
// ---------------------------------------------------------------------------
int CRequestHeartbeat::WriteHeartbeatRequest()
{
    WriteU32(gnMassiveSessionID, 1); // dword_8327F2CC ; li r5, 1
    WriteU8(43, 1);                  // li r4, 0x2B    ; li r5, 1
    WriteU32(gnMassivePlayerID, 1);  // dword_8327F2D0 ; li r5, 1
    WriteU8(42, 1);                  // li r4, 0x2A    ; li r5, 1
    return FinishBaseBlock(214, 1, 1); // li r4, 0xD6 ; li r5, 1 ; li r6, 1
}

// ---------------------------------------------------------------------------
// CRequestHeartbeat::CreateRequest @ 0x82BD8818
//
// Rejects a null builder (-1100). Otherwise chains the base CreateRequest(
// pBuilder, 256, 256), writes the block, marks the request ready to submit
// (SetStatus 2), and returns 0.
// ---------------------------------------------------------------------------
int CRequestHeartbeat::CreateRequest(CRequestBuilder* pBuilder)
{
    if (!pBuilder)
        return -1100;                // li r3, -0x44C

    CRequestObject::CreateRequest(pBuilder, 256, 256); // li r5, 0x100 ; li r6, 0x100
    WriteHeartbeatRequest();
    SetStatus(2);                    // li r4, 2
    return 0;
}

} // namespace MassiveAdClient3
