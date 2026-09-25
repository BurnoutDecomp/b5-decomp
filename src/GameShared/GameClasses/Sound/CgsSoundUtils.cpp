#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (PathLine stage machines)
#include "rw/math/fpu/scalar_operation.h"            // rw::math::fpu::Clamp (PathLine<2>::Update's fsel pair)

#include <algorithm>   // std::sort (SelectionHistory::FindRandomOldest)
#include <cmath>

// CgsSound::Utils::Slope::Slope(const SlopeParams&) @ 0x826A1F70.
//
// Reconstructed from the X360 pseudocode/asm (member access by name -- no offset
// cast). The constructor copies the four range floats out of the source params,
// then nudges mfMaxInput away from mfMinInput by a tiny epsilon when the input span
// is (near) zero, guaranteeing a non-zero denominator for the later GetValue scale
// division.
//
//   0x826A1F70  lfs  f0, flt_82001CC0   ; 0.0f
//   ...         stfs f0, 0/4/8/0xC(r3)  ; zero mParams (the SlopeParams ctor)
//   0x826A1F88  lwz/stw 0(r4)->0(r3)    ; mfMinInput  = params.mfMinInput
//   0x826A1F94  lwz/stw 4(r4)->4(r3)    ; mfMaxInput  = params.mfMaxInput
//   0x826A1FA0  lwz/stw 8(r4)->8(r3)    ; mfMinOutput = params.mfMinOutput
//   0x826A1FAC  lwz/stw 0xC(r4)->0xC(r3); mfMaxOutput = params.mfMaxOutput
//   0x826A1FA4  fsubs f13, mfMaxInput, mfMinInput
//   0x826A1FB0  fabs  f12, f13
//   0x826A1FC0  fcmpu f12, flt_820AD47C ; 0.000001f
//   0x826A1FC4  bgelr                   ; if |span| >= 1e-6 return
//   0x826A1FC8  fadds mfMaxInput += 1e-6

