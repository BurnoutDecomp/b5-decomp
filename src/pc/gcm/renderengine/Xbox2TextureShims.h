#pragma once

#include "types.hpp"

// X360 (Xenon) XGRAPHICS entry points called by the renderengine::Texture paths.
// These are XDK platform APIs; on a non-X360 build they resolve to the platform
// shim layer. Only the signatures the call sites need are declared.
// NOT reconstructed here — external platform APIs.
//
// X360 XGSet*TextureHeader: lay out a texture header of the given type in-place
// (all return HRESULT). See Xbox360 XDK "XGSetTextureHeader" documentation.

extern int XGSetCubeTextureHeader(int liEdgeLen, int liLevels, int liUsage, int liFormat,
                                   int liExpand, int liUnused, int liMicroTile, u32* lpHeader);

extern int XGSetVolumeTextureHeader(int liWidth, int liHeight, int liDepth, int liLevels,
                                     int liUsage, int liFormat, int liTiling, int liPHeader);

extern int XGSetArrayTextureHeader(int liWidth, int liHeight, int liArraySize, int liLevels,
                                    int liUsage, int liFormat, int liTiling, int liPHeader);

extern int XGSetTextureHeader(int liWidth, int liHeight, int liLevels, int liUsage,
                               int liFormat, int liTiling, int liPHeader, int liMicroTile);

extern int XGSetLineTextureHeader(int liEdgeLen, int liLevels, int liUsage, int liFormat,
                                   int liTiling, int liPHeader, int liMicroTile, u32* lpHeader);
