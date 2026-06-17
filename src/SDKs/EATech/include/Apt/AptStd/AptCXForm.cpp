#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"

#include <cstring>   // memcpy

// ===========================================================================
// AptCXForm copy / construction.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AptCXForm::AptCXForm                @ 0x82AE2338  (copy ctor)
//   AptCXForm::AptFloatArrayCXFormCopy  @ 0x82AD5C60
//   AptCXForm::AptUint32CXFormCopy      @ 0x82ADC340
//
// SHAPE is the PC DecFIGS DWARF layout (see AptCXForm.h): two 20-byte colour
// helpers (vptr + float[4] ARGB), AptCXForm == 40 bytes.
// ===========================================================================

namespace
{
    // X360 per-channel clamp bound (flt_821455F4 = -255.0, flt_82010C20 = 255.0).
    const f32 KF_CXFORM_CHANNEL_MIN = -255.0f;
    const f32 KF_CXFORM_CHANNEL_MAX =  255.0f;

    // Unpack one packed-ARGB u32 into four float channels in ARGB order.
    //
    // Faithful to the X360 numerics @0x82ADC340: each byte is converted to
    // float and (for the scale term) multiplied by (1/255) then by 255 -- a
    // per-channel identity scale (flt_82010C1C * flt_82010C20 == 1.0) -- then
    // clamped to [-255, 255]. The translate term skips the multiply but the
    // numeric result is identical, so a single unpack covers both terms.
    //
    // Channel order matches the binary's byte extraction of the ARGB word:
    //   out[0] = (argb >> 24) & 0xFF  (Alpha)
    //   out[1] = (argb >> 16) & 0xFF  (Red)
    //   out[2] = (argb >>  8) & 0xFF  (Green)
    //   out[3] = (argb      ) & 0xFF  (Blue)
    static void UnpackArgbToChannels(u32 luArgb, f32* lpafOut)
    {
        const u32 lauChannels[4] =
        {
            (luArgb >> 24) & 0xFFu,   // Alpha
            (luArgb >> 16) & 0xFFu,   // Red
            (luArgb >>  8) & 0xFFu,   // Green
            (luArgb      ) & 0xFFu,   // Blue
        };

        for ( u32 luChannel = 0; luChannel < 4; ++luChannel )
        {
            f32 lfValue = (f32)lauChannels[luChannel] * (1.0f / 255.0f) * 255.0f;
            if ( lfValue < KF_CXFORM_CHANNEL_MIN )
            {
                lfValue = KF_CXFORM_CHANNEL_MIN;
            }
            if ( lfValue > KF_CXFORM_CHANNEL_MAX )
            {
                lfValue = KF_CXFORM_CHANNEL_MAX;
            }
            lpafOut[luChannel] = lfValue;
        }
    }
}

// Bitwise copy of another AptCXForm over this one. The X360 copy ctor's only
// real call is memcpy(this, src, 40); this method is that flat copy.
void AptCXForm::AptCXFormCopy(const AptCXForm* pCXForm)
{
    if ( pCXForm )
    {
        memcpy(this, pCXForm, sizeof(AptCXForm));   // 40 bytes
    }
}

// Copy from a flat ARGB-as-floats record: scale channels from mafScale,
// translate channels from mafTranslate (X360 @0x82AD5C60 dword copy).
void AptCXForm::AptFloatArrayCXFormCopy(const AptFloatArrayCXForm* pCXForm)
{
    if ( pCXForm )
    {
        scale.CopyFromFloatArray(pCXForm->mafScale);
        translate.CopyFromFloatArray(pCXForm->mafTranslate);
    }
}

// Copy from a packed-ARGB record: unpack each u32 to four clamped float
// channels in ARGB order (X360 @0x82ADC340).
void AptCXForm::AptUint32CXFormCopy(const AptUint32CXForm* pCXForm)
{
    if ( pCXForm )
    {
        f32 lafScale[4];
        f32 lafTranslate[4];
        UnpackArgbToChannels(pCXForm->muScaleArgb,     lafScale);
        UnpackArgbToChannels(pCXForm->muTranslateArgb, lafTranslate);
        scale.CopyFromFloatArray(lafScale);
        translate.CopyFromFloatArray(lafTranslate);
    }
}

// Copy constructor: default-construct (sets the two vptrs + zeroes the eight
// channels), then flat-copy the source over the top when one is supplied.
// Mirrors the X360 ctor @0x82AE2338.
AptCXForm::AptCXForm(AptCXForm* pCXForm)
    : scale()
    , translate()
{
    if ( pCXForm )
    {
        memcpy(this, pCXForm, sizeof(AptCXForm));   // 40 bytes
    }
}

AptCXForm::AptCXForm(AptFloatArrayCXForm* pFloatCXForm)
    : scale()
    , translate()
{
    AptFloatArrayCXFormCopy(pFloatCXForm);
}

AptCXForm::AptCXForm(AptUint32CXForm* pUintCXForm)
    : scale()
    , translate()
{
    AptUint32CXFormCopy(pUintCXForm);
}
