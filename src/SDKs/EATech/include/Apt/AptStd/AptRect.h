#pragma once

// ===========================================================================
// EATech Apt -- AptRect (leak AptStd/AptRect.h, verbatim). A plain float
// rectangle (left/top/right/bottom), used by the Apt render/character bounds.
// ===========================================================================

struct AptRect
{
    float fLeft, fTop, fRight, fBottom;
};
