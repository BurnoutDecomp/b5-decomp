from pathlib import Path
p=Path('vendor/dirtysdk/src/x360/protossl_x360.cpp');s=p.read_text(); point=s.index('struct ProtoSSLRefT\n')
q='''// Native type shape from DirtySDK 5.5.3 DWARF, gated by ARTIST offsets.
struct CryptMD5T { u8 strData[72]; u32 uCount; u32 uRegs[4]; };
struct CryptSha1T { u32 uCount, uPartialCount, H[5]; u8 strData[64]; };
struct CryptArc4T { u8 state[256], walk, swap; };
struct CipherSuiteT { u8 uIdent[2], uKey, uEnc, uMac; };
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
struct SecureStateT
{
    u32 uTimer, uSendSeqn;
    s32 iSendProg, iSendSize;
    u32 uRecvSeqn;
    s32 iRecvProg, iRecvSize, iRecvBase;
    const CipherSuiteT* pCipher;
    u8 ClientRandom[32], ServerRandom[32], PreMasterKey[48], MasterKey[48], KeyBlock[128];
    u8 *pServerMAC, *pClientMAC, *pServerKey, *pClientKey;
    CryptMD5T HandshakeMD5;
    CryptSha1T HandshakeSHA;
    CryptArc4T ReadArc4, WriteArc4;
    X509CertificateT Cert;
    u8 SendData[16384], RecvData[16384];
};
CGS_STATIC_ASSERT((offsetof(SecureStateT, HandshakeMD5) == 0x154), "ARTIST MD5 context");
CGS_STATIC_ASSERT((offsetof(SecureStateT, HandshakeSHA) == 0x1b0), "ARTIST SHA1 context");
CGS_STATIC_ASSERT((offsetof(SecureStateT, ReadArc4) == 0x20c), "ARTIST read cipher");
CGS_STATIC_ASSERT((offsetof(SecureStateT, WriteArc4) == 0x30e), "ARTIST write cipher");
CGS_STATIC_ASSERT((offsetof(SecureStateT, SendData) == 0x8d8), "ARTIST send record");
CGS_STATIC_ASSERT((offsetof(SecureStateT, RecvData) == 0x48d8), "ARTIST receive record");
CGS_STATIC_ASSERT((sizeof(SecureStateT) == 0x88d8), "ARTIST secure allocation");
extern "C" void CryptMD5Init(CryptMD5T*);
extern "C" void CryptMD5Update(CryptMD5T*, const void*, s32);
extern "C" void CryptMD5Final(CryptMD5T*, void*, s32);
extern "C" void CryptSha1Init(CryptSha1T*);
extern "C" void CryptSha1Update(CryptSha1T*, const u8*, u32);
extern "C" void CryptSha1Final(CryptSha1T*, void*, u32);
extern "C" void CryptArc4Apply(CryptArc4T*, u8*, s32);
extern "C" s32 SocketSendto(SocketT*, const char*, s32, s32, const sockaddr*, s32);
extern "C" s32 SocketRecvfrom(SocketT*, char*, s32, s32, sockaddr*, s32*);
extern "C" void ProtoSSLUpdate(ProtoSSLRefT*);

'''
s=s[:point]+q+s[point:];s=s.replace('    void* pSecure;','    SecureStateT* pSecure;');p.write_text(s)
