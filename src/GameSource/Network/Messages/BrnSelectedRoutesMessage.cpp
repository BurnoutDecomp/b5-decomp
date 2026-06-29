#include "types.hpp"

#include "GameSource/Network/Messages/BrnSelectedRoutesMessage.h"
#include "GameSource/GameState/BrnGameStateTypes.h"       // BrnGameState::LandmarkIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT

#include <cstring>                                          // memcpy (the X360 44-byte payload moves)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::SelectedRoutesMessage::Construct             @ 0x8257AF58
//   BrnNetwork::SelectedRoutesMessage::PrepareForSend        @ 0x8257AF80
//   BrnNetwork::SelectedRoutesMessage::PackOrUnpack          @ 0x8257B088
//   BrnNetwork::SelectedRoutesMessage::GetPackedMessageSize  @ 0x8257B2A0
//   BrnNetwork::SelectedRoutesMessage::Retrieve              @ 0x8257E770
//   (GetName is bodied inline in the header @ 0x827DFD30; Destruct and
//    OldMessagesAreValid have empty recovered bodies -- see below.)
//
// A RELIABLE message carrying one selected-routes event payload: a
// SpecificGameModeEventInterface::Event record (the event id, the per-junction
// traffic-light trigger id, and the event's landmark-index set) plus the round
// number it applies to. The payload sits right after the 0x28-byte ReliableMessage
// base:
//   +0x28  Event mEvent          (44 bytes -> +0x54)
//            +0x28  maLandmarkIndices[16]   (16 x LandmarkIndex == 16 x s16)
//            +0x48  mTrafficLightTriggerId  (u32 / LightTriggerId)
//            +0x4C  miNumLandmarks          (s32)
//            +0x50  miEventID               (s32)
//   +0x54  s32 miRoundNumber
//
// The trigger id is a BrnTraffic::LightTriggerId (a u32 handle; modelled as u32 here
// exactly as the committed Event home does, to avoid the BrnGameModeParams.h <-> this
// include cycle). The X360 build packs/unpacks the handle as two separate quantised
// fields -- the \"hull\" (bits 8..23) and the \"light-trigger index\" (bits 0..7) -- and
// recomposes them with the constant format tag 0x39 in the high byte. That decompose/
// recompose is the inlined BrnTraffic::LightTriggerId accessor + ::Set the DecFIGS DWARF
// names; reproduced here as the plain integer arithmetic the asm emits (the 0x39000000
// is the `lis r9, 0x3900` / `oris r11, r11, 0x3900` immediate in the instruction stream,
// not an undecoded rodata constant).

namespace BrnNetwork
{
    // DecFIGS DWARF (BrnSelectedRoutesMessage.cpp:25-30). Recovered constant values.
    const s32 KI_MIN_ROUND_NUM     = 0;
    const s32 KI_MAX_ROUND_NUM     = 9;
    const s32 KI_MIN_NUM_LANDMARKS = 2;
    const s32 KI_MAX_NUM_LANDMARKS = 16;
    const s32 KI_MIN_LANDMARK_ID   = 0;
    const s32 KI_MAX_LANDMARK_ID   = 50;

    // Wire type id stamped onto the reliable send (CgsNetwork::ReliableMessage::
    // PrepareForSend leType arg; li r4, 0x17 in the X360 PrepareForSend body).
    static const s32 KI_SELECTED_ROUTES_MESSAGE_TYPE = 23;

    // The LightTriggerId format tag in the high byte (the 0x39000000 immediate). Construct
    // seeds mTrafficLightTriggerId to this default and PackOrUnpack ORs it back in when it
    // recomposes the handle from the (hull, light-trigger-index) wire fields.
    static const u32 KU_LIGHT_TRIGGER_ID_TAG = 0x39000000u;

    // ---------------------------------------------------------------------------------
    // Construct @ 0x8257AF58
    //   Seeds the two inherited player ids to -1 (the inlined MessageWithPlayerIDs init),
    //   zeroes the embedded Event (event id 0, landmark count 0, trigger id == the default
    //   format tag) and the round number, then chains to the Message base ctor. The X360
    //   sets the fields then tail-calls CgsNetwork::Message::Construct.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesMessage::Construct()
    {
        // Inlined MessageWithPlayerIDs base init (stw -1 @ +0x20 / +0x24).
        mSendingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;
        mRecvingPlayerID = CgsNetwork::MessageWithPlayerIDs::KI_INVALID_PLAYER_ID;

        // Inlined Event zero-init (the X360 stores these four words directly):
        //   miRoundNumber          = 0          (stw 0,           +0x54)
        //   mEvent.miEventID       = 0          (stw 0,           +0x50)
        //   mEvent.mTrafficLightTriggerId = tag (stw 0x39000000,  +0x48)
        //   mEvent.miNumLandmarks  = 0          (stw 0,           +0x4C)
        miRoundNumber = 0;
        {
            // LightTriggerId default handle (== the format tag).
            const u32 luTriggerID = KU_LIGHT_TRIGGER_ID_TAG;
            mEvent.Construct(0, luTriggerID, 0, 0);
        }

        CgsNetwork::Message::Construct();
    }

