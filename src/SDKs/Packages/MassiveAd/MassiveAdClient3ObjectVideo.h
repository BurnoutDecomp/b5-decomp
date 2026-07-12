#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveAdObjectVideo -- a video ad-object (vendor
// middleware). A CMassiveAdObject subtype and a sibling of the DONE
// CMassiveAdObjectTexture (W28): like the texture object it carries a minimum
// impression size and a minimum impression angle threshold, and additionally a
// leading video-specific float; it installs its own X360 vftable
// (off_821871EC). It is built by CRequestEnterZone::ReadIEBlock (parsing a
// delivered ad) and by CMassiveAdObjectVideoDynamic::CreateSlave.
//
// SHAPE and BODIES are reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly (there is no Feb-2007 source / DecFIGS DWARF for this subsystem).
// Per-function X360 addresses:
//     MassiveAdClient3::CMassiveAdObjectVideo::CMassiveAdObjectVideo          @ 0x82BDAB80
//     MassiveAdClient3::CMassiveAdObjectVideo::GetBestImpression             @ 0x82BD6CC8  (BLOCKED, see below)
//     MassiveAdClient3::CMassiveAdObjectVideo::`scalar deleting destructor'  @ 0x82BDBD68
//
// This is vendor/SDK code reconstructed in its canonical vendor home (a sibling
// of MassiveAdClient3AdObject.h / MassiveAdClient3ObjectTexture.h under
// SDKs/Packages/MassiveAd). The MassiveAdClient3 namespace and the class name
// are external middleware identifiers PRESERVED VERBATIM (not the project
// mp/lf/KI_ scheme).
//
// Layout (X360-relative offsets, reproduced by NAME over the CMassiveAdObject
// base; the leading vftable pointer is modelled by the virtual dtor, so absolute
// host offsets differ from the X360 4-byte-pointer offsets and are not asserted):
//   +0x60  mfField60   (float;            stfs of the ctor's a6)
//   +0x64  muMinSize   (unsigned __int16; sth  of the ctor's a4)
//   +0x68  mfMinAngle  (float;            stfs of the ctor's a5)
// muMinSize / mfMinAngle are the video's impression-acceptance thresholds --
// GetBestImpression compares a candidate impression's size against muMinSize and
// its angle value against mfMinAngle. mfField60 (the ctor's a6) is a
// video-specific float that is stored by the ctor but not read by either of the
// two reconstructed funcs, so its role is not grounded here.
//
// GetBestImpression @ 0x82BD6CC8 is NOT reconstructed here -- it is BLOCKED, for
// the same reason as its CMassiveAdObjectTexture twin (@ 0x82BD6898). Its body
// reaches into several un-homed collaborator layouts that cannot be grounded
// beyond raw asm displacements: it dereferences a pointer at CMassiveAdObject
// +0x2C to an unknown ad-object type (read at that object's +0x40 / +0x5E /
// +0x60), reads its second argument's +0x08 as a CMassiveAdObjectSubscriber*
// through an unknown "subscription slot" struct, memcpys a 32-byte "Impression"
// candidate struct (typed fields at +0x08 / +0x10 / +0x14) into its third
// argument, and reads deeper CMassiveAdObject base fields (+0x0C, +0x14
// name/path, +0x2C) that belong to the not-yet-reconstructed CMassiveAdObject
// TU. Homing those would require fabricating un-attested type layouts, so the
// function is left for when those collaborators are recovered.
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3AdObject.h"  // CMassiveAdObject

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// CMassiveAdObjectVideo -- a CMassiveAdObject with a leading video float plus
// size/angle impression thresholds and its own vftable.
// ---------------------------------------------------------------------------
class CMassiveAdObjectVideo : public CMassiveAdObject
{
public:
    // @ 0x82BDAB80. Chains the CMassiveAdObject base ctor (pcName, a3, a9, a10),
    // installs this class's vftable (off_821871EC, modelled by the virtual dtor),
    // and stores the video float (fField60 -> +0x60), the size threshold
    // (uMinSize -> +0x64) and the angle threshold (fMinAngle -> +0x68). The a7
    // and a8 arguments are part of the ABI but unused by this ctor (a7's register
    // is clobbered before the base chain and a8 is never touched).
    CMassiveAdObjectVideo(const char* pcName, int a3, unsigned short uMinSize,
                          float fMinAngle, float fField60, int a7, int a8,
                          int a9, int a10);

    // @ 0x82BDBD68 (scalar deleting destructor). No own teardown -- the X360
    // scalar deleting destructor installs this class's vftable, chains
    // ~CMassiveAdObject, and (when the low bit of the delete flag is set) frees
    // the object through CMassiveBaseObject::operator delete. The vftable install
    // + base-dtor chain are the compiler-emitted virtual-dtor body; the
    // conditional free is the deleting-destructor thunk MSVC synthesises around
    // it. mfField60 / muMinSize / mfMinAngle are POD -- nothing of the video
    // object's own is torn down.
    virtual ~CMassiveAdObjectVideo();

private:
    float          mfField60;   // +0x60 (a6; stfs) -- video-specific float, role not grounded
    unsigned short muMinSize;   // +0x64 (a4; sth)
    float          mfMinAngle;  // +0x68 (a5; stfs)
};

} // namespace MassiveAdClient3
