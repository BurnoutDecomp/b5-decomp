#pragma once

// ===========================================================================
// XCAM::CStreamEngine -- the per-title stream engine of the console XCAM (Xbox
// Live Vision camera) streaming SDK in the console executable. It owns the
// send side (encoder, packetizer, QOS rate controller) and the pool of remote
// receive pipelines (CRemoteConsoleList), and every CRemoteConsole holds a
// pointer back to it.
//
// This is the minimal owning header: only the leading part of the engine is
// declared -- the parts the CRemoteConsole send/QOS bodies reach. The engine's
// own methods (bar the one those bodies call) and its members past the
// console list are not homed yet; the CStreamEngine TU extends this header
// additively. The object is only reached through pointers here, never
// constructed from this declaration.
//
// Console layout (byte offsets from this), attested by the CRemoteConsole and
// CQOSController asm (engine+0x44 / +0x1B8 / +0x1BC / +0x200 / +0x268 and the
// list's +0x8620 / +0x8624). The subobjects are contiguous: 0x44 + 0x178
// (CEncoder) == 0x1BC, 0x1BC + 0x44 (CPacketizer) == 0x200, 0x200 + 0x68
// (CQOSController) == 0x268.
//   +0x000 RemoteConsoleConfig   the stream-setup block CRemoteConsoleList /
//                                CRemoteConsole::Initialize read (base)
//   +0x044 mEncoder              CEncoder (its muLastEncodeTick is +0x1B8)
//   +0x1BC mPacketizer           CPacketizer
//   +0x200 mQOSController        CQOSController
//   +0x268 mRemoteConsoleList    CRemoteConsoleList
// On the x64 PC target the subobjects widen, so access is by named member.
// `XCAM` is a console SDK boundary, so its identifiers are preserved verbatim per
// the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XCam/XCamRemoteConsole.h"      // RemoteConsoleConfig
#include "SDKs/XCam/XCamEncoder.h"            // CEncoder
#include "SDKs/XCam/XCamPacketizer.h"         // CPacketizer
#include "SDKs/XCam/XCamQOSController.h"      // CQOSController
#include "SDKs/XCam/XCamRemoteConsoleList.h"  // CRemoteConsoleList

namespace XCAM
{

class CStreamEngine : public RemoteConsoleConfig
{
public:
    // Re-derive every active console's disabled state from the signed-in users'
    // communication privileges / friend relations. Returns the list result.
    int UpdatePrivilegeBits();

    CEncoder           mEncoder;            // +0x044
    CPacketizer        mPacketizer;         // +0x1BC
    CQOSController     mQOSController;      // +0x200
    CRemoteConsoleList mRemoteConsoleList;  // +0x268
};

} // namespace XCAM
