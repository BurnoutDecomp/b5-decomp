#pragma once

// =====================================================================================
// Canonical RenderWare audio-core home for the decoder registry:
//   rw::audio::core::DecoderDesc      -- per-decoder registration record (run-time type)
//   rw::audio::core::DecoderRegistry  -- intrusive singly-linked list of DecoderDesc
//
// EARenderWare "rwaudio" middleware: the registry every streaming codec (Xas1Dec, XasDec,
// EaXmaDec, EaLayer3Dec, ...) registers its static DecoderDesc into via RegisterDecoder,
// and that SndPlayer1 / PacketPlayer look a codec up in by GUID via GetDecoderHandle. It
// is the direct sibling of rw::audio::core::PlugInRegistry (see PlugIn.h) -- same intrusive
// head/tail/count linked-list shape, one slot narrower (no per-record sequence byte).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// store/offset (see DecoderRegistry.cpp). There is NO matching translation unit in the
// the LegacySrc reference and no DecFIGS DWARF for these types, so every offset below is
// grounded directly in the disassembly of the bodied member functions.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for a third-party API).
// =====================================================================================

#include "types.hpp"                       // u32 (project primitives)
#include "rw/audio/core/PlugIn.h"          // rw::audio::core::System + System::New2<>

namespace rw
{
namespace audio
{
namespace core
{

class Decoder;
struct DecoderBuffer;

typedef u32  (*DecoderGetSizeFn)(u32 auNumChannels, u32 *apuAlignment);
typedef bool (*DecoderCreateInstanceEventFn)(Decoder *apDecoder);
typedef void (*DecoderReleaseEventFn)(Decoder *apDecoder);
typedef s32  (*DecoderDecodeEventFn)(Decoder *apDecoder, DecoderBuffer *apBuffer,
                                     s32 aiNumSamples);

// -------------------------------------------------------------------------------------
// DecoderDesc -- the registration record for one codec. Codecs hand back the address of
// their static instance from GetDecoderDesc(); the registry threads records into an
// intrusive singly-linked list by their +0x10 link. Layout grounded in
// DecoderRegistry::GetDecoderHandle @0x82B67C80 and RegisterDecoder @0x82B67CB0:
//   ...                                  (object header / name / factory callbacks)
//   +0x10  listNode   (the link field; the registry stores &listNode as its node handle,
//                    and the owning record is node = &listNode - 0x10)
//   +0x14  muId     (the registration GUID, compared in GetDecoderHandle/RegisterDecoder)
//
// The registry only ever touches +0x10 / +0x14, so the +0x00..+0x0F prefix (the codec's
// name pointer + descriptor factory callbacks, cf. PlugInDescRunTime in PlugIn.h) is kept
// as opaque storage to preserve the two attested offsets; expand it when a TU needs the
// descriptor body. Kept a `struct` to match the forward declarations in Xas1Dec.h/XasDec.h.
// -------------------------------------------------------------------------------------
struct DecoderDesc
{
    DecoderGetSizeFn             pGetSize;             // +0x00
    DecoderCreateInstanceEventFn pCreateInstanceEvent; // +0x04
    DecoderReleaseEventFn        pReleaseEvent;        // +0x08 (nullable)
    DecoderDecodeEventFn         pDecodeEvent;         // +0x0C
    void                        *listNode;                // +0x10 -- intrusive next link
    u32                          muId;                  // +0x14 -- registration GUID
    u16                          muMaxBlockSize;        // +0x18 -- zero for direct decoders
};

// The decoder registry keeps its original ListQueue and opaque public handle.
class DecoderRegistry
{
public:
    typedef void *DecoderHandle;

    static DecoderRegistry *CreateInstance(System *system);
    DecoderHandle GetDecoderHandle(u32 guid);
    DecoderHandle RegisterDecoder(DecoderDesc *info);
    void RegisterStandardRunTimeDecoders();
    Decoder *DecoderFactory(DecoderHandle decoderHandle,
                            u32 numChannels, u32 maxSlots, System *system);

private:
    friend class System;

    DecoderDesc *GetDecoderDescFromNode(ListNode *node);

    ListQueue mDecoderDescList;
    System *mpSystem;
};

} // namespace core
} // namespace audio
} // namespace rw
