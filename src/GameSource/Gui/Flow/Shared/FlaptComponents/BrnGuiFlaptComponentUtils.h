#ifndef BRN_GUI_FLAPT_COMPONENT_UTILS_H
#define BRN_GUI_FLAPT_COMPONENT_UTILS_H

#include "types.hpp"
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef (by value)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // BrnFlapt::TextFieldRef
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h
//
// Free-function helpers shared by the BrnGui flapt components (declaration home for
// BrnGuiFlaptComponentUtils.cpp). Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// sret returns are modelled, per the module house style, as an explicit lpOut first
// parameter that is written and returned. See the .cpp for the bodies/addresses.
// ============================================================================

namespace BrnGui
{
    // AttachToTextFieldComponent @ 0x8241CE78 -- resolve the apt component keyed
    // "<lpcParentName>_<lpcComponentName>" out of lFile, hide it, walk to its parent
    // clip, and return that parent's child text field named lpcTextFieldName.
    BrnFlapt::TextFieldRef* AttachToTextFieldComponent(BrnFlapt::TextFieldRef* lpOutRef,
                                                       const char* lpcTextFieldName,
                                                       const char* lpcComponentName,
                                                       const char* lpcParentName,
                                                       const BrnFlapt::FileRef& lFile);

    // TryFindTextFieldFromMovieClip @ 0x8241CEF8 -- recursively search lMovieClip for
    // a component named lpcComponentName; on a hit write the parent's child text field
    // named lpcTextFieldName via lpOutTextFieldRef and return true, else clear it and
    // return false. Takes the search-root MovieClipRef BY VALUE (X360 asm).
    bool TryFindTextFieldFromMovieClip(BrnFlapt::MovieClipRef lMovieClip,
                                       const char* lpcComponentName,
                                       const char* lpcTextFieldName,
                                       BrnFlapt::TextFieldRef* lpOutTextFieldRef);
}

#endif // BRN_GUI_FLAPT_COMPONENT_UTILS_H
