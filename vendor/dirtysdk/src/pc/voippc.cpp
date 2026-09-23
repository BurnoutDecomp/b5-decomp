// ============================================================================
// voippc.cpp -- Voip for the PC build.
//
// [PC platform layer] The PC build has no voice devices, offline or on the LAN: the voice
// module exists (the CGS VoIP manager requires a ref and a successful port binding to reach
// its ready state) but reports no headset, carries no voice connections and only records the
// routing masks it is given.
//
// The console voice module is a single module-wide instance returned by VoipGetRef; the PC
// keeps that one instance here. VoipShutdown leaves it in place so a later VoipGetRef (the
// VoIP manager's next Prepare) still finds it.
// ============================================================================

#include "voip.h"

struct VoipRefT
{
    s32 iPort;       // controller port of the talker (VOIP_CONTROL_PORT)
    u32 uMicMask;    // connections the microphone is routed to
    u32 uSpkrMask;   // connections the speakers play
};

namespace
{
    VoipRefT gVoipRef = { 0, 0, 0 };
}

extern "C" VoipRefT* VoipGetRef(void)
{
    return &gVoipRef;
}

extern "C" void VoipShutdown(VoipRefT* /*pVoip*/)
{
    // No devices or connections to release.
}

extern "C" s32 VoipControl(VoipRefT* pVoip, s32 iControl, s32 iValue)
{
    if (iControl == VOIP_CONTROL_PORT)
    {
        pVoip->iPort = iValue;
        return 1;
    }
    return -1;
}

extern "C" u32 VoipLocal(VoipRefT* /*pVoip*/)
{
    // No headset: VOIP_LOCAL_HEADSETOK and VOIP_LOCAL_TALKING are clear.
    return 0;
}

extern "C" void VoipDisconnect(VoipRefT* /*pVoip*/, s32 /*iConnID*/)
{
    // No voice connections.
}

extern "C" void VoipMicrophone(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->uMicMask = uConnMask;
}

extern "C" void VoipSpeaker(VoipRefT* pVoip, u32 uConnMask)
{
    pVoip->uSpkrMask = uConnMask;
}
