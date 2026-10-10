#pragma once

#include "types.hpp"

// Xenon (X360) D3D9 predicated-draw extensions the occlusion-cull manager calls to
// gate mesh draws on a previously-issued GPU occlusion query. These are platform
// externals (XDK D3D extensions) with no Burnout-side home; only the signatures the
// call sites need are declared so the reconstructed bodies compile and link
// store-for-store against the same calls the X360 image makes.
//
// HONEST PLACEHOLDER: external platform APIs, NOT reconstructed here. On a non-X360
// build they resolve to the platform shim layer; the declarations alone are what the
// compile gate exercises. The device pointer is the same global the X360 image reads
// from off_83271608 (renderengine::gpD3DDevice).

namespace CgsGraphics
{
    // The device pointer the X360 image reads from off_83271608 (== the renderengine
    // global gpD3DDevice).
    extern void* gpXboxD3DDevice;

    // Begin a predicated draw keyed off the occlusion-query id `luIdentifier`. (X360
    // D3DDevice_BeginConditionalRendering(pDevice, Identifier).)
    void D3DDevice_BeginConditionalRendering(void* lpDevice, u32 luIdentifier);

    // End the predicated draw region. (X360 D3DDevice_EndConditionalRendering(pDevice).)
    void D3DDevice_EndConditionalRendering(void* lpDevice);

    // Open / close a GPU survey (an occlusion count) keyed on `luIdentifier`; the draws between
    // them are what is counted. (Console signatures: D3DDevice_BeginConditionalSurvey(pDevice,
    // Identifier, Flags) / D3DDevice_EndConditionalSurvey(pDevice, Flags).)
    void D3DDevice_BeginConditionalSurvey(void* lpDevice, u32 luIdentifier, u32 luFlags);
    void D3DDevice_EndConditionalSurvey(void* lpDevice, u32 luFlags);
}