    // ---------------------------------------------------------------------------------
    // Destruct @ (no standalone body recovered -- empty in the DecFIGS unity dump).
    //   Declared as a sibling-.cpp method by the owning header; the recovered body is
    //   empty, so it is reproduced as a no-op (no member or base teardown is emitted).
    // ---------------------------------------------------------------------------------
    void SelectedRoutesMessage::Destruct()
    {
    }

    // ---------------------------------------------------------------------------------
    // PrepareForSend @ 0x8257AF80
    //   Re-arms the slot only if it is not already VALID: asserts the event pointer and
    //   round-number range, stores the round number, copies the Event payload in (the
    //   X360 44-byte memcpy), stamps the reliable send (type 23 + frame) through the base,
    //   then asserts the message came out reliable.
    // ---------------------------------------------------------------------------------
    void SelectedRoutesMessage::PrepareForSend(u16 lu16Frame, s32 liRoundNumber, Event* lpEvent)
    {
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            return;

        CGS_ASSERT(lpEvent != 0, "lpEvent");
        CGS_ASSERT(liRoundNumber >= KI_MIN_ROUND_NUM, "liRoundNumber>=KI_MIN_ROUND_NUM");
        CGS_ASSERT(liRoundNumber <= KI_MAX_ROUND_NUM, "liRoundNumber<=KI_MAX_ROUND_NUM");

        miRoundNumber = liRoundNumber;          // stw, +0x54
        mEvent        = *lpEvent;               // 44-byte payload copy in (memcpy +0x28, 0x2C)

        CgsNetwork::ReliableMessage::PrepareForSend(KI_SELECTED_ROUTES_MESSAGE_TYPE, lu16Frame);
        CGS_ASSERT(IsReliable(), "IsReliable()");
    }

    // ---------------------------------------------------------------------------------
    // Retrieve @ 0x8257E770
    //   Asserts both out-pointers are non-null. If the slot is VALID, copies the round
    //   number and Event payload out, clears the VALID flag, and returns true; otherwise
    //   returns false without touching the outputs.
    // ---------------------------------------------------------------------------------
    bool SelectedRoutesMessage::Retrieve(s32* lpiRoundNumber, Event* lpEvent)
    {
        CGS_ASSERT(lpiRoundNumber != 0, "lpiRoundNumber");
        CGS_ASSERT(lpEvent != 0, "lpEvent");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0)
            return false;

        *lpiRoundNumber = miRoundNumber;        // lwz +0x54 -> *(a2)
        *lpEvent        = mEvent;               // 44-byte payload copy out (memcpy +0x28, 0x2C)

