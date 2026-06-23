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

class CMassiveRecord : public CMassiveBaseObject
{
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