namespace CgsSound
{
namespace Utils
{

// flt_820AD47C -- the degenerate-span epsilon used by this ctor (1e-6f).
static const f32 KF_MIN_INPUT_SPAN = 0.000001f;

// gafArraySinTable (DWARF CgsSoundUtils.cpp:85, float32_t[513]) -- the image's .data table at
// 0x82F2D920, read entry by entry with tools/re/x360rd.py and carried as that data: a quarter
// sine over 512 intervals, entry i the 5-decimal literal of sin(i * 3.14159 / 1024) (all 513
// match that, the authored 3.14159 included; entries 511 and 512 are both 1.0). No CRT
// initialiser writes it -- the five sites that materialise 0x82F2D920 are all readers:
// GetOutput's two table arms and the E_ONE_MINUS_EQPWR copies inlined into
// MusicStream::UpdateVoiceParams (0x826BB950), World::EmitterEffect::ProcessUpdate (0x826E6DC4)
// and Streaming::StreamingEffect::Detach (0x826EEBA8).
f32 gafArraySinTable[513] =
{
    /*   0 */ 0.00000f, 0.00307f, 0.00614f, 0.00920f, 0.01227f, 0.01534f, 0.01841f, 0.02147f,
    /*   8 */ 0.02454f, 0.02761f, 0.03067f, 0.03374f, 0.03681f, 0.03987f, 0.04294f, 0.04600f,
    /*  16 */ 0.04907f, 0.05213f, 0.05520f, 0.05826f, 0.06132f, 0.06438f, 0.06744f, 0.07050f,
    /*  24 */ 0.07356f, 0.07662f, 0.07968f, 0.08274f, 0.08580f, 0.08885f, 0.09191f, 0.09496f,
    /*  32 */ 0.09802f, 0.10107f, 0.10412f, 0.10717f, 0.11022f, 0.11327f, 0.11632f, 0.11937f,
    /*  40 */ 0.12241f, 0.12545f, 0.12850f, 0.13154f, 0.13458f, 0.13762f, 0.14066f, 0.14369f,
    /*  48 */ 0.14673f, 0.14976f, 0.15280f, 0.15583f, 0.15886f, 0.16189f, 0.16491f, 0.16794f,
    /*  56 */ 0.17096f, 0.17398f, 0.17700f, 0.18002f, 0.18304f, 0.18605f, 0.18907f, 0.19208f,
    /*  64 */ 0.19509f, 0.19810f, 0.20110f, 0.20411f, 0.20711f, 0.21011f, 0.21311f, 0.21611f,
    /*  72 */ 0.21910f, 0.22209f, 0.22508f, 0.22807f, 0.23106f, 0.23404f, 0.23702f, 0.24000f,
    /*  80 */ 0.24298f, 0.24595f, 0.24893f, 0.25190f, 0.25487f, 0.25783f, 0.26079f, 0.26375f,
    /*  88 */ 0.26671f, 0.26967f, 0.27262f, 0.27557f, 0.27852f, 0.28146f, 0.28441f, 0.28735f,
    /*  96 */ 0.29028f, 0.29322f, 0.29615f, 0.29908f, 0.30201f, 0.30493f, 0.30785f, 0.31077f,
    /* 104 */ 0.31368f, 0.31659f, 0.31950f, 0.32241f, 0.32531f, 0.32821f, 0.33111f, 0.33400f,
    /* 112 */ 0.33689f, 0.33978f, 0.34266f, 0.34554f, 0.34842f, 0.35129f, 0.35416f, 0.35703f,
    /* 120 */ 0.35989f, 0.36276f, 0.36561f, 0.36847f, 0.37132f, 0.37416f, 0.37701f, 0.37985f,
    /* 128 */ 0.38268f, 0.38552f, 0.38834f, 0.39117f, 0.39399f, 0.39681f, 0.39962f, 0.40243f,
    /* 136 */ 0.40524f, 0.40804f, 0.41084f, 0.41364f, 0.41643f, 0.41922f, 0.42200f, 0.42478f,
    /* 144 */ 0.42755f, 0.43033f, 0.43309f, 0.43586f, 0.43862f, 0.44137f, 0.44412f, 0.44687f,
    /* 152 */ 0.44961f, 0.45235f, 0.45508f, 0.45781f, 0.46054f, 0.46326f, 0.46598f, 0.46869f,
    /* 160 */ 0.47140f, 0.47410f, 0.47680f, 0.47949f, 0.48218f, 0.48487f, 0.48755f, 0.49023f,
    /* 168 */ 0.49290f, 0.49556f, 0.49823f, 0.50088f, 0.50354f, 0.50619f, 0.50883f, 0.51147f,
    /* 176 */ 0.51410f, 0.51673f, 0.51936f, 0.52197f, 0.52459f, 0.52720f, 0.52980f, 0.53240f,
    /* 184 */ 0.53500f, 0.53759f, 0.54017f, 0.54275f, 0.54532f, 0.54789f, 0.55046f, 0.55302f,
    /* 192 */ 0.55557f, 0.55812f, 0.56066f, 0.56320f, 0.56573f, 0.56826f, 0.57078f, 0.57330f,
    /* 200 */ 0.57581f, 0.57831f, 0.58081f, 0.58331f, 0.58580f, 0.58828f, 0.59076f, 0.59323f,
    /* 208 */ 0.59570f, 0.59816f, 0.60062f, 0.60307f, 0.60551f, 0.60795f, 0.61038f, 0.61281f,
    /* 216 */ 0.61523f, 0.61765f, 0.62006f, 0.62246f, 0.62486f, 0.62725f, 0.62964f, 0.63202f,
    /* 224 */ 0.63439f, 0.63676f, 0.63912f, 0.64148f, 0.64383f, 0.64618f, 0.64851f, 0.65085f,
    /* 232 */ 0.65317f, 0.65549f, 0.65781f, 0.66011f, 0.66242f, 0.66471f, 0.66700f, 0.66928f,
    /* 240 */ 0.67156f, 0.67383f, 0.67609f, 0.67835f, 0.68060f, 0.68285f, 0.68508f, 0.68731f,
    /* 248 */ 0.68954f, 0.69176f, 0.69397f, 0.69618f, 0.69838f, 0.70057f, 0.70275f, 0.70493f,
    /* 256 */ 0.70711f, 0.70927f, 0.71143f, 0.71358f, 0.71573f, 0.71787f, 0.72000f, 0.72213f,
    /* 264 */ 0.72425f, 0.72636f, 0.72846f, 0.73056f, 0.73265f, 0.73474f, 0.73682f, 0.73889f,
    /* 272 */ 0.74095f, 0.74301f, 0.74506f, 0.74710f, 0.74914f, 0.75116f, 0.75319f, 0.75520f,
    /* 280 */ 0.75721f, 0.75921f, 0.76120f, 0.76319f, 0.76517f, 0.76714f, 0.76910f, 0.77106f,
    /* 288 */ 0.77301f, 0.77495f, 0.77689f, 0.77882f, 0.78074f, 0.78265f, 0.78456f, 0.78645f,
    /* 296 */ 0.78835f, 0.79023f, 0.79211f, 0.79398f, 0.79584f, 0.79769f, 0.79954f, 0.80138f,
    /* 304 */ 0.80321f, 0.80503f, 0.80685f, 0.80866f, 0.81046f, 0.81225f, 0.81404f, 0.81581f,
    /* 312 */ 0.81758f, 0.81935f, 0.82110f, 0.82285f, 0.82459f, 0.82632f, 0.82804f, 0.82976f,
    /* 320 */ 0.83147f, 0.83317f, 0.83486f, 0.83655f, 0.83822f, 0.83989f, 0.84155f, 0.84321f,
    /* 328 */ 0.84485f, 0.84649f, 0.84812f, 0.84974f, 0.85135f, 0.85296f, 0.85456f, 0.85615f,
    /* 336 */ 0.85773f, 0.85930f, 0.86087f, 0.86242f, 0.86397f, 0.86551f, 0.86705f, 0.86857f,
    /* 344 */ 0.87009f, 0.87159f, 0.87309f, 0.87459f, 0.87607f, 0.87754f, 0.87901f, 0.88047f,
    /* 352 */ 0.88192f, 0.88336f, 0.88480f, 0.88622f, 0.88764f, 0.88905f, 0.89045f, 0.89184f,
    /* 360 */ 0.89322f, 0.89460f, 0.89597f, 0.89732f, 0.89867f, 0.90002f, 0.90135f, 0.90267f,
    /* 368 */ 0.90399f, 0.90530f, 0.90660f, 0.90789f, 0.90917f, 0.91044f, 0.91171f, 0.91296f,
    /* 376 */ 0.91421f, 0.91545f, 0.91668f, 0.91790f, 0.91911f, 0.92032f, 0.92151f, 0.92270f,
    /* 384 */ 0.92388f, 0.92505f, 0.92621f, 0.92736f, 0.92851f, 0.92964f, 0.93077f, 0.93188f,
    /* 392 */ 0.93299f, 0.93409f, 0.93518f, 0.93627f, 0.93734f, 0.93840f, 0.93946f, 0.94051f,
    /* 400 */ 0.94154f, 0.94257f, 0.94359f, 0.94460f, 0.94561f, 0.94660f, 0.94759f, 0.94856f,
    /* 408 */ 0.94953f, 0.95049f, 0.95143f, 0.95237f, 0.95331f, 0.95423f, 0.95514f, 0.95604f,
    /* 416 */ 0.95694f, 0.95783f, 0.95870f, 0.95957f, 0.96043f, 0.96128f, 0.96212f, 0.96295f,
    /* 424 */ 0.96378f, 0.96459f, 0.96539f, 0.96619f, 0.96698f, 0.96775f, 0.96852f, 0.96928f,
    /* 432 */ 0.97003f, 0.97077f, 0.97150f, 0.97223f, 0.97294f, 0.97364f, 0.97434f, 0.97503f,
    /* 440 */ 0.97570f, 0.97637f, 0.97703f, 0.97768f, 0.97832f, 0.97895f, 0.97957f, 0.98018f,
    /* 448 */ 0.98079f, 0.98138f, 0.98196f, 0.98254f, 0.98311f, 0.98366f, 0.98421f, 0.98475f,
    /* 456 */ 0.98528f, 0.98580f, 0.98631f, 0.98681f, 0.98730f, 0.98778f, 0.98826f, 0.98872f,
    /* 464 */ 0.98918f, 0.98962f, 0.99006f, 0.99048f, 0.99090f, 0.99131f, 0.99171f, 0.99210f,
    /* 472 */ 0.99248f, 0.99285f, 0.99321f, 0.99356f, 0.99391f, 0.99424f, 0.99456f, 0.99488f,
    /* 480 */ 0.99518f, 0.99548f, 0.99577f, 0.99604f, 0.99631f, 0.99657f, 0.99682f, 0.99706f,
    /* 488 */ 0.99729f, 0.99751f, 0.99772f, 0.99793f, 0.99812f, 0.99830f, 0.99848f, 0.99864f,
    /* 496 */ 0.99880f, 0.99894f, 0.99908f, 0.99920f, 0.99932f, 0.99943f, 0.99953f, 0.99962f,
    /* 504 */ 0.99970f, 0.99977f, 0.99983f, 0.99988f, 0.99992f, 0.99996f, 0.99998f, 1.00000f,
    /* 512 */ 1.00000f
};

// KF_LAST_ELEMENT_IN_ARRAY (DWARF CgsSoundUtils.cpp:129) = flt_820AA7B0 = 511.0f; the E_POWER
// arm multiplies by its negation, flt_820AD414 = -511.0f.
static const f32 KF_LAST_ELEMENT_IN_ARRAY = 511.0f;

// Both table arms read an entry the same way: fctiwz, then `slwi r10,r10,2 ; subf r11,r10,r11 ;
// lfs 0(r11)` (0x8268975C..0x82689764, 0x826897A4..0x826897AC) -- table - (lnIndex << 2) in
// 32-bit address arithmetic, i.e. entry -lnIndex. A NaN converts to 0x80000000 (fctiwz, and
// x64's cvttss2si alike), whose byte offset wraps to 0: the console reads entry 0.
static f32 ReadSinTable(s32 lnIndex)
{
    const u32 luByteOffset = static_cast<u32>(lnIndex) << 2;
    return gafArraySinTable[(0u - luByteOffset) >> 2];
}

// ARTIST @ 0x82689698 (DWARF CgsSoundUtils.cpp:159). Every caller clamps its fraction to [0, 1]
// first -- the fsel pair of Slope::GetValue, PathLine<2/3>::Update and InterpolateLine::Update,
// which also turns a NaN into 1.
f32 Curve::GetOutput(f32 lfFraction, ECurveType leCurve)
{
    // `fcmpu f31,1.0 ; bgt -> fire ; fcmpu f31,0.0 ; bge -> past` (0x826896CC..0x826896D8): the
    // assert fires for an input above 1 or below 0 only -- a NaN passes it on the console.
    CGS_ASSERT(!(lfFraction > 1.0f || lfFraction < 0.0f),
               "( lfInput <= 1.0f ) && ( lfInput >= 0.0f )");

    switch (leCurve)
    {
        case E_LINEAR:
            return lfFraction;

        case E_POWER:
            // fmuls by -511 (flt_820AD414), fctiwz: entry trunc(lfFraction * 511).
            return ReadSinTable(static_cast<s32>(lfFraction * -KF_LAST_ELEMENT_IN_ARRAY));

        case E_EQ_PWR_SQ:
        {
            const f32 lfPower = GetOutput(lfFraction, E_POWER);
            return lfPower * lfPower;
        }

        case E_ONE_MINUS_EQPWR:
            // Its own lookup, not a call into E_POWER (0x82689780..0x826897B0): fmsubs
            // lfFraction * 511 - 511 rounded ONCE (std::fma -- bit-exact to it for every float in
            // [0, 1]), fctiwz -- entry trunc(511 * (1 - lfFraction)) -- then 1 - the entry.
            return 1.0f - ReadSinTable(static_cast<s32>(std::fma(
                lfFraction, KF_LAST_ELEMENT_IN_ARRAY, -KF_LAST_ELEMENT_IN_ARRAY)));

        case E_ONE_MINUS_EQPWR_SQ:
        {
            const f32 lfOneMinus = GetOutput(lfFraction, E_ONE_MINUS_EQPWR);
            return lfOneMinus * lfOneMinus;
        }

        default:
            return 0.0f;
    }
}

// InterpolateLine @ ARTIST 0x826A1E90 and its inlined Initialize/Reset accessors.
// Lengths supplied to this utility are milliseconds in the authored controls.
void InterpolateLine::Initialize(f32 lfStart, f32 lfFinish, f32 lfLength,
                                 Curve::ECurveType leCurve)
{
    mfElapsedTime = 0.0f;
    mfLength = lfLength * 0.001f;
    if (mfLength <= 0.0f)
        mfLength = 0.01f;
    mfStart = lfStart;
    mfFinish = lfFinish;
    meCurveTypes = leCurve;
    mfCurrentValue = lfStart;
    mbComplete = false;
}

void InterpolateLine::Reset(f32 lfValue)
{
    mfElapsedTime = 0.0f;
    mbComplete = false;
    if (lfValue == 0.0f)
    {
        mfCurrentValue = mfStart;
    }
    else
    {
        mfStart = lfValue;
        mfCurrentValue = lfValue;
    }
}

f32 InterpolateLine::GetValueFloat() const
{
    return mfCurrentValue;
}

void InterpolateLine::Update(f32 lfDeltaTime)
{
    if (mbComplete)
        return;

    CGS_ASSERT(mfLength != 0.0f, "mfLength != 0.0f");
    mfElapsedTime += lfDeltaTime;
    if (mfElapsedTime <= mfLength)
    {
        f32 lfFraction = mfElapsedTime / mfLength;
        lfFraction = (std::max)(0.0f, (std::min)(1.0f, lfFraction));
        mfCurrentValue = Curve::GetOutput(lfFraction, meCurveTypes) *
                         (mfFinish - mfStart) + mfStart;
    }
    else
    {
        mfCurrentValue = mfFinish;
        mbComplete = true;
    }
}

f32 Graph::GetYValue(f32 lfX) const
{
    if (!maPoints || muNumOfPoints == 0)
        return 0.0f;
    if (lfX <= maPoints[0].x)
        return maPoints[0].y;

    for (u8 luPoint = 1; luPoint < muNumOfPoints; ++luPoint)
    {
        if (lfX <= maPoints[luPoint].x)
        {
            const Vector2& lrLeft = maPoints[luPoint - 1];
            const Vector2& lrRight = maPoints[luPoint];
            const f32 lfWidth = lrRight.x - lrLeft.x;
            const f32 lfFraction = lfWidth != 0.0f ? (lfX - lrLeft.x) / lfWidth : 0.0f;
            return lrLeft.y + (lrRight.y - lrLeft.y) * lfFraction;
        }
    }
    return maPoints[muNumOfPoints - 1].y;
}

Slope::Slope(const SlopeParams& params)
    : mParams(params)
{
    if (std::fabs(mParams.mfMaxInput - mParams.mfMinInput) < KF_MIN_INPUT_SPAN)
    {
        mParams.mfMaxInput += KF_MIN_INPUT_SPAN;
    }
}

// Nothing to undo: the console's stack Slopes (PassbyEffect::UpdateParams builds two, at
// 0x826D5208 and 0x826D5258) are never destroyed -- no destructor call follows either
// GetValue. The declaration (DWARF CgsSoundUtils.h:386) had no body, so the first Slope
// object in the game would not have linked.
Slope::~Slope()
{
}

// ---------------------------------------------------------------------------
// CgsSound::Utils::Slope::GetValue(f32, Curve::ECurveType) const  @ 0x826897F0 (DWARF CgsSoundUtils.h:330)
//   0x82689804..0x82689824  f = (lfInput - mfMinInput) / (mfMaxInput - mfMinInput)  (fsubs, fsubs, fdivs)
//   0x82689828..0x82689838  `fneg ; fsel` + `fsubs ; fsel` -- rw::math::fpu::Clamp(f, 0, 1): a NaN -> 1.0
//   0x8268983C             Curve::GetOutput(f, leCurve) -- the curve rides r5, because the f32 input
//                          takes f1 and EATS the r4 slot (`mr r4, r5` before the call)
//   0x82689840..0x8268984C  `fsubs f13, mfMaxOutput, mfMinOutput ; fmadds f1, f13, f1(out), f0(mfMinOutput)`
//                          -- ONE rounding: std::fmaf with the console's operands
// Its one caller is PassbyEffect::UpdateParams @0x826D5068 (the pass-by pitch and volume scales).
// ---------------------------------------------------------------------------
f32 Slope::GetValue(f32 lfInput, Curve::ECurveType leCurve) const
{
    const f32 lfFraction = rw::math::fpu::Clamp(
        (lfInput - mParams.mfMinInput) / (mParams.mfMaxInput - mParams.mfMinInput), 0.0f, 1.0f);
    return std::fmaf(mParams.mfMaxOutput - mParams.mfMinOutput, Curve::GetOutput(lfFraction, leCurve),
                     mParams.mfMinOutput);
}

// ============================================================================================
// CgsSound::Utils::PathLine stage-machine bodies, reconstructed store-for-store from
// BURNOUT_X360.XEX. Two instantiations are attested by the X360 build:
//
//   PathLine<2>  -- AddLinkedStage @0x826A84D8, AddStage @0x82690B40, ClearStages @0x8268F278,
//                   Initialize @0x826A8460, Update @0x8268F2D0
//   PathLine<3>  -- ClearStages @0x8268EEA0, AddStage @0x8268EEF8
//
// PathLine<2>'s members are written as EXPLICIT SPECIALIZATIONS (`template <>`), matching the
// X360's dedicated <2> code. PathLine<3>'s two members are written as the GENERIC out-of-line
// template body plus a PER-MEMBER explicit instantiation for <3u> (there is no explicit
// specialization of PathLine<3>). A whole-struct `template struct PathLine<3u>;` is deliberately
// NOT used: the header also declares AddLinkedStage / Update / Initialize which have no <3> body
// in scope, so a full instantiation would fail to link. Only the two X360-attested <3> members
// are instantiated.
// ============================================================================================

// flt_82013F90 -- length input scale (ms fixed-point -> the path's internal units).
static const f32 KF_STAGE_LENGTH_SCALE = 0.001f;
// flt_82002138 -- minimum stage length floor applied when the scaled length is <= 0.
static const f32 KF_MIN_STAGE_LENGTH   = 0.0099999998f;

// -------------------------------------------------------------------------------------------
// PathLine<2> -- explicit specializations.
// -------------------------------------------------------------------------------------------

// AddStage @ 0x82690B40 -- append one interpolation stage.
template <>
s32 PathLine<2>::AddStage(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages != 2, "mnNumStages != TNumPoints");

