// DirtySDK native STP1 frame protection, ARTIST82B3ABA8..82B3AE74.
#include "cryptstp1.h"
#include <stddef.h>
#include <string.h>

CGS_STATIC_ASSERT((sizeof(CryptStp1T) == 524), "ARTIST STP1 size");
CGS_STATIC_ASSERT((offsetof(CryptStp1T, recvArc4) == 8), "ARTIST STP1 receive ARC4");
CGS_STATIC_ASSERT((offsetof(CryptStp1T, sendArc4) == 266), "ARTIST STP1 send ARC4");

// Existing DirtySDK native MD5 provider cryptmd5.cpp, ARTIST 82B414B8 etc.
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
extern "C" {
void CryptMD5Init(CryptMD5T*);
void CryptMD5Update(CryptMD5T*, const void*, s32);
void CryptMD5Final(CryptMD5T*, void*, s32);
}

extern "C" s32 CryptStp1OpenWallet(const u8* wallet, u8* secret, u8* ticket)
{
    if (secret) memcpy(secret, wallet, 32);
    if (ticket) memcpy(ticket, wallet + 32, 52);
    return 1;
}
extern "C" s32 CryptStp1UseSecret(CryptStp1T* state, const u8* secret)
{
    state->enabled = 0;
    if (secret)
    {
        state->decryptStarted = 0;
        state->enabled = 1;
        CryptArc4Init(&state->sendArc4, secret, 16, -1);
        CryptArc4Init(&state->recvArc4, secret + 16, 16, -1);
    }
    return state->enabled;
}
// ARTIST82B3AC88, omitted from IDA exports; verified by ppcdis.
extern "C" void CryptStp1DecryptData(CryptStp1T* state, u8* data, s32 size)
{
    if (state->enabled)
    {
        CryptArc4Apply(&state->recvArc4, data, size);
        state->decryptStarted = 1;
    }
}
extern "C" s32 CryptStp1DecryptHash(CryptStp1T* state, const u8* data, s32 size)
{
    if (!state->enabled || !state->decryptStarted) return 0;
    if (size < 8) return -1;
    CryptMD5T context;
    u8 digest[16];
    CryptMD5Init(&context);
    CryptMD5Update(&context, data, size - 8);
    CryptMD5Final(&context, digest, 16);
    for (s32 i = 0; i < 8; ++i) if (digest[i] != data[size - 8 + i]) return -2;
    return 0;
}
extern "C" s32 CryptStp1DecryptSize(CryptStp1T* state, s32 size)
{
    if (state->enabled && state->decryptStarted && size >= 8) size -= 8;
    return size;
}
extern "C" s32 CryptStp1EncryptSize(CryptStp1T* state, s32 size)
{
    return state->enabled ? size + 8 : size;
}
extern "C" s32 CryptStp1EncryptHash(CryptStp1T* state, u8* data, s32 size)
{
    const s32 payload = size - 8;
    if (state->enabled)
    {
        if (payload < 0) return -1;
        CryptMD5T context;
        CryptMD5Init(&context);
        CryptMD5Update(&context, data, payload);
        CryptMD5Final(&context, data + payload, 8);
    }
    return 0;
}
extern "C" void CryptStp1EncryptData(CryptStp1T* state, u8* data, s32 size)
{
    if (state->enabled) CryptArc4Apply(&state->sendArc4, data, size);
}