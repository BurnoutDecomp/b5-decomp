// DirtySDK 5.5.3 native RSA, ARTIST 82B41C28..82B42320 and 82B6FC80.
#include "cryptrsa.h"
#include "dirtylib.h"
#include <cstddef>
#include <cstring>
extern "C" u32 NetTick(void);

CGS_STATIC_ASSERT((sizeof(CryptRSAT) == 1304), "ARTIST RSA context extent");
CGS_STATIC_ASSERT((offsetof(CryptRSAT, iKeyModSize) == 1024), "ARTIST RSA modulus size");
CGS_STATIC_ASSERT((offsetof(CryptRSAT, KeyModData) == 1028), "ARTIST RSA modulus");
CGS_STATIC_ASSERT((offsetof(CryptRSAT, iKeyExpSize) == 1160), "ARTIST RSA exponent size");
CGS_STATIC_ASSERT((offsetof(CryptRSAT, KeyExpData) == 1164), "ARTIST RSA exponent");

// ARTIST 82B41E48: bytes to big-endian base-65536 words.
static s32 ToWords(u16* out, s32 width, const u8* input, s32 length)
{
    const s32 words = (length + 1) / 2;
    if (width < 0) width = words;
    if (width > words)
    {
        const s32 pad = width - words;
        for (s32 i = 0; i < pad; ++i) *out++ = 0;
        width = words;
    }
    if (length / 2 != words)
    {
        *out++ = *input++;
        --width;
    }
    for (; width > 0; --width, input += 2)
        *out++ = static_cast<u16>((u32(input[0]) << 8) | input[1]);
    return words;
}

// ARTIST 82B41C28: fixed-width shift/add modular multiplication.
static void Multiply(u16* output, s32 width, const u16* a, const u16* b, const u16* modulus)
{
    u16 temp0[256], temp1[256];
    u16* current = temp0;
    u16* candidate = temp1;
    std::memset(current, 0, width * sizeof(u16));
    for (s32 bit = width * 16 - 1, index = 0; bit >= 0; --bit, ++index)
    {
        u32 carry = 0;
        for (s32 j = width - 1; j >= 0; --j)
        {
            const u32 value = u32(current[j]) + current[j] + carry;
            current[j] = static_cast<u16>(value);
            carry = value >> 16;
        }
        u32 borrow = 0;
        for (s32 j = width - 1; j >= 0; --j)
        {
            const u32 value = u32(current[j]) - modulus[j] - borrow;
            candidate[j] = static_cast<u16>(value);
            borrow = (value >> 16) & 1;
        }
        if (!borrow || carry)
        {
            u16* swap = current; current = candidate; candidate = swap;
        }
        if ((a[index / 16] & (u32(1) << (bit % 16))) != 0)
        {
            carry = 0;
            for (s32 j = width - 1; j >= 0; --j)
            {
                const u32 value = u32(current[j]) + b[j] + carry;
                current[j] = static_cast<u16>(value);
                carry = value >> 16;
            }
            borrow = 0;
            for (s32 j = width - 1; j >= 0; --j)
            {
                const u32 value = u32(current[j]) - modulus[j] - borrow;
                candidate[j] = static_cast<u16>(value);
                borrow = (value >> 16) & 1;
            }
            if (!borrow || carry)
            {
                u16* swap = current; current = candidate; candidate = swap;
            }
        }
    }
    std::memcpy(output, current, width * sizeof(u16));
}

// ARTIST 82B41EF0: fixed exponent fast paths and the original general path.
static void Exponentiate(u8* output, const u8* input, const u8* modulus, s32 length,
                         const u8* exponent, s32 exponentLength)
{
    u16 result[256], modWords[256], power[256];
    const s32 width = ToWords(modWords, -1, modulus, length);
    ToWords(power, -1, input, length);
    u32 value = 0;
    for (s32 i = 0; i < exponentLength; ++i)
        value = (value << 8) | exponent[i];
    if (value == 3 || value == 17 || value == 65537)
    {
        const s32 squares = value == 3 ? 1 : value == 17 ? 4 : 16;
        Multiply(result, width, power, power, modWords);
        for (s32 i = 1; i < squares; ++i)
            Multiply(result, width, result, result, modWords);
        Multiply(result, width, result, power, modWords);
    }
    else
    {
        NetPrintfCode("cryptrsa: warning; executing general case code, which is likely to be slow\n");
        std::memset(result, 0, sizeof(result));
        result[width - 1] = 1;
        for (; value; value >>= 1)
        {
            if (value & 1)
                Multiply(result, width, result, power, modWords);
            Multiply(power, width, power, power, modWords);
        }
    }
    // Native ARTIST writes two bytes per complete word. For odd lengths it
    // leaves the final destination byte untouched, rather than widening output.
    const s32 outputWords = length / 2;
    const u16* word = result + width - outputWords;
    for (s32 i = 0; i < outputWords; ++i)
    {
        *output++ = static_cast<u8>(*word >> 8);
        *output++ = static_cast<u8>(*word++);
    }
}

extern "C" void CryptRSAInit(CryptRSAT* state, const u8* modulus, s32 modulusLength,
                             const u8* exponent, s32 exponentLength)
{
    state->iKeyModSize = modulusLength;
    state->iKeyExpSize = exponentLength;
    std::memcpy(state->KeyModData, modulus, modulusLength);
    std::memcpy(state->KeyExpData, exponent, exponentLength);
}

extern "C" void CryptRSAInitMaster(CryptRSAT* state, const u8* master, s32 length)
{
    u32 tick = NetTick();
    for (s32 i = 0; i < state->iKeyModSize; ++i)
        state->EncryptBlock[i] = ((u32(1) << (i & 31)) & tick) != 0;
    for (s32 i = 0; i < state->iKeyModSize; ++i)
    {
        u8 value = state->EncryptBlock[i];
        do
        {
            tick = 69069u * (tick + 1);
            value = static_cast<u8>(value ^ tick);
        } while (value == 0);
        state->EncryptBlock[i] = value;
    }
    state->EncryptBlock[0] = 0;
    state->EncryptBlock[1] = 2;
    state->EncryptBlock[state->iKeyModSize - length - 1] = 0;
    std::memcpy(state->EncryptBlock + state->iKeyModSize - length, master, length);
}

extern "C" void CryptRSAInitSignature(CryptRSAT* state, const u8* signature, s32 length)
{
    std::memcpy(state->EncryptBlock, signature, length);
}

extern "C" void CryptRSAEncrypt(CryptRSAT* state)
{
    Exponentiate(state->EncryptBlock, state->EncryptBlock, state->KeyModData,
        state->iKeyModSize, state->KeyExpData, state->iKeyExpSize);
}
