#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"          // Vector3
#include "SDKs/EA/GameTalk/GameTalk.h"   // EA::GameTalk::GameTalkMessage

namespace BrnDirector
{
    class DirectorResourceManager;
    class MainDirector;

    namespace Camera { class Camera; }
    namespace DirectorIO { struct InputBuffer; }

    // BrnDirector::DirectorDevTools - the authoring-tool bridge for the director:
    // a GameTalk message handler ("SendCamera" / "CameraPos" / render-metrics /
    // the ICE editor commands) plus the per-frame live-camera pump that answers
    // them. Layout asm-derived from GameTalkMsgHandler @0x822095A0 /
    // LiveCamUpdate @0x82239C80 (X360 offsets in the comments; the PC layout
    // keeps the ORDER -- the pointer members widen).
    class alignas(16) DirectorDevTools
    {
    public:
        // The live-camera request latch (X360 +0x00): the GameTalk handler raises
        // it, LiveCamUpdate consumes it. FLAG: enumerator names inferred (the
        // X360 stores the raw 1 / 2).
        enum ELiveCamRequest
        {
            E_LIVECAMREQUEST_NONE        = 0,
            E_LIVECAMREQUEST_SEND_CAMERA = 1,   // "SendCamera" -> ship the current camera to the tool
            E_LIVECAMREQUEST_FLY_CAMERA  = 2,   // "CameraPos"  -> fly the debug camera to the parsed pose
        };

        void Construct(MainDirector* lpDirectorModule,
                       DirectorResourceManager* lpDirectorResourceManager);
        void Update(const DirectorIO::InputBuffer* lpInput, const Camera::Camera& lrCamera);

        // @0x822095A0 (this TU, DWARF-free; the asserts name BrnDirectorDevTools.cpp
        // :130/:219/:244) -- the GameTalk message callback. Single-key messages only;
        // dispatches on the key name (see the .cpp command map). Static -- it works
        // through mpInstance.
        static void GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage);

    private:
        // @0x82239C80 (this TU) -- consume muLiveCamRequest: SEND_CAMERA ships the
        // camera's position + at-axis to "Tool.CanEdit" as a "CameraPos" message on
        // the "Camera" channel; FLY_CAMERA points the arbitrator's debug fly-world
        // camera at the parsed pose. Both clear the latch.
        void LiveCamUpdate(const Camera::Camera& lrCamera);

        u32 muLiveCamRequest;                              // X360 +0x00 (ELiveCamRequest)
        Vector3 mFlyCamPosition;                           // X360 +0x10 ("CameraPos" floats 0-2)
        Vector3 mFlyCamTarget;                             // X360 +0x20 ("CameraPos" floats 3-5, the look-at point)
        DirectorResourceManager* mpDirectorResourceManager; // X360 +0x30
        MainDirector* mpDirectorModule;                    // X360 +0x34
        s32 miCurrentTakeGuid;                             // X360 +0x38 (the last ICEEdit/ICEAssemblyEdit GUID)

        static DirectorDevTools* mpInstance;
    };
}
