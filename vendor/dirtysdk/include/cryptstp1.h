#pragma once
#include "types.hpp"
#include "cryptarc4.h"

// ARTIST82B3AC10: two independent 258-byte ARC4 states at +8/+0x10A.
struct CryptStp1T
{
    s32 enabled;
    s32 decryptStarted;
    CryptArc4T recvArc4;
    CryptArc4T sendArc4;
};
extern "C" {
s32 CryptStp1OpenWallet(const u8* wallet, u8* secret, u8* ticket);
s32 CryptStp1UseSecret(CryptStp1T* state, const u8* secret);
void CryptStp1DecryptData(CryptStp1T* state, u8* data, s32 size);
s32 CryptStp1DecryptHash(CryptStp1T* state, const u8* data, s32 size);
s32 CryptStp1DecryptSize(CryptStp1T* state, s32 size);
s32 CryptStp1EncryptSize(CryptStp1T* state, s32 size);
s32 CryptStp1EncryptHash(CryptStp1T* state, u8* data, s32 size);
void CryptStp1EncryptData(CryptStp1T* state, u8* data, s32 size);
}