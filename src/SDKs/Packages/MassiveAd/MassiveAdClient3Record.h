#pragma once

// ===========================================================================
// MassiveAdClient3::CMassiveRecord -- the MassiveAd client's per-asset
// impression/interaction record (vendor middleware). One record tracks two
// embedded impression accumulators (CMassiveRecordImpression) plus a list of
// dynamically-created interaction sub-records. It derives the polymorphic
// CMassiveBaseObject like every other MassiveAd client object.
//
// There is NO Feb-2007 leak source and NO DecFIGS dwarfdump for this subsystem;
// SHAPE and BODIES are both reconstructed from the X360 ARTIST.XEX pseudocode +
// disassembly. Per-function X360 addresses:
//     MassiveAdClient3::CMassiveRecord::CMassiveRecord            @ 0x82BDD6D0
//     MassiveAdClient3::CMassiveRecord::RemoveInteractionRecords  @ 0x82BDD568
//     MassiveAdClient3::CMassiveRecord::Reset                     @ 0x82BDD7A8
//     MassiveAdClient3::CMassiveRecord::`scalar deleting destructor' @ 0x82BDD828
//     (the chained ~CMassiveRecord lives in this same record TU and is modelled
//      by the virtual destructor below.)
//
// Per the naming convention the vendor SDK identifiers (the MassiveAdClient3
// namespace and the CMassiveRecord class) are PRESERVED VERBATIM -- external
// middleware API, not project-owned code.
//
// Layout (members BY NAME over the 0x14-byte CMassiveBaseObject base; the X360-
// absolute offsets are reproduced by member NAME, not asserted on the 64-bit
// host where the base subobject and the two embedded impressions are wider).
// Constructor stores (ctor args a2/a3/a4 = nType/nField14/nField18):
//     +0x14  mnField14       (a3; ctor arg)
//     +0x18  mnField18       (a4; ctor arg)
//     +0x1C  mnType          (a2; ctor arg)
//     +0x20  mImpression0    (CMassiveRecordImpression, ctor index 2)
//     +0x80  mImpression1    (CMassiveRecordImpression, ctor index 1)
//     +0xE0  mnFieldE0       (0)
//     +0xE4  mInteractionRecords (CMassiveList: head/tail/current/count)
// (The two embedded impressions sit 0x60 apart, matching the impression's
//  natural size; on the wider host they are simply two by-value members.)
// ===========================================================================

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"                       // CMassiveBaseObject, CMassiveList
#include "SDKs/Packages/MassiveAd/MassiveAdClient3RecordImpression.h"       // CMassiveRecordImpression

namespace MassiveAdClient3
{

// ---------------------------------------------------------------------------
// InteractionRecord -- one interaction sub-record listed in a CMassiveRecord's
// mInteractionRecords. These are polymorphic payloads: the first vtable slot is
// the vector deleting destructor CMassiveRecord::RemoveInteractionRecords
// @ 0x82BDD568 invokes as `(**data)(data, 1)`. CMassiveAsset::ReportImpressions
// @ 0x82BDA980 additionally reads three data fields of the listed sub-record
// when it emits each interaction report -- all three offsets are attested by
// that TU's asm (lwz +0x14, ld +0x18, lhz +0x20). The intervening bytes have no
// recovered meaning and are modelled by a name-less pad; the X360-absolute
// offsets are reproduced BY NAME, not asserted on the wider host (there is no
// standalone symbol / DecFIGS for this vendor sub-record type). Both the record
// TU (teardown) and the asset TU (reporting) share this one definition.
struct InteractionRecord
{
    struct VTable
    {
        void* (*mpfnVectorDeletingDestructor)( InteractionRecord* lpThis, int bDelete );
    };

    const VTable*      mpVTable;    // +0x00 (slot 0 == the vector deleting destructor)
    unsigned char      maPad04[16]; // +0x04..+0x13 (no recovered meaning)
    unsigned int       mnField14;   // +0x14 (u32; ReportImpressions -> AddInteractionR uTag31)
    unsigned long long mnField18;   // +0x18 (u64; ReportImpressions -> AddInteractionR nTag51)
    unsigned short     msField20;   // +0x20 (u16; ReportImpressions -> AddInteractionR sTag32)

    unsigned int       GetField14() const { return mnField14; }
    unsigned long long GetField18() const { return mnField18; }
    unsigned short     GetField20() const { return msField20; }
};

class CMassiveRecord : public CMassiveBaseObject
{
    // CMassiveAsset owns the record list this record lives on and drives its
    // reporting: CMassiveAsset::ReportImpressions @ 0x82BDA980 reads this record's
    // two embedded impression accumulators, its two ctor-arg fields (+0x14/+0x18)
    // and its interaction list directly (bare lwz/list walks in that TU's asm).
    // Friendship keeps every one of those a named-member access rather than an
    // offset hack -- the same coupling the zone manager has with CMassiveAsset.
    friend class CMassiveAsset;

public:
    // @ 0x82BDD6D0. Chains the base ctor ("CMassiveRecord"), installs this
    // class's vftable (off_82187BB8 -- modelled by the virtual dtor), stores the
    // three ctor-arg fields, constructs the two embedded impressions (index 2
    // then index 1), and zeroes the trailing field + the (empty) interaction
    // list. Argument order matches the X360 ABI: a2=nType, a3=nField14,
    // a4=nField18.
    CMassiveRecord(int nType, int nField14, int nField18);

    // The chained ~CMassiveRecord (called by the scalar deleting destructor
    // @ 0x82BDD828). It tears down the interaction records, then the embedded
    // impressions + base are destroyed as members/base. Virtual because the X360
    // installs this class's vftable (the scalar deleting destructor is its
    // compiler-emitted thunk: ~dtor then a conditional base operator delete).
    virtual ~CMassiveRecord();

    // @ 0x82BDD568. Walks the interaction-record list, virtually deletes each
    // listed sub-record (`(**data)(data, 1)` -- the vector deleting destructor),
    // then RemoveAll's the list. Returns the RemoveAll result.
    int RemoveInteractionRecords();

    // @ 0x82BDD7A8. Resets both embedded impression accumulators in place
    // (preserving each impression's parent-record pointer) and then clears the
    // interaction records (tail-calls RemoveInteractionRecords).
    void Reset();

    // Additive accessor (FLAG: not its own X360 function). CMassiveAsset::RecordFind
    // @ 0x82BDA590 walks the asset's record list and matches on this field
    // (record+0x18 == the search key); exposed BY NAME so the asset does not read
    // the private member by offset.
    int GetField18() const { return mnField18; }

    // Additive helper (FLAG: not its own X360 function). CMassiveAsset::RecordFind,
    // on RE-finding an existing record, zeroes record+0x50 and record+0x80+0x30 --
    // i.e. mnField30 of each embedded impression (0x20+0x30 and 0x80+0x30). Since
    // the impressions are this record's private members, the two stores are homed
    // here BY NAME as this helper instead of a cross-object offset poke.
    void ClearImpressionField30();

private:
    int                      mnField14;            // +0x14 (a3)
    int                      mnField18;            // +0x18 (a4)
    int                      mnType;               // +0x1C (a2)
    CMassiveRecordImpression mImpression0;         // +0x20 (ctor index 2)
    CMassiveRecordImpression mImpression1;         // +0x80 (ctor index 1)
    int                      mnFieldE0;            // +0xE0 (0)
    CMassiveList             mInteractionRecords;  // +0xE4 (head/tail/current/count)
};

} // namespace MassiveAdClient3
