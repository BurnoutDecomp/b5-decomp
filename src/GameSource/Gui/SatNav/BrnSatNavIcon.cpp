// BrnSatNavIcon.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The out-of-line BrnGui::CrashNavMapIcon
// members the X360 emits as real functions:
//   operator=                 @0x8244BA18 -- copy-assign (memberwise; embedded TextField)
//   Construct`adjustor{144}'  @0x827DD610 -- base-adjusting thunk forwarding to Construct
//
// The icon's inline Set*/Get* mutators stay in the header (the X360 emits them inline at
// the call sites). Construct itself (the icon's full constructor) is a separate, not-yet-
// reconstructed TU; the adjustor here only adjusts `this` and forwards to it.

#include "GameSource/Gui/SatNav/BrnSatNavIcon.h"

namespace BrnGui
{

// @ 0x8244BA18
//   The X360 byte-copies the icon starting at this+0x04 (the +0x00 vtable slot is NOT
//   copied), copies the embedded TextField via TextField::operator=, then the two
//   trailing dirty flags @+0x1EC/+0x1ED. Reproduced as a member-by-name copy of the
//   modelled fields:
//     - the MapIconBrnBase data lane (position / rotation / alpha / state) [+0x10..+0x2B]
//     - muId                                                                [+0x2C]
//     - mIconText (TextField::operator=)                                    [+0xC4]
//     - mbIsDirty / mbDirtyIconState                                        [+0x1EC/+0x1ED]
//   The vtable pointer is left untouched (matching the X360 byte-copy anchor at +0x04 and
//   C++ copy-assign of a polymorphic object).
CrashNavMapIcon& CrashNavMapIcon::operator=(const CrashNavMapIcon& lrSource)
{
    // ---- MapIconBrnBase data members (protected; copied field-by-field, NOT the vtable) ----
    mv2Position         = lrSource.mv2Position;
    mfRotationInRadians = lrSource.mfRotationInRadians;
    mfAlpha             = lrSource.mfAlpha;
    meState             = lrSource.meState;

    // ---- CrashNavMapIcon own fields ----
    muId      = lrSource.muId;
    mIconText = lrSource.mIconText;          // TextField::operator= (X360 @0x824470F0)
    mbIsDirty        = lrSource.mbIsDirty;
    mbDirtyIconState = lrSource.mbDirtyIconState;

    return *this;
}

// @ 0x827DD610  (Construct`adjustor{144}')
//   addi r3, r3, -0x90 ; b CrashNavMapIcon::Construct
//   A compiler-emitted base-adjusting thunk: it shifts `this` back by 0x90 (the offset of
//   the secondary base whose vtable carries this Construct slot) to the full
//   CrashNavMapIcon object, then tail-calls the real constructor. Construct is a separate
//   not-yet-reconstructed TU (declared-only); the adjustor faithfully reproduces the
//   `this`-adjust + forward (the X360 `addi r3,r3,-0x90; b Construct`).
CrashNavMapIcon* CrashNavMapIcon::ConstructAdjustor144(s32 liA, s32 liB, s32 liC)
{
    // The X360's only behaviour here is the `this -= 0x90` base adjust + tail-call into
    // the full constructor. On the host build the full object is reached directly, so the
    // adjust collapses to a plain forward to Construct (the out-of-scope real ctor TU).
    return Construct(liA, liB, liC);
}

} // namespace BrnGui
