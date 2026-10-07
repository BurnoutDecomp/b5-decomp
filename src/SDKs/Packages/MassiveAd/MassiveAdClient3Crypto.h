#pragma once

// MassiveAd client hashing / token helpers: the SHA-1 core (RFC 3174 reference shape), the
// HMAC-SHA1 request signature, the MD5 hex digest of the hardware id, and the Mersenne-Twister
// session-token generator. The MD5 core itself is the LibTomCrypt one (LibTomCrypt/tomcrypt_massive.h).

#include <cstddef>

// ---------------------------------------------------------------------------
// SHA-1 core. Context is 0x68 bytes: five chaining words, the 64-bit bit length as two
// words, a 16-bit block cursor (unsigned on this build), the 64-byte block, then the
// computed / corrupted flags.
// ---------------------------------------------------------------------------
enum
{
    shaSuccess = 0,
    shaNull,
    shaInputTooLong,
    shaStateError
};

enum
{
    SHA1HashSize = 20
};

struct SHA1Context
{
    unsigned int   Intermediate_Hash[SHA1HashSize / 4]; // +0x00
    unsigned int   Length_Low;                          // +0x14
    unsigned int   Length_High;                         // +0x18
    unsigned short Message_Block_Index;                 // +0x1C
    unsigned char  Message_Block[64];                   // +0x1E
    int            Computed;                            // +0x60
    int            Corrupted;                           // +0x64
};

extern "C"
{
int SHA1Reset(SHA1Context* context);
int SHA1Input(SHA1Context* context, const unsigned char* message_array, unsigned int length);
int SHA1Result(SHA1Context* context, unsigned char Message_Digest[SHA1HashSize]);

// Mersenne Twister MT19937 (reference mt19937ar shape; state is process-wide).
void          init_genrand(unsigned long s);
void          init_by_array(unsigned long init_key[], int key_length);
unsigned long genrand_int32(void);
}

namespace MassiveAdClient3
{

// Lower-case hex MD5 digest of pData[0..nDataLength). Returns a pointer to one process-wide
// 33-byte buffer that the next call overwrites (callers copy it straight onto the wire).
char* CalculateMD5Hash(const void* pData, int nDataLength);

// HMAC-SHA1 of pData under pcKey (keys longer than 64 bytes are hashed first). Returns a
// MassiveMalloc'd 20-byte digest the caller releases with MassiveFree, or null.
void* CalculateSHA1HMac(const void* pData, int nDataLength, const char* pcKey, int nKeyLength);

// Seeds the twister from the formatted server time and writes four 32-bit draws as a hex
// string (at most 33 bytes, strncpy semantics) into pacTokenBuffer.
void MassivePRNG(char* pacTokenBuffer);

} // namespace MassiveAdClient3
