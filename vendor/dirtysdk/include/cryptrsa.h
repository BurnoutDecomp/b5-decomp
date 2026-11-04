#ifndef DIRTYSDK_CRYPT_RSA_H
#define DIRTYSDK_CRYPT_RSA_H
#include "types.hpp"
struct CryptRSARandomT { u32 u0, u1; };
struct CryptRSAT
{
    u8 EncryptBlock[1024];
    s32 iKeyModSize;
    u8 KeyModData[129];
    s32 iKeyExpSize;
    u8 KeyExpData[129];
    CryptRSARandomT Random;
};
#ifdef __cplusplus
extern "C" {
#endif
void CryptRSAInit(CryptRSAT*, const u8*, s32, const u8*, s32);
void CryptRSAInitMaster(CryptRSAT*, const u8*, s32);
void CryptRSAInitSignature(CryptRSAT*, const u8*, s32);
void CryptRSAEncrypt(CryptRSAT*);
#ifdef __cplusplus
}
#endif
#endif
