#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveRecordImpression -- the per-impression sub-record
// the MassiveAd client embeds in a CMassiveRecord to track one ad impression's
// timing / visibility accumulators (vendor middleware). It derives the
// polymorphic CMassiveBaseObject (MassiveAdClient3.h) like every other MassiveAd
// client object.
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CMassiveRecordImpression::CMassiveRecordImpression @ 0x82BDD5E8
//     MassiveAdClient3::CMassiveRecordImpression::`vector deleting destructor' @ 0x82BDD680
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveRecordImpression class) are PRESERVED VERBATIM --
// external middleware API, not project-owned code.
//
// Layout (members BY NAME over the 0x14-byte CMassiveBaseObject base). The ctor
// touches the offsets below; the gaps it leaves unwritten (+0x3C, the tail past
// +0x58) are natural alignment padding modelled by name-less byte spans. The
// X360-absolute offsets are reproduced by member NAME, not asserted on the
// 64-bit host (the base subobject is wider there). Only mnParentRecord (+0x14)
// is exercised by a recovered reader (it is the ctor arg, the owning record back-
// pointer); the rest are the zero/float-zero-initialised accumulator slots the
// ctor writes, modelled by name as the X360 stores them:
//     +0x14  mnParentRecord  (a2; the owning CMassiveRecord)
//     +0x18  mnField18       (0)
//     +0x1C  msField1C       (u16, 0)
//     +0x20  mfField20       (float, flt_82001CC0 == 0.0f)
//     +0x24  msField24       (u16, 0)
//     +0x28  mfField28       (float, flt_82001CC0 == 0.0f)
//     +0x2C  msField2C       (u16, 0)
//     +0x30  mnField30       (0)
//     +0x34  mnField34       (0)
//     +0x38  mnField38       (0)
//     +0x40  mnField40       (i64, 0)
//     +0x48  mnField48       (i64, 0)
//     +0x50  mnField50       (i64, 0)
//     +0x58  msField58       (u16, 0)
// ===========================================================================

#include <cstdint>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"

namespace MassiveAdClient3
{

class CMassiveRecordImpression : public CMassiveBaseObject
{
public:
    // @ 0x82BDD5E8. Chains the base ctor ("CMassiveRecordImpression"), installs
    // this class's vftable (off_82187B98 -- modelled by the virtual dtor), stores
    // the owning-record pointer at +0x14, and zero/float-zero-initialises every
    // accumulator slot.
    explicit CMassiveRecordImpression(int nParentRecord);

    // @ 0x82BDD680 (vector deleting destructor). Rewrites the vftable and chains
    // the base dtor; the deleting thunk frees via the base operator delete when
    // its low bit is set.
    virtual ~CMassiveRecordImpression();

    // Additive accessor (FLAG: not its own X360 function -- the impression has no
    // standalone Reset symbol). CMassiveRecord::Reset @ 0x82BDD7A8 zeros every
    // accumulator slot of each embedded impression IN PLACE while preserving
    // mnParentRecord (it reads +0x14 then writes it back unchanged). This exposes
    // that exact field-reset to the owning record BY NAME instead of having the
    // record reach into these private members. The float slots reset to 0.0f
    // (flt_82001CC0), every integer/u16 slot to 0.
    void Reset();

    // Additive setter (FLAG: not its own X360 function). CMassiveAsset::RecordFind
    // clears this one accumulator slot (mnField30 at impression+0x30) on a re-found
    // record -- a targeted subset of the full Reset above. Routed through the owning
    // CMassiveRecord::ClearImpressionField30; exposed BY NAME so the record does not
    // poke this private slot by offset.
    void SetField30(int nValue) { mnField30 = nValue; }

private:
    int           mnParentRecord; // +0x14 (a2)
    int           mnField18;      // +0x18
    std::uint16_t msField1C;      // +0x1C
    std::uint8_t  maPad1E[2];     // +0x1E (alignment before mfField20)
    float         mfField20;      // +0x20 (flt_82001CC0 == 0.0f)
    std::uint16_t msField24;      // +0x24
    std::uint8_t  maPad26[2];     // +0x26 (alignment before mfField28)
    float         mfField28;      // +0x28 (flt_82001CC0 == 0.0f)
    std::uint16_t msField2C;      // +0x2C
    std::uint8_t  maPad2E[2];     // +0x2E (alignment before mnField30)
    int           mnField30;      // +0x30
    int           mnField34;      // +0x34
    int           mnField38;      // +0x38
    std::uint8_t  maPad3C[4];     // +0x3C (X360 gap; aligns the +0x40 i64)
    std::int64_t  mnField40;      // +0x40
    std::int64_t  mnField48;      // +0x48
    std::int64_t  mnField50;      // +0x50
    std::uint16_t msField58;      // +0x58
};

} // namespace MassiveAdClient3
