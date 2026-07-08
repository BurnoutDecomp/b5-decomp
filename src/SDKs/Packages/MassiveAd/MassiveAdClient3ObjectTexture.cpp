#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectTexture.h"

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectTexture definition home.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (no leak
// source / DecFIGS). Stores are reproduced member-for-member against the X360
// disassembly; see MassiveAdClient3ObjectTexture.h for the per-offset layout map
// and for why GetBestImpression @ 0x82BD6898 is blocked. The class installs its
// own X360 vftable (off_8218719C) over the CMassiveAdObject slot -- modelled by
// the virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectTexture::CMassiveAdObjectTexture @ 0x82BDAB18
//
// asm store order (after the base chain):
//   bl   CMassiveAdObject::CMassiveAdObject   ; r3=this, r4=name, r5=a3, r6=a7, r7=a8
//   stfs f31(=a5), 0x64(r31)                  ; mfMinAngle = a5
//   sth  r30(=a4), 0x60(r31)                  ; muMinSize  = a4
//   stw  off_8218719C, 0(r31)                 ; install the texture vftable
// The base ctor receives (name, a3, a7, a8) -- the incoming a6 (register r7) is
// overwritten by a8 before the chain, so it never reaches the base and is unused
// here. a5 arrives in f1 as a double and is stored single (stfs), so the member
// is a float. The vftable install is modelled by the virtual dtor.
// ---------------------------------------------------------------------------
CMassiveAdObjectTexture::CMassiveAdObjectTexture(const char* pcName, int a3,
                                                 unsigned short uMinSize,
                                                 float fMinAngle, int a6, int a7,
                                                 int a8)
    : CMassiveAdObject(pcName, a3, a7, a8)  // bl base ctor (name, a3, a7, a8)
    , muMinSize(uMinSize)                    // sth  r30(=a4), 0x60
    , mfMinAngle(fMinAngle)                  // stfs f31(=a5), 0x64
{
    (void)a6;  // a6 (r7) is clobbered before the base chain and never used
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectTexture::~CMassiveAdObjectTexture @ 0x82BDBCC8
//   (vector deleting destructor body)
//
// asm: bl ~CMassiveAdObject (chain base dtor); if (a2 & 1) bl
//      CMassiveBaseObject::operator delete (conditional free); return this. The
//      texture vftable install + base-dtor chain are the compiler-emitted
//      virtual-dtor body; the conditional free is the deleting-destructor thunk
//      MSVC synthesises around it. Nothing of the texture's own is torn down
//      (muMinSize / mfMinAngle are POD).
// ---------------------------------------------------------------------------
CMassiveAdObjectTexture::~CMassiveAdObjectTexture()
{
}

} // namespace MassiveAdClient3
