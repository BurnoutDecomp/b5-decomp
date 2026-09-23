#ifndef DIRTYSDK_VOIP_H
#define DIRTYSDK_VOIP_H

#include "types.hpp"

// DirtySDK 5.5.3 - voip/include/voip.h
// Voice chat: one module-wide voice ref (VoipGetRef), headset state (VoipLocal), per
// connection speaker / microphone routing masks. The CGS VoIP manager is the only caller.
// PC body: ../src/pc/voippc.cpp.

// Opaque voice module handle.
struct VoipRefT;

// VoipLocal flag bits.
#define VOIP_LOCAL_HEADSETOK   (0x40)    // a headset is plugged in
#define VOIP_LOCAL_TALKING     (0x100)   // the local user is talking

// VoipControl selectors.
#define VOIP_CONTROL_PORT      (('p' << 24) | ('o' << 16) | ('r' << 8) | 't')   // controller port of the talker

#ifdef __cplusplus
extern "C" {
#endif

// The module's voice ref.
VoipRefT* VoipGetRef(void);

// Shut the voice module down.
void VoipShutdown(VoipRefT* pVoip);

// Control (VOIP_CONTROL_*). The port selector returns non-zero when the talker is bound to
// the port; an unknown selector returns -1.
s32  VoipControl(VoipRefT* pVoip, s32 iControl, s32 iValue);

// Local headset / talking flags (VOIP_LOCAL_*).
u32  VoipLocal(VoipRefT* pVoip);

// Drop voice connection iConnID.
void VoipDisconnect(VoipRefT* pVoip, s32 iConnID);

// Route the microphone / speakers to the connections in uConnMask.
void VoipMicrophone(VoipRefT* pVoip, u32 uConnMask);
void VoipSpeaker(VoipRefT* pVoip, u32 uConnMask);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_VOIP_H
