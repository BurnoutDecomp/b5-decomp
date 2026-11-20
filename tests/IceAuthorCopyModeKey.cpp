// ICEAuthor::SetState's key-edge choice when it enters a key state, checked on the production
// body: run_ice_author_copy_mode_key.py extracts SetState, ResolveEndKey and IsEndKeyState
// verbatim and renames only the receiver. ARTIST 0x8253CEE4..0x8253CEF0: entering
// SELECT_COPY_MODE asks IsEndKeyState(miPrevState) (`lwz r4, 0xE9C(r31)`), so copy mode keeps
// the edge of the key state it was entered from; every other key state answers for itself.
#include <cstdio>
#include "SDKs/Packages/ICE/ICEAuthor.hpp"

namespace ICE
{
    static const f32 KF_FIXTURE_BRACKET_START = 0.25f;
    static const f32 KF_FIXTURE_BRACKET_END   = 0.75f;

    // The edit-take calls SetState makes. Only the seek (SetParameter with the snap flag
    // set, SetState's step 5) is recorded: it is the key edge the transition chose.
    struct FakeTake
    {
        f32 mfParameter = 0.5f;
        f32 mfSeekTarget = -1.0f;

        void SetParameter(f32 lfParameter, bool lbSnap, bool)
        {
            if (lbSnap)
            {
                mfSeekTarget = lfParameter;
            }
            mfParameter = lfParameter;
        }
        f32  GetParameter() const { return mfParameter; }
        u16  GetCurrentInterval(s32) const { return 1; }
        bool DataChanged() const { return true; }
        void DiscardUndo() {}
        void NewEditBuffer() {}
        void FreeEditBuffer() {}
        void PushUndo() {}
        void SetSubParameter(f32) {}
        void SetSubTake(const void*, bool) {}
    };

    struct SetStateFixture
    {
        s32         miState          = E_ICE_AUTHOR_STATE_OFF;
        s32         miPrevState      = E_ICE_AUTHOR_STATE_OFF;
        s32         miCurrentChannel = 0;
        f32         mfSavedParameter = 0.0f;
        f32         mfSavedRangeLo   = 0.0f;
        f32         mfSavedRangeHi   = 1.0f;
        f32         mfRangeLo        = 0.0f;
        f32         mfRangeHi        = 1.0f;
        f32         mfZoomLo         = 0.0f;
        f32         mfZoomHi         = 1.0f;
        const void* mpSubTakeData    = nullptr;
        FakeTake    mEditTake;

        void GetIntervalBracket(u16, f32* lpfStart, f32* lpfEnd) const
        {
            *lpfStart = KF_FIXTURE_BRACKET_START;
            *lpfEnd   = KF_FIXTURE_BRACKET_END;
        }
        void AdjustZoomRange() {}
        template <class... T> void RefreshElementValueCache(T...) {}
        bool IsEndKeyState(s32 liKey) const;
#include "ice_set_state_decl.inc"
    };

#include "ice_resolve_end_key.inc"
#include "ice_is_end_key_state.inc"
#include "ice_set_state.inc"
}

// The pre-fix SetState took a trailing f32 defaulted to 0.0f; call whichever shape the
// extracted body has, passing the old default when it has two parameters.
template <class T>
static auto CallSetState(T& lrAuthor, s32 liState, int) -> decltype(lrAuthor.SetState(liState, 0.0f), void())
{
    lrAuthor.SetState(liState, 0.0f);
}
template <class T>
static void CallSetState(T& lrAuthor, s32 liState, long)
{
    lrAuthor.SetState(liState);
}

static unsigned guChecks;
static unsigned guFailures;

static void Check(bool lbGood, const char* lpcWhy)
{
    ++guChecks;
    if (!lbGood)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcWhy);
    }
}

// The key edge SetState seeks to on the transition liFrom -> liTo.
static f32 SeekOnEnter(s32 liFrom, s32 liTo)
{
    ICE::SetStateFixture lAuthor;
    lAuthor.miState = liFrom;
    CallSetState(lAuthor, liTo, 0);
    return lAuthor.mEditTake.mfSeekTarget;
}

int main()
{
    using namespace ICE;
    const f32 lfStart = KF_FIXTURE_BRACKET_START;
    const f32 lfEnd   = KF_FIXTURE_BRACKET_END;

    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_END_KEY_SELECTED, E_ICE_AUTHOR_STATE_SELECT_COPY_MODE) == lfEnd,
          "copy mode entered from END_KEY_SELECTED keeps the end key");
    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_EDIT_END_KEY, E_ICE_AUTHOR_STATE_SELECT_COPY_MODE) == lfEnd,
          "copy mode entered from EDIT_END_KEY keeps the end key");
    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_ASSEMBLY_EDIT_END, E_ICE_AUTHOR_STATE_SELECT_COPY_MODE) == lfEnd,
          "copy mode entered from ASSEMBLY_EDIT_END keeps the end key");
    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_START_KEY_SELECTED, E_ICE_AUTHOR_STATE_SELECT_COPY_MODE) == lfStart,
          "copy mode entered from START_KEY_SELECTED keeps the start key");
    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS, E_ICE_AUTHOR_STATE_END_KEY_SELECTED) == lfEnd,
          "END_KEY_SELECTED seeks the end key");
    Check(SeekOnEnter(E_ICE_AUTHOR_STATE_INTERVAL_SETTINGS, E_ICE_AUTHOR_STATE_START_KEY_SELECTED) == lfStart,
          "START_KEY_SELECTED seeks the start key");

    std::printf("%s ice author copy-mode key edge: %u checks, %u failures\n",
                guFailures ? "FAIL" : "PASS", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
