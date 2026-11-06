#include <vectorintrinsics.h>
struct Plain { __vector4 mV; };
struct __declspec(passinreg) Reg { __vector4 mV; };
struct Copy { __vector4 mV; Copy() {} Copy(const Copy& q):mV(q.mV) {} };
struct __declspec(passinreg) RegCopy { __vector4 mV; RegCopy() {} RegCopy(const RegCopy& q):mV(q.mV) {} };
typedef char size_check[sizeof(Plain)==16 ? 1 : -1];
typedef char align_check[__alignof(Plain)==16 ? 1 : -1];
__declspec(noinline) Plain plain(Plain q) { return q; }
__declspec(noinline) Reg reg(Reg q) { return q; }
__declspec(noinline) Copy copy(Copy q) { return q; }
__declspec(noinline) RegCopy regcopy(RegCopy q) { return q; }
