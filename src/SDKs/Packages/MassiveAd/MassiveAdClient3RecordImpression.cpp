#include "SDKs/Packages/MassiveAd/MassiveAdClient3RecordImpression.h"

// ===========================================================================
// MassiveAdClient3::CMassiveRecordImpression -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). Stores reproduced
// member-for-member; see MassiveAdClient3RecordImpression.h for the layout. The
// class installs its own vftable over the CMassiveBaseObject slot -- modelled by
// the virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveRecordImpression::CMassiveRecordImpression @ 0x82BDD5E8
//
//   bl CMassiveBaseObject::CMassiveBaseObject(this, "CMassiveRecordImpression")
//   stw r30(=a2), 0x14(this)                  -> mnParentRecord = a2
//   lfs f0, flt_82001CC0 (IDA-decoded 0.0)    -> the float-zero constant
//   stfs f0, 0x28(this) ; stfs f0, 0x20(this) -> mfField28 = mfField20 = 0.0f
//   stw  off_82187B98, 0(this)                -> install vftable (virtual dtor)
//   li   r11, 0
//   stw  r11, 0x38 ; 0x34 ; 0x30 ; 0x18       -> mnField38/34/30/18 = 0
//   std  r11, 0x50 ; 0x48 ; 0x40              -> mnField50/48/40 = 0 (i64)
//   sth  r11, 0x2C ; 0x58 ; 0x24 ; 0x1C       -> msField2C/58/24/1C = 0 (u16)
//
// flt_82001CC0 is the shared rodata single-precision constant the X360 stores
// into both float slots; IDA decodes it as 0.0, so both float fields initialise
// to 0.0f. (The HIDWORD(v4)=vtable / LODWORD(v4)=0 pairing the pseudocode shows
// for +0x40/+0x48/+0x50/+0x80... is the X360 packing one zero/vtable dword pair;
// the asm stores plain 64-bit zeros via `std r11(=0)`, reproduced as i64 = 0.)
// ---------------------------------------------------------------------------
CMassiveRecordImpression::CMassiveRecordImpression(int nParentRecord)
    : CMassiveBaseObject("CMassiveRecordImpression")
    , mnParentRecord(nParentRecord)  // stw a2, 0x14
    , mnField18(0)                   // stw 0, 0x18
    , msField1C(0)                   // sth 0, 0x1C
    , maPad1E()
    , mfField20(0.0f)                // stfs flt_82001CC0(0.0), 0x20
    , msField24(0)                   // sth 0, 0x24
    , maPad26()
    , mfField28(0.0f)                // stfs flt_82001CC0(0.0), 0x28
    , msField2C(0)                   // sth 0, 0x2C
    , maPad2E()
    , mnField30(0)                   // stw 0, 0x30
    , mnField34(0)                   // stw 0, 0x34
    , mnField38(0)                   // stw 0, 0x38
    , maPad3C()
    , mnField40(0)                   // std 0, 0x40
    , mnField48(0)                   // std 0, 0x48
    , mnField50(0)                   // std 0, 0x50
    , msField58(0)                   // sth 0, 0x58
{
}

// ---------------------------------------------------------------------------
// CMassiveRecordImpression::~CMassiveRecordImpression @ 0x82BDD680
//                                                     (vector deleting destructor)
//
// No own teardown -- the X360 vector deleting destructor chains
// ~CMassiveBaseObject and conditionally frees through the base operator delete.
// ---------------------------------------------------------------------------
CMassiveRecordImpression::~CMassiveRecordImpression()
{
}

// ---------------------------------------------------------------------------
// CMassiveRecordImpression::Reset
//
// The X360 has no standalone impression Reset symbol; CMassiveRecord::Reset
// @ 0x82BDD7A8 inlines this per-impression accumulator clear (twice -- once per
// embedded impression). The asm reads mnParentRecord (+0x14 within the
// impression), then re-stores it unchanged, while zeroing every other slot:
//   stfs flt_82001CC0(0.0), +0x20 / +0x28      -> mfField20 = mfField28 = 0.0f
//   stw  0, +0x18 / +0x30 / +0x34              -> mnField18/30/34 = 0
//   std  0, +0x40 / +0x48 / +0x50              -> mnField40/48/50 = 0 (i64)
//   sth  0, +0x1C / +0x24 / +0x2C / +0x58      -> msField1C/24/2C/58 = 0 (u16)
// mnParentRecord is preserved; reproduced here BY NAME, store-for-store.
// ---------------------------------------------------------------------------
void CMassiveRecordImpression::Reset()
{
    // mnParentRecord (+0x14) is intentionally left untouched.
    mnField18 = 0;
    msField1C = 0;
    mfField20 = 0.0f;
    msField24 = 0;
    mfField28 = 0.0f;
    msField2C = 0;
    mnField30 = 0;
    mnField34 = 0;
    mnField40 = 0;
    mnField48 = 0;
    mnField50 = 0;
    msField58 = 0;
}

} // namespace MassiveAdClient3