    // Length is supplied in milliseconds; store it in seconds and clamp to a floor
    // so Update never divides by (near) zero.
    maLength[mnNumStages] = lfLength * 0.001f;
    if (maLength[mnNumStages] <= 0.0f)
    {
        maLength[mnNumStages] = 0.01f;
    }

    maFinish[mnNumStages]     = lfFinish;
    maStart[mnNumStages]      = lfStart;
    maCurveTypes[mnNumStages] = leCurve;
    maIsLinked[mnNumStages]   = false;

    mbComplete = false;

    // The first stage seeds the running output value with its start level.
    if (mnNumStages == 0)
    {
        mfCurrentValue = maStart[0];
    }

    ++mnNumStages;
    return mnNumStages;
}

// AddLinkedStage @ 0x826A84D8 -- append a stage that ramps from the previous stage's finish.
template <>
s32 PathLine<2>::AddLinkedStage(f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages, "mnNumStages");

    // A linked stage always ramps from the previous stage's finish level, so the
    // start is seeded to 0.0f here and carried over at stage-advance time (Update).
    AddStage(0.0f, lfFinish, lfLength, leCurve);

    // Mark the stage that AddStage just appended (mnNumStages was ++'d) as linked.
    maIsLinked[mnNumStages - 1] = true;

