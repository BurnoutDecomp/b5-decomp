#include "types.hpp"
#include "cryptsha1.h"
#include "cryptrsa.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
struct ProtoSSLRefT { char strHost[256]; };
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
extern "C" void CryptMD5Init(CryptMD5T*);
extern "C" void CryptMD5Update(CryptMD5T*,const void*,s32);
extern "C" void CryptMD5Final(CryptMD5T*,void*,s32);
extern "C" s32 NetPrintfCode(const char*, ...) {return 0;}
extern "C" u32 NetTick() {return 0x12345678;}
struct X509NameT { char strCountry[32], strState[32], strCity[32], strOrg[32], strUnit[32], strCommon[32]; };
struct X509CertificateT
{
    s32 version;
    X509NameT Issuer, Subject;
    char strGoodFrom[32], strGoodTill[32];
    s32 iSerialSize; u8 SerialData[32];
    s32 iSigType, iSigSize; u8 SigData[128];
    s32 iKeyType; u8 KeyParm[64]; s32 iKeySize; u8 KeyData[256];
    s32 iKeyModSize; u8 KeyModData[129]; s32 iKeyExpSize; u8 KeyExpData[129];
};
// ARTIST 82B35FA0: parse an ASN.1 tag and short/long-form length.
static const u8* ParseHeader(const u8* data, const u8* end, s32* type, s32* size)
{
    if (size) *size = 0;
    if (type) *type = 0;
    if (!data || data == end) return 0;
    if (type) *type = *data;
    ++data;
    if (data == end) return 0;
    u32 length = *data++;
    if (length > 0x7f)
    {
        s32 count = length & 0x7f;
        length = 0;
        while (count > 0)
        {
            if (data == end) return 0;
            length = (length << 8) | *data++;
            --count;
        }
    }
    if (size) *size = static_cast<s32>(length);
    return data;
}

// ARTIST 82146F20 / 82B36040: native ASN.1 OID prefixes.
struct ProtoSSLOidT { s32 type, size; u8 bytes[16]; };
static const ProtoSSLOidT kProtoSSLOids[] = {
    {1,3,{0x55,0x04,0x06}}, {3,3,{0x55,0x04,0x07}},
    {2,3,{0x55,0x04,0x08}}, {4,3,{0x55,0x04,0x0a}},
    {5,3,{0x55,0x04,0x0b}}, {6,3,{0x55,0x04,0x03}},
    {7,9,{0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01}},
    {8,9,{0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x04}},
    {9,9,{0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x05}},
    {0,0,{0}}
};
static s32 ParseObject(const u8* data, s32 length)
{
    for (const ProtoSSLOidT* oid = kProtoSSLOids; oid->type; ++oid)
        if (length >= oid->size && std::memcmp(data, oid->bytes, oid->size) == 0)
            return oid->type;
    return 0;
}
static void ParseString(const u8* data, s32 length, char* output)
{
    s32 remaining = 32;
    while (length > 0 && remaining > 1)
    {
        *output++ = static_cast<char>(*data++);
        --length;
        --remaining;
    }
    if (remaining > 0) *output = 0;
}
static void ParseName(const u8* data, const u8* end, X509NameT* output)
{
    s32 type = 0, length = 0, oid = 0;
    while ((data = ParseHeader(data, end, &type, &length)) != 0)
    {
        if (type == 48 || type == 49)
            continue;
        if (type == 6)
            oid = ParseObject(data, length);
        if ((type == 19 || type == 20) && oid >= 1 && oid <= 6)
        {
            char* const fields[] = { output->strCountry, output->strState, output->strCity,
                output->strOrg, output->strUnit, output->strCommon };
            ParseString(data, length, fields[oid - 1]);
        }
        data += length;
    }
}

#include "protossl_cert_data.inc"
struct ProtoSSLCATrustT
{
    const char *country, *state, *city, *org, *unit, *common;
    const u8* modulus;
    u32 modulusSize;
    u8 exponent[4];
};
static const ProtoSSLCATrustT kProtoSSLTrust[] = {
    { "US", "California", "Redwood City", "Electronic Arts, Inc.", "Online Technology Group", "OTG3 Certificate Authority", kCAModulus0, 128, {0,1,0,1} },
    { "US", "", "", "RSA Data Security, Inc.", "Secure Server Certification Authority", "", kCAModulus1, 125, {0,1,0,1} },
    { "US", "", "", "Equifax", "Equifax Secure Certificate Auth", "", kCAModulus2, 128, {0,1,0,1} },
    { 0, 0, 0, 0, 0, 0, 0, 0, {0,0,0,0} }
};

