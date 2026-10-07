#pragma once

// The slice of LibTomCrypt linked into the MassiveAd client: MD5 plus the misc helpers it
// pulls in. Built with LTC_SMALL_CODE (table-driven MD5 rounds) and LTC_CLEAN_STACK.

typedef unsigned int       ulong32;
typedef unsigned long long ulong64;

enum
{
    CRYPT_OK          = 0,
    CRYPT_INVALID_ARG = 16
};

struct md5_state
{
    ulong64       length;   // +0x00 message length in bits
    ulong32       state[4]; // +0x08
    ulong32       curlen;   // +0x18
    unsigned char buf[64];  // +0x1C
};

typedef union Hash_state
{
    struct md5_state md5;
} hash_state;

extern "C"
{
void crypt_argchk(const char* v, const char* s, int d);
void zeromem(void* dst, unsigned long len);
void burn_stack(unsigned long len);

int md5_init(hash_state* md);
int md5_process(hash_state* md, const unsigned char* in, unsigned long inlen);
int md5_done(hash_state* md, unsigned char* hash);
}