    return mnNumStages;
}

// ClearStages @ 0x8268F278 -- reset the path to empty and mark it complete.
template <>
void PathLine<2>::ClearStages()
{
    mfElapsedTime  = 0.0f;
    mnNumStages    = 0;
    mfCurrentValue = 0.0f;
    mnCurrentStage = 0;

    for (s32 lk = 0; lk < 2; ++lk)
    {
        maLength[lk]     = 0.0f;
        maStart[lk]      = 0.0f;
        maFinish[lk]     = 0.0f;
        maIsLinked[lk]   = false;
        maCurveTypes[lk] = Curve::E_LINEAR;
    }

    mbComplete = true;
}

// Initialize @ 0x826A8460 -- reset then seed a single stage; latch the running value.
template <>
void PathLine<2>::Initialize(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    ClearStages();
    AddStage(lfStart, lfFinish, lfLength, leCurve);
    mfCurrentValue = lfStart;
}

// Update @ 0x8268F2D0 -- advance the cursor by lfDeltaTime, updating mfCurrentValue.
template <>
void PathLine<2>::Update(f32 lfDeltaTime)
{
    if (mbComplete || mnNumStages == 0)
    {
        return;
    }

    const s32 lnStage = mnCurrentStage;
    CGS_ASSERT(maLength[lnStage], "maLength[mnCurrentStage]");

    mfElapsedTime += lfDeltaTime;

    if (mfElapsedTime > maLength[lnStage])
    {
        // Current stage finished: snap to its finish level.
        mfCurrentValue = maFinish[lnStage];

        if (lnStage >= mnNumStages - 1)
        {
            mbComplete = true;
        }
        else
        {
            const s32 lnNext = lnStage + 1;
            mfElapsedTime -= maLength[lnStage];
            mnCurrentStage = lnNext;

            // A linked stage starts from the previous stage's finish level.
            if (maIsLinked[lnNext])
            {
                maStart[lnNext] = maFinish[lnNext - 1];
            }
        }
    }
    else
    {
        // Interpolate within the current stage.
        const f32 lfRange = maFinish[lnStage] - maStart[lnStage];

        // 0x8268F3D4..0x8268F410: `fdivs`, then `fneg ; fsel` (Max(0, x): a NaN stays) and
        // `fsubs ; fsel` (Min(1, x): a NaN becomes 1.0) -- rw::math::fpu::Clamp's own forms. A NaN
        // position (a NaN step or length, or 0 / 0 on a zero-length stage after the maLength
        // tripwire) reads the curve at 1.0, i.e. the stage's finish level.
        const f32 lfFraction = rw::math::fpu::Clamp(mfElapsedTime / maLength[lnStage], 0.0f, 1.0f);

        // `fmadds f0, f1(GetOutput), f31(range), f0(maStart[mnCurrentStage])` @0x8268F428: ONE rounding
        // (FX-AIBUZZ, crash parity 2026-09-25; the PC multiplied, rounded, then added).
        mfCurrentValue = std::fmaf(Curve::GetOutput(lfFraction, maCurveTypes[lnStage]), lfRange,
                                   maStart[mnCurrentStage]);
    }
}

