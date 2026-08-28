// =====================================================================================
// rw::audio::core::ReverbModel1 bodies -- the "ReverbModel1" reverb plug-in.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every store/branch. No Feb-2007 leak source, no DecFIGS DWARF, and no
// ProStreet08 rwaudio PDB entry exist for this type. See plugins/ReverbModel1.h for the
// byte-exact layout and the per-function X360 addresses.
//
// The reverb chains the ReverbFilters.h building blocks over DelayLine ring buffers: six
// feedback comb sections plus up to three all-pass sections, reconfigured whenever the
// reverb-time / room-size / damping attributes or the sample rate change (RwacTimerClient
// -> ConfigureModel). ConfigureModel recomputes the section distances / delays / gains and
// re-sizes the delay lines; UpdateLatencyAndDecay derives the reverb latency the mixer must
// pre-roll from the largest comb/all-pass delay and the loop gains (Schroeder RT60 form).
//
// The former blockers on CalculateCombDelays / CalculateG1Values / Process were RESOLVED in
// wave E and all three are now bodied below:
//   * The rodata coefficient tables were recovered from the XEX image (idat big-endian
//     dump; raw dump preserved in the workflow repo, scratchpad/waveE/): the comb delay
//     quantisation table flt_82170638[1652] (all entries PRIME, 2..13999 -- the classic
//     mutually-prime comb-delay trick), the G1 distance knots flt_82170540[9], and the
//     per-rate G1 gain knot rows unk_82170564. They are homed as the named file-local
//     static const arrays below, each citing its X360 address.
//   * CombFilter::CombFilterApplyFunc (@0x82B64D00) turned out to be a thin dispatch onto
//     the EXPORTED comb recurrence kernel rw::audio::core::CombFilterFunc (@0x82B64C20);
//     both are now homed in ReverbFilters.{h,cpp} alongside their AllPassFilter twins, so
//     Process can install the comb sections' IFilter apply/reset pointers exactly as
//     Delay::Process installs the DelayFilter pair. (The separate all-pass reset "STUB"
//     pointer resolves to the image's shared empty thunk 0x82AD5078 -- the ICF-folded
//     empty AllPassFilter reset, homed as AllPassFilter::AllPassFilterResetFunc -- the
//     same folded thunk the ~ReverbModel1 / Delay teardown uses.)
//
// ~ReverbModel1 (Dtor) is now bodied: its leading "STUB" teardown of the embedded TimerHandle
// resolves to the same shared empty thunk 0x82AD5078 (trivial ~TimerHandle -- all POD members,
// nothing to release), exactly as resolved for rw::audio::core::Delay's teardown (W42). Only
// the embedded DelayLines and the (no-op) TimerHandle get destructor calls; the trivial
// CombFilter / AllPassFilter sub-objects have their destructors elided, per the asm.
// =====================================================================================

#include "rw/audio/core/plugins/ReverbModel1.h"
#include "rw/audio/core/Voice.h"   // the owning Voice (mfFadeStart decay accumulator)
#include "rw/audio/core/PlugIn.h"        // rw::audio::core::System (mTimerManager @+0x60)
#include "rw/audio/core/TimerManager.h"  // rw::audio::core::TimerManager::AddTimer
#include "rw/audio/core/IFilter.h"       // rw::audio::core::IFilter (filter apply/reset install)

#include <cmath>   // log10
#include <cstring> // memcpy / memset (the X360 XMemCpy / XMemSet)
#include <new>     // placement new (DelayLine sub-object construction)

