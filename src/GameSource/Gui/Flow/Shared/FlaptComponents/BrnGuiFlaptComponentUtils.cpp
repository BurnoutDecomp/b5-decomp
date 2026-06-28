// ===================================================================================
// BrnGui flapt-component free-function utilities
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.cpp
//
//   AttachToTextFieldComponent    @ 0x8241CE78
//   TryFindTextFieldFromMovieClip @ 0x8241CEF8
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Both are free functions in namespace
// BrnGui (DecFIGS: BrnGuiFlaptComponentUtils.cpp). They resolve a named apt movie
// component and dig the named text field out of its parent clip.
//
// CALLING CONVENTION (from the X360 asm):
//  - AttachToTextFieldComponent returns a TextFieldRef by value; the sret slot is
//    modeled, per this module's house style, as an explicit lpOutRef first parameter
//    that is written and returned. The remaining params follow the DecFIGS order
//    (lpcTextFieldName, lpcComponentName, lpcParentName, lFile).
//  - TryFindTextFieldFromMovieClip takes the search-root MovieClipRef BY VALUE (8
//    bytes, one 64-bit GPR on the X360), then the recursive component name, the text
//    field name, and the out TextFieldRef*; it returns bool.
//
// The X360 dev-assert (lpOutTextFieldRef) folds into CGS_ASSERT per the module house
// style. Member-by-name throughout; the only structural copy is the three-pointer
// TextFieldRef value the X360 splats word-by-word, here written as its named members.
// ===================================================================================
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"        // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"   // BrnFlapt::MovieClipRef
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"   // BrnFlapt::TextFieldRef
#include "GameShared/GameClasses/Core/CgsStringUtils.h"  // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

namespace BrnGui
{
    using BrnFlapt::FileRef;
    using BrnFlapt::MovieClipRef;
    using BrnFlapt::TextFieldRef;

    // Longest composite component key formatted on the stack (127 usable chars +
    // the hard NUL terminator written at index 127).
    static const s32 KI_COMPONENT_PATH_LENGTH = 128;

    // @ 0x8241CE78 ---------------------------------------------------------------
    // Resolve the apt movie component keyed "<lpcParentName>_<lpcComponentName>"
    // out of lFile, hide it, walk up to its parent clip, and return the parent's
    // child text field named lpcTextFieldName (by value into lpOutRef).
    TextFieldRef* AttachToTextFieldComponent(TextFieldRef* lpOutRef,
                                             const char* lpcTextFieldName,
                                             const char* lpcComponentName,
                                             const char* lpcParentName,
                                             const FileRef& lFile)
    {
        char lacComponentPath[KI_COMPONENT_PATH_LENGTH];
        CgsCore::SnPrintf(lacComponentPath, KI_COMPONENT_PATH_LENGTH - 1,
                          "%s_%s", lpcParentName, lpcComponentName);
        lacComponentPath[KI_COMPONENT_PATH_LENGTH - 1] = '\0';

        MovieClipRef lComponentClip;
        lFile.FindComponent(&lComponentClip, lacComponentPath);
        lComponentClip.SetVisible(false);

        MovieClipRef lParentClip;
        lComponentClip.GetParent(&lParentClip);

        lParentClip.FindChildTextField(lpOutRef, lpcTextFieldName);
        return lpOutRef;
    }

    // @ 0x8241CEF8 ---------------------------------------------------------------
    // Recursively search lMovieClip for a component named lpcComponentName; on a hit
    // return its parent's child text field named lpcTextFieldName via lpOutTextFieldRef
    // and true, otherwise clear lpOutTextFieldRef and return false.
    bool TryFindTextFieldFromMovieClip(MovieClipRef lMovieClip,
                                       const char* lpcComponentName,
                                       const char* lpcTextFieldName,
                                       TextFieldRef* lpOutTextFieldRef)
    {
        CGS_ASSERT(lpOutTextFieldRef != 0, "lpOutTextFieldRef");

        MovieClipRef lFoundClip;
        if (lMovieClip.TryFindChildComponentRecursively(lpcComponentName, &lFoundClip))
        {
            MovieClipRef lParentClip;
            lFoundClip.GetParent(&lParentClip);

            TextFieldRef lTextField;
            lParentClip.FindChildTextField(&lTextField, lpcTextFieldName);

            lpOutTextFieldRef->mpTextFieldInstance = lTextField.mpTextFieldInstance;
            lpOutTextFieldRef->mpParentMovie       = lTextField.mpParentMovie;
            lpOutTextFieldRef->mpTransform         = lTextField.mpTransform;
            return true;
        }

        lpOutTextFieldRef->mpTextFieldInstance = 0;
        lpOutTextFieldRef->mpParentMovie       = 0;
        lpOutTextFieldRef->mpTransform         = 0;
        return false;
    }
}