// ARTIST 82B360D8: native X509 parser, issuer selection and RSA signature check.
extern "C" s32 ProtoSSLParseCertificate(ProtoSSLRefT* state, X509CertificateT* cert,
                                        const u8* input, s32 inputLength)
{
    const u8* const end = input + inputLength;
    s32 type, length;
    std::memset(cert, 0, sizeof(*cert));
    const u8* cursor = ParseHeader(input, end, &type, &length);
    if (!cursor || type != 48) return -1;
    const u8* const hashable = cursor;
    cursor = ParseHeader(cursor, end, &type, &length);
    if (!cursor || type != 48) return -2;
    const s32 hashableLength = length + 4;
    const u8* const tbsEnd = cursor + length;
    if (*cursor != 2)
    {
        cursor = ParseHeader(cursor, end, 0, &length);
        if (!cursor) return -3;
        cursor += length;
    }
    cursor = ParseHeader(cursor, tbsEnd, &type, &length);
    if (!cursor || length < 0 || length > 32) return -4;
    cert->iSerialSize = length;
    std::memcpy(cert->SerialData, cursor, length);
    cursor = ParseHeader(cursor + length, tbsEnd, &type, &length);
    if (!cursor || type != 48) return -5;
    const u8* const signatureAlgorithmEnd = cursor + length;
    cursor = ParseHeader(cursor, tbsEnd, &type, &length);
    if (!cursor || type != 6) return -6;
    cert->iSigType = ParseObject(cursor, length);
    if (!cert->iSigType)
    {
        NetPrintfCode("protossl: unsupported signature algorithm\n");
        return -7;
    }
    cursor = ParseHeader(signatureAlgorithmEnd, tbsEnd, &type, &length);
    if (!cursor || type != 48) return -8;
    const u8* const issuerEnd = cursor + length;
    ParseName(cursor, issuerEnd, &cert->Issuer);
    cursor = ParseHeader(issuerEnd, end, &type, &length);
    if (!cursor || type != 48) return -9;
    cursor = ParseHeader(cursor, end, &type, &length);
    if (!cursor || type != 23) return -10;
    ParseString(cursor, length, cert->strGoodFrom);
    cursor = ParseHeader(cursor + length, end, &type, &length);
    if (!cursor || type != 23) return -11;
    ParseString(cursor, length, cert->strGoodTill);
    cursor = ParseHeader(cursor + length, end, &type, &length);
    if (!cursor || type != 48) return -12;
    const u8* const subjectEnd = cursor + length;
    ParseName(cursor, subjectEnd, &cert->Subject);
    cursor = ParseHeader(subjectEnd, end, &type, &length);
    if (!cursor || type != 48) return -13;
    cursor = ParseHeader(cursor, end, &type, &length);
    if (!cursor || type != 48) return -14;
    const u8* const keyAlgorithmEnd = cursor + length;
    cursor = ParseHeader(cursor, keyAlgorithmEnd, &type, &length);
    if (!cursor || type != 6) return -15;
    cert->iKeyType = ParseObject(cursor, length);
    cursor = ParseHeader(keyAlgorithmEnd, end, &type, &length);
    if (!cursor || type != 3 || length < 1 || length > 256) return -16;
    cert->iKeySize = length - 1;
    std::memcpy(cert->KeyData, cursor + 1, cert->iKeySize);
    cursor = ParseHeader(tbsEnd, signatureAlgorithmEnd, &type, &length);
    if (!cursor || type != 48) return -18;
    const u8* const outerAlgorithmEnd = cursor + length;
    cursor = ParseHeader(cursor, outerAlgorithmEnd, &type, &length);
    if (!cursor || type != 6) return -19;
    cert->iSigType = ParseObject(cursor, length);
    cursor = ParseHeader(outerAlgorithmEnd, end, &type, &length);
    if (!cursor || type != 3 || length - 1 < 0 || length - 1 > 128) return -20;
    cert->iSigSize = length - 1;
    std::memcpy(cert->SigData, cursor + 1, cert->iSigSize);
    if (cert->iKeyType == 7)
    {
        const u8* const keyEnd = cert->KeyData + cert->iKeySize;
        cursor = ParseHeader(cert->KeyData, keyEnd, &type, &length);
        if (!cursor || type != 48) return -21;
        cursor = ParseHeader(cursor, keyEnd, &type, &length);
        if (!cursor || type != 2 || length < 4 || length > 129) return -22;
        const u8* const next = cursor + length;
        if (*cursor == 0) { ++cursor; --length; }
        cert->iKeyModSize = length;
        std::memcpy(cert->KeyModData, cursor, length);
        cursor = ParseHeader(next, keyEnd, &type, &length);
        if (!cursor || type != 2 || length < 1 || length > 129) return -23;
        if (*cursor == 0) { ++cursor; --length; }
        cert->iKeyExpSize = length;
        std::memcpy(cert->KeyExpData, cursor, length);
    }
    if (std::strcmp(state->strHost, cert->Subject.strCommon) != 0)
    {
        NetPrintfCode("protossl: subject mismatch %s != %s\n", state->strHost, cert->Subject.strCommon);
        return -24;
    }
    const ProtoSSLCATrustT* ca = kProtoSSLTrust;
    for (; ca->country; ++ca)
    {
        // ARTIST compares country, state, city, org and common; it skips unit.
        if (std::strcmp(ca->country, cert->Issuer.strCountry) == 0 &&
            std::strcmp(ca->state, cert->Issuer.strState) == 0 &&
            std::strcmp(ca->city, cert->Issuer.strCity) == 0 &&
            std::strcmp(ca->org, cert->Issuer.strOrg) == 0 &&
            std::strcmp(ca->common, cert->Issuer.strCommon) == 0)
            break;
    }
    if (!ca->country)
    {
        NetPrintfCode("protossl: -25\n");
        return -25;
    }
    if (ca->modulusSize != static_cast<u32>(cert->iSigSize))
    {
        NetPrintfCode("protossl: modulus size mismatch\n");
        return -26;
    }
    u8 expected[20], actual[20];
    s32 hashLength;
    if (cert->iSigType == 8)
    {
        CryptMD5T md5;
        CryptMD5Init(&md5);
        CryptMD5Update(&md5, hashable, hashableLength);
        CryptMD5Final(&md5, expected, 16);
        hashLength = 16;
    }
    else if (cert->iSigType == 9)
    {
        CryptSha1T sha;
        CryptSha1Init(&sha);
        CryptSha1Update(&sha, hashable, hashableLength);
        CryptSha1Final(&sha, expected, 20);
        hashLength = 20;
    }
    else
    {
        NetPrintfCode("protossl: unknown signature algorithm, should never get here\n");
        hashLength = 0;
    }
    CryptRSAT rsa;
    CryptRSAInit(&rsa, ca->modulus, ca->modulusSize, ca->exponent, 4);
    CryptRSAInitSignature(&rsa, cert->SigData, cert->iSigSize);
    CryptRSAEncrypt(&rsa);
    std::memcpy(actual, rsa.EncryptBlock + (cert->iSigSize & ~1) - hashLength, hashLength);
    if (std::memcmp(expected, actual, hashLength) != 0)
    {
        NetPrintfCode("protossl: signature hash mismatch\n");
        return -27;
    }
    return 0;
}

static int Hex(const char* s,u8* out) {const int n=static_cast<int>(std::strlen(s)/2);for(int i=0;i<n;++i){unsigned v;std::sscanf(s+i*2,"%2x",&v);out[i]=static_cast<u8>(v);}return n;}
int main(int argc,char** argv){if(argc!=3)return 2;u8 data[4096]={};const int len=Hex(argv[2],data);ProtoSSLRefT state={};X509CertificateT cert={};std::strncpy(state.strHost,argv[1],255);std::printf("%d",ProtoSSLParseCertificate(&state,&cert,data,len));}
