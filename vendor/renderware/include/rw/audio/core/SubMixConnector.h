#pragma once

// =====================================================================================
// rw::audio::core::SubMixConnector -- one node in a SubMix's intrusive list of inbound
// connections (a Send/Route attaching into a SubMix).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
// SubMixConnector::Disconnect @0x82B9C3C0 is authoritative. There is NO matching TU in
// the reference dump and no DecFIGS DWARF for this type, so every offset below is
// grounded directly in the disassembly of the bodied member function.
//
// Disconnect unlinks this connector from its SubMix's doubly-linked connector list
// (head pointer lives in the SubMix at +0x28, link via mpNext / mppPrev), then -- when
// asked to fold-back -- accumulates this connection's per-channel gains into the SubMix's
// channel gain array before clearing the back-pointer/cursor fields.
//
// SubMixConnector layout (grounded in Disconnect's stores/loads):
//   +0x00  mpNext   (_DWORD*; next connector in the SubMix list; *result)
//   +0x04  mppPrev  (_DWORD**; address of the previous node's mpNext slot; *(result+4))
//   +0x08  mField08 (cleared to 0 on disconnect; stw r7,8(r3))
//   +0x0C  mpSubMix (SubMix*; the owning SubMix; *(result+12))
//   +0x10  mbField10 (char; cleared to 0 on disconnect; stb r7,0x10(r3))
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API).
// =====================================================================================

#include "types.hpp" // f32

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// SubMix -- only the surface SubMixConnector::Disconnect touches is reconstructed:
//   +0x21  mbNumChannels  (char; per-channel gain-array length; lbz 0x21)
//   +0x28  mpConnectorHead (SubMixConnector*; head of the inbound-connector list; +0x28)
//   +0x34  mafChannelGain[] (f32[]; per-channel accumulated gains; base +0x34, stride 4,
//                            indexed [0, mbNumChannels))
//   +0x8D  mbDirty        (char; set to 1 when a fold-back changes the gains; stb 0x8D)
// The intervening bytes are the (un-homed) SubMix body, preserved as opaque storage so
// the named offsets stay exact. FLAGGED: SubMix has its own home TU; only the fields the
// bodied Disconnect walks are modelled here.
// -------------------------------------------------------------------------------------
class SubMixConnector; // fwd

class SubMix
{
public:
    // FLAG (rwaudio PDB reconcile -- ProStreet08Milestone.pdb, rw::audio::core::SubMix):
    // SubMix is a PlugIn-derived type; +0x21 is the PlugIn base's mOutputChannels and +0x24
    // is float* mpSubMixBuffer (both ALIGN with ARTIST). Beyond +0x24 ProStreet DIVERGES
    // from ARTIST (PDB: +0x28 mSendList, +0x2c mSubMixListNode, +0x34 mDeClickValueTotal[6]
    // -- only 6 floats), so the ARTIST-derived layout below WINS for +0x28..+0x8D; names
    // there stay ARTIST-grounded.
    char mHeader00[0x21];          // +0x00 .. +0x20 -- PlugIn base (opaque here)
    char mbNumChannels;            // +0x21 -- PlugIn base mOutputChannels
    char mGap22[0x24 - 0x22];      // +0x22 .. +0x23 -- opaque
    f32 *mpSubMixBuffer;           // +0x24 -- (PDB) copied into a connecting connector's
                                   //          mpSubMixBuffer (Route::ConnectByPointerHandler: lwz 0x24)
    SubMixConnector *mpConnectorHead; // +0x28 (ARTIST; PDB-divergent region)
    char mGap2C[0x34 - 0x2C];      // +0x2C .. +0x33 -- opaque
    f32 mafChannelGain[(0x8D - 0x34) / 4]; // +0x34 .. -- per-channel gain accumulators (ARTIST)
    char mbDirty;                  // +0x8D
};

class SubMixConnector
{
public:
    // Unlink this connector from its SubMix's connector list. When `foldBackGains` is
    // non-null it points at a per-channel gain array (f32[mbNumChannels]) whose values
    // are added into the SubMix's channel gains (marking the SubMix dirty) before the
    // connector's owning-SubMix / cursor / flag fields are cleared.
    //
    // (IDA renders this as `int(int result, int a2)`; r3 = this, r4 = foldBackGains.)
    static SubMixConnector *Disconnect(SubMixConnector *self, const f32 *foldBackGains);

    // FLAG (rwaudio PDB reconcile -- rw::audio::core::SubMixConnector [sizeof=20], offsets
    // MATCH ARTIST): +0x00 is a ListDNode (pnext/pprev), +0x08 is float* mpSubMixBuffer (was
    // an opaque int), +0x10 is mNumSubMixChannels (was mbField10). x64 widths.
    SubMixConnector *mpNext;   // +0x00  ListDNode.pnext
    SubMixConnector **mppPrev; // +0x04  ListDNode.pprev
    f32 *mpSubMixBuffer;       // +0x08  (was mField08/int) -- the owning SubMix's buffer
    SubMix *mpSubMix;          // +0x0C
    u8 mNumSubMixChannels;     // +0x10  (was mbField10)
};

} // namespace core
} // namespace audio
} // namespace rw
