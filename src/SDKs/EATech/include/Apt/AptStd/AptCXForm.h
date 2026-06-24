#pragma once

// ===========================================================================
// EATech Apt (ActionScript player) colour transform (CXForm).
//
// An AptCXForm is the APT analogue of the SWF CXFORM: a pair of per-channel
// colour helpers, a multiplicative "scale" term and an additive "translate"
// term, each carrying four ARGB channels (Alpha, Red, Green, Blue).
//
// SHAPE is authoritative from the PC DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/SDKs/EATech/include/Apt/AptStd/AptCXForm.h)
// and is corroborated byte-for-byte by the X360 binary:
//   - AptColorHelper          : vptr (+0) + float mfVal[4] (+4)        = 20 bytes
//   - AptColorHelperTemplate  : adds the compile-time SCALE_FACTOR only (no data)
//   - AptColorHelperScale     : AptColorHelperTemplate<255>            = 20 bytes
//   - AptColorHelperTranslate : AptColorHelperTemplate<255>            = 20 bytes
//   - AptCXForm               : scale (+0) + translate (+20)           = 40 bytes
// The 40-byte size is proven by the X360 copy-constructor's memcpy(this,src,40)
// (@0x82AE2338) and by the eight per-channel float stores in the float-array /
// uint32 copy paths (@0x82AD5C60 / @0x82ADC340).
//
// NOTE: this is the X360/PC revision of the type. The Feb-2007 PS3 leak carried
// an OLDER AptColorHelper that packed all four channels into a single uint32_t
// (mnVal); that revision is 16 bytes and does NOT match this binary. The
// float-per-channel DWARF layout is the one shipped here.
// ===========================================================================

#include "types.hpp"

struct AptFloatArrayCXForm;
struct AptUint32CXForm;

// Per-channel colour helper: four float channels in ARGB order, behind a vptr.
// (The X360 ctor writes a vtable pointer at offset 0 of each helper subobject;
//  the virtual destructor below reproduces that vptr slot.)
struct AptColorHelper
{
    enum AptColorValue
    {
        Alpha = 0,
        Red   = 1,
        Green = 2,
        Blue  = 3,
    };

    explicit AptColorHelper()
    {
        // X360 ctor zeroes all four channels (stfs 0.0) before any copy.
        mfVal[0] = 0.0f;
        mfVal[1] = 0.0f;
        mfVal[2] = 0.0f;
        mfVal[3] = 0.0f;
    }
    virtual ~AptColorHelper() {}

    // Bulk-set the four channels from a contiguous ARGB float run.
    void CopyFromFloatArray(const f32* lpafChannels)
    {
        mfVal[0] = lpafChannels[0];
        mfVal[1] = lpafChannels[1];
        mfVal[2] = lpafChannels[2];
        mfVal[3] = lpafChannels[3];
    }

    f32 GetValuef(AptColorValue eColor) const { return mfVal[eColor]; }

    // X360 @0x82AD4938 -- MSVC `vector deleting destructor` for the base
    // AptColorHelper. The thunk restores the AptColorHelper vtable (off_82145590)
    // at +0x00 and, when bit0 of the hidden flags arg is set, frees the block via
    // the GLOBAL operator delete (the channels are plain floats, so the trivial
    // destructor itself does no member work). Returns `this`.
    //   flags: hidden MSVC second parameter (bit0 = also free the block).
    void* VectorDeletingDestructor(char flags);

protected:
    // [+0x04] four channels, ARGB order (mfVal[0]=Alpha .. mfVal[3]=Blue).
    f32 mfVal[4];
};

// gperf-style compile-time scale parameter; carries no runtime storage, so the
// helper subobject stays 20 bytes regardless of scale_factor.
template< int scale_factor >
struct AptColorHelperTemplate : public AptColorHelper
{
    enum
    {
        SCALE_FACTOR = scale_factor
    };

    virtual ~AptColorHelperTemplate() {}
};

struct AptColorHelperScale : public AptColorHelperTemplate<255>
{
    // X360 @0x82AE1818 -- MSVC `scalar deleting destructor`. Restores the same
    // base vtable (off_82145590) at +0x00 (the derived helpers add no data and
    // no member cleanup), then conditionally frees the block via the GLOBAL
    // operator delete. Returns `this`.
    void* ScalarDeletingDestructor(char flags);
};

struct AptColorHelperTranslate : public AptColorHelperTemplate<255>
{
    // X360 @0x82AE1860 -- MSVC `scalar deleting destructor`. Same shape as the
    // AptColorHelperScale thunk (shared base vtable + conditional global delete).
    void* ScalarDeletingDestructor(char flags);
};

// Flat ARGB-as-floats source: eight contiguous floats, scale[4] then
// translate[4] (X360 @0x82AD5C60 copies a2[0..3] then a2[4..7] with no gap).
struct AptFloatArrayCXForm
{
    f32 mafScale[4];
    f32 mafTranslate[4];
};

// Flat packed-ARGB source: two contiguous u32s, scale then translate
// (X360 @0x82ADC340 reads a2[0] then a2[1]; each is one packed ARGB word).
struct AptUint32CXForm
{
    u32 muScaleArgb;
    u32 muTranslateArgb;
};

struct AptCXForm
{
    AptCXForm() {}
    AptCXForm(AptCXForm* pCXForm);
    AptCXForm(AptFloatArrayCXForm* pFloatCXForm);
    AptCXForm(AptUint32CXForm* pUintCXForm);

    void AptCXFormCopy(const AptCXForm* pCXForm);
    void AptFloatArrayCXFormCopy(const AptFloatArrayCXForm* pCXForm);
    void AptUint32CXFormCopy(const AptUint32CXForm* pCXForm);

    // [+0x00] multiplicative term, [+0x14] additive term.
    AptColorHelperScale     scale;
    AptColorHelperTranslate translate;
};
