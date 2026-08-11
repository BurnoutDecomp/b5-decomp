#pragma once

// ===========================================================================
// EATech Apt -- AptScriptColour: the ActionScript "Color" object.
//
// AptScriptColour : AptObject -- the scriptable AS `Color` object created by
// `new Color(targetMovieClip)`. It is a property-bearing AptObject (named members
// + prototype + the GC property hash) that additionally binds a single target
// movie-clip value (mpTarget, a counted AptCIH value); its native methods
// (getRGB / setRGB / getTransform / setTransform) read and write the bound clip's
// colour transform (the AptRenderItem's AptCXForm).
//
// SHAPE + BODIES from the X360 ARTIST.XEX pseudocode + assembly:
//     AptScriptColour::AptScriptColour          @ 0x82AF0EE0  (ctor; binds target)
//     AptScriptColour::DestroyGCPointers        @ 0x82AF0FD8
//     AptScriptColour::RegisterReferences       @ 0x82AE2658
//     AptScriptColour::objectMemberLookup       @ 0x82AF8C40
//     AptScriptColour::CleanNativeFunctions     @ 0x82AD64D0  (static; pool teardown)
//     AptScriptColour::sMethod_getRGB           @ 0x82AECF48  (static native method)
//     AptScriptColour::sMethod_getTransform     @ 0x82AF5918  (static native method)
//     AptScriptColour::sMethod_setTransform     @ 0x82AE6BA8  (static native method)
//     AptScriptColour::`vector deleting destructor' @ 0x82AF58B8 (compiler thunk -- dropped)
//
// LAYOUT (sizeof = 36 / 0x24, pinned by the deleting-dtor thunk's
// `AptObject::operator delete(this, 36)` and by `operator new(36)`):
//   AptObject base ......... 32 bytes (AptValueWithHash 28 + mClassFlags @+0x1C)
//   mpTarget  AptValue* ..... +0x20   the bound target movie-clip value (a counted
//                                     AptCIH; AddRef'd by the ctor, Released by
//                                     DestroyGCPointers).
//
// vtable object-type index = AptVFT_ScriptColour (18), confirmed by the ctor's
// `li r4, 0x12` argument to the AptValueWithHash base.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptObject.h"

class AptNativeFunction;   // SDKs/EATech/include/Apt/AptNativeFunction.h (the cached
                           // method singletons; held by pointer only)

struct AptScriptColour : public AptObject
{
    // Construct an AS Color bound to pTarget (a movie-clip value). The X360 ctor
    // stores + AddRef's pTarget only when it is a defined CharacterInstHandle (or a
    // CIHNone) whose character is a movie-clip-like type (sprite/movieclip);
    // otherwise the binding is left null.   @0x82AF0EE0
    explicit AptScriptColour(AptValue* pTarget);

    // dtor -- the X360 emits only the compiler deleting-destructor thunk; the user
    // body is empty (mpTarget is released in DestroyGCPointers, the GC teardown
    // path, not here -- the AptObject base + the AptValueWithHash hash tear down
    // automatically).
    virtual ~AptScriptColour();

    // ---- AptValue object-model virtual overrides --------------------------
    // Resolve the four native Color members (setRGB / getRGB / getTransform /
    // setTransform), lazily building + GC-rooting each backing AptNativeFunction on
    // first lookup; any other name falls through to null (the base hash handles it).
    virtual AptValue* objectMemberLookup(AptValue* const pThis,
                                         const AptNativeString* const pName) const;  // @0x82AF8C40
    virtual void      RegisterReferences();   // @0x82AE2658 (GC mark: hash + target)
    virtual void      DestroyGCPointers();    // @0x82AF0FD8 (release target + hash)

    // Release the four cached native-method singletons back to the pool (Apt
    // shutdown / pool clear). Static -- the singletons are class-wide.  @0x82AD64D0
    static void CleanNativeFunctions();

    // ---- native ActionScript methods (static; the apt VM call dispatch invokes
    // them with the Color value as the receiver and the script arg count). Each
    // returns an AptValue* result (the AS "undefined" value when it has nothing to
    // hand back). Their addresses back the cached AptNativeFunction singletons.
    //   setRGB       -- write the target's additive RGB from an int arg (body HOMED
    //                   in the .cpp, decompiled from the PS3 DecFIGS EXTERNAL twin
    //                   @0xF349AC).
    //   getRGB       @0x82AECF48 -- pack the target's additive RGB into an int.
    //   getTransform @0x82AF5918 -- build an Object of the 8 transform channels.
    //   setTransform @0x82AE6BA8 -- write the 8 transform channels from an Object.
    static AptValue* sMethod_setRGB(AptScriptColour* pThis, int nArgCount);
    static AptValue* sMethod_getRGB(AptScriptColour* pThis);
    static AptValue* sMethod_getTransform(AptScriptColour* pThis, int nArgCount);
    static AptValue* sMethod_setTransform(AptScriptColour* pThis, int nArgCount);

private:
    // +0x20 -- the bound target movie-clip value (counted AptCIH). The ctor stores
    // it as a bare AptValue*; the colour-transform methods narrow it to AptCIH to
    // reach the character instance / render item.
    AptValue* mpTarget;

    // The four cached native-method values, shared by every Color instance (the AS
    // methods are class-wide singletons created on first objectMemberLookup, GC-
    // rooted, and torn down by CleanNativeFunctions). X360 globals
    // off_8324E3BC / C0 / C4 / C8.
    static AptNativeFunction* spMethod_setRGB;
    static AptNativeFunction* spMethod_getRGB;
    static AptNativeFunction* spMethod_getTransform;
    static AptNativeFunction* spMethod_setTransform;
};
