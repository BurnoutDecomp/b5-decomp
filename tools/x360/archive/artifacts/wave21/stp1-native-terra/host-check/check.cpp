#include "cryptstp1.h"
#include <cstring>
#include <cstdio>
int main() {
  u8 secret[32],reverse[32],packet[32] = "test frame", original[32];
  for (int i=0;i<32;++i) secret[i]=u8(i);
  for (int i=0;i<16;++i) { reverse[i]=secret[i+16]; reverse[i+16]=secret[i]; }
  CryptStp1T tx={},rx={},disabled={};
  if (CryptStp1EncryptSize(&disabled,10)!=10 || CryptStp1DecryptHash(&disabled,packet,2)!=0) return 1;
  if (CryptStp1UseSecret(&tx,secret)!=1 || CryptStp1UseSecret(&rx,reverse)!=1) return 2;
  const int n=10,total=CryptStp1EncryptSize(&tx,n); memcpy(original,packet,n);
  if (total!=18 || CryptStp1EncryptHash(&tx,packet,total)!=0) return 3;
  CryptStp1EncryptData(&tx,packet,total);
  const u8 expected[]={0x9d,0xf9,0x33,0x8d,0x67,0x84,0x6b,0xad,0x6b,0xbe,0x4b,0x0a,0x47,0x1c,0x5f,0x27,0xfd,0x35};
  if (memcmp(packet,expected,total)!=0) return 4;
  CryptStp1DecryptData(&rx,packet,total);
  if (memcmp(packet,original,n)!=0 || CryptStp1DecryptHash(&rx,packet,total)!=0 || CryptStp1DecryptSize(&rx,total)!=n) return 5;
  packet[0]^=1; if (CryptStp1DecryptHash(&rx,packet,total)!=-2) return 6;
  if (CryptStp1DecryptHash(&rx,packet,7)!=-1) return 7;
  puts("stp1 host checks passed"); return 0;
}