namespace rw
{
namespace audio
{
namespace core
{

// The shared rwaudio System singleton (off_83271928). Its object is defined/owned by the
// System TU; ReleaseEvent removes the timer through it.
extern "C" System *off_83271928;

// off_8217F59C -- the ReverbModel1 v-table installed at construction. off_820AA810 -- the
// base PlugIn v-table the destructor reinstalls before any free. off_82F8FA14 -- the
// registered run-time descriptor record (its label is the string "ReverbModel1"). These are
// opaque data symbols in the XEX (no exported contents); modelled as honest placeholder
// storage so the bodies below link without fabricating their contents.
static void *const KRV_ReverbModel1VTable = nullptr; // off_8217F59C
static void *const KRV_BasePlugInVTable   = nullptr; // off_820AA810 (base PlugIn v-table the dtor reinstalls)
// off_82F8FA14 -- the "ReverbModel1" runtime descriptor, REAL (descriptor-record
// wave). Metadata FLAG'd null.
static PlugInDescRunTime KRV_ReverbModel1Desc = {
    "ReverbModel1",
    reinterpret_cast<void *>(&ReverbModel1::GetSize),        // @0x82B9ADA8
    reinterpret_cast<void *>(&ReverbModel1::CreateInstance), // @0x82BA67F8
    0,
    reinterpret_cast<void *>(&ReverbModel1::Process),        // @0x82B9F7D0
    0, 0, 0, 0,
    0,
    0x524D3130u,       // 'RM10'
    4, 0, 3, 0, 0, 0,
    0
};

// The full mixer frame is processed per Process call; the reverb targets a 48 kHz internal
// rate (flt_820AA808) and its section maths use these fixed immediates.
static const f32 KF_ZERO = 0.0f;                 // flt_82001CC0
static const f32 KF_INTERNAL_RATE = 48000.0f;    // flt_820AA808
static const f32 KF_COMB_MIX = 0.16666667f;      // flt_8206C7DC (1/6 comb sum)
static const f32 KF_DECAY_FLOOR = 0.366f;        // flt_8217F380 (G2 decay-time floor)

// -------------------------------------------------------------------------------------
// Recovered rodata (idat big-endian dump from BURNOUT_X360_ARTIST.XEX; raw dump kept in
// the workflow repo, scratchpad/waveE/ReverbModel1.rodata.txt).
// -------------------------------------------------------------------------------------

// flt_8217F374 -- metres -> seconds: 1/344.8 (the neighbouring rodata word 0x82170530
// holds the 344.8 m/s speed-of-sound constant this is the reciprocal of).
static const f32 KF_DIST_TO_SECONDS = 0.0029002321f;
// flt_82109EE4 == 1/48000 -- the delay rate-scale for above-internal-rate output.
static const f32 KF_INV_INTERNAL_RATE = 0.000020833333f;
// flt_82013F90 -- the damping floor bias over the largest comb G1.
static const f32 KF_DAMPING_EPSILON = 0.001f;
// flt_82170534 / flt_82170538 / flt_8217053C -- the G1 sample-rate band edges.
static const f32 KF_G1_RATE_LO  = 10000.0f;
static const f32 KF_G1_RATE_MID = 25000.0f;
static const f32 KF_G1_RATE_HI  = 50000.0f;
// flt_8217F378 == 1/15000 (the 10k..25k band span), flt_8217F37C == 1/25000 (25k..50k).
static const f32 KF_G1_INV_LO_SPAN = 0.000066666667f;
static const f32 KF_G1_INV_HI_SPAN = 0.000039999999f;

// flt_82170540[9] -- the comb reflection-distance knots (metres, 0..100 in 12.5 steps)
// the G1 interpolation walks; CalculateCombDistances clamps every distance to <= 100.
static const f32 KF_G1_DISTANCE_KNOTS[9] = {
    0.0f, 12.5f, 25.0f, 37.5f, 50.0f, 62.5f, 75.0f, 87.5f, 100.0f
};

// unk_82170564 -- the G1 gain knot rows, one row per band edge (row0 = 10 kHz,
// row1 = 25 kHz, row2 = 50 kHz), 9 gain knots per distance knot. The on-image row
// stride is 0x48 (18 f32): each of the first two rows carries its 9 gains followed by
// a repeat of the 9 distance knots; the last row carries only its 9 gains (the
// "ReverbModel1" descriptor strings begin at 0x82170618, ending the table). The
// asm indexes rows at +0x48*band and the next row at +0x48 more, so the 18-float
// stride (KI_G1_ROW_STRIDE) is load-bearing and the flat 45-entry image is kept.
enum { KI_G1_ROW_STRIDE = 18 };
static const f32 KF_G1_GAIN_ROWS[45] = {
    // row 0 (10 kHz): gains, then the distance-knot echo
    0.0f, 0.03f, 0.07f, 0.11f, 0.14f, 0.16f, 0.18f, 0.22f, 0.24f,
    0.0f, 12.5f, 25.0f, 37.5f, 50.0f, 62.5f, 75.0f, 87.5f, 100.0f,
    // row 1 (25 kHz): gains, then the distance-knot echo
    0.0f, 0.12f, 0.23f, 0.3f, 0.35f, 0.4f, 0.43f, 0.46f, 0.5f,
    0.0f, 12.5f, 25.0f, 37.5f, 50.0f, 62.5f, 75.0f, 87.5f, 100.0f,
    // row 2 (50 kHz): gains only (the table ends here)
    0.0f, 0.31f, 0.45f, 0.53f, 0.57f, 0.61f, 0.64f, 0.67f, 0.7f
};

// flt_82170638[1652] -- the comb delay quantisation table: the primes 2..13999, in
// order (mutually-prime comb delays avoid coincident reflections). CalculateCombDelays
// scans it monotonically (the index persists across the six ascending-distance
// sections) and rounds each computed delay UP to the next prime strictly greater than
// it. The table ends at 0x82172008 (the "ATTRIBUTE_SETREVERBTIME" string), observed in
// the image; the asm's bound check is the matching index bound 1652 (0x674).
enum { KI_COMB_DELAY_TABLE_COUNT = 1652 };
static const f32 KF_COMB_DELAY_TABLE[KI_COMB_DELAY_TABLE_COUNT] = {
    2.0f, 3.0f, 5.0f, 7.0f, 11.0f, 13.0f, 17.0f, 19.0f, 23.0f, 29.0f,
    31.0f, 37.0f, 41.0f, 43.0f, 47.0f, 53.0f, 59.0f, 61.0f, 67.0f, 71.0f,
    73.0f, 79.0f, 83.0f, 89.0f, 97.0f, 101.0f, 103.0f, 107.0f, 109.0f, 113.0f,
    127.0f, 131.0f, 137.0f, 139.0f, 149.0f, 151.0f, 157.0f, 163.0f, 167.0f, 173.0f,
    179.0f, 181.0f, 191.0f, 193.0f, 197.0f, 199.0f, 211.0f, 223.0f, 227.0f, 229.0f,
    233.0f, 239.0f, 241.0f, 251.0f, 257.0f, 263.0f, 269.0f, 271.0f, 277.0f, 281.0f,
    283.0f, 293.0f, 307.0f, 311.0f, 313.0f, 317.0f, 331.0f, 337.0f, 347.0f, 349.0f,
    353.0f, 359.0f, 367.0f, 373.0f, 379.0f, 383.0f, 389.0f, 397.0f, 401.0f, 409.0f,
    419.0f, 421.0f, 431.0f, 433.0f, 439.0f, 443.0f, 449.0f, 457.0f, 461.0f, 463.0f,
    467.0f, 479.0f, 487.0f, 491.0f, 499.0f, 503.0f, 509.0f, 521.0f, 523.0f, 541.0f,
    547.0f, 557.0f, 563.0f, 569.0f, 571.0f, 577.0f, 587.0f, 593.0f, 599.0f, 601.0f,
    607.0f, 613.0f, 617.0f, 619.0f, 631.0f, 641.0f, 643.0f, 647.0f, 653.0f, 659.0f,
    661.0f, 673.0f, 677.0f, 683.0f, 691.0f, 701.0f, 709.0f, 719.0f, 727.0f, 733.0f,
    739.0f, 743.0f, 751.0f, 757.0f, 761.0f, 769.0f, 773.0f, 787.0f, 797.0f, 809.0f,
    811.0f, 821.0f, 823.0f, 827.0f, 829.0f, 839.0f, 853.0f, 857.0f, 859.0f, 863.0f,
    877.0f, 881.0f, 883.0f, 887.0f, 907.0f, 911.0f, 919.0f, 929.0f, 937.0f, 941.0f,
    947.0f, 953.0f, 967.0f, 971.0f, 977.0f, 983.0f, 991.0f, 997.0f, 1009.0f, 1013.0f,
    1019.0f, 1021.0f, 1031.0f, 1033.0f, 1039.0f, 1049.0f, 1051.0f, 1061.0f, 1063.0f, 1069.0f,
    1087.0f, 1091.0f, 1093.0f, 1097.0f, 1103.0f, 1109.0f, 1117.0f, 1123.0f, 1129.0f, 1151.0f,
    1153.0f, 1163.0f, 1171.0f, 1181.0f, 1187.0f, 1193.0f, 1201.0f, 1213.0f, 1217.0f, 1223.0f,
    1229.0f, 1231.0f, 1237.0f, 1249.0f, 1259.0f, 1277.0f, 1279.0f, 1283.0f, 1289.0f, 1291.0f,
    1297.0f, 1301.0f, 1303.0f, 1307.0f, 1319.0f, 1321.0f, 1327.0f, 1361.0f, 1367.0f, 1373.0f,
    1381.0f, 1399.0f, 1409.0f, 1423.0f, 1427.0f, 1429.0f, 1433.0f, 1439.0f, 1447.0f, 1451.0f,
    1453.0f, 1459.0f, 1471.0f, 1481.0f, 1483.0f, 1487.0f, 1489.0f, 1493.0f, 1499.0f, 1511.0f,
    1523.0f, 1531.0f, 1543.0f, 1549.0f, 1553.0f, 1559.0f, 1567.0f, 1571.0f, 1579.0f, 1583.0f,
    1597.0f, 1601.0f, 1607.0f, 1609.0f, 1613.0f, 1619.0f, 1621.0f, 1627.0f, 1637.0f, 1657.0f,
    1663.0f, 1667.0f, 1669.0f, 1693.0f, 1697.0f, 1699.0f, 1709.0f, 1721.0f, 1723.0f, 1733.0f,
    1741.0f, 1747.0f, 1753.0f, 1759.0f, 1777.0f, 1783.0f, 1787.0f, 1789.0f, 1801.0f, 1811.0f,
    1823.0f, 1831.0f, 1847.0f, 1861.0f, 1867.0f, 1871.0f, 1873.0f, 1877.0f, 1879.0f, 1889.0f,
    1901.0f, 1907.0f, 1913.0f, 1931.0f, 1933.0f, 1949.0f, 1951.0f, 1973.0f, 1979.0f, 1987.0f,
    1993.0f, 1997.0f, 1999.0f, 2003.0f, 2011.0f, 2017.0f, 2027.0f, 2029.0f, 2039.0f, 2053.0f,
    2063.0f, 2069.0f, 2081.0f, 2083.0f, 2087.0f, 2089.0f, 2099.0f, 2111.0f, 2113.0f, 2129.0f,
    2131.0f, 2137.0f, 2141.0f, 2143.0f, 2153.0f, 2161.0f, 2179.0f, 2203.0f, 2207.0f, 2213.0f,
    2221.0f, 2237.0f, 2239.0f, 2243.0f, 2251.0f, 2267.0f, 2269.0f, 2273.0f, 2281.0f, 2287.0f,
    2293.0f, 2297.0f, 2309.0f, 2311.0f, 2333.0f, 2339.0f, 2341.0f, 2347.0f, 2351.0f, 2357.0f,
    2371.0f, 2377.0f, 2381.0f, 2383.0f, 2389.0f, 2393.0f, 2399.0f, 2411.0f, 2417.0f, 2423.0f,
    2437.0f, 2441.0f, 2447.0f, 2459.0f, 2467.0f, 2473.0f, 2477.0f, 2503.0f, 2521.0f, 2531.0f,
    2539.0f, 2543.0f, 2549.0f, 2551.0f, 2557.0f, 2579.0f, 2591.0f, 2593.0f, 2609.0f, 2617.0f,
    2621.0f, 2633.0f, 2647.0f, 2657.0f, 2659.0f, 2663.0f, 2671.0f, 2677.0f, 2683.0f, 2687.0f,
    2689.0f, 2693.0f, 2699.0f, 2707.0f, 2711.0f, 2713.0f, 2719.0f, 2729.0f, 2731.0f, 2741.0f,
    2749.0f, 2753.0f, 2767.0f, 2777.0f, 2789.0f, 2791.0f, 2797.0f, 2801.0f, 2803.0f, 2819.0f,
    2833.0f, 2837.0f, 2843.0f, 2851.0f, 2857.0f, 2861.0f, 2879.0f, 2887.0f, 2897.0f, 2903.0f,
    2909.0f, 2917.0f, 2927.0f, 2939.0f, 2953.0f, 2957.0f, 2963.0f, 2969.0f, 2971.0f, 2999.0f,
    3001.0f, 3011.0f, 3019.0f, 3023.0f, 3037.0f, 3041.0f, 3049.0f, 3061.0f, 3067.0f, 3079.0f,
    3083.0f, 3089.0f, 3109.0f, 3119.0f, 3121.0f, 3137.0f, 3163.0f, 3167.0f, 3169.0f, 3181.0f,
    3187.0f, 3191.0f, 3203.0f, 3209.0f, 3217.0f, 3221.0f, 3229.0f, 3251.0f, 3253.0f, 3257.0f,
    3259.0f, 3271.0f, 3299.0f, 3301.0f, 3307.0f, 3313.0f, 3319.0f, 3323.0f, 3329.0f, 3331.0f,
    3343.0f, 3347.0f, 3359.0f, 3361.0f, 3371.0f, 3373.0f, 3389.0f, 3391.0f, 3407.0f, 3413.0f,
    3433.0f, 3449.0f, 3457.0f, 3461.0f, 3463.0f, 3467.0f, 3469.0f, 3491.0f, 3499.0f, 3511.0f,
    3517.0f, 3527.0f, 3529.0f, 3533.0f, 3539.0f, 3541.0f, 3547.0f, 3557.0f, 3559.0f, 3571.0f,
    3581.0f, 3583.0f, 3593.0f, 3607.0f, 3613.0f, 3617.0f, 3623.0f, 3631.0f, 3637.0f, 3643.0f,
    3659.0f, 3671.0f, 3673.0f, 3677.0f, 3691.0f, 3697.0f, 3701.0f, 3709.0f, 3719.0f, 3727.0f,
    3733.0f, 3739.0f, 3761.0f, 3767.0f, 3769.0f, 3779.0f, 3793.0f, 3797.0f, 3803.0f, 3821.0f,
    3823.0f, 3833.0f, 3847.0f, 3851.0f, 3853.0f, 3863.0f, 3877.0f, 3881.0f, 3889.0f, 3907.0f,
    3911.0f, 3917.0f, 3919.0f, 3923.0f, 3929.0f, 3931.0f, 3943.0f, 3947.0f, 3967.0f, 3989.0f,
    4001.0f, 4003.0f, 4007.0f, 4013.0f, 4019.0f, 4021.0f, 4027.0f, 4049.0f, 4051.0f, 4057.0f,
    4073.0f, 4079.0f, 4091.0f, 4093.0f, 4099.0f, 4111.0f, 4127.0f, 4129.0f, 4133.0f, 4139.0f,
    4153.0f, 4157.0f, 4159.0f, 4177.0f, 4201.0f, 4211.0f, 4217.0f, 4219.0f, 4229.0f, 4231.0f,
    4241.0f, 4243.0f, 4253.0f, 4259.0f, 4261.0f, 4271.0f, 4273.0f, 4283.0f, 4289.0f, 4297.0f,
    4327.0f, 4337.0f, 4339.0f, 4349.0f, 4357.0f, 4363.0f, 4373.0f, 4391.0f, 4397.0f, 4409.0f,
    4421.0f, 4423.0f, 4441.0f, 4447.0f, 4451.0f, 4457.0f, 4463.0f, 4481.0f, 4483.0f, 4493.0f,
    4507.0f, 4513.0f, 4517.0f, 4519.0f, 4523.0f, 4547.0f, 4549.0f, 4561.0f, 4567.0f, 4583.0f,
    4591.0f, 4597.0f, 4603.0f, 4621.0f, 4637.0f, 4639.0f, 4643.0f, 4649.0f, 4651.0f, 4657.0f,
    4663.0f, 4673.0f, 4679.0f, 4691.0f, 4703.0f, 4721.0f, 4723.0f, 4729.0f, 4733.0f, 4751.0f,
    4759.0f, 4783.0f, 4787.0f, 4789.0f, 4793.0f, 4799.0f, 4801.0f, 4813.0f, 4817.0f, 4831.0f,
    4861.0f, 4871.0f, 4877.0f, 4889.0f, 4903.0f, 4909.0f, 4919.0f, 4931.0f, 4933.0f, 4937.0f,
    4943.0f, 4951.0f, 4957.0f, 4967.0f, 4969.0f, 4973.0f, 4987.0f, 4993.0f, 4999.0f, 5003.0f,
    5009.0f, 5011.0f, 5021.0f, 5023.0f, 5039.0f, 5051.0f, 5059.0f, 5077.0f, 5081.0f, 5087.0f,
    5099.0f, 5101.0f, 5107.0f, 5113.0f, 5119.0f, 5147.0f, 5153.0f, 5167.0f, 5171.0f, 5179.0f,
    5189.0f, 5197.0f, 5209.0f, 5227.0f, 5231.0f, 5233.0f, 5237.0f, 5261.0f, 5273.0f, 5279.0f,
    5281.0f, 5297.0f, 5303.0f, 5309.0f, 5323.0f, 5333.0f, 5347.0f, 5351.0f, 5381.0f, 5387.0f,
    5393.0f, 5399.0f, 5407.0f, 5413.0f, 5417.0f, 5419.0f, 5431.0f, 5437.0f, 5441.0f, 5443.0f,
    5449.0f, 5471.0f, 5477.0f, 5479.0f, 5483.0f, 5501.0f, 5503.0f, 5507.0f, 5519.0f, 5521.0f,
    5527.0f, 5531.0f, 5557.0f, 5563.0f, 5569.0f, 5573.0f, 5581.0f, 5591.0f, 5623.0f, 5639.0f,
    5641.0f, 5647.0f, 5651.0f, 5653.0f, 5657.0f, 5659.0f, 5669.0f, 5683.0f, 5689.0f, 5693.0f,
    5701.0f, 5711.0f, 5717.0f, 5737.0f, 5741.0f, 5743.0f, 5749.0f, 5779.0f, 5783.0f, 5791.0f,
    5801.0f, 5807.0f, 5813.0f, 5821.0f, 5827.0f, 5839.0f, 5843.0f, 5849.0f, 5851.0f, 5857.0f,
    5861.0f, 5867.0f, 5869.0f, 5879.0f, 5881.0f, 5897.0f, 5903.0f, 5923.0f, 5927.0f, 5939.0f,
    5953.0f, 5981.0f, 5987.0f, 6007.0f, 6011.0f, 6029.0f, 6037.0f, 6043.0f, 6047.0f, 6053.0f,
    6067.0f, 6073.0f, 6079.0f, 6089.0f, 6091.0f, 6101.0f, 6113.0f, 6121.0f, 6131.0f, 6133.0f,
    6143.0f, 6151.0f, 6163.0f, 6173.0f, 6197.0f, 6199.0f, 6203.0f, 6211.0f, 6217.0f, 6221.0f,
    6229.0f, 6247.0f, 6257.0f, 6263.0f, 6269.0f, 6271.0f, 6277.0f, 6287.0f, 6299.0f, 6301.0f,
    6311.0f, 6317.0f, 6323.0f, 6329.0f, 6337.0f, 6343.0f, 6353.0f, 6359.0f, 6361.0f, 6367.0f,
    6373.0f, 6379.0f, 6389.0f, 6397.0f, 6421.0f, 6427.0f, 6449.0f, 6451.0f, 6469.0f, 6473.0f,
    6481.0f, 6491.0f, 6521.0f, 6529.0f, 6547.0f, 6551.0f, 6553.0f, 6563.0f, 6569.0f, 6571.0f,
    6577.0f, 6581.0f, 6599.0f, 6607.0f, 6619.0f, 6637.0f, 6653.0f, 6659.0f, 6661.0f, 6673.0f,
    6679.0f, 6689.0f, 6691.0f, 6701.0f, 6703.0f, 6709.0f, 6719.0f, 6733.0f, 6737.0f, 6761.0f,
    6763.0f, 6779.0f, 6781.0f, 6791.0f, 6793.0f, 6803.0f, 6823.0f, 6827.0f, 6829.0f, 6833.0f,
    6841.0f, 6857.0f, 6863.0f, 6869.0f, 6871.0f, 6883.0f, 6899.0f, 6907.0f, 6911.0f, 6917.0f,
    6947.0f, 6949.0f, 6959.0f, 6961.0f, 6967.0f, 6971.0f, 6977.0f, 6983.0f, 6991.0f, 6997.0f,
    7001.0f, 7013.0f, 7019.0f, 7027.0f, 7039.0f, 7043.0f, 7057.0f, 7069.0f, 7079.0f, 7103.0f,
    7109.0f, 7121.0f, 7127.0f, 7129.0f, 7151.0f, 7159.0f, 7177.0f, 7187.0f, 7193.0f, 7207.0f,
    7211.0f, 7213.0f, 7219.0f, 7229.0f, 7237.0f, 7243.0f, 7247.0f, 7253.0f, 7283.0f, 7297.0f,
    7307.0f, 7309.0f, 7321.0f, 7331.0f, 7333.0f, 7349.0f, 7351.0f, 7369.0f, 7393.0f, 7411.0f,
    7417.0f, 7433.0f, 7451.0f, 7457.0f, 7459.0f, 7477.0f, 7481.0f, 7487.0f, 7489.0f, 7499.0f,
    7507.0f, 7517.0f, 7523.0f, 7529.0f, 7537.0f, 7541.0f, 7547.0f, 7549.0f, 7559.0f, 7561.0f,
    7573.0f, 7577.0f, 7583.0f, 7589.0f, 7591.0f, 7603.0f, 7607.0f, 7621.0f, 7639.0f, 7643.0f,
    7649.0f, 7669.0f, 7673.0f, 7681.0f, 7687.0f, 7691.0f, 7699.0f, 7703.0f, 7717.0f, 7723.0f,
    7727.0f, 7741.0f, 7753.0f, 7757.0f, 7759.0f, 7789.0f, 7793.0f, 7817.0f, 7823.0f, 7829.0f,
    7841.0f, 7853.0f, 7867.0f, 7873.0f, 7877.0f, 7879.0f, 7883.0f, 7901.0f, 7907.0f, 7919.0f,
    7927.0f, 7933.0f, 7937.0f, 7949.0f, 7951.0f, 7963.0f, 7993.0f, 8009.0f, 8011.0f, 8017.0f,
    8039.0f, 8053.0f, 8059.0f, 8069.0f, 8081.0f, 8087.0f, 8089.0f, 8093.0f, 8101.0f, 8111.0f,
    8117.0f, 8123.0f, 8147.0f, 8161.0f, 8167.0f, 8171.0f, 8179.0f, 8191.0f, 8209.0f, 8219.0f,
    8221.0f, 8231.0f, 8233.0f, 8237.0f, 8243.0f, 8263.0f, 8269.0f, 8273.0f, 8287.0f, 8291.0f,
    8293.0f, 8297.0f, 8311.0f, 8317.0f, 8329.0f, 8353.0f, 8363.0f, 8369.0f, 8377.0f, 8387.0f,
    8389.0f, 8419.0f, 8423.0f, 8429.0f, 8431.0f, 8443.0f, 8447.0f, 8461.0f, 8467.0f, 8501.0f,
    8513.0f, 8521.0f, 8527.0f, 8537.0f, 8539.0f, 8543.0f, 8563.0f, 8573.0f, 8581.0f, 8597.0f,
    8599.0f, 8609.0f, 8623.0f, 8627.0f, 8629.0f, 8641.0f, 8647.0f, 8663.0f, 8669.0f, 8677.0f,
    8681.0f, 8689.0f, 8693.0f, 8699.0f, 8707.0f, 8713.0f, 8719.0f, 8731.0f, 8737.0f, 8741.0f,
    8747.0f, 8753.0f, 8761.0f, 8779.0f, 8783.0f, 8803.0f, 8807.0f, 8819.0f, 8821.0f, 8831.0f,
    8837.0f, 8839.0f, 8849.0f, 8861.0f, 8863.0f, 8867.0f, 8887.0f, 8893.0f, 8923.0f, 8929.0f,
    8933.0f, 8941.0f, 8951.0f, 8963.0f, 8969.0f, 8971.0f, 8999.0f, 9001.0f, 9007.0f, 9011.0f,
    9013.0f, 9029.0f, 9041.0f, 9043.0f, 9049.0f, 9059.0f, 9067.0f, 9091.0f, 9103.0f, 9109.0f,
    9127.0f, 9133.0f, 9137.0f, 9151.0f, 9157.0f, 9161.0f, 9173.0f, 9181.0f, 9187.0f, 9199.0f,
    9203.0f, 9209.0f, 9221.0f, 9227.0f, 9239.0f, 9241.0f, 9257.0f, 9277.0f, 9281.0f, 9283.0f,
    9293.0f, 9311.0f, 9319.0f, 9323.0f, 9337.0f, 9341.0f, 9343.0f, 9349.0f, 9371.0f, 9377.0f,
    9391.0f, 9397.0f, 9403.0f, 9413.0f, 9419.0f, 9421.0f, 9431.0f, 9433.0f, 9437.0f, 9439.0f,
    9461.0f, 9463.0f, 9467.0f, 9473.0f, 9479.0f, 9491.0f, 9497.0f, 9511.0f, 9521.0f, 9533.0f,
    9539.0f, 9547.0f, 9551.0f, 9587.0f, 9601.0f, 9613.0f, 9619.0f, 9623.0f, 9629.0f, 9631.0f,
    9643.0f, 9649.0f, 9661.0f, 9677.0f, 9679.0f, 9689.0f, 9697.0f, 9719.0f, 9721.0f, 9733.0f,
    9739.0f, 9743.0f, 9749.0f, 9767.0f, 9769.0f, 9781.0f, 9787.0f, 9791.0f, 9803.0f, 9811.0f,
    9817.0f, 9829.0f, 9833.0f, 9839.0f, 9851.0f, 9857.0f, 9859.0f, 9871.0f, 9883.0f, 9887.0f,
    9901.0f, 9907.0f, 9923.0f, 9929.0f, 9931.0f, 9941.0f, 9949.0f, 9967.0f, 9973.0f, 10007.0f,
    10009.0f, 10037.0f, 10039.0f, 10061.0f, 10067.0f, 10069.0f, 10079.0f, 10091.0f, 10093.0f, 10099.0f,
    10103.0f, 10111.0f, 10133.0f, 10139.0f, 10141.0f, 10151.0f, 10159.0f, 10163.0f, 10169.0f, 10177.0f,
    10181.0f, 10193.0f, 10211.0f, 10223.0f, 10243.0f, 10247.0f, 10253.0f, 10259.0f, 10267.0f, 10271.0f,
    10273.0f, 10289.0f, 10301.0f, 10303.0f, 10313.0f, 10321.0f, 10331.0f, 10333.0f, 10337.0f, 10343.0f,
    10357.0f, 10369.0f, 10391.0f, 10399.0f, 10427.0f, 10429.0f, 10433.0f, 10453.0f, 10457.0f, 10459.0f,
    10463.0f, 10477.0f, 10487.0f, 10499.0f, 10501.0f, 10513.0f, 10529.0f, 10531.0f, 10559.0f, 10567.0f,
    10589.0f, 10597.0f, 10601.0f, 10607.0f, 10613.0f, 10627.0f, 10631.0f, 10639.0f, 10651.0f, 10657.0f,
    10663.0f, 10667.0f, 10687.0f, 10691.0f, 10709.0f, 10711.0f, 10723.0f, 10729.0f, 10733.0f, 10739.0f,
    10753.0f, 10771.0f, 10781.0f, 10789.0f, 10799.0f, 10831.0f, 10837.0f, 10847.0f, 10853.0f, 10859.0f,
    10861.0f, 10867.0f, 10883.0f, 10889.0f, 10891.0f, 10903.0f, 10909.0f, 10937.0f, 10939.0f, 10949.0f,
    10957.0f, 10973.0f, 10979.0f, 10987.0f, 10993.0f, 11003.0f, 11027.0f, 11047.0f, 11057.0f, 11059.0f,
    11069.0f, 11071.0f, 11083.0f, 11087.0f, 11093.0f, 11113.0f, 11117.0f, 11119.0f, 11131.0f, 11149.0f,
    11159.0f, 11161.0f, 11171.0f, 11173.0f, 11177.0f, 11197.0f, 11213.0f, 11239.0f, 11243.0f, 11251.0f,
    11257.0f, 11261.0f, 11273.0f, 11279.0f, 11287.0f, 11299.0f, 11311.0f, 11317.0f, 11321.0f, 11329.0f,
    11351.0f, 11353.0f, 11369.0f, 11383.0f, 11393.0f, 11399.0f, 11411.0f, 11423.0f, 11437.0f, 11443.0f,
    11447.0f, 11467.0f, 11471.0f, 11483.0f, 11489.0f, 11491.0f, 11497.0f, 11503.0f, 11519.0f, 11527.0f,
    11549.0f, 11551.0f, 11579.0f, 11587.0f, 11593.0f, 11597.0f, 11617.0f, 11621.0f, 11633.0f, 11657.0f,
    11677.0f, 11681.0f, 11689.0f, 11699.0f, 11701.0f, 11717.0f, 11719.0f, 11731.0f, 11743.0f, 11777.0f,
    11779.0f, 11783.0f, 11789.0f, 11801.0f, 11807.0f, 11813.0f, 11821.0f, 11827.0f, 11831.0f, 11833.0f,
    11839.0f, 11863.0f, 11867.0f, 11887.0f, 11897.0f, 11903.0f, 11909.0f, 11923.0f, 11927.0f, 11933.0f,
    11939.0f, 11941.0f, 11953.0f, 11959.0f, 11969.0f, 11971.0f, 11981.0f, 11987.0f, 12007.0f, 12011.0f,
    12037.0f, 12041.0f, 12043.0f, 12049.0f, 12071.0f, 12073.0f, 12097.0f, 12101.0f, 12107.0f, 12109.0f,
    12113.0f, 12119.0f, 12143.0f, 12149.0f, 12157.0f, 12161.0f, 12163.0f, 12197.0f, 12203.0f, 12211.0f,
    12227.0f, 12239.0f, 12241.0f, 12251.0f, 12253.0f, 12263.0f, 12269.0f, 12277.0f, 12281.0f, 12289.0f,
    12301.0f, 12323.0f, 12329.0f, 12343.0f, 12347.0f, 12373.0f, 12377.0f, 12379.0f, 12391.0f, 12401.0f,
    12409.0f, 12413.0f, 12421.0f, 12433.0f, 12437.0f, 12451.0f, 12457.0f, 12473.0f, 12479.0f, 12487.0f,
    12491.0f, 12497.0f, 12503.0f, 12511.0f, 12517.0f, 12527.0f, 12539.0f, 12541.0f, 12547.0f, 12553.0f,
    12569.0f, 12577.0f, 12583.0f, 12589.0f, 12601.0f, 12611.0f, 12613.0f, 12619.0f, 12637.0f, 12641.0f,
    12647.0f, 12653.0f, 12659.0f, 12671.0f, 12689.0f, 12697.0f, 12703.0f, 12713.0f, 12721.0f, 12739.0f,
    12743.0f, 12757.0f, 12763.0f, 12781.0f, 12791.0f, 12799.0f, 12809.0f, 12821.0f, 12823.0f, 12829.0f,
    12841.0f, 12853.0f, 12889.0f, 12893.0f, 12899.0f, 12907.0f, 12911.0f, 12917.0f, 12919.0f, 12923.0f,
    12941.0f, 12953.0f, 12959.0f, 12967.0f, 12973.0f, 12979.0f, 12983.0f, 13001.0f, 13003.0f, 13007.0f,
    13009.0f, 13033.0f, 13037.0f, 13043.0f, 13049.0f, 13063.0f, 13093.0f, 13099.0f, 13103.0f, 13109.0f,
    13121.0f, 13127.0f, 13147.0f, 13151.0f, 13159.0f, 13163.0f, 13171.0f, 13177.0f, 13183.0f, 13187.0f,
    13217.0f, 13219.0f, 13229.0f, 13241.0f, 13249.0f, 13259.0f, 13267.0f, 13291.0f, 13297.0f, 13309.0f,
    13313.0f, 13327.0f, 13331.0f, 13337.0f, 13339.0f, 13367.0f, 13381.0f, 13397.0f, 13399.0f, 13411.0f,
    13417.0f, 13421.0f, 13441.0f, 13451.0f, 13457.0f, 13463.0f, 13469.0f, 13477.0f, 13487.0f, 13499.0f,
    13513.0f, 13523.0f, 13537.0f, 13553.0f, 13567.0f, 13577.0f, 13591.0f, 13597.0f, 13613.0f, 13619.0f,
    13627.0f, 13633.0f, 13649.0f, 13669.0f, 13679.0f, 13681.0f, 13687.0f, 13691.0f, 13693.0f, 13697.0f,
    13709.0f, 13711.0f, 13721.0f, 13723.0f, 13729.0f, 13751.0f, 13757.0f, 13759.0f, 13763.0f, 13781.0f,
    13789.0f, 13799.0f, 13807.0f, 13829.0f, 13831.0f, 13841.0f, 13859.0f, 13873.0f, 13877.0f, 13879.0f,
    13883.0f, 13901.0f, 13903.0f, 13907.0f, 13913.0f, 13921.0f, 13931.0f, 13933.0f, 13963.0f, 13967.0f,
    13997.0f, 13999.0f
};

// KI_REVERB_SCRATCH_SAMPLES -- Process reserves 768 samples (0xC00 bytes) off the System
// object table's scratch cursor (word [3]) as every delay line's shared local buffer.
enum { KI_REVERB_SCRATCH_SAMPLES = 768 };

// -------------------------------------------------------------------------------------
// GetSize @0x82B9ADA8 -- the plug-in instance footprint.
// -------------------------------------------------------------------------------------
int ReverbModel1::GetSize()
{
    return 1088; // li r3, 0x440
}

// -------------------------------------------------------------------------------------
// GetPlugInDescRunTime @0x82B9AD98 -- return the address of the registered descriptor
// record (its label is the string "ReverbModel1").
// -------------------------------------------------------------------------------------
char **ReverbModel1::GetPlugInDescRunTime()
{
    return reinterpret_cast<char **>(&KRV_ReverbModel1Desc); // &off_82F8FA14 (the record address)
}

// -------------------------------------------------------------------------------------
// GetPpuTicksEvent @0x82B9E9D8 -- the CPU cycles the last timer callback took (the timer's
// recorded tick count). lwz r3, 0x14C(r3) == mTimer.mCpuTicks (mTimer @+0x13C, +0x10).
// -------------------------------------------------------------------------------------
u32 ReverbModel1::GetPpuTicksEvent(ReverbModel1 *self)
{
    return self->mTimer.mCpuTicks;
}

// -------------------------------------------------------------------------------------
// ReverbModel1::ReverbModel1 @0x82B9E938 -- install the v-table then default-construct every
// embedded sub-filter / delay-line / timer, in the asm's order: the 3 all-pass sections, the
// 3 all-pass delay lines, the timer, the 6 comb sections, the 6 comb delay lines.
// -------------------------------------------------------------------------------------
ReverbModel1 *ReverbModel1::ReverbModel1_ctor(ReverbModel1 *self)
{
    self->mBase.mpVTable = KRV_ReverbModel1VTable; // stw off_8217F59C @ +0x00

    for (int i = 0; i < 3; ++i)
        AllPassFilter::AllPassFilter_ctor(&self->mAllPass[i]);
    for (int i = 0; i < 3; ++i)
        new (&self->mAllPassDelay[i]) DelayLine();

    TimerHandle::TimerHandle_ctor(&self->mTimer);

    for (int i = 0; i < 6; ++i)
        CombFilter::CombFilter_ctor(&self->mComb[i]);
    for (int i = 0; i < 6; ++i)
        new (&self->mCombDelay[i]) DelayLine();

    return self;
}

// -------------------------------------------------------------------------------------
// ~ReverbModel1 @0x82B9E9E0 -- tear the reverb network down, in the asm's order:
//   1. destruct the six comb delay lines in reverse (mCombDelay[5]..[0]);
//   2. ~TimerHandle(&mTimer) -- trivial (POD members), ICF-folded onto the image's shared
//      empty thunk 0x82AD5078 (a lone `blr`), so the call is reproduced but does nothing;
//   3. destruct the three all-pass delay lines in reverse (mAllPassDelay[2]..[0]);
//   4. reinstall the base PlugIn v-table (off_820AA810) at +0x00.
// The trivial CombFilter / AllPassFilter sub-objects own nothing, so the compiler elided
// their destructor calls entirely -- none is made here, matching the asm. The X360 return is
// the last ~DelayLine call's register value, which the sole caller (the vector deleting
// destructor) discards; `self` is returned for a clean, well-typed result.
// -------------------------------------------------------------------------------------
ReverbModel1 *ReverbModel1::Dtor(ReverbModel1 *self)
{
    for (int i = 5; i >= 0; --i)
        self->mCombDelay[i].~DelayLine();      // bl ~DelayLine (r30: self+0x3E0 down by 0x3C)

    self->mTimer.~TimerHandle();               // bl STUB (0x82AD5078) -- trivial/no-op ~TimerHandle

    for (int i = 2; i >= 0; --i)
        self->mAllPassDelay[i].~DelayLine();   // bl ~DelayLine (r30: self+0x100 down by 0x3C)

    self->mBase.mpVTable = KRV_BasePlugInVTable; // stw off_820AA810 @ +0x00

    return self;
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82B9EA48 -- run the destructor, then conditionally free.
//   Dtor(self); if (flags & 1) operator delete(self); return self;
// -------------------------------------------------------------------------------------
void *ReverbModel1::VectorDeletingDestructor(ReverbModel1 *self, char flags)
{
    Dtor(self);
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

// -------------------------------------------------------------------------------------
// CalculateCombDistances @0x82B9AE30 -- map the room-size attribute (clamped to [2, 83.3])
// to the six comb reflection distances. The nearest wall is 0.8*room; the farthest is
// 1.5*that (capped at 100, which pins the room to its max and the near tap to 66.67), and
// the middle four taps are linearly spaced across the [near, far] span in 0.2 steps.
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateCombDistances(ReverbModel1 *self, f32 *pRoomSize,
                                         f32 *pCombDistances)
{
    (void)self; // `this` is passed (r3) but unused by the body

    if (*pRoomSize > 83.300003f)
        *pRoomSize = 83.300003f;
    else if (*pRoomSize < 2.0f)
        *pRoomSize = 2.0f;

    f32 nearDist = *pRoomSize * 0.80000001f;         // v5
    f32 farDist  = (*pRoomSize * 0.80000001f) * 1.5f; // v6
    if (farDist > 100.0f)
    {
        *pRoomSize = 83.333328f;
        farDist  = 100.0f;
        nearDist = 66.666664f;
    }

    pCombDistances[5] = farDist;
    pCombDistances[0] = nearDist;

    const f32 step = (farDist - nearDist) * 0.2f;    // v8
    const f32 d1 = step + nearDist;                  // v9
    pCombDistances[1] = d1;
    pCombDistances[2] = d1 + step;
    const f32 d3 = (d1 + step) + step;           // v10
    pCombDistances[3] = d3;
    pCombDistances[4] = d3 + step;
    return 1;
}

// -------------------------------------------------------------------------------------
// CalculateCombDelays @0x82B9AEE0 -- quantise the six comb reflection distances into
// integer delay lengths.
//
// Each distance is turned into a delay in samples (metres -> seconds -> samples) and then
// rounded UP to the first entry of the prime table strictly greater than it. The table
// index PERSISTS across the six sections (the distances are ascending, so the scan sweeps
// the table once and every section lands on a DIFFERENT prime -- the mutually-prime comb
// delays that keep the reflections from coinciding). Above the 48 kHz internal rate the
// delays are computed at 48 kHz and then scaled by rate/48000.
//
// `self` is passed (r3) but unused by the body. Only pCombDelays[5] is pre-zeroed; when the
// scan runs off the end of the table the section keeps whatever it already held (faithful --
// the asm simply skips the store).
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateCombDelays(ReverbModel1 *self, f64 sampleRate,
                                      const f32 *pCombDistances, s32 *pCombDelays)
{
    (void)self; // `this` is passed (r3) but unused by the body

    pCombDelays[5] = 0; // stw r11(0), 0x14(r5) at entry -- the only pre-zeroed section

    s32 tableIndex = 0; // r11 -- persists across the six sections
    for (int i = 0; i < 6; ++i)
    {
        // Above 48 kHz the delay is computed at the internal rate and scaled afterwards.
        f64 rate;      // f0
        f32 rateScale; // f13
        if (sampleRate > KF_INTERNAL_RATE)
        {
            rate = KF_INTERNAL_RATE;
            rateScale = static_cast<f32>(sampleRate * KF_INV_INTERNAL_RATE);
        }
        else
        {
            rate = sampleRate;
            rateScale = 1.0f;
        }

        const f32 delay = static_cast<f32>(rate * (pCombDistances[i] * KF_DIST_TO_SECONDS));

        if (tableIndex < KI_COMB_DELAY_TABLE_COUNT)
        {
            // Walk to the first prime strictly greater than the computed delay; the bound is
            // re-tested after every step, and an exhausted table skips the store.
            while (tableIndex < KI_COMB_DELAY_TABLE_COUNT
                   && KF_COMB_DELAY_TABLE[tableIndex] <= delay)
            {
                ++tableIndex;
            }
            if (tableIndex < KI_COMB_DELAY_TABLE_COUNT)
                pCombDelays[i] = static_cast<s32>(KF_COMB_DELAY_TABLE[tableIndex++]); // fctiwz
        }

        if (rateScale > 1.0f)
            pCombDelays[i] = static_cast<s32>(static_cast<f32>(pCombDelays[i]) * rateScale);
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// CalculateG1Values @0x82B9AFD0 -- the six comb decay gains.
//
// The gain knot rows are tabulated per sample-rate band (10 k / 25 k / 50 kHz); the rate is
// clamped into its band and the two bracketing rows are blended into a nine-entry gain
// frame. Each comb distance is then located between two distance knots and its gain read by
// linear interpolation across that span.
//
// The blended frame is built as ONE contiguous 18-float stack frame -- the nine distance
// knots followed by the nine blended gains -- and the asm's knot scan is UNGUARDED (it walks
// forward until a knot is >= the distance and would read into the gain half if it ever ran
// off the end). CalculateCombDistances clamps every distance to <= 100 == the last knot, so
// the scan always stops inside the knot half; the adjacency is reproduced (a single array),
// and NO bound check is added, exactly as the asm has none.
//
// Finally, if the damping attribute has moved since the last configure, it is floored just
// above the largest comb gain and every comb section is reset / re-sized / re-primed, its
// gain divided down by the (possibly just-floored) damping.
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateG1Values(ReverbModel1 *self, f32 *pCombG1, f64 sampleRate)
{
    // ---- band select: clamp the rate into its band and derive the blend fraction --------
    f64 clamped; // f0 -- the rate clamped into the band being interpolated over
    s32 band;    // r31 -- the lower gain row of the band
    f32 edge;    // f12 / f13 -- the band's upper rate knot
    f32 invSpan; // f0 -- the reciprocal of the band's rate span
    if (sampleRate >= KF_G1_RATE_HI)
    {
        clamped = KF_G1_RATE_HI;
        band    = 1;
        edge    = KF_G1_RATE_HI;
        invSpan = KF_G1_INV_HI_SPAN;
    }
    else if (sampleRate > KF_G1_RATE_MID)
    {
        clamped = sampleRate;
        band    = 1;
        edge    = KF_G1_RATE_HI;
        invSpan = KF_G1_INV_HI_SPAN;
    }
    else
    {
        clamped = (sampleRate > KF_G1_RATE_LO) ? sampleRate : static_cast<f64>(KF_G1_RATE_LO);
        band    = 0;
        edge    = KF_G1_RATE_MID;
        invSpan = KF_G1_INV_LO_SPAN;
    }
    const f32 frac = static_cast<f32>(edge - clamped) * invSpan; // f31 -- weight of the LOW row
    const f32 w    = 1.0f - frac;                                // f29 -- weight of the HIGH row

    // ---- build the interpolation frame: knots [0..8] then blended gains [9..17] ---------
    // The two halves are ONE contiguous frame on the X360 stack (var_A0 .. var_A0+0x44) and
    // that adjacency is what the unguarded scan below relies on -- keep them in one array.
    f32 lTable[18];
    lTable[0] = KF_ZERO;         // stfs flt_82001CC0
    memset(&lTable[1], 0, 0x44); // XMemSet -- the asm's 68-byte clear (every word is
                                 // overwritten by the loop below; reproduced as-is)

    const f32 *pLowRow = &KF_G1_GAIN_ROWS[KI_G1_ROW_STRIDE * band]; // r10
    for (int k = 0; k < 9; ++k)
    {
        lTable[k]     = KF_G1_DISTANCE_KNOTS[k];
        lTable[9 + k] = pLowRow[KI_G1_ROW_STRIDE + k] * w + pLowRow[k] * frac; // fmadds
    }

    // ---- per-section interpolation ------------------------------------------------------
    const f32 invStep = 1.0f / (lTable[1] - lTable[0]); // fdivs (== 1/12.5)
    for (int i = 0; i < 6; ++i)
    {
        const f32 dist = self->mfCombDistances[i];

        // Walk to the first knot at or above the distance (unguarded, as in the asm).
        s32  idx   = 0;    // r11
        bool found = false; // r6
        do
        {
            ++idx;
            if (dist <= lTable[idx])
                found = true;
        } while (!found);

        const f32 t = (lTable[idx] - dist) * invStep;
        pCombG1[i] = lTable[9 + idx] * (1.0f - t) + lTable[8 + idx] * t;
    }

    // ---- damping epilogue ---------------------------------------------------------------
    if (self->mfDamping != self->mfLastDamping)
    {
        // Floor the damping just above the largest comb gain so the loop stays stable.
        const f32 dampingFloor = pCombG1[5] + KF_DAMPING_EPSILON;
        if (self->mfDamping < dampingFloor)
            self->mfDamping = dampingFloor;

        for (int i = 0; i < 6; ++i)
        {
            CombFilter::CombFilterResetFunc(&self->mComb[i]);
            self->mCombDelay[i].Resize(self->miCombDelays[i] + 3); // return ignored (asm)
            self->mCombDelay[i].Reset(self->miCombDelays[i] + 1);
            pCombG1[i] = pCombG1[i] / self->mfDamping; // damping re-read every iteration
        }
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// CalculateG2Values @0x82B9B1F8 -- the six comb damping gains. With the reverb time clamped
// to at least 0.366, each damping gain is (1 - combG1[i]) * (1 - 0.366/reverbTime).
// -------------------------------------------------------------------------------------
int ReverbModel1::CalculateG2Values(ReverbModel1 *self, f32 *pCombG2)
{
    if (self->mfDecayTime < KF_DECAY_FLOOR)
        self->mfDecayTime = KF_DECAY_FLOOR;

    const f32 damp = 1.0f - (KF_DECAY_FLOOR / self->mfDecayTime); // v5
    for (int i = 0; i < 6; ++i)
        pCombG2[i] = (1.0f - self->mfCombG1[i]) * damp;
    return 1;
}

// -------------------------------------------------------------------------------------
// ConfigureModel @0x82BA3A10 -- (re)build the reverb network. Depending on which attribute
// changed since the last configure, recompute the comb distances / delays / gains, re-tune
// and re-size every comb section, then (unless only a live-parameter tweak) rebuild the
// all-pass network from the reverb mode, and finally latch the applied state and refresh the
// latency. Returns 0 if any delay-line resize fails, else 1.
// -------------------------------------------------------------------------------------
int ReverbModel1::ConfigureModel(ReverbModel1 *self, System *pSystem)
{
    (void)pSystem; // passed (r4 = mBase.mpSystem) but unused by the body

    bool recomputedRoom = false; // v2 -- the room size changed (comb geometry rebuilt)
    bool recomputedRate = false; // v3 -- the sample rate changed (delays rebuilt)

    if (self->mfRoomSize != self->mfLastRoomSize)
    {
        CalculateCombDistances(self, &self->mfRoomSize, self->mfCombDistances);
        CalculateCombDelays(self, KF_INTERNAL_RATE, self->mfCombDistances, self->miCombDelays);
        CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
        recomputedRoom = true;
    }
    else if (self->mfLastSampleRate != KF_INTERNAL_RATE)
    {
        CalculateCombDelays(self, KF_INTERNAL_RATE, self->mfCombDistances, self->miCombDelays);
        CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
        recomputedRate = true;
    }
    else
    {
        if (self->mfDamping != self->mfLastDamping)
            CalculateG1Values(self, self->mfCombG1, KF_INTERNAL_RATE);
        CalculateG2Values(self, self->mfCombG2);
    }

    // Re-tune every comb section and, when its target delay changed, re-size + re-prime it.
    for (int i = 0; i < 6; ++i)
    {
        CombFilter::SetGains(&self->mComb[i], -self->mfCombG1[i], -self->mfCombG2[i],
                             -self->mfCombG1[i], KF_COMB_MIX);
        if (self->mCombDelay[i].miReadPosition != self->miCombDelays[i] + 1)
        {
            if (!self->mCombDelay[i].Resize(self->miCombDelays[i] + 3))
                return 0;
            self->mCombDelay[i].Reset(self->miCombDelays[i] + 1);
        }
    }

    if (self->mbAllPassConfigured && !recomputedRate)
    {
        // Network already sized: on a geometry change just re-prime the existing lines.
        if (recomputedRoom)
        {
            for (int i = 0; i < self->mbAllPassCount; ++i)
                self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
        }
    }
    else
    {
        // Size the all-pass network from the reverb mode (1 / 2|4 / other).
        f32 gain0; // v17 -- section-0 gain, shared as the base of the gain array
        const u8 mode = self->mBase.mbChannelCount;
        if (mode == 1)
        {
            self->mbAllPassCount = 1;
            self->miAllPassDelaySamples[0] = 288;
            gain0 = 0.7f;
        }
        else
        {
            u8  count;  // v18
            f32 gain1;  // v19 -- section-1 gain
            if (mode == 2 || mode == 4)
            {
                count = 2;
                gain0 = 0.63f;
                gain1 = 0.77777779f;
                self->miAllPassDelaySamples[1] = 259;
            }
            else
            {
                count = 3;
                gain0 = 0.63f;
                self->miAllPassDelaySamples[1] = 288;
                self->miAllPassDelaySamples[2] = 259;
                gain1 = 0.7f;
                self->mfAllPassGain[2] = 0.77777779f;
            }
            self->mfAllPassGain[1] = gain1;
            self->mbAllPassCount = count;
            self->miAllPassDelaySamples[0] = 320;
        }
        self->mfAllPassGain[0] = gain0;

        for (int i = 0; i < self->mbAllPassCount; ++i)
        {
            AllPassFilter::SetGains(&self->mAllPass[i], self->mfAllPassGain[i],
                                    self->mfAllPassMixGain);
            if (!self->mAllPassDelay[i].Resize(self->miAllPassDelaySamples[i] + 2))
                return 0;
            self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
        }
        self->mbAllPassConfigured = 1;
    }

    // Latch the applied attribute snapshot and refresh the reverb latency.
    self->mfLastDecayTime  = self->mfDecayTime;
    self->mfLastRoomSize   = self->mfRoomSize;
    self->mfLastDamping    = self->mfDamping;
    self->mfLastSampleRate = KF_INTERNAL_RATE;
    UpdateLatencyAndDecay(self);
    return 1;
}

// -------------------------------------------------------------------------------------
// UpdateLatencyAndDecay @0x82B9F6A8 -- recompute the reverb's reported latency (the number
// of samples the mixer must pre-roll) from the longest comb / all-pass delay and the largest
// loop gain, via the Schroeder RT60 form  d - d*10/log10(g), and fold the delta into the
// upstream voice's latency accumulator (voice +0x28).
// -------------------------------------------------------------------------------------
void ReverbModel1::UpdateLatencyAndDecay(ReverbModel1 *self)
{
    // Comb contribution: the largest comb delay (miCombDelays is ascending) and comb-0 gain.
    const f32 combDelay = static_cast<f32>(self->miCombDelays[5]);
    const f32 combLatency = combDelay - static_cast<f32>(
        (combDelay * 10.0f) / static_cast<f32>(log10(self->mfCombG2[0])));

    // All-pass contribution: the largest section gain and delay length.
    f32 maxGain = 0.0f;
    for (int i = 0; i < self->mbAllPassCount; ++i)
        if (self->mfAllPassGain[i] > maxGain)
            maxGain = self->mfAllPassGain[i];

    s32 maxApDelay = 0;
    for (int i = 0; i < self->mbAllPassCount; ++i)
        if (self->miAllPassDelaySamples[i] > maxApDelay)
            maxApDelay = self->miAllPassDelaySamples[i];

    const f32 apDelay = static_cast<f32>(maxApDelay);
    const f32 newDecay = (apDelay - static_cast<f32>(
        (apDelay * 10.0f) / static_cast<f32>(log10(maxGain)))) + combLatency;

    // Fold the decay-tail change into the owning voice's accumulator
    // (voice+0x28 == Voice::mfFadeStart, by name -- the expel-after-decay deadline;
    // 2026-08-25 wave 4: the former "+0x28 latency word" raw reinterpret is retired),
    // then latch it.
    self->mBase.mpVoice->mfFadeStart += (newDecay - self->mBase.mDecaySamples);
    self->mBase.mDecaySamples = newDecay;
}

// -------------------------------------------------------------------------------------
// RwacTimerClient @0x82BA5720 -- the per-frame timer callback. While the reverb is active it
// reconfigures the network whenever a cached attribute (reverb time / room / damping) or the
// sample rate diverges from the applied snapshot; on the frame the reverb time drops to zero
// it flushes every comb / all-pass delay line and clears the comb loop state (a clean tail-
// out). (Registered via a function-pointer cast; its return value is discarded by the
// scheduler, so ConfigureModel's status is forwarded only when it runs.)
// -------------------------------------------------------------------------------------
int ReverbModel1::RwacTimerClient(ReverbModel1 *self)
{
    if (self->mfDecayTime > 0.0f)
    {
        if (self->mfDecayTime != self->mfLastDecayTime
            || self->mfRoomSize != self->mfLastRoomSize
            || self->mfDamping != self->mfLastDamping
            || self->mfLastSampleRate != KF_INTERNAL_RATE)
        {
            return ConfigureModel(self, reinterpret_cast<System *>(self->mBase.mpSystem));
        }
    }
    else if (self->mfLastDecayTime > 0.0f)
    {
        for (int i = 0; i < 6; ++i)
        {
            self->mCombDelay[i].Reset(self->miCombDelays[i] + 1);
            CombFilter::CombFilterResetFunc(&self->mComb[i]);
        }
        for (int i = 0; i < self->mbAllPassCount; ++i)
            self->mAllPassDelay[i].Reset(self->miAllPassDelaySamples[i]);
    }
    return 1;
}

// -------------------------------------------------------------------------------------
// ReleaseEvent @0x82B9ADB0 -- tear the reverb down: release every comb / all-pass delay line
// back to the allocator, and (if it was registered) remove the reconfigure timer through the
// shared System singleton. (The X360 return is the last cleanup call's register value, which
// callers discard.)
// -------------------------------------------------------------------------------------
int ReverbModel1::ReleaseEvent(ReverbModel1 *self)
{
    for (int i = 0; i < 6; ++i)
        self->mCombDelay[i].Release();

    for (int i = 0; i < self->mbAllPassCount; ++i)
        self->mAllPassDelay[i].Release();

    if (self->mbTimerAdded)
        System::RemoveTimer(off_83271928, &self->mTimer);

    return 0;
}

// -------------------------------------------------------------------------------------
// Process @0x82B9F7D0 -- process one frame through the reverb network.
//
// Every call re-installs the comb / all-pass sections' IFilter apply+reset dispatch funcs and
// hands each section to its delay line together with a shared 768-sample scratch buffer
// carved off the System object table's scratch cursor (word [3]; restored at the tail) --
// exactly the Delay::Process idiom.
//
// While the reverb time is positive the six comb sections are run over the frame (the first
// overwrites the destination, the other five accumulate into it), the context's src/dst slots
// are ping-ponged, and the summed comb output is coloured by the all-pass ladder, whose
// per-mode shape also duplicates the result across the output channels. The slots are then
// swapped BACK (the pair is a net no-op, but both swaps are real stores and are reproduced).
// When the reverb time has fallen to zero the frame is silenced instead.
// -------------------------------------------------------------------------------------
int ReverbModel1::Process(ReverbModel1 *self, AudioProcessContext *ctx)
{
    // The frame's channel-buffer descriptors, latched at entry (r25 / r24).
    AudioChannelBuffer *pSrc = ctx->mpSrcBuffer;
    AudioChannelBuffer *pDst = ctx->mpDstBuffer;

    // Reserve the shared 768-sample scratch region off the System object table's scratch
    // cursor (word [3]); the cursor is restored at the tail.
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    f32 **ppScratchTable = reinterpret_cast<f32 **>(pSystem->mpObjectTable);
    f32 *pScratchTop = ppScratchTable[3];                        // r21 (saved to restore)
    f32 *pLocalBuffer = pScratchTop - KI_REVERB_SCRATCH_SAMPLES; // r21 - 0xC00 bytes
    ppScratchTable[3] = pLocalBuffer;

    // Install the comb sections' dispatch funcs through the IFilter view their delay lines
    // call (the leading two slots of each section ARE the apply/reset function pointers).
    for (int i = 0; i < 6; ++i)
    {
        IFilter *pCombInterface = reinterpret_cast<IFilter *>(&self->mComb[i]);
        pCombInterface->mpApplyFunc =
            reinterpret_cast<IFilterApplyFunc>(&CombFilter::CombFilterApplyFunc); // stw @ +0x00
        pCombInterface->mpResetFunc =
            reinterpret_cast<IFilterResetFunc>(&CombFilter::CombFilterResetFunc); // stw @ +0x04
        self->mCombDelay[i].SetFilter(pCombInterface);
        self->mCombDelay[i].SetLocalBuffer(pLocalBuffer, KI_REVERB_SCRATCH_SAMPLES);
    }

    // Same for the live all-pass sections (the count is re-read every iteration, as in the asm).
    for (int i = 0; i < self->mbAllPassCount; ++i)
    {
        IFilter *pAllPassInterface = reinterpret_cast<IFilter *>(&self->mAllPass[i]);
        pAllPassInterface->mpApplyFunc =
            reinterpret_cast<IFilterApplyFunc>(&AllPassFilter::AllPassFilterApplyFunc);
        pAllPassInterface->mpResetFunc =
            reinterpret_cast<IFilterResetFunc>(&AllPassFilter::AllPassFilterResetFunc);
        self->mAllPassDelay[i].SetFilter(pAllPassInterface);
        self->mAllPassDelay[i].SetLocalBuffer(pLocalBuffer, KI_REVERB_SCRATCH_SAMPLES);
    }

    if (self->mfDecayTime > KF_ZERO)
    {
        // Comb bank: section 0 overwrites the destination, the other five accumulate (src=1).
        self->mCombDelay[0].ApplyFilter(256, pSrc, pDst, 0);
        for (int i = 1; i < 6; ++i)
            self->mCombDelay[i].ApplyFilter(256, pSrc, pDst, 1);

        // Ping-pong the context slots: the comb output becomes the all-pass input.
        AudioChannelBuffer *pOldSrc = ctx->mpSrcBuffer; // r31
        AudioChannelBuffer *pOldDst = ctx->mpDstBuffer; // r29
        ctx->mpDstBuffer = pOldSrc;
        ctx->mpSrcBuffer = pOldDst;

        // All-pass ladder: input = pOldDst (the comb sum), output = pOldSrc. Each memcpy
        // duplicates the freshly written channel 0 of pOldSrc into another channel
        // (channel k lives at mpSamples + muStride * k; 0x400 bytes == the 256-sample frame).
        const u8 mode = self->mBase.mbChannelCount;
        if (mode == 1)
        {
            self->mAllPassDelay[0].ApplyFilter(256, pOldDst, pOldSrc, 0);
        }
        else if (mode == 2)
        {
            self->mAllPassDelay[1].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 1,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 1 <- channel 0
            self->mAllPassDelay[0].ApplyFilter(256, pOldDst, pOldSrc, 0);
        }
        else if (mode == 4)
        {
            self->mAllPassDelay[1].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 1,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 1 <- channel 0
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 3,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 3 <- channel 0
            self->mAllPassDelay[0].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 2,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 2 <- channel 0
        }
        else
        {
            self->mAllPassDelay[2].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 2,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 2 <- channel 0
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 4,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 4 <- channel 0
            self->mAllPassDelay[1].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 1,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 1 <- channel 0
            self->mAllPassDelay[0].ApplyFilter(256, pOldDst, pOldSrc, 0);
            memcpy(pOldSrc->mpSamples + pOldSrc->muStride * 3,
                   pOldSrc->mpSamples, 0x400); // XMemCpy -- channel 3 <- channel 0
            memset(pOldSrc->mpSamples + pOldSrc->muStride * 5,
                   0, 0x400);                  // XMemSet -- channel 5 silenced
        }

        // Swap the slots back (loc_82B9FB0C) -- the pair is a net no-op but both are real
        // stores, so both are reproduced.
        AudioChannelBuffer *pSwap = ctx->mpSrcBuffer;
        ctx->mpSrcBuffer = ctx->mpDstBuffer;
        ctx->mpDstBuffer = pSwap;
    }
    else
    {
        // Reverb time has fallen to zero: silence every channel of the frame's src buffer.
        for (u32 channel = 0; channel < self->mBase.mbChannelCount; ++channel)
            memset(pSrc->mpSamples + pSrc->muStride * channel, 0, 0x400); // XMemSet
    }

    ppScratchTable[3] = pScratchTop; // restore the scratch cursor
    return 1;
}

// -------------------------------------------------------------------------------------
// CreateInstance @0x82BA67F8 -- placement-init a ReverbModel1 over `self`.
//
// Initialize<ReverbModel1>(self, 0x28) constructs the plug-in and bases its attribute table
// at self+0x28 (mfDecayTime). The templated PlugIn::Initialize helper lives in another TU;
// its locally-observable effect (construct + attribute base) is reproduced inline here, as
// the Gain / Iir2* shapes do. FLAGGED: the real Initialize<T> body (and the base
// mpSystem/mpVoice wiring it performs from the voice/factory) lives in the PlugIn TU.
//
// Then the reverb sets its own defaults, sizes each comb / all-pass delay line from the
// section's read-length (miMode), computes the shared all-pass mix gain, and registers the
// per-frame reconfigure timer. Returns 0 if the timer registration fails, else 1.
// -------------------------------------------------------------------------------------
int ReverbModel1::CreateInstance(ReverbModel1 *self)
{
    ReverbModel1_ctor(self);
    self->mBase.mpAttributes = &self->mfDecayTime; // attribute table base @ self+0x28

    self->mbTimerAdded = 0;

    // The all-pass section count follows the reverb mode (1 / 2|4 / other).
    const u8 mode = self->mBase.mbChannelCount;
    u8 allPassCount;
    if (mode == 1)
        allPassCount = 1;
    else if (mode == 2 || mode == 4)
        allPassCount = 2;
    else
        allPassCount = 3;
    self->mbAllPassCount = allPassCount;

    // Attribute / cache defaults.
    self->mfDecayTime      = KF_ZERO;   // reverb off until an attribute drives it
    self->mfLastDecayTime  = KF_ZERO;
    self->mfLastSampleRate = KF_ZERO;
    self->mfRoomSize       = 15.0f;     // flt_820047C4
    self->mfDamping        = 1.0f;      // flt_82001C98
    self->mfLastDamping    = 1.0f;

    // Size each delay line from its section's read-length descriptor (miMode).
    for (int i = 0; i < 6; ++i)
        self->mCombDelay[i].Init(1, 0, self->mComb[i].miMode);
    for (int i = 0; i < allPassCount; ++i)
        self->mAllPassDelay[i].Init(1, 0, self->mAllPass[i].miMode);

    // Shared all-pass mix gain = 2 / (mode>4 ? mode-1 : mode).
    const s32 modeS = self->mBase.mbChannelCount;
    const f32 denom = (modeS > 4) ? static_cast<f32>(modeS - 1) : static_cast<f32>(modeS);
    self->mbAllPassConfigured = 0;
    self->mfAllPassMixGain = 2.0f / denom; // flt_82001D9C == 2.0

    // Register the per-frame reconfigure timer on the System's TimerManager (collection 1).
    System *pSystem = reinterpret_cast<System *>(self->mBase.mpSystem);
    if (TimerManager::AddTimer(
            &pSystem->mTimerManager, &self->mTimer,
            reinterpret_cast<TimerManager::TimerCallback>(&ReverbModel1::RwacTimerClient),
            self, "ReverbModel1", 1, 1))
    {
        return 0;
    }

    self->mbTimerAdded = 1;
    return 1;
}

} // namespace core
} // namespace audio
} // namespace rw
