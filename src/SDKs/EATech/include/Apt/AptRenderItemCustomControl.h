#pragma once

// ===========================================================================
// EATech Apt -- AptRenderItemCustomControl: the renderable for a custom
// (game-supplied) control embedded in an .apt movie.
//
// AptRenderItemCustomControl : AptRenderItemSprite. Like a sprite it is a
// container (its display-list children render through the tree), but instead of
// just bracketing them with its transforms it hands the control's descriptor to
// the host: three EA strings (the control TYPE, its TARGET, and a free-form
// PROPERTIES blob) plus, once the host has resolved them, an integer render-data
// handle (the "Z-id"). Render forwards either the strings or the resolved handle
// to the host custom-control draw hooks (AptRenderHooks.h); the destructor asks
// the host to tear down whatever render data the handle owns.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (no PS3 External / DecFIGS / Feb-2007
// for this type -- it is an X360-era addition, like the render-item Clone vtable
// slot). LAYOUT (named, x64-width; console 72 bytes / 18 dwords -- proven by the
// Clone/dtor pool Allocate/Deallocate size 0x48):
//   AptRenderItemSprite base (AptRenderItem[13] + mInstanceName[13]), then
//   [14] +0x38  mTypeStr               EAStringC  (Get/SetTypeStr)
//   [15] +0x3C  mTargetStr             EAStringC  (GetTargetStr)
//   [16] +0x40  mCustomPropertiesStr   EAStringC  (SetCustomPropertiesStr)
//   [17] +0x44  mZId                   int32_t    (Get/SetZId; the host render-data
//                                                  handle -- 0 = none)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptRenderItemSprite.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"

struct AptRenderItemCustomControl : public AptRenderItemSprite
{
    EAStringC mTypeStr;              // [14] +0x38 -- the control type name
    EAStringC mTargetStr;           // [15] +0x3C -- the control target name
    EAStringC mCustomPropertiesStr; // [16] +0x40 -- free-form properties blob
    int32_t   mZId;                 // [17] +0x44 -- host render-data handle (0 = none)

    // Clone copy-ctor @0x82AEF460 -- delegate the base/sprite copy up the chain
    // (character/matrices/instance-name + extended state), copy the three
    // descriptor strings (taking references on their shared buffers), stamp the
    // custom-control render-type flag (0x400000), and start with no Z-id.
    AptRenderItemCustomControl(const AptRenderItemCustomControl* pSource, int nCreatedOnTick, bool bCopyExtended);

    // "From sprite" ctor @0x82AEF550 -- promote a plain sprite render item to a
    // custom control: same base/sprite copy chain (inheriting the sprite's
    // character / matrices / instance name), but the descriptor strings start
    // EMPTY and there is no Z-id. Used by CopyFromSprite.
    AptRenderItemCustomControl(const AptRenderItemSprite* pSourceSprite, int nCreatedOnTick, bool bCopyExtended);

    virtual ~AptRenderItemCustomControl();   // @0x82AEF5B8

    // CopyFromSprite @0x82AEF9F8 -- the render-tree revision swap that converts a
    // sprite render item into a custom control: under the Apt render-tree lock,
    // pool-allocate + from-sprite-construct a new custom control, install it as the
    // source sprite's next render revision, take a reference, and return it.
    static AptRenderItem* CopyFromSprite(AptRenderItem* pSourceSprite, int nCreatedOnTick, bool bCopyExtended);   // @0x82AEF9F8

    // ---- render-item vtable overrides -------------------------------------
    virtual AptRenderItem* Clone(int nCreatedOnTick, bool bCopyExtended) override;   // @0x82AEFDE8
    virtual void Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const override;   // @0x82AEF8F8

    // PopRenderData @0x82ADB538 / PushRenderDataAbsolute @0x82ADB4F8 -- notify the
    // host render-data hooks for the (inherited) instance name. Unlike the sprite
    // forms these do NOT push/pop the matrix bracket (the host positions the
    // control). The pCtx/eOp/nTick params are part of the inherited signature.
    virtual void PopRenderData(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const override;   // @0x82ADB538
    virtual void PushRenderDataAbsolute(AptRenderingContext* pCtx) const override;   // @0x82ADB4F8

    // ---- descriptor accessors --------------------------------------------
    const EAStringC& GetTypeStr() const   { return mTypeStr; }     // @0x82AD5038
    const EAStringC& GetTargetStr() const { return mTargetStr; }   // @0x82AD5040

    EAStringC& SetTypeStr(const EAStringC& strType)             { return (mTypeStr = strType); }              // @0x82AE5A90
    EAStringC& SetCustomPropertiesStr(const EAStringC& strProp) { return (mCustomPropertiesStr = strProp); }  // @0x82AE5A88

    // ---- render-data handle (Z-id) ---------------------------------------
    int  GetZId() const { return mZId; }   // @0x82AD5048
    AptRenderItemCustomControl* SetZId(int nZId) { mZId = nZId; return this; }   // @0x82AD5050
};