// -------------------------------------------------------------------------------------------
// PathLine<3> -- generic out-of-line template bodies + per-member explicit instantiation.
// -------------------------------------------------------------------------------------------

// ClearStages @ 0x8268EEA0 -- reset the path to empty and mark it complete.
template <u32 tuNumStages>
void PathLine<tuNumStages>::ClearStages()
{
    mfElapsedTime  = 0.0f;
    mnNumStages    = 0;
    mfCurrentValue = 0.0f;
    mnCurrentStage = 0;

    for (u32 lu = 0; lu < tuNumStages; ++lu)
    {
        maLength[lu]     = 0.0f;
        maStart[lu]      = 0.0f;
        maFinish[lu]     = 0.0f;
        maIsLinked[lu]   = false;
        maCurveTypes[lu] = Curve::E_LINEAR;
    }

    mbComplete = true;
}

// AddStage @ 0x8268EEF8 -- append one interpolation stage.
template <u32 tuNumStages>
s32 PathLine<tuNumStages>::AddStage(f32 lfStart, f32 lfFinish, f32 lfLength,
                                    Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages != static_cast<s32>(tuNumStages), "mnNumStages != TNumPoints");

    maLength[mnNumStages] = lfLength * KF_STAGE_LENGTH_SCALE;
    if (maLength[mnNumStages] <= 0.0f)
    {
        maLength[mnNumStages] = KF_MIN_STAGE_LENGTH;
    }

    maFinish[mnNumStages]     = lfFinish;
    maStart[mnNumStages]      = lfStart;
    maCurveTypes[mnNumStages] = leCurve;
    maIsLinked[mnNumStages]   = false;
    mbComplete                = false;

    if (mnNumStages == 0)
    {
        mfCurrentValue = maStart[0];
    }

    ++mnNumStages;
    return mnNumStages;
}

