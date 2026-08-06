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
// SubMix -- only the surface the bodied rwaudiocore functions actually touch is
// reconstructed. Every console offset below is attested by a load/store in ARTIST:
//   +0x21  mbNumChannels    (char; per-channel gain-array length; lbz 0x21 @0x82B9C428,
//                            0x82B9C460, 0x82BA0024)
//   +0x24  mpSubMixBuffer   (f32*; lwz 0x24 @0x82BA001C, 0x82BA0C90)
//   +0x28  mpConnectorHead  (SubMixConnector*; head of the inbound-connector list;
//                            lwz/stw 0x28 @0x82B9C3CC, 0x82BA002C, 0x82BA0C34)
//   +0x2C  mSubMixListNode  (ListDNode; the by-name registry link -- `addi rN, subMix,
//                            0x2C` @0x82B9C38C / 0x82BA0C50, `addic. r10, r11, -0x2C`
//                            @0x82B9FFFC)
//   +0x34  mafChannelGain[] (f32[]; per-channel accumulated gains; base +0x34, stride 4,
//                            indexed [0, mbNumChannels) -- @0x82B9C434..0x82B9C468)
//   +0x4C  mName[]          (char[]; the by-name registry key; `addi r8, r10, 0x4C` then
//                            the inlined strcmp @0x82B9FFC0..0x82B9FFE8)
//   +0x8C  mbSubMixAdded    (u8; stb 1 @0x82B9C3B8, lbz @0x82BA0C40)
//   +0x8D  mbDirty          (char; set to 1 when a fold-back changes the gains; stb 0x8D
//                            @0x82B9C420)
// The remaining bytes are the (un-homed) SubMix body, preserved as opaque storage so the
// named offsets stay exact. FLAGGED: SubMix has its own home TU (class:rw::audio::core::
// SubMix -- GetSize @0x82B982F0, GetPlugInDescRunTime @0x82B9C370, CreateInstanceHandler
// @0x82B9C380, Process @0x82B9C480, ReleaseEvent @0x82BA0C18, vector-deleting dtor
// @0x82BA1DE8; all still `todo`). Only the fields the bodied callers walk are modelled.
// -------------------------------------------------------------------------------------
class SubMixConnector; // fwd

class SubMix
{
public:
    // FLAG (rwaudio PDB reconcile -- ProStreet08Milestone.pdb, rw::audio::core::SubMix
    // [sizeof=144 console]): SubMix is a PlugIn-derived type; +0x21 is the PlugIn base's
    // mOutputChannels and +0x24 is float* mpSubMixBuffer.
    //
    // CORRECTED (wave L): an earlier revision of this comment claimed "beyond +0x24
    // ProStreet DIVERGES from ARTIST". That is DISPROVEN. The PDB agrees with ARTIST at
    // every offset ARTIST actually touches -- 0x21, 0x24, 0x28, 0x2C, 0x30, 0x4C, 0x8C,
    // 0x8D -- and the PDB member sizes TILE those offsets exactly (0x34 + 6*4 == 0x4C;
    // 0x4C + 64 == 0x8C), which is what turned a guess into a measurement. Names below
    // stay ARTIST-grounded where consumers already depend on them; the PDB spelling is
    // recorded per member.
    //
    // The wrong comment came with a real one-byte layout defect, also fixed here: the old
    // `f32 mafChannelGain[(0x8D - 0x34) / 4]` spanned +0x34..+0x8B (it swallowed mName),
    // so the trailing `char mbDirty` packed at +0x8C -- one byte EARLY -- and +0x8C
    // (mbSubMixAdded) was missing outright.
    char mHeader00[0x21];          // +0x00 .. +0x20 -- PlugIn base (opaque here)
    char mbNumChannels;            // +0x21 -- PlugIn base mOutputChannels
    char mGap22[0x24 - 0x22];      // +0x22 .. +0x23 -- opaque
    f32 *mpSubMixBuffer;           // +0x24 -- (PDB) copied into a connecting connector's
                                   //          mpSubMixBuffer (Route::ConnectByPointerHandler: lwz 0x24)
    SubMixConnector *mpConnectorHead; // +0x28 -- (PDB: ListDStack mSendList {phead})
    ListDNode mSubMixListNode;     // +0x2C/+0x30 -- link into the global by-name registry
                                   //   (PDB name). Pushed by SubMix::CreateInstanceHandler
                                   //   @0x82B9C380, walked by Send::ConnectByNameHandler
                                   //   @0x82B9FF80, unlinked by SubMix::ReleaseEvent
                                   //   @0x82BA0C18.
    f32 mafChannelGain[6];         // +0x34 .. +0x4B -- per-channel gain accumulators
                                   //   (PDB: mDeClickValueTotal[6]). ARTIST indexes this
                                   //   only over [0, mbNumChannels) (<= 6 output channels),
                                   //   so narrowing from the old f32[22] reaches nothing.
    char mName[64];                // +0x4C .. +0x8B -- the by-name registry key (PDB name)
    u8 mbSubMixAdded;              // +0x8C -- "registered in sSubMixList" flag
                                   //   (PDB: mSubMixAdded)
    char mbDirty;                  // +0x8D -- (PDB: mDeClickRequired)

