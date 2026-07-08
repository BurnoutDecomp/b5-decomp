#include "SDKs/Packages/MassiveAd/MassiveAdClient3Record.h"

// ===========================================================================
// MassiveAdClient3::CMassiveRecord -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE and BODIES both from the X360 asm (no leak / DecFIGS). Stores reproduced
// member-for-member; see MassiveAdClient3Record.h for the layout. The class
// installs its own vftable (off_82187BB8) over the CMassiveBaseObject slot --
// modelled by the virtual destructor, so no vftable store is written by hand.
// ===========================================================================

namespace MassiveAdClient3
{

namespace
{
// The interaction-record payloads listed in mInteractionRecords are polymorphic
// objects whose first vtable slot is the vector deleting destructor. The X360
// teardown calls it as `(**data)(data, 1)` -- load the vtable, call slot 0 with
// (this, 1). Modelled as a one-slot interface so the call is BY NAME rather than
// a raw vtable dereference; the listed pointers are stored as void* (the X360
// keeps an untyped owner pointer in each list node).
struct InteractionRecord
{
    struct VTable
    {
        void* (*mpfnVectorDeletingDestructor)( InteractionRecord* lpThis, int bDelete );
    };
    const VTable* mpVTable;   // slot 0 == the vector deleting destructor
};
}

// ---------------------------------------------------------------------------
// CMassiveRecord::CMassiveRecord @ 0x82BDD6D0
//
//   CMassiveBaseObject::CMassiveBaseObject(this, "CMassiveRecord");
//   this[5] = a3;  this[6] = a4;  this[7] = a2;     -> mnField14/mnField18/mnType
//   *this = &off_82187BB8;                           -> install vftable (virtual dtor)
//   CMassiveRecordImpression(this + 0x20, 2);        -> mImpression0(index 2)
//   CMassiveRecordImpression(this + 0x80, 1);        -> mImpression1(index 1)
//   this[56..60] = 0;                                -> mnFieldE0 + empty list
//
// The two embedded impressions are constructed via the member-init list (X360
// indices 2 then 1). mInteractionRecords default-constructs empty; the X360
// `stw 0, 0xE0..0xF0` zeroes mnFieldE0 and the list's four slots, which is what
// the (empty) members already are -- the explicit zero-init below mirrors the
// X360 stores for the non-class field.
// ---------------------------------------------------------------------------
CMassiveRecord::CMassiveRecord(int nType, int nField14, int nField18)
    : CMassiveBaseObject("CMassiveRecord")
    , mnField14(nField14)        // this[5] = a3
    , mnField18(nField18)        // this[6] = a4
    , mnType(nType)              // this[7] = a2
    , mImpression0(2)            // CMassiveRecordImpression(this+0x20, 2)
    , mImpression1(1)            // CMassiveRecordImpression(this+0x80, 1)
    , mnFieldE0(0)               // this[56] = 0
    , mInteractionRecords()      // this[57..60] = 0 (empty list)
{
}

// ---------------------------------------------------------------------------
// CMassiveRecord::~CMassiveRecord (chained by the scalar deleting destructor
// @ 0x82BDD828)
//
// The X360 destructor tears down the interaction records before the embedded
// impressions and the base are destroyed (the latter run as member/base dtors).
// RemoveInteractionRecords is the explicit teardown step; the two impression
// members and the CMassiveBaseObject base unwind automatically afterwards.
// ---------------------------------------------------------------------------
CMassiveRecord::~CMassiveRecord()
{
    RemoveInteractionRecords();
}

// ---------------------------------------------------------------------------
// CMassiveRecord::RemoveInteractionRecords @ 0x82BDD568
//
//   list = &mInteractionRecords;            (a1 + 228 == +0xE4)
//   list->GoToStart();
//   while ( list->mpCurrent ) {             (*(a1 + 236) == +0xEC)
//       data = list->GetCurrData();
//       if ( data ) (**data)(data, 1);      virtual vector-deleting dtor
//       list->GoToNext();
//   }
//   return list->RemoveAll();
// ---------------------------------------------------------------------------
int CMassiveRecord::RemoveInteractionRecords()
{
    CMassiveList* lpList = &mInteractionRecords;

    lpList->GoToStart();
    while ( lpList->GetCurrent() )
    {
        InteractionRecord* lpData = static_cast<InteractionRecord*>( lpList->GetCurrData() );
        if ( lpData )
            lpData->mpVTable->mpfnVectorDeletingDestructor( lpData, 1 );
        lpList->GoToNext();
    }

    lpList->RemoveAll();
    return 0;
}

// ---------------------------------------------------------------------------
// CMassiveRecord::Reset @ 0x82BDD7A8
//
// Resets both embedded impression accumulators in place (each keeps its parent-
// record pointer; see CMassiveRecordImpression::Reset for the per-slot clear),
// then tail-calls RemoveInteractionRecords to empty the interaction list. The
// X360 inlines the two impression clears (one at +0x34.., one at +0x94..) before
// the branch into RemoveInteractionRecords; reproduced here BY NAME.
// ---------------------------------------------------------------------------
void CMassiveRecord::Reset()
{
    mImpression0.Reset();
    mImpression1.Reset();
    RemoveInteractionRecords();
}

// ---------------------------------------------------------------------------
// CMassiveRecord::ClearImpressionField30 (additive helper -- see the header)
//
// Models the two stores CMassiveAsset::RecordFind @ 0x82BDA590 makes on a
// re-found record (`*(record + 0x50) = 0; *(record + 0xB0) = 0`) -- i.e. mnField30
// of each embedded impression (mImpression0 @ +0x20, mImpression1 @ +0x80). Done
// BY NAME on this record's own members instead of an offset poke from the asset.
// ---------------------------------------------------------------------------
void CMassiveRecord::ClearImpressionField30()
{
    mImpression0.SetField30(0);  // record + 0x50
    mImpression1.SetField30(0);  // record + 0xB0
}

} // namespace MassiveAdClient3