// ARTIST @0x826A81C0 for PathLine<3>.  The two-argument update keeps the
// currently-active destination live, advances through the ordinary path state
// machine, then snaps a completed path to the supplied destination.
template <u32 tuNumStages>
void PathLine<tuNumStages>::Update(f32 lfDeltaTime, f32 lfFinish)
{
    maFinish[mnCurrentStage] = lfFinish;
    Update(lfDeltaTime);
    if (mbComplete)
        mfCurrentValue = lfFinish;
}

// Generic update used by the three-stage airborne-throttle path.  This is the
// same state machine as the independently exported PathLine<2> specialization.
template <u32 tuNumStages>
void PathLine<tuNumStages>::Update(f32 lfDeltaTime)
{
    if (mbComplete || mnNumStages == 0)
        return;

    const s32 lnStage = mnCurrentStage;
    CGS_ASSERT(maLength[lnStage], "maLength[mnCurrentStage]");
    mfElapsedTime += lfDeltaTime;

    if (mfElapsedTime > maLength[lnStage])
    {
        mfCurrentValue = maFinish[lnStage];
        if (lnStage >= mnNumStages - 1)
        {
            mbComplete = true;
        }
        else
        {
            const s32 lnNext = lnStage + 1;
            mfElapsedTime -= maLength[lnStage];
            mnCurrentStage = lnNext;
            if (maIsLinked[lnNext])
                maStart[lnNext] = maFinish[lnNext - 1];
        }
    }
    else
    {
        f32 lfFraction = mfElapsedTime / maLength[lnStage];
        lfFraction = (std::max)(0.0f, (std::min)(1.0f, lfFraction));
        // `fmadds f0, f1(GetOutput), f31(finish - start), f0(maStart[mnCurrentStage])` @0x8268F178: ONE
        // rounding (FX-AIBUZZ, crash parity 2026-09-25; the PC multiplied, rounded, then added).
        mfCurrentValue = std::fmaf(Curve::GetOutput(lfFraction, maCurveTypes[lnStage]),
                                   maFinish[lnStage] - maStart[lnStage], maStart[lnStage]);
    }
}

