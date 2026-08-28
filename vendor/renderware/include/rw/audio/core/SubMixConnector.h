#pragma once

// =====================================================================================
// rw::audio::core::SubMixConnector -- one node in a SubMix's intrusive list of inbound
// connections (a Send/Route attaching into a SubMix).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm of
// SubMixConnector::Disconnect @0x82B9C3C0 is authoritative. There is NO matching TU in
// the reference dump and no DecFIGS DWARF for `SubMixConnector` itself, so every offset
// of that class is grounded directly in the disassembly of the bodied member function.
// (DWARF *does* cover neighbouring rwaudiocore vocabulary and is used where it applies:
// `private/linklist.h` for ListDNode/ListDStack and `plugins/submix.h` for SubMix's two
// private statics -- see the ListDStack and SubMix blocks below.)
//
// Disconnect unlinks this connector from its SubMix's doubly-linked connector list
// (head pointer lives in the SubMix at +0x28, link via mpNext / mppPrev), then -- when
// asked to fold-back -- accumulates this connection's per-channel gains into the SubMix's
// channel gain array before clearing the back-pointer/cursor fields.
//
// SubMixConnector layout (grounded in Disconnect's stores/loads; the member names below
// are the ones the class actually declares -- see its PDB-reconcile FLAG):
//   +0x00  mpNext            (next connector in the SubMix list; *result)
//   +0x04  mppPrev           (address of the previous node's mpNext slot; *(result+4))
//   +0x08  mpSubMixBuffer    (f32*; cleared to 0 on disconnect; stw r7,8(r3))
//   +0x0C  mpSubMix          (SubMix*; the owning SubMix; *(result+12))
//   +0x10  mNumSubMixChannels (u8; cleared to 0 on disconnect; stb r7,0x10(r3))
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API).
// =====================================================================================

#include "types.hpp"             // f32
#include "rw/audio/core/ITask.h" // rw::audio::core::ListDNode (its home in this tree)

#include <cstddef> // offsetof -- the relative layout pins below

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// ListDStack -- rwaudiocore's 1-pointer intrusive doubly-linked-list head.
//
// DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/audio/core/private/
// linklist.h): `struct rw::audio::core::ListDStack { private: ListDNode *phead; public:
// ListDStack(); Reset(); IsEmpty(); GetHead() const; Push(ListDNode*); Pop();
// Remove(ListDNode*); }`. Corroborated by the ProStreet08Milestone map
// (?Push@ListDStack@core@audio@rw@@QAAXPAVListDNode@234@@Z and siblings).
//
// FLAG (reconstruction shape): only the data member is modelled, and it is left public.
// The accessors are inlined at every ARTIST call site (SubMix::CreateInstanceHandler
// @0x82B9C380 / ReleaseEvent @0x82BA0C18 / Send::ConnectByNameHandler @0x82B9FF80 all
// open-code the push/remove/walk against `phead`), so there is no out-of-line body to
// home; this mirrors exactly how the sibling `ListDNode` is modelled in ITask.h.
// FLAG (home): DWARF puts ListDNode *and* ListDStack in `rw/audio/core/private/
// linklist.h`. This tree has no such header yet and already homes ListDNode in ITask.h,
// so ListDStack lands beside its first consumer here. A future rwaudiocore pass should
// move both into linklist.h; this file must not create a new shared header.
// -------------------------------------------------------------------------------------
struct ListDStack
{
    ListDNode *phead; // +0x00 (console)
};

// -------------------------------------------------------------------------------------
// SubMix -- MOVED (phase E 2026-08-28). The partial, opaque-base SubMix that used to be
// declared here was evidence scaffolding for the era when the class had no home: it
// modelled only the fields the bodied rwaudiocore functions walk, kept the PlugIn base as
// an opaque `char mHeader00[0x21]`, and carried reconstruction aliases (mbNumChannels,
// mpConnectorHead, mafChannelGain, mbDirty) in place of the vendor's names.
//
// The REAL class now lives in rw/audio/core/SubMix.h: a true `SubMix : public PlugIn`
// with the vendor submix.h member names and a full set of bodies. Including that header
// from here would be circular (it needs ListDStack/SubMixConnector from this one), so a
// forward declaration is all this header keeps -- SubMixConnector only ever stores a
// SubMix*, which needs no complete type.
// -------------------------------------------------------------------------------------
class SubMix;          // fwd -- the real declaration is in rw/audio/core/SubMix.h
class SubMixConnector; // fwd

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
