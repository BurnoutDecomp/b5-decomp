#pragma once

// =====================================================================================
// rw::audio::core::SubMix -- the "SubMix" plug-in: a named mix bus that other voices
// Send/Route into by name, and which then publishes the accumulated result as its own
// voice's output.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every store. Decode report
// progress/scratch_dossiers/submix_decode_codex.md.
//   GetPlugInDescRunTime      @0x82B9C370 -> the registered record (off_82F902E0, 'Sub0')
//   GetSize                   @0x82B982F0 -> console 0x90; host sizeof
//   CreateInstance            @0x82BA4680
//   CreateInstanceHandler     @0x82B9C380 -- the deferred registry push
//   Process                   @0x82B9C480
//   ReleaseEvent              @0x82BA0C18 (vt[0])
//   `vector deleting destructor' @0x82BA1DE8 (vt[3]); vt[1]/vt[2] are ICF-shared no-ops
//
// ⭐ VENDOR HEADER: references/Feb-2007/BrnEntityModuleUnity/SDKs/Packages/rwaudiocore/
// 2.11.00/include/rw/audio/core/plugins/submix.h is the authoritative naming source. It
// declares `class SubMix : public PlugIn`, the one-pointer ConstructorParams, the
// EnumerateSubMixReset/EnumerateSubMix pair, and every member below; the ARTIST layout
// matches it field for field. This header REPLACES the partial, opaque-base SubMix that
// used to live inside SubMixConnector.h as evidence scaffolding.
//
// SubMix is a real polymorphic PlugIn subclass (its ReleaseEvent is vt[0], which this tree
// models as the C++ destructor -- the same mapping Dac uses). Do NOT reintroduce an
// explicit vptr word or an opaque `char mHeader00[]` base: either creates a divergent x64
// layout, and the phase-D probe crash came from exactly that class of mistake.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API.
// =====================================================================================

#include "types.hpp"                        // f32, u8
#include "rw/audio/core/PlugIn.h"           // PlugIn (the polymorphic base) + System
#include "rw/audio/core/SubMixConnector.h"  // ListDStack / ListDNode / SubMixConnector

namespace rw
{
namespace audio
{
namespace core
{

class Mixer;
typedef Mixer AudioProcessContext;

// -------------------------------------------------------------------------------------
// SubMix -- console sizeof 0x90. Layout grounded in CreateInstance @0x82BA4680,
// CreateInstanceHandler @0x82B9C380, Process @0x82B9C480 and ReleaseEvent @0x82BA0C18;
// names from the vendor header.
//   +0x00..+0x23  the PlugIn base (mOutputChannels @+0x21 is the bus's channel count)
//   +0x24  mpSubMixBuffer      -- the accumulation buffer, 256 floats per channel
//   +0x28  mSendList           -- head of the INBOUND connector list (Sends/Routes feeding
//                                this bus); SubMixConnector::Disconnect unlinks from here
//   +0x2C  mSubMixListNode     -- this bus's link in the global by-name registry
//   +0x34  mDeClickValueTotal[6] -- per-channel residual step left by disconnects
//   +0x4C  mName[64]           -- the by-name registry key
//   +0x8C  mSubMixAdded        -- set once the deferred handler has pushed the node
//   +0x8D  mDeClickRequired    -- a disconnect folded gain back; next Process must de-click
// -------------------------------------------------------------------------------------
class SubMix : public PlugIn
{
public:
    enum { KU_GUID = 0x53756230u };   // 'Sub0'
    enum { KI_MAX_CHANNELS = 6 };     // vendor channel.h MAX_CHANNELS

    // The single constructor parameter (vendor ConstructorParams): the bus's name.
    struct ConstructorParams
    {
        const char *pName;  // +0x00
    };

    // The queued command CreateInstance pushes into the System ring (console stride 8).
    // RECORD-STRIDE RULE (the X360-literal trap): the producer's cursor advance and
    // CreateInstanceHandler's return must BOTH be this record's size. On the host that is
    // sizeof(SubMixCreateCommand) (16 -- both fields widen), never the console literal 8;
    // a hard-coded 8 would let the next record overwrite the widened SubMix* and
    // desynchronise the whole command replay.
    struct SubMixCreateCommand
    {
        int (*mpHandler)(void *); // +0x00 -- &SubMix::CreateInstanceHandler
        SubMix *mpTarget;          // +0x04
    };

    // vt[0] == ReleaseEvent @0x82BA0C18 -- drain every inbound connector, unlink from the
    // global registry, and free the accumulation buffer. (This tree maps the console's
    // ReleaseEvent slot onto the C++ destructor; see Dac for the same mapping.)
    virtual ~SubMix();

    static char **GetPlugInDescRunTime();                       // @0x82B9C370
    static int    GetSize();                                    // @0x82B982F0
    static int    CreateInstance(SubMix *self,
                                 const ConstructorParams *params); // @0x82BA4680
    static int    CreateInstanceHandler(void *apCommand);       // @0x82B9C380
    static int    Process(SubMix *self, AudioProcessContext *ctx,
                          bool discontinuity);                  // @0x82B9C480

    // The by-name registry walk. Both were INLINED into Send::ConnectByNameHandler by the
    // X360 build, so there is no standalone ARTIST address for either -- their bodies are
    // the decoded inline ranges there. Declared here because they are the vendor's own API
    // and the walk is SubMix-owned state.
    static void    EnumerateSubMixReset();
    static SubMix *EnumerateSubMix();

    const char *GetName() const { return mName; }
    f32        *GetBuffer() const { return mpSubMixBuffer; }

    // The inbound connector list head, typed. A SubMixConnector begins with its ListDNode,
    // so the node address and the connector address coincide -- which is exactly what the
    // vendor's own SubMixConnector::GetConnectorFromNode expresses.
    SubMixConnector *GetSendListHead() const
    {
        return reinterpret_cast<SubMixConnector *>(mSendList.phead);
    }
    void SetSendListHead(SubMixConnector *apConnector)
    {
        mSendList.phead = reinterpret_cast<ListDNode *>(apConnector);
    }

    f32       *mpSubMixBuffer;                    // +0x24
    ListDStack mSendList;                         // +0x28
    ListDNode  mSubMixListNode;                   // +0x2C
    f32        mDeClickValueTotal[KI_MAX_CHANNELS]; // +0x34 .. +0x4B
    char       mName[64];                         // +0x4C .. +0x8B
    u8         mSubMixAdded;                      // +0x8C
    u8         mDeClickRequired;                  // +0x8D

    // The by-name registry: two statics the vendor declares private and the X360 build
    // reaches from Send::ConnectByNameHandler because it inlined the (likewise private)
    // enumerate pair into it. Left public here rather than inventing an unattested friend.
    //   ARTIST: off_8327EE68 (sSubMixList) / dword_8327EE00 (spSubMixNextNode), both BSS
    //   and therefore zero-initialised -- neither has raw bytes in the XEX to quote.
    static ListDStack sSubMixList;      // off_8327EE68 -- registry head
    static ListDNode *spSubMixNextNode; // dword_8327EE00 -- safe-iteration cursor
};

} // namespace core
} // namespace audio
} // namespace rw
