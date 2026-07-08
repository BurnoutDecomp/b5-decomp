#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectTexture -- a texture ad-object (vendor
// middleware). A CMassiveAdObject subtype: it adds a minimum-impression size and
// a minimum-impression angle threshold over the base ad object, and installs its
// own X360 vftable (off_8218719C). It is built by CRequestEnterZone::ReadIEBlock
// (parsing a delivered ad) and by CMassiveAdObjectTextureDynamic::CreateSlave.
//
// SHAPE and BODIES are reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly (there is no Feb-2007 source / DecFIGS DWARF for this subsystem).
// Per-function X360 addresses:
//     MassiveAdClient3::CMassiveAdObjectTexture::CMassiveAdObjectTexture       @ 0x82BDAB18
//     MassiveAdClient3::CMassiveAdObjectTexture::GetBestImpression            @ 0x82BD6898  (BLOCKED, see below)
//     MassiveAdClient3::CMassiveAdObjectTexture::`vector deleting destructor' @ 0x82BDBCC8
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3AdObject.h under SDKs/Packages/MassiveAd). The
// MassiveAdClient3 namespace and the class name are external middleware
// identifiers PRESERVED VERBATIM (not the project mp/lf/KI_ scheme).
//
// Layout (X360-relative offsets, reproduced by NAME over the CMassiveAdObject
// base; the leading vftable pointer is modelled by the virtual dtor, so absolute
// host offsets differ from the X360 4-byte-pointer offsets and are not asserted):
//   +0x60  muMinSize   (unsigned __int16; sth of the ctor's a4)
//   +0x64  mfMinAngle  (float;            stfs of the ctor's a5)
// Both are the texture's impression-acceptance thresholds -- GetBestImpression
// (its own separate consumer) compares a candidate impression's size against
// muMinSize and its angle value against mfMinAngle.
//
// GetBestImpression @ 0x82BD6898 is NOT reconstructed here -- it is BLOCKED. Its
// body reaches into several un-homed collaborator layouts that cannot be grounded
// beyond raw asm displacements: it dereferences a pointer at CMassiveAdObject
// +0x2C to an unknown ad-object type (read at that object's +0x40 / +0x5E / +0x60),
// reads its second argument's +0x08 as a CMassiveAdObjectSubscriber* through an
// unknown "subscription slot" struct, and reads deeper CMassiveAdObject base
// fields (+0x14 name/path, +0x2C) that belong to the not-yet-reconstructed
// CMassiveAdObject TU. Homing those would require fabricating un-attested type
// layouts, so the function is left for when those collaborators are recovered.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectTexture -- a CMassiveAdObject with size/angle impression
// thresholds and its own vftable.
// ---------------------------------------------------------------------------
class CMassiveAdObjectTexture : public CMassiveAdObject
{
public:
    // @ 0x82BDAB18. Chains the CMassiveAdObject base ctor (pcName, a3, a7, a8),
    // installs this class's vftable (off_8218719C, modelled by the virtual dtor),
    // and stores the size threshold (uMinSize -> +0x60) and angle threshold
    // (fMinAngle -> +0x64). The a6 argument is part of the ABI but unused by this
    // ctor (its register is clobbered before the base chain).
    CMassiveAdObjectTexture(const char* pcName, int a3, unsigned short uMinSize,
                            float fMinAngle, int a6, int a7, int a8);

    // @ 0x82BDBCC8 (vector deleting destructor). No own teardown -- the X360
    // vector deleting destructor installs this class's vftable, chains
    // ~CMassiveAdObject, and (when the low bit of the delete flag is set) frees
    // the object through CMassiveBaseObject::operator delete. The vftable install
    // + base-dtor chain are the compiler-emitted virtual-dtor body; the
    // conditional free is the deleting-destructor thunk MSVC synthesises around
    // it. muMinSize / mfMinAngle are POD -- nothing of the texture's own is torn
    // down.
    virtual ~CMassiveAdObjectTexture();

private:
    unsigned short muMinSize;   // +0x60 (a4; sth)
    float          mfMinAngle;  // +0x64 (a5; stfs)
};

} // namespace MassiveAdClient3
