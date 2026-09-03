#ifndef BRN_DIAG_BOUND_SURFACES_H
#define BRN_DIAG_BOUND_SURFACES_H

// ============================================================================================
// [DIAG] NOT IN THE X360 BINARY. Inert unless BRN_RT_PROBE is set. DELETE-WHEN-STABLE.
//
// WHY THIS EXISTS -- the tyre-mark campaign's one unmeasured claim. The skid pass reports
// hr == S_OK, a triangle strip of 12 real vertices, a resolved gWorldViewProj, an on-screen
// NDC, a real tread texture and every render state the console asks for -- and changes not
// one pixel. Eight waves have measured everything UPSTREAM of the raster; nobody has ever
// compared the SURFACE the trail draws into against the surface the WORLD drew into.
//
// A draw that succeeds into a surface nobody resolves is exactly "everything succeeds,
// nothing appears", and it is indistinguishable in every other diagnostic. So this prints,
// at a NAMED pass boundary, the two things that settle it:
//
//   * IDirect3DDevice9::GetRenderTarget(0) and GetDepthStencilSurface() -- the POINTERS, so
//     two call sites can be compared for identity rather than for plausibility;
//   * each surface's D3DSURFACE_DESC -- width, height, format, multisample type -- so a
//     pointer that differs can be told apart from a pointer that differs AND is a different
//     size (a shadow map, an env-map face, a down-sample buffer).
//
// The back buffer is printed alongside so "off-screen" is a comparison, not an adjective.
//
// GATING. Two conditions, both required, because the interesting frames are minutes into a
// run and the boring ones are thousands of frames of title screen:
//   * BRN_RT_PROBE must name a non-zero value;
//   * BrnDiag::gFilmLatch.muSkidLatched must be raised (the first tyre-mark segment ever
//     laid), so the ladder spends its budget on the drift and not on the boot.
// The budget is per-LABEL, so one pass boundary cannot starve another.
// ============================================================================================

namespace BrnDiag
{
    // Print the currently bound colour target 0, depth/stencil surface and back buffer, with
    // each one's D3DSURFACE_DESC, tagged with lpcLabel. Cheap and silent when the probe is off.
    // Defined in pc/gcm/renderengine/XenonD3D9Shims.cpp (the TU that owns the device).
    void LogBoundSurfaces(const char* lpcLabel);
}

#endif
