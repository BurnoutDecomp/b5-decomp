#pragma once

// =====================================================================================
// rw::audio::core::SubMixConnector -- one node in a SubMix's intrusive list of inbound
// connections (a Send/Route attaching into a SubMix).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
// SubMixConnector::Disconnect @0x82B9C3C0 is authoritative. There is NO matching TU in
// the Feb-2007 PS3 leak and no DecFIGS DWARF for this type, so every offset below is
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
    char mHeader00[0x21];          // +0x00 .. +0x20 -- opaque SubMix header
    char mbNumChannels;            // +0x21
    char mGap22[0x24 - 0x22];      // +0x22 .. +0x23 -- opaque
    int mField24;                  // +0x24 -- copied into a connecting connector's mField08
                                   //          (Route::ConnectByPointerHandler: lwz 0x24)
    SubMixConnector *mpConnectorHead; // +0x28
    char mGap2C[0x34 - 0x2C];      // +0x2C .. +0x33 -- opaque
    f32 mafChannelGain[(0x8D - 0x34) / 4]; // +0x34 .. -- per-channel gain accumulators
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

    SubMixConnector *mpNext;   // +0x00
    SubMixConnector **mppPrev; // +0x04
    int mField08;              // +0x08
    SubMix *mpSubMix;          // +0x0C
    char mbField10;            // +0x10
};

} // namespace core
} // namespace audio
} // namespace rw
