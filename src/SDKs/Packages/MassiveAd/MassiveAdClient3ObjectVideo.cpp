#include "SDKs/Packages/MassiveAd/MassiveAdClient3ObjectVideo.h"

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectVideo definition home.
//
// SHAPE and BODIES are reconstructed from BURNOUT_X360_ARTIST.XEX (no leak
// source / DecFIGS). Stores are reproduced member-for-member against the X360
// disassembly; see MassiveAdClient3ObjectVideo.h for the per-offset layout map
// and for why GetBestImpression @ 0x82BD6CC8 is blocked. The class installs its
// own X360 vftable (off_821871EC) over the CMassiveAdObject slot -- modelled by
// the virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectVideo::CMassiveAdObjectVideo @ 0x82BDAB80
//
// asm register setup + store order:
//   mr   r30, r6          ; r30 = a4 (u16)
//   fmr  f31, f1          ; f31 = a5
//   mr   r6,  r9          ; base ctor arg4 <- a9
//   fmr  f30, f2          ; f30 = a6
//   mr   r7,  r10         ; base ctor arg5 <- a10
//   bl   CMassiveAdObject::CMassiveAdObject  ; r3=this, r4=name, r5=a3, r6=a9, r7=a10
//   stfs f30(=a6), 0x60(r31)                 ; mfField60  = a6
//   stfs f31(=a5), 0x68(r31)                 ; mfMinAngle = a5
//   sth  r30(=a4), 0x64(r31)                 ; muMinSize  = a4
//   stw  off_821871EC, 0(r31)                ; install the video vftable
// The base ctor receives (name, a3, a9, a10). a5 and a6 arrive in f1/f2 as
// doubles and are stored single (stfs), so both members are float. a7 (r7) is
// overwritten by a10 before the chain and a8 (r8) is never touched, so neither
// reaches the base and both are unused here. The vftable install is modelled by
// the virtual dtor.
// ---------------------------------------------------------------------------
CMassiveAdObjectVideo::CMassiveAdObjectVideo(const char* pcName, int a3,
                                             unsigned short uMinSize,
                                             float fMinAngle, float fField60,
                                             int a7, int a8, int a9, int a10)
    : CMassiveAdObject(pcName, a3, a9, a10)  // bl base ctor (name, a3, a9, a10)
    , mfField60(fField60)                     // stfs f30(=a6), 0x60
    , muMinSize(uMinSize)                      // sth  r30(=a4), 0x64
    , mfMinAngle(fMinAngle)                    // stfs f31(=a5), 0x68
{
    (void)a7;  // a7 (r7) is clobbered before the base chain and never used
    (void)a8;  // a8 (r8) is never touched by this ctor
}

// ---------------------------------------------------------------------------
// CMassiveAdObjectVideo::~CMassiveAdObjectVideo @ 0x82BDBD68
//   (scalar deleting destructor body)
//
// asm: bl ~CMassiveAdObject (chain base dtor); if (a2 & 1) bl
//      CMassiveBaseObject::operator delete (conditional free); return this. The
//      video vftable install + base-dtor chain are the compiler-emitted
//      virtual-dtor body; the conditional free is the deleting-destructor thunk
//      MSVC synthesises around it. Nothing of the video object's own is torn
//      down (mfField60 / muMinSize / mfMinAngle are POD).
// ---------------------------------------------------------------------------
CMassiveAdObjectVideo::~CMassiveAdObjectVideo()
{
}

} // namespace MassiveAdClient3
