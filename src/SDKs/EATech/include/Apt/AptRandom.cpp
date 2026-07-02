// ===========================================================================
// EATech Apt -- the AS Random PRNG.   DECOMPILED from the X360 ARTIST.XEX.
//   AptSeedRand @0x82ADA4B0 / AptRand @0x82AE04F0 / the regenerate pass
//   sub_82AE0378 (the refill AptRand tail-calls when the countdown runs out).
//
// A Mersenne-Twister (MT19937) variant: the standard 624-word state, the
// standard twist matrix 0x9908B0DF and c-mask 0xEFC60000, but a NON-standard
// tempering b-mask 0x9D2C56FF (MT's is 0x9D2C5680 -- the EA fork sets the low
// byte). The countdown static is .data-initialised to -1 (read from the
// decrypted XEX @0x82F73380), so the very first AptRand decrements it to -2
// and the regenerate auto-seeds with 0x1105 -- no explicit init call needed.
//
// State globals (console): dword_82F73380 = the words-remaining countdown;
// off_8324E2A0 = the read cursor into the state; dword_8324D838[624] = the
// state itself (.bss). AptSeedRand: state[0] = seed|1, then 623 words of
// state[i] = state[i-1] * 0x10DCD, countdown = 0 (forcing a regenerate on the
// next draw). The regenerate runs the standard three-phase twist (227 + 396 +
// the wrap element), resets the countdown to 623 and the cursor to state[1],
// and returns temper(state[0]). AptRand's fast path returns
// temper(*cursor++). Temper: y^=y>>11; y^=(y<<7)&0x9D2C56FF;
// y^=(y<<15)&0xEFC60000; y^=y>>18.
// ===========================================================================

#include "SDKs/EATech/include/Apt/Apt.h"

namespace
{
    const int          KN_APT_MT_N     = 624;
    const int          KN_APT_MT_M     = 397;
    const unsigned int KU_APT_MT_A     = 0x9908B0DFu;   // the twist matrix
    const unsigned int KU_APT_MT_B     = 0x9D2C56FFu;   // EA's tempering b (MT: ..80)
    const unsigned int KU_APT_MT_C     = 0xEFC60000u;   // the standard tempering c
    const unsigned int KU_APT_MT_HI    = 0x80000000u;
    const unsigned int KU_APT_MT_LO    = 0x7FFFFFFFu;

    int           gnAptRandCountdown = -1;              // dword_82F73380 (.data -1)
    unsigned int* gpAptRandCursor    = 0;               // off_8324E2A0
    unsigned int  gaAptRandState[KN_APT_MT_N];          // dword_8324D838 (.bss)

    inline unsigned int AptRandTemper(unsigned int y)
    {
        y ^= y >> 11;
        y ^= (y << 7)  & KU_APT_MT_B;
        y ^= (y << 15) & KU_APT_MT_C;
        y ^= y >> 18;
        return y;
    }
}

// AptSeedRand @0x82ADA4B0 -- state[0] = seed|1; state[i] = state[i-1] * 0x10DCD
// (623 words); countdown = 0 so the next draw regenerates.
void AptSeedRand(unsigned int nSeed)
{
    gnAptRandCountdown = 0;
    unsigned int nWord = nSeed | 1u;
    gaAptRandState[0] = nWord;
    for (int i = 1; i < KN_APT_MT_N; ++i)
    {
        nWord *= 0x10DCDu;
        gaAptRandState[i] = nWord;
    }
}

// sub_82AE0378 -- the regenerate pass: auto-seed a never-seeded state
// (countdown < -1 after the failed decrement), run the standard MT twist,
// reset the countdown/cursor, and return temper(state[0]).
static unsigned int AptRandRegenerate()
{
    if (gnAptRandCountdown < -1)
        AptSeedRand(0x1105u);

    gnAptRandCountdown = KN_APT_MT_N - 1;   // 0x26F: 623 draws before the next refill
    gpAptRandCursor    = &gaAptRandState[1];

    // The standard three-phase twist over the 624-word state.
    int i = 0;
    for (; i < KN_APT_MT_N - KN_APT_MT_M; ++i)           // 227 words
    {
        const unsigned int y = (gaAptRandState[i] & KU_APT_MT_HI)
                             | (gaAptRandState[i + 1] & KU_APT_MT_LO);
        gaAptRandState[i] = gaAptRandState[i + KN_APT_MT_M]
                          ^ (y >> 1) ^ ((y & 1u) ? KU_APT_MT_A : 0u);
    }
    for (; i < KN_APT_MT_N - 1; ++i)                     // 396 words
    {
        const unsigned int y = (gaAptRandState[i] & KU_APT_MT_HI)
                             | (gaAptRandState[i + 1] & KU_APT_MT_LO);
        gaAptRandState[i] = gaAptRandState[i + KN_APT_MT_M - KN_APT_MT_N]
                          ^ (y >> 1) ^ ((y & 1u) ? KU_APT_MT_A : 0u);
    }
    {                                                    // the wrap element
        const unsigned int y = (gaAptRandState[KN_APT_MT_N - 1] & KU_APT_MT_HI)
                             | (gaAptRandState[0] & KU_APT_MT_LO);
        gaAptRandState[KN_APT_MT_N - 1] = gaAptRandState[KN_APT_MT_M - 1]
                                        ^ (y >> 1) ^ ((y & 1u) ? KU_APT_MT_A : 0u);
    }

    return AptRandTemper(gaAptRandState[0]);
}

// AptRand @0x82AE04F0 -- the fast path: temper the cursor word; refill when
// the countdown runs out.
unsigned int AptRand()
{
    if (--gnAptRandCountdown < 0)
        return AptRandRegenerate();
    return AptRandTemper(*gpAptRandCursor++);
}