    // The by-name SubMix registry: two PRIVATE STATICS owned by SubMix.
    //   DWARF: references/DecFIGS/dwarfdump/SDKs/EATech/include/rw/audio/core/plugins/
    //     submix.h -- `extern ListDStack sSubMixList;` (submix.h:151) and
    //     `extern ListDNode *spSubMixNextNode;` (submix.h:152). Names AND types are
    //     DWARF-supplied, not inferred.
    //   ProStreet08Milestone.map corroborates the owner and the access:
    //     ?sSubMixList@SubMix@core@audio@rw@@0VListDStack@234@A
    //     ?spSubMixNextNode@SubMix@core@audio@rw@@0PAVListDNode@234@A   ('0' == private static)
    //   ARTIST: off_8327EE68 / dword_8327EE00, both .data zero-init.
    // spSubMixNextNode is the persistent cursor of the (X360-inlined) EnumerateSubMixReset
    // / EnumerateSubMix pair -- ConnectByNameHandler is the only writer in the export.
    //
    // DECLARATION-ONLY, deliberately: the DEFINITIONS belong to the seeded-but-`todo`
    // class:rw::audio::core::SubMix TU and stay unresolved at link until it lands (the
    // normal leaf-first situation; `cl /c` cannot see it). A header-side definition would
    // be wrong here -- this is NOT the "no TU will ever supply the body" case.
    //
    // FLAG (access divergence): the originals are private ('0' in the mangling), reached
    // from Send::ConnectByNameHandler because the X360 build inlined SubMix's likewise-
    // private EnumerateSubMixReset/EnumerateSubMix into it -- i.e. the original grants
    // Send access somehow (friendship is the obvious mechanism, but nothing in ARTIST,
    // the DWARF or the map attests it, so it is NOT modelled). Left public here rather
    // than inventing an unattested `friend`.
    static ListDStack sSubMixList;      // off_8327EE68 -- registry head
    static ListDNode *spSubMixNextNode; // dword_8327EE00 -- safe-iteration cursor

private:
    // Never called. A member-function body is a complete-class context, so the offsetof
    // pins compile here where a bare in-class static_assert would not (same idiom as
    // plugins/VuMeter.h).
    //
    // Only POINTER-WIDTH-INVARIANT facts are pinned. The absolute console columns shift on
    // this LLP64 gate (mpSubMixBuffer / mpConnectorHead / mSubMixListNode all widen), but
    // the RELATIVE deltas inside the pointer-free tail do not -- and those deltas are what
    // fix the two PDB-supplied array sizes against the ARTIST-attested offsets.
    static void _AssertLayout()
    {
        // gain array -> name: 6 floats (asm 0x34 -> 0x4C).
        static_assert(offsetof(SubMix, mName) - offsetof(SubMix, mafChannelGain) == 0x18,
                      "mafChannelGain is exactly 6 floats (asm base 0x34, mName at 0x4C)");
        // name -> registered flag: 64 chars (asm 0x4C -> 0x8C).
        static_assert(offsetof(SubMix, mbSubMixAdded) - offsetof(SubMix, mName) == 0x40,
                      "mName is exactly 64 bytes (asm 0x4C -> stb 0x8C)");
        // the two flag bytes are adjacent (asm stb 0x8C vs stb 0x8D).
        static_assert(offsetof(SubMix, mbDirty) - offsetof(SubMix, mbSubMixAdded) == 1,
                      "mbDirty follows mbSubMixAdded (asm 0x8C then 0x8D)");
    }
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
