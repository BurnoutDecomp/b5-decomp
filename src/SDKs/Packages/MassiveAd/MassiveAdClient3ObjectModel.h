#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectModel -- a model ad-object (vendor
// middleware).
//
// Thin CMassiveAdObject leaf: one min-impression-size threshold plus its own
// X360 vftable (off_82187214). It is built at two recovered sites --
// CRequestEnterZone::ReadIEBlock @ 0x82BDB628 and
// CMassiveAdObjectModelDynamic::CreateSlaveM @ 0x82BDE850 -- and at BOTH the
// constructor is compiler-INLINED (allocate via CMassiveListNode::operator new,
// `bl CMassiveAdObject::CMassiveAdObject`, `clrlwi ..,16` + `stw` of the min
// size at +0x60, then the vftable store at +0x00). It therefore has no X360
// address of its own and is reconstructed inline here (inlining reversal).
//
// MEASURED at the CreateSlaveM site (0x82BDE874..0x82BDE8A0):
//     li     r7, 0                     ; base ctor arg5 (zone) = 0
//     lwz    r29, 0x60(master)         ; the master's own min-size word
//     lwz    r6,  0x38(master)         ; base ctor arg4
//     lwz    r5,  0x48(master)         ; base ctor arg3 (mnInvElementID)
//     lwz    r4,  0x14(master)         ; base ctor arg2 (mpcAdObjectName)
//     bl     CMassiveAdObject::CMassiveAdObject
//     clrlwi r10, r29, 16              ; & 0xFFFF
//     stw    r10, 0x60(slave)          ; mnMinSize
//     stw    off_82187214, 0(slave)    ; this class's vftable
//
// The ledger TU class:MassiveAdClient3::CMassiveAdObjectModel owns the two real
// bodies:
//     CMassiveAdObjectModel::GetBestImpression            @ 0x82BD6E00 (vftable slot 9)
//     CMassiveAdObjectModel::`vector deleting destructor' @ 0x82BDBDB8 (slot 0)
// GetBestImpression is deliberately NOT declared here (its parameter types reach
// collaborator layouts that are not yet homed) -- the same deferred stance the
// sibling Texture/Video homes take; its slot-9 override lands with that TU.
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3AdObject.h / MassiveAdClient3ObjectTexture.h under
// SDKs/Packages/MassiveAd). The MassiveAdClient3 namespace and the class name
// are external middleware identifiers PRESERVED VERBATIM.
//
// Layout (X360-relative; the host widens every pointer, so the absolute offsets
// are documentation, not assertions):
//   +0x00..+0x5F  CMassiveAdObject base   (sizeof 0x60 on the X360)
//   +0x60         mnMinSize               (X360 sizeof(CMassiveAdObjectModel) == 0x64)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

class CMassiveAdObjectModel : public CMassiveAdObject
{
public:
    // Inlined X360 constructor (see the header note -- it has no own address).
    // The argument shape mirrors the measured out-of-line sibling
    // CMassiveAdObjectTexture ctor @ 0x82BDAB18: (name, invElementID,
    // threshold(s), field38, zone). ReadIEBlock @ 0x82BDB628 passes the parsed
    // block fields; CreateSlaveM @ 0x82BDE850 passes the master's own
    // (mpcAdObjectName, mnInvElementID, min size, field38, 0).
    CMassiveAdObjectModel(const char* pcName, int nInvElementID, int nMinSize,
                          int nField38, int nZone)
        : CMassiveAdObject(pcName, nInvElementID, nField38, nZone)
        , mnMinSize(nMinSize & 0xFFFF)  // clrlwi r10, r29, 16 at both inline sites
    {
    }

    // @ 0x82BDBDB8 (the X360 vector deleting destructor; its body belongs to the
    // class:MassiveAdClient3::CMassiveAdObjectModel ledger TU). Declared virtual
    // here to model the off_82187214 vftable install the inlined ctor performs.
    virtual ~CMassiveAdObjectModel();

protected:
    // +0x60 -- the minimum impression size this model ad-object will accept.
    // Every recovered store masks the value with & 0xFFFF (so it is u16-ranged),
    // but the slot is read and written as a full 32-bit word (`lwz`/`stw`), so it
    // is modelled as an int rather than an unsigned short.
    int mnMinSize;
};

} // namespace MassiveAdClient3
