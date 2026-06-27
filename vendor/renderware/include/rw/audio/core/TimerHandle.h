#pragma once

// =====================================================================================
// rw::audio::core::TimerHandle -- a named profiling/measurement timer record.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm
// of TimerHandle::TimerHandle @0x82B6BEA8 is authoritative. There is NO matching TU in
// the Feb-2007 PS3 leak and no DecFIGS DWARF for this type, so every offset below is
// grounded directly in the disassembly of the bodied constructor.
//
// The constructor is invoked by many rwaudio plug-in/instance constructors
// (PlugIn::Initialize<>, Delay, ReverbModel1, PacketPlayer, AiffWriter, Snd9Service, ...)
// as an embedded sub-object -- it sets up a freshly-default timer entry whose name is the
// shared literal "Unknown" and whose state field is 3.
//
// Layout grounded in the ctor store sequence (only the four written members are named;
// the +0x04/+0x08 gap is left as opaque storage so offsets stay exact):
//   +0x00  miField0     (int,         init 0;      stw r10,0(r3))
//   +0x04  (gap)        (opaque, untouched by ctor)
//   +0x08  (gap)        (opaque, untouched by ctor)
//   +0x0C  mpName       (const char*, init "Unknown"; stw r11,0xC(r3))
//   +0x10  miField10    (int,         init 0;      stw r10,0x10(r3))
//   +0x14  mbState      (char,        init 3;      stb r9,0x14(r3))  <- single byte (stb)
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API).
// =====================================================================================

#include "types.hpp" // (project primitives)

namespace rw
{
namespace audio
{
namespace core
{

class TimerHandle
{
public:
    // Default-construct a timer entry: zero the leading field, point the name at the
    // shared "Unknown" literal, zero +0x10, and set the state byte to 3.
    // (IDA renders this as taking/returning `int` because `this` is the only arg; the
    // asm register usage -- r3 = this -- is authoritative. The +0x14 store is a `stb`,
    // so mbState is a single byte, not the word IDA's `*(result+20)=3` implies.)
    static TimerHandle *TimerHandle_ctor(TimerHandle *self);

    // FLAG (rwaudio PDB reconcile -- IDA Files/ProStreet08Milestone.pdb,
    // rw::audio::core::TimerHandle [sizeof=24]): offsets match the ARTIST ctor; the PDB
    // supplies authoritative names and fills the two ctor-untouched gaps (mpCallback @+0x04,
    // mpContext @+0x08) the earlier recon left opaque. x64 widths; +0xNN are X360 offsets.
    void *mpItemHandleNode;          // +0x00  Collection::ItemHandle.pNode (was miField0)
    void (*mpCallback)(void *, f32); // +0x04  per-tick callback (was opaque gap)
    void *mpContext;                 // +0x08  callback context (was opaque gap)
    const char *mpName;              // +0x0C
    u32 mCpuTicks;                   // +0x10  (was miField10)
    u8 mStage;                       // +0x14  timer stage (was mbState)
    u8 mTimerVisibility;             // +0x15
};

} // namespace core
} // namespace audio
} // namespace rw
