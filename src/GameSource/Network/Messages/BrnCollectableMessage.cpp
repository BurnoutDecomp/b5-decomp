#include "types.hpp"
#include "GameSource/Network/Messages/BrnCollectableMessage.h"
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::CollectableMessage::PrepareForSend       @ 0x8257BDD0
//   BrnNetwork::CollectableMessage::GetPackedMessageSize @ 0x8257BEF0
//   BrnNetwork::CollectableMessage::PackOrUnpack         @ 0x8257BF18
//   BrnNetwork::CollectableMessage::Retrieve             @ 0x8257EC68
// (GetName @ 0x827DFBB8 is inline in the owning header.)
//
// A reliable message announcing a collectable pickup (jump / smash / billboard) keyed by
// CgsID. See the header for the layout; the base ReliableMessage adds no data, so the
// first leaf member (mCollectableID) lands at +0x28 and meType at +0x30.

namespace BrnNetwork
{
    // Message-type id (asm `li r4, 0x1D` in PrepareForSend).
    static const s32 KI_COLLECTABLE_MESSAGE_TYPE = 29;

    // Worst-case sizing payload written by GetPackedMessageSize before it measures the
    // packed size. Neither value affects the packed bit count (the CgsID always packs a
    // full 64 bits and meType a fixed [0,3] range), so these are just the fixed constants
    // the X360 build stamps: a 64-bit sentinel id (asm builds 0xBCDAA3E74B86216F via
    // lis/ori/insrdi) and E_COLLECTABLE_TYPE_BILLBOARD (li r10, 2).
    static const CgsID KU_SIZING_COLLECTABLE_ID = 0xBCDAA3E74B86216FULL;

    // ---------------------------------------------------------------------------------
    // PrepareForSend @ 0x8257BDD0
    //   Only stamps the send if the slot is not already VALID. Asserts the collectable
    //   type is in range, stores the collectable id + type, stamps the reliable send
    //   (wire type 29 + frame) through the base, then asserts the message came out
    //   reliable.
    // ---------------------------------------------------------------------------------
    void CollectableMessage::PrepareForSend(u16 lu16Frame, CgsID lCollectableID, ECollectableType leType)
    {
        if (IsMessageValid())
            return;

        CGS_ASSERT(leType >= E_COLLECTABLE_TYPE_JUMP && leType < E_COLLECTABLE_TYPE_MAX,
                   "Trying to send a collectable type of: ");

        mCollectableID = lCollectableID;   // std, +0x28
        meType         = leType;           // stw, +0x30

        CgsNetwork::ReliableMessage::PrepareForSend(KI_COLLECTABLE_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // ---------------------------------------------------------------------------------
    // GetPackedMessageSize @ 0x8257BEF0
    //   Stamps the sizing payload (fixed sentinel id + billboard type) so the size query
    //   measures a clean worst case, then tail-calls the base reliable-message size query
    //   (the X360 tail-branch target is the COMDAT/ICF-folded
    //   CgsNetwork::TestConnectionMessage::GetPackedMessageSize -- identical body).
    // ---------------------------------------------------------------------------------
    s32 CollectableMessage::GetPackedMessageSize()
    {
        mCollectableID = KU_SIZING_COLLECTABLE_ID;   // std 0xBCDAA3E74B86216F, +0x28
        meType         = E_COLLECTABLE_TYPE_BILLBOARD; // stw 2, +0x30

        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // ---------------------------------------------------------------------------------
    // PackOrUnpack @ 0x8257BF18
    //   (De)serialises the payload. Wire order: base reliable id, collectable id, then the
    //   collectable type as a quantised int in [0, 3]. Note the CgsID field's per-field
    //   status is NOT folded into the running result (matches the X360 asm, which discards
    //   PackOrUnpackCgsID's return); only the base result and the type result are OR-ed.
    // ---------------------------------------------------------------------------------
    CgsNetwork::PackOrUnpackResult CollectableMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        (void)CgsNetwork::PackOrUnpackCgsID(this, &mCollectableID);

        s32 liType = meType;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &liType,
                                               E_COLLECTABLE_TYPE_JUMP, E_COLLECTABLE_TYPE_MAX) | lxResult;
        meType = static_cast<ECollectableType>(liType);

        return lxResult;
    }

    // ---------------------------------------------------------------------------------
    // Retrieve @ 0x8257EC68
    //   Asserts both out-pointers are non-null. If the slot is VALID, copies the id + type
    //   out, clears the VALID flag, asserts the flag is now clear, and returns true;
    //   otherwise returns false without touching the outputs.
    // ---------------------------------------------------------------------------------
    bool CollectableMessage::Retrieve(CgsID* lpCollectableID, ECollectableType* lpeType)
    {
        CGS_ASSERT(lpCollectableID, "lpCollectableID");
        CGS_ASSERT(lpeType, "lpeType");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpCollectableID = mCollectableID;                       // ld  +0x28
        *lpeType         = static_cast<ECollectableType>(meType); // lwz +0x30

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }
} // namespace BrnNetwork
