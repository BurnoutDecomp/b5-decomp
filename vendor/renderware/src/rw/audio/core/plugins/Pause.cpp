// =====================================================================================
// rw::audio::core::Pause bodies -- the "pause / fade-gate" audio plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source and no DecFIGS DWARF
// exist.
//   CreateInstance            @0x82BA36B8 -- store-for-store
//   GetPlugInDescRunTime      @0x82B9A130 -- returns the registered "Pause" descriptor
//   GetSize                   @0x82B982D0 -- returns 0x40
//   PreProcess                @0x82B9A140 -- store-for-store / branch-for-branch
//   `scalar deleting destructor' @0x82BA1B48 -- reinstalls base vtable, conditional free
// See plugins/Pause.h for the byte-exact layout.
// =====================================================================================

#include "rw/audio/core/plugins/Pause.h"

namespace rw
{
namespace audio
{
namespace core
{

// flt_82001CC0 == 0.0f, flt_82001C98 == 1.0f (the two compare/init immediates).
static const f32 KF_ZERO = 0.0f;
static const f32 KF_ONE = 1.0f;

// off_8217F4C4 -- the Pause v-table installed at construction. off_820AA810 -- the base
// PlugIn v-table the scalar deleting destructor reinstalls before any free.
// off_82F8F510 -- the registered run-time descriptor record ("Pause"). These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KPP_PauseVTable = nullptr;        // off_8217F4C4
static void *const KPP_BasePlugInVTable = nullptr;   // off_820AA810
static char *KPP_PauseDescRecord = nullptr;          // off_82F8F510 (the "Pause" record)

// -------------------------------------------------------------------------------------
// GetSize @0x82B982D0 -- the plug-in instance footprint.
// -------------------------------------------------------------------------------------
int Pause::GetSize()
{
    return 0x40; // li r3, 0x40
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9A130 -- return the address of the registered descriptor
// record (the run-time-type entry the factory consults; its label is the string "Pause").
// -------------------------------------------------------------------------------------
char **Pause::GetPlugInDescRunTime()
{
    return &KPP_PauseDescRecord; // &off_82F8F510
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA36B8 -- placement-init a Pause over `self`.
//   if (self) self->mpVTable = off_8217F4C4;
//   self->mpAttribute(+0x0C) = &self->mAttribute[0](+0x28);
//   self->mAttribute[0].mfValue = 0.0;  self->mGain = 1.0;
//   self->mPauseState = 2;      self->mDiscontinuity = 0;
//   return 1;
// -------------------------------------------------------------------------------------
int Pause::CreateInstance(Pause *self)
{
    if (self)
        self->mpVTable = KPP_PauseVTable; // off_8217F4C4

    // *(self+0x0C) = self+0x28 : point the base attribute-table slot at mAttribute[0].
    *reinterpret_cast<f32 **>(&self->mBase04[0x0C - 0x04]) = &self->mAttribute[0].mfValue;

    self->mAttribute[0].mfValue = KF_ZERO; // flt_82001CC0 @ +0x28
    self->mPauseState = 2;        // li r8,2; stb @ +0x37
    self->mGain = KF_ONE;         // flt_82001C98 @ +0x30
    self->mDiscontinuity = 0;     // li r9,0; stb @ +0x38
    return 1;
}

// -------------------------------------------------------------------------------------
// PreProcess @0x82B9A140 -- decide this block's audible length from the pause amount.
//
// Stores the requested length, then (if forced, or already active) latches the ramp
// state from mfPauseAmount: ==1.0 -> fully open (state 0, length 0 here); ==0.0 ->
// muted (state 2, length 0). With mfPauseAmount==0.0 the block is fully gated: a fresh
// (state 0 or 1) gate passes the full 0x40-sample ramp, otherwise nothing. With
// mfPauseAmount!=0.0 the block passes min(rampLength, requested) for an active ramp
// (states 1..3) and zero once muted (state>=4) or fully open (state 0).
// Branch-for-branch with the asm.
// -------------------------------------------------------------------------------------
int Pause::PreProcess(Pause *self, int /*a2*/, char force, int length)
{
    self->mOutputSamplesRequested = static_cast<u16>(length); // sth r6 @ +0x34

    // if (force != 0) OR (mDiscontinuity != 0): latch the ramp state from the pause amount.
    if ((force & 0xFF) != 0 || self->mDiscontinuity != 0)
    {
        self->mDiscontinuity = 1; // stb r10(1) @ +0x38
        f32 amount = self->mAttribute[0].mfValue;
        if (amount == KF_ONE) // fcmpu f0,f13(1.0)
        {
            self->mSamplesRemainingUntilStateChange = 0; // li r10,0; stb @ +0x36
            self->mPauseState = 0;                        // stb r10(0) @ +0x37
        }
        else if (amount == KF_ZERO) // fcmpu f0,f12(0.0)
        {
            self->mSamplesRemainingUntilStateChange = 0; // li r9,0; stb @ +0x36
            self->mPauseState = 2;                        // li r10,2; stb @ +0x37
        }
    }

    f32 amount = self->mAttribute[0].mfValue;
    u8 state = self->mPauseState;

    if (amount == KF_ZERO) // fully gated path
    {
        if (state == 0 || state == 1)
            self->mSamplesRemainingUntilStateChange = 0x40; // stb @ +0x36
        return length;                // mr r3,r6
    }

    // amount != 0.0
    if (state < 1)
        return 0;       // state 0: nothing this block
    if (state >= 4)
        return 0;       // muted: nothing
    if (state != 1)     // states 2..3: (re)arm the ramp length
        self->mSamplesRemainingUntilStateChange = 0x40;

    int out = self->mSamplesRemainingUntilStateChange;
    if (out > length)   // clamp to the requested length
        out = length;
    self->mOutputSamplesRequested = static_cast<u16>(out); // sth r10 @ +0x34
    return out;                              // mr r3,r10
}

// -------------------------------------------------------------------------------------
// `scalar deleting destructor' @0x82BA1B48
//   self->mpVTable = off_820AA810; // reinstall base PlugIn vtable
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Pause::ScalarDeletingDestructor(Pause *self, char flags)
{
    self->mpVTable = KPP_BasePlugInVTable; // off_820AA810
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