// Per-member explicit instantiation for the X360-attested PathLine<3> instance.
template void PathLine<3u>::ClearStages();
template s32  PathLine<3u>::AddStage(f32, f32, f32, Curve::ECurveType);
template void PathLine<3u>::Update(f32);
template void PathLine<3u>::Update(f32, f32);

// ============================================================================================
// CgsSound::Utils::SelectionHistory<512u,u16,u16,65536ull> -- the recently-used-selection
// tracker. Bodies reconstructed from BURNOUT_X360_ARTIST.XEX; explicit instantiation below.
// ============================================================================================

// @ 0x82690D40 -- FindResult timestamp comparator (std::sort predicate). 2-arg / no-this leaf:
//   lhz timestamps, subfc/subfe/clrlwi == (left.mTimeStamp < right.mTimeStamp).
bool SelectionHistory<512u, u16, u16, 65536ull>::LessThanTimeStamp(
    const SelectionHistory<512u, u16, u16, 65536ull>::FindResult& lkrLeft,
    const SelectionHistory<512u, u16, u16, 65536ull>::FindResult& lkrRight)
{
    return lkrLeft.mTimeStamp < lkrRight.mTimeStamp;
}

// @ 0x826C5900 -- reseed mRandom, rebuild maHistory as the identity permutation, Fisher-Yates
// shuffle, then reset the running timestamp to KU_SIZE. The X360 inlines the whole Random refill
// spine + the bounded LCG draw; reconstructed here through the CgsNumeric::Random public API.
// CONFIDENCE low: the exact bounded-draw mapping (RandomUInt(0,KU_SIZE) vs the inlined
// (seed>>32)&0x1FF) is reconstructed by intent, not verified store-for-store.
void SelectionHistory<512u, u16, u16, 65536ull>::Randomize(unsigned int luSeed)
{
    mRandom.SetSeed(luSeed);

    // Identity permutation: maHistory[i] holds selection index i (loop @0x826C5AEC).
    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        maHistory[lu].mTimeStamp = lu;
    }

    // Fisher-Yates shuffle driven by mRandom (loop @0x826C5B10).
    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        const u16 luSwap             = static_cast<u16>(mRandom.RandomUInt(0, KU_SIZE));
        const u16 luTemp             = maHistory[lu].mTimeStamp;
        maHistory[lu].mTimeStamp     = maHistory[luSwap].mTimeStamp;
        maHistory[luSwap].mTimeStamp = luTemp;
    }

    mCurrentTimeStamp = KU_SIZE;
}

