// MassiveAd client session-token generator over a Mersenne Twister MT19937 (vendor middleware).

#include "SDKs/Packages/MassiveAd/MassiveAdClient3Crypto.h"

#include <cstring>

#include "SDKs/Packages/MassiveAd/MassiveAdClient3.h"
#include "SDKs/Packages/MassiveAd/MassiveAdClient3ClientCore.h"

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL
#define UPPER_MASK 0x80000000UL
#define LOWER_MASK 0x7fffffffUL

static unsigned long mt[N];
static int mti = N + 1;

void init_genrand(unsigned long s)
{
    mt[0] = s & 0xffffffffUL;
    for (mti = 1; mti < N; mti++)
    {
        mt[mti] = (1812433253UL * (mt[mti - 1] ^ (mt[mti - 1] >> 30)) + mti);
        mt[mti] &= 0xffffffffUL;
    }
}

void init_by_array(unsigned long init_key[], int key_length)
{
    int i, j, k;
    init_genrand(19650218UL);
    i = 1;
    j = 0;
    k = (N > key_length ? N : key_length);
    for (; k; k--)
    {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1664525UL)) + init_key[j] + j;
        mt[i] &= 0xffffffffUL;
        i++;
        j++;
        if (i >= N)
        {
            mt[0] = mt[N - 1];
            i = 1;
        }
        if (j >= key_length)
        {
            j = 0;
        }
    }
    for (k = N - 1; k; k--)
    {
        mt[i] = (mt[i] ^ ((mt[i - 1] ^ (mt[i - 1] >> 30)) * 1566083941UL)) - i;
        mt[i] &= 0xffffffffUL;
        i++;
        if (i >= N)
        {
            mt[0] = mt[N - 1];
            i = 1;
        }
    }

    mt[0] = 0x80000000UL; // MSB is 1; assuring non-zero initial array
}

unsigned long genrand_int32(void)
{
    unsigned long y;
    static unsigned long mag01[2] = { 0x0UL, MATRIX_A };

    if (mti >= N)
    {
        int kk;

        if (mti == N + 1)
        {
            init_genrand(5489UL);
        }

        for (kk = 0; kk < N - M; kk++)
        {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (; kk < N - 1; kk++)
        {
            y = (mt[kk] & UPPER_MASK) | (mt[kk + 1] & LOWER_MASK);
            mt[kk] = mt[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N - 1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
        mt[N - 1] = mt[M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }

    y = mt[mti++];

    // Tempering
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

namespace MassiveAdClient3
{

void MassivePRNG(char* pacTokenBuffer)
{
    char lacToken[33];
    char lacTime[128];
    unsigned char lacKey[176];

    long long lnTime = CMassiveClientCore::Instance()->GetTime();
    CMassiveSystem::Instance()->GetServerTimeFormatted(lnTime, lacTime, sizeof(lacTime));
    std::memcpy(lacKey, lacTime, std::strlen(lacTime));
    init_by_array(reinterpret_cast<unsigned long*>(lacKey), static_cast<int>(std::strlen(lacTime)) / 4);

    unsigned long luA = genrand_int32();
    unsigned long luB = genrand_int32();
    unsigned long luC = genrand_int32();
    unsigned long luD = genrand_int32();
    MassiveFormatString(lacToken, 33, "%x%x%x%x", luA, luB, luC, luD);
    std::strncpy(pacTokenBuffer, lacToken, 33);
}

} // namespace MassiveAdClient3
