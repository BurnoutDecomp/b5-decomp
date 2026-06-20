#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // template <typename T, u32 N> class Array (global ns)

namespace BrnDirector
{

// MomentHandle - opaque placeholder. No DWARF and no home TU exists for this type;
// the embedded Array<MomentHandle, N> only needs it to be a complete type with a
// known size so MomentSelector can hold it by value. Its real layout is deferred to
// its own TU. FLAG: placeholder type, fields unknown.
struct MomentHandle
{
    // opaque - deferred to its own TU
};

// MomentSelector - picks the "best" director moment for a camera/director state to
// run. NO DWARF / no Feb-2007 source exists for this class; the member set below is
// a SEMANTIC-PARITY model inferred from the five reconstructed function bodies and
// their assert strings (field NAMES are inferred, see FLAGS). Members are named, not
// laid out at byte offsets; X360 offsets are quoted in comments for reference only.
class MomentSelector
{
public:
    // Provisional capacity of the moment-handle array. The five functions in this TU
    // only ever read mMomentHandleArray.GetLength() (the live count), never indexing
    // by KU_MAX_MOMENTS, so the value does not affect their behaviour. FLAG: real
    // capacity is UNKNOWN - placeholder.
    static const u32 KU_MAX_MOMENTS = 16;

    // Returns whether a moment is currently selected. The assert strings in this TU
    // ("HasSelectedMoment()" / "!HasSelectedMoment()") name this query, so it is a
    // real inline method the asserts call.
    bool HasSelectedMoment() const { return mbHasSelectedMoment; }

    // Set the recency-weighting factor (0..1). @ 0x821F57F0
    void SetRecencyFactor(f32 lfRecencyFactor01);

    // Set the maximum number of simultaneously-active moments. @ 0x82208570
    void SetMaxActiveMoments(u32 luMaxMoments);

    // Pick the best moment for the given context, asserting none is currently
    // selected. @ 0x82254DA8
    int SelectBestMoment(int liContext);

    // Re-pick the best moment, excluding the previously-selected one, after clearing
    // the current selection. @ 0x82254E18
    int SelectNewBestMoment(int liContext);

    // Clear the current selection. @ 0x821F5868
    void CancelSelection();

private:
    // The shared selection core. DECLARE-ONLY here: it lives in a separate TU and is
    // delegated to by SelectBestMoment / SelectNewBestMoment. FLAG: parameter types
    // (int liContext, int liExcludeMomentIndex) are inferred from the call sites.
    int SelectBestMomentWithExclusion(int liContext, int liExcludeMomentIndex);

    // The moment-handle array (X360 ~+404). SetMaxActiveMoments() reads its
    // .GetLength(); Array::GetLength() internally CGS_ASSERTs it was Construct/Clear'd.
    Array<MomentHandle, KU_MAX_MOMENTS> mMomentHandleArray;

    f32  mfRecencyFactor;            // X360 v3[115] / +460
    u32  muMaxActiveMoments;         // X360 v2[117] / +468
    int  miLastSelectedMomentIndex;  // X360 +472 (read as exclusion in SelectNewBestMoment). FLAG: name inferred.
    bool mbHasSelectedMoment;        // X360 *(x+480)
    bool mbPrepared;                 // X360 *(x+481)
    bool mbMaxActiveMomentsSet;      // X360 *(x+482), set true in SetMaxActiveMoments. FLAG: name/purpose inferred.
};

} // namespace BrnDirector