// @ 0x826DC9C8 -- default ctor: clear the running timestamp and the whole history table, then
// default-prime the embedded Random (X360 inlines Random::Construct) and reshuffle via Randomize
// with the fixed 0xDEADF00D seed literal. CONFIDENCE medium (shares the inlined-PRNG caveat).
SelectionHistory<512u, u16, u16, 65536ull>::SelectionHistory()
{
    mCurrentTimeStamp = 0;

    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        maHistory[lu].mTimeStamp = 0;
    }

    mRandom.Construct();
    Randomize(0xDEADF00Du);
}

// @ 0x826C6800 -- stamp maHistory[lSelection] with the current monotonic timestamp and advance
// it; re-randomise when the timestamp is about to wrap (mCurrentTimeStamp == 0xFFFE). The asm's
// CgsDev::Assert trio collapses to one CGS_ASSERT (path + line-number args dropped).
void SelectionHistory<512u, u16, u16, 65536ull>::Update(u16 lSelection)
{
    CGS_ASSERT(lSelection < KU_SIZE, "lSelection < KU_SIZE");

    if (mCurrentTimeStamp == 0xFFFE)
    {
        Randomize(0xDEADF00Du);
    }

    maHistory[lSelection].mTimeStamp = mCurrentTimeStamp;
    ++mCurrentTimeStamp;
}

// @ 0x82702840 -- FindRandomOldest<unsigned short, 32>. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Builds a FindResult scratch table (tuNOldest entries, all seeded
// to the { 0xFFFF, KU_SIZE } "never-selected" sentinel @0x827028A0), overwrites the first
// luNumOfItems with { maHistory[item].mTimeStamp, item }, sorts those ascending by timestamp
// (std::_Sort @0x82702948, predicate LessThanTimeStamp), then draws a random pick from the
// oldest (luNumOfItems/2 + 1) half and returns that entry's mIndex.
//
//   luMod = (luNumOfItems >> 1) + 1               ; asm @0x8270294C..0x82702950
//   pick  = mRandom.RandomUInt(0, luMod)          ; the inlined LCG draw @0x82702980..0x827029BC
//   return laResults[pick].mIndex                 ; lhzx r3 @0x827029C0
//
// The asm inlines the bounded LCG draw directly against mRandom.muSeed (ld/mulld/std @+0x30,
// multiplier 0x5851F42D4C957F2D, +1); the `twllei r31,0` @0x827029A8 is RandomUInt's internal
// "luMod > 0" guard (CgsRandom.h:303). Reconstructed here through the public RandomUInt(0,luMod)
// API rather than re-deriving the LCG by raw offset, matching how Randomize() above is homed.
// CONFIDENCE low: the exact bounded-draw mapping ((oldSeed>>32) % luMod vs RandomUInt) is
// reconstructed by intent, sharing the inlined-PRNG caveat flagged on Randomize().
template <u32 tuSize, typename StoredType, typename TimeStampType, u64 tuModulo>
template <typename LookupType, u32 tuNOldest>
StoredType SelectionHistory<tuSize, StoredType, TimeStampType, tuModulo>::FindRandomOldest(
    const LookupType* lpaItems, u16 luNumOfItems)
{
    CGS_ASSERT(luNumOfItems < tuNOldest, "luNumOfItems < NOldest");

    // Seed every scratch slot with the max-timestamp / out-of-range sentinel (loop @0x82702898).
    FindResult laResults[tuNOldest];
    for (u32 lu = 0; lu < tuNOldest; ++lu)
    {
        laResults[lu].mTimeStamp = static_cast<TimeStampType>(0xFFFFu);  // sth 0xFFFF
        laResults[lu].mIndex     = static_cast<StoredType>(KU_SIZE);     // sth 0x200 (512)
    }

    // Fill the first luNumOfItems entries from the candidate selections (loop @0x827028C4).
    for (u16 lu = 0; lu < luNumOfItems; ++lu)
    {
        CGS_ASSERT(lpaItems[lu] < KU_SIZE, "lpaItems[ i ] < KU_SIZE");
        laResults[lu].mIndex     = static_cast<StoredType>(lpaItems[lu]);
        laResults[lu].mTimeStamp = maHistory[lpaItems[lu]].mTimeStamp;
    }

    // Oldest-first ordering: ascending timestamp (std::_Sort @0x82702948).
    std::sort(laResults, laResults + luNumOfItems, &SelectionHistory::LessThanTimeStamp);

    // Random pick from the oldest (n/2 + 1) half.
    const u32 luMod  = (static_cast<u32>(luNumOfItems) >> 1) + 1u;
    const u32 luPick = mRandom.RandomUInt(0, luMod);
    return laResults[luPick].mIndex;
}

// X360-attested member-template instantiation: FindRandomOldest<unsigned short, 32>.
template u16
SelectionHistory<512u, u16, u16, 65536ull>::FindRandomOldest<u16, 32u>(const u16*, u16);

}
}
