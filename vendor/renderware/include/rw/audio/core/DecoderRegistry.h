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
// Feb-2007 PS3 leak and no DecFIGS DWARF for these types, so every offset below is
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

// -------------------------------------------------------------------------------------
// DecoderDesc -- the registration record for one codec. Codecs hand back the address of
// their static instance from GetDecoderDesc(); the registry threads records into an
// intrusive singly-linked list by their +0x10 link. Layout grounded in
// DecoderRegistry::GetDecoderHandle @0x82B67C80 and RegisterDecoder @0x82B67CB0:
//   ...                                  (object header / name / factory callbacks)
//   +0x10  mpNext   (the link field; the registry stores &mpNext as its node handle,
//                    and the owning record is node = &mpNext - 0x10)
//   +0x14  muId     (the registration GUID, compared in GetDecoderHandle/RegisterDecoder)
//
// The registry only ever touches +0x10 / +0x14, so the +0x00..+0x0F prefix (the codec's
// name pointer + descriptor factory callbacks, cf. PlugInDescRunTime in PlugIn.h) is kept
// as opaque storage to preserve the two attested offsets; expand it when a TU needs the
// descriptor body. Kept a `struct` to match the forward declarations in Xas1Dec.h/XasDec.h.
// -------------------------------------------------------------------------------------
struct DecoderDesc
{
    char mHeader[0x10]; // +0x00 .. +0x0F -- opaque descriptor header (name + callbacks)
    void *mpNext;       // +0x10 -- intrusive next link (registry node handle == &mpNext)
    u32 muId;           // +0x14 -- registration GUID
};

// The intrusive list threads DecoderDesc records by their +0x10 link. The registry's node
// handle is the address of a link slot; the owning DecoderDesc is (link - 0x10). A small
// "link view" keeps the walk member-by-name: a link slot is {mpNext, muId} laid over
// DecoderDesc at +0x10 (next) / +0x14 (id).
struct DecoderDescLink
{
    void *mpNext; // +0x00 == DecoderDesc +0x10
    u32 muId;     // +0x04 == DecoderDesc +0x14
};

// -------------------------------------------------------------------------------------
// DecoderRegistry -- owns the linked list of DecoderDesc records. Layout grounded in
// CreateInstance @0x82B6C728, GetDecoderHandle @0x82B67C80, RegisterDecoder @0x82B67CB0:
//   +0x00  mpHead   (head node's +0x10 link, i.e. node->mpNext slot; null when empty)
//   +0x04  mppTail  (tail link slot; set on the first registration)
//   +0x08  muCount  (number of registered decoders)
//   +0x0C  mpSystem (back-pointer to the owning System, set in CreateInstance)
// Allocated 16-aligned via System::New2<DecoderRegistry> (see CreateInstance).
// -------------------------------------------------------------------------------------
class DecoderRegistry
{
public:
    static DecoderRegistry *CreateInstance(System *system);
    static DecoderDesc *GetDecoderHandle(DecoderRegistry *self, int id);
    static DecoderDesc *RegisterDecoder(DecoderRegistry *self, DecoderDesc *info);
    static DecoderDesc *RegisterStandardRunTimeDecoders(DecoderRegistry *self);

    void *mpHead;       // +0x00
    void **mppTail;     // +0x04
    u32 muCount;        // +0x08
    System *mpSystem;   // +0x0C
};

} // namespace core
} // namespace audio
} // namespace rw
