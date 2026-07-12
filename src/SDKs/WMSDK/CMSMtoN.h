// ===========================================================================
// SDKs/WMSDK/CMSMtoN.h
//
// CMSMtoN -- the general M->N video resampler embedded by WMSDKRESIZER (X360
// WMSDK path). Only the two entry points WMSDKRESIZER touches are attested here:
//   CMSMtoN::Reset    (X360 `bl CMSMtoN__Reset`,   called from WMSDKRESIZER::Reset)
//   CMSMtoN::~CMSMtoN (X360 `bl CMSMtoN___CMSMtoN`, called from ~WMSDKRESIZER)
//
// This is a DECLARATION-ONLY home: CMSMtoN's member layout has not been
// recovered, so it carries no data members. WMSDKRESIZER stores the sub-object
// as an opaque byte region and hands its base address to these methods (the same
// way the asm does). The bodies live in CMSMtoN's own translation unit.
//
// WMSDK is an external SDK boundary, so the identifiers are kept verbatim per the
// naming convention (external boundary exception).
// ===========================================================================
#pragma once
#include "types.hpp"

class CMSMtoN
{
public:
    // Re-initialise the resampler for (dstWidth, height, srcWidth). The X360
    // call ignores the return value (r3 == this).
    void* Reset(int iDstWidth, int iHeight, int iSrcWidth);

    ~CMSMtoN();
};
