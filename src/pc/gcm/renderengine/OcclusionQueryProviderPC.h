#pragma once

#include <d3d9.h>
#include "types.hpp"

namespace renderengine
{
    using OcclusionSurveySubmissionTokenPC = u64;

    // FLAG PC-platform leaf: D3D9 query provider for the currently inactive
    // renderer path. The original Xenos conditional polarity is unresolved.
    void OcclusionQueryProviderPCBeginSurvey(IDirect3DDevice9* lpDevice, u32 luIdentifier, u32 luFlags = 0);
    // Actual active XDK survey flags, including unavailable native-query scopes.
    u32 OcclusionQueryProviderPCGetSurveyFlags();
    void OcclusionQueryProviderPCEndSurvey();
    void OcclusionQueryProviderPCBeginConditional(IDirect3DDevice9* lpDevice, u32 luIdentifier);
    void OcclusionQueryProviderPCEndConditional();
    bool OcclusionQueryProviderPCShouldDraw(IDirect3DDevice9* lpDevice);
    OcclusionSurveySubmissionTokenPC OcclusionQueryProviderPCBeginSurveySubmission();
    void OcclusionQueryProviderPCFinishSurveySubmission(
        OcclusionSurveySubmissionTokenPC luToken, bool lbSubmitted);
    void OcclusionQueryProviderPCInvalidate(IDirect3DDevice9* lpDevice);
    void OcclusionQueryProviderPCResume(IDirect3DDevice9* lpDevice);

    // FlipSwapChainPC uses this policy. BasicFlipSwapChainPC accepts an
    // explicit policy so its reset protocol remains usable with non-D3D test
    // devices without making those devices pretend to implement IDirect3D.
    struct OcclusionQueryLifecyclePC
    {
        static void Invalidate(IDirect3DDevice9Ex* lpDevice);
        static void Resume(IDirect3DDevice9Ex* lpDevice);
    };
}