        mx8Flags &= ~CgsNetwork::KX8_FLAGS_VALID;
        CGS_ASSERT((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) == 0,
                   "!CgsNetwork::ReliableMessage::IsMessageValid()");
        return true;
    }

    // ---------------------------------------------------------------------------------
    // PackOrUnpack @ 0x8257B088
    //   (De)serialises the payload. The Event handle is split into its scalar wire fields,
    //   each routed through the shared field primitives whose per-field status is OR-ed
    //   into the running result (0 == all succeeded). On unpack the local landmark buffer
    //   is filled from the wire and the Event is rebuilt via Event::Construct; on pack the
    //   same path is exercised so the round-trip stays symmetric (the X360 always pulls the
    //   current landmark set out via GetLandmark before the field pass and rebuilds the
    //   Event after it).
    //
    //   Wire fields, in order:
    //     base reliable id            (ReliableMessage::PackOrUnpack)
    //     miRoundNumber               PackOrUnpackInt [0, 9]
    //     lu8NumLandmarks             PackOrUnpackU8  [0, 255]
    //     liEventID                   PackOrUnpackInt [0, 0x7FFFFFFF]
    //     liHull                      PackOrUnpackInt [0, 0x7FFFFFFF]
    //     liLightTrigger              PackOrUnpackInt [0, 0x7FFFFFFF]
    //     each landmark id            PackOrUnpackS16 [0, 0xFFFF]
    // ---------------------------------------------------------------------------------
    CgsNetwork::PackOrUnpackResult SelectedRoutesMessage::PackOrUnpack()
    {
        const u32 luTriggerID    = mEvent.GetTrafficLightTriggerId();
        s32       liHull         = static_cast<s32>((luTriggerID >> 8) & 0xFFFF); // bits 8..23
        s32       liLightTrigger = static_cast<s32>(luTriggerID & 0xFF);          // bits 0..7
        u8        lu8NumLandmarks = static_cast<u8>(mEvent.GetNumLandmarks());
        s32       liEventID       = mEvent.GetEventID();

        // Pull the current landmark set out into the s16 wire buffer (on pack this is the
        // payload to send; on unpack it is overwritten by the field pass below).
        s16 la16LandmarkIndices[KI_MAX_NUM_LANDMARKS];
        {
            const s32 liCount = mEvent.GetNumLandmarks();
            for (s32 liLandmarkIndex = 0; liLandmarkIndex < liCount; ++liLandmarkIndex)
            {
                const BrnGameState::LandmarkIndex lLandmarkIndex = mEvent.GetLandmark(liLandmarkIndex);
                la16LandmarkIndices[liLandmarkIndex] = static_cast<s16>(static_cast<s32>(lLandmarkIndex));
            }
        }

        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();
        lxResult = CgsNetwork::PackOrUnpackInt(this, &miRoundNumber, KI_MIN_ROUND_NUM, KI_MAX_ROUND_NUM) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackU8(this, &lu8NumLandmarks, 0, 255) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &liEventID, 0, 0x7FFFFFFF) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &liHull, 0, 0x7FFFFFFF) | lxResult;
        lxResult = CgsNetwork::PackOrUnpackInt(this, &liLightTrigger, 0, 0x7FFFFFFF) | lxResult;

        // The Event rebuild buffer (LandmarkIndex wrappers over the s16 wire values).
        BrnGameState::LandmarkIndex laLandmarkIndex[KI_MAX_NUM_LANDMARKS];
        for (s32 liLandmarkIndex = 0; liLandmarkIndex < lu8NumLandmarks; ++liLandmarkIndex)
        {
            lxResult = CgsNetwork::PackOrUnpackS16(this, &la16LandmarkIndices[liLandmarkIndex], 0, 0xFFFF) | lxResult;
            laLandmarkIndex[liLandmarkIndex] =
                BrnGameState::LandmarkIndex(static_cast<s32>(la16LandmarkIndices[liLandmarkIndex]));
        }

        // The X360 asserts the recomposed handle's two fields stay in range
        // (BrnTrafficLightTrigger.h:211/212); KU_MAX_HULLS == 0x190 (400).
        CGS_ASSERT(static_cast<u32>(liHull) < 0x190u, "luHull < KU_MAX_HULLS");
        CGS_ASSERT(static_cast<u32>(liLightTrigger) < 0x100u, "luLightTriggerIndex < 256");

        // Recompose the LightTriggerId handle (the inlined BrnTraffic::LightTriggerId::Set):
        //   (hull << 8) | 0x39000000 | lightTriggerIndex
        const u32 luRebuiltTriggerID =
            (static_cast<u32>(liHull) << 8) | KU_LIGHT_TRIGGER_ID_TAG | static_cast<u32>(liLightTrigger);

        mEvent.Construct(liEventID, luRebuiltTriggerID, laLandmarkIndex, static_cast<s32>(lu8NumLandmarks));
        return lxResult;
    }

    // ---------------------------------------------------------------------------------
    // GetPackedMessageSize @ 0x8257B2A0
    //   Fills the Event with the worst-case payload (the full 16-landmark set, all ids 0,
    //   default trigger handle, event id 0) and round number 8 so the base reliable size
    //   query measures the maximum packed size, then tail-calls the (COMDAT-folded) base
    //   ReliableMessage::GetPackedMessageSize.
    // ---------------------------------------------------------------------------------
    s32 SelectedRoutesMessage::GetPackedMessageSize()
    {
        miRoundNumber = 8;                                  // stw 8, +0x54

        BrnGameState::LandmarkIndex laLandmarkIndex[KI_MAX_NUM_LANDMARKS];
        for (s32 liLandmarkIndex = 0; liLandmarkIndex < KI_MAX_NUM_LANDMARKS; ++liLandmarkIndex)
            laLandmarkIndex[liLandmarkIndex] = BrnGameState::LandmarkIndex(0);

        const u32 lTriggerID = KU_LIGHT_TRIGGER_ID_TAG;     // default handle (lis 0x3900)
        mEvent.Construct(0, lTriggerID, laLandmarkIndex, KI_MAX_NUM_LANDMARKS);

        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    // ---------------------------------------------------------------------------------
    // OldMessagesAreValid @ (no standalone body recovered -- empty in the DecFIGS unity
    //   dump). Declared virtual by the owning header; the recovered body is empty. The
    //   sibling reliable messages whose \"old messages are valid\" predicate is unrecovered
    //   default to \"old messages are still valid\", so this returns true.
    //   FLAGGED: body not recovered from the X360 binary (empty in the leak unity dump);
    //   the `return true` is the conventional default, not an attested value.
    // ---------------------------------------------------------------------------------
    bool SelectedRoutesMessage::OldMessagesAreValid() const
    {
        return true;
    }
} // namespace BrnNetwork
