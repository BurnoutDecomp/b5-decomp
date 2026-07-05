#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::VoIPClient::operator=        @ 0x82893618
//   CgsNetwork::VoIPManager::GetRegisteredTalker    @ 0x8287E4D8
//   CgsNetwork::VoIPManager::DisableCommsWithPlayer @ 0x828937B8
//   CgsNetwork::VoIPManager::EnableAllComms         @ 0x82893868
//
// VoipSpeaker / VoipMicrophone are the DirtySock platform voice-routing entry
// points (free functions in the SDK layer): VoipSpeaker(voiceRef, connMask)
// enables/routes the given connections' speakers, VoipMicrophone(voiceRef,
// connMask) does the same for the microphones. Declared extern here; their
// bodies live in the DirtySock SDK, not in this game TU.
extern "C" int VoipSpeaker(void* lpVoiceRef, unsigned int lu32ConnectionMask);
extern "C" int VoipMicrophone(void* lpVoiceRef, unsigned int lu32ConnectionMask);

namespace CgsNetwork
{
    // ---- VoIPClient::operator= @ 0x82893618 -----------------------------------
    //
    // The X360 codegen is the compiler-synthesised memberwise copy-assignment:
    // the scalar prefix (mPlayerID/miConnectionID/mbIsBlocked plus the +9 byte)
    // followed by the two HeadsetStatusMessage members copied field-for-field
    // through the Message base. The reconstruction assigns each member by name.
    VoIPClient& VoIPClient::operator=(const VoIPClient& arOther)
    {
        mPlayerID       = arOther.mPlayerID;
        miConnectionID  = arOther.miConnectionID;
        mbIsBlocked     = arOther.mbIsBlocked;
        mbCanTalk       = arOther.mbCanTalk;
        mHeadsetSendMsg = arOther.mHeadsetSendMsg;
        mHeadsetRecMsg  = arOther.mHeadsetRecMsg;
        return *this;
    }

    // ---- GetRegisteredTalker @ 0x8287E4D8 -------------------------------------
    //
    // Linear-scan the seven-entry maRegisteredTalkers array for the talker whose
    // mPlayerID matches liPlayerID. On a hit return &maRegisteredTalkers[i]; if
    // the scan exhausts all seven slots, fire the assert and return null.
    //
    // The X360 loop walks a raw pointer from (this+0x18) in 0x54-byte strides,
    // comparing the word at each slot's +0 (mPlayerID) against liPlayerID; the
    // matched address it returns is (this + 0x18 + 0x54*i), i.e. &maRegisteredTalkers[i].
    VoIPClient* VoIPManager::GetRegisteredTalker(VoIPNetworkPlayerID liPlayerID)
    {
        for (s32 liIndex = 0; liIndex < 7; ++liIndex)
        {
            if (maRegisteredTalkers[liIndex].mPlayerID == liPlayerID)
                return &maRegisteredTalkers[liIndex];
        }

        CGS_ASSERT(false, "Failed to find registered talker\n");
        return nullptr;
    }

    // ---- DisableCommsWithPlayer @ 0x828937B8 ----------------------------------
    //
    // Find the talker for liPlayerID, clear its mbCanTalk flag, then mute the
    // player: route the speaker to their connection mask and mute the microphone
    // toward them (the mic is only fed the mask when the local player actually has
    // a headset, otherwise 0 -- i.e. we never transmit if we have no mic).
    //
    // The X360 code forwards this function's r4 (liPlayerID) straight into
    // GetRegisteredTalker; if no talker is found (or on the mpVoiceRef assert
    // path) it returns that null result unchanged.
    VoIPClient* VoIPManager::DisableCommsWithPlayer(VoIPNetworkPlayerID liPlayerID)
    {
        VoIPClient* lpTalker = GetRegisteredTalker(liPlayerID);
        if (lpTalker)
        {
            lpTalker->mbCanTalk = false;
            if (mpVoiceRef)
            {
                u32 lu32ConnectionMask = GetConnectionMask();
                CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

                VoipSpeaker(mpVoiceRef, lu32ConnectionMask);

                u32 lu32MicMask = 0;
                if (mbLocalPlayerHasHeadset)
                    lu32MicMask = lu32ConnectionMask;
                VoipMicrophone(mpVoiceRef, lu32MicMask);
            }
        }
        return lpTalker;
    }

    // ---- EnableAllComms @ 0x82893868 ------------------------------------------
    //
    // Re-enable voice for every registered talker: set mbCanTalk on all seven
    // slots, then (if voice is up) restore full speaker + headset-gated microphone
    // routing to the current connection mask.
    VoIPManager* VoIPManager::EnableAllComms()
    {
        for (s32 liIndex = 0; liIndex < 7; ++liIndex)
            maRegisteredTalkers[liIndex].mbCanTalk = true;

        if (mpVoiceRef)
        {
            u32 lu32ConnectionMask = GetConnectionMask();
            CGS_ASSERT(mpVoiceRef, "mpVoiceRef");

            VoipSpeaker(mpVoiceRef, lu32ConnectionMask);

            u32 lu32MicMask = 0;
            if (mbLocalPlayerHasHeadset)
                lu32MicMask = lu32ConnectionMask;
            VoipMicrophone(mpVoiceRef, lu32MicMask);
        }
        return this;
    }
}
