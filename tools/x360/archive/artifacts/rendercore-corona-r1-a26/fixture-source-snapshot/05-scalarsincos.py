"""XMScalarSinCos @0x821F0C08: an exact Python model of the console words (ROUNDING_RULE 1 for vmsum4fp128: products
exact in f64, summed left to right in f64, cast once; rule 3 fnmsubs; rule 4 for the rest), checked against the
real words on emu64 -- once with the emulator's vmsum4fp128 switched to the rule-1 model, once with its own exact
(Fraction) model, to count where the two vmsum models part."""
import math
import random
import struct
import sys

import numpy as np

import sincos_emu as S
from sincos_emu import new_cpu, val, bits, f32, fma, mul, add, sub, same
import vmxemu as V

PI = val(0x40490FDB)
TWO_PI = val(0x40C90FDB)
RECIP_TWO_PI = val(0x3E22F983)


def table(address):
    return [val(struct.unpack(">I", bytes(V.x360rd.rd(address + 4 * k, 4)))[0]) for k in range(4)]


COS_A, SIN_A = table(0x82000C00), table(0x82000BD0)
COS_B, SIN_B = table(0x82000C10), table(0x82000BE0)
COS_C, SIN_C = table(0x82000C20), table(0x82000BF0)


def vmsum4_rule1(a, b):
    s = float(a[0]) * float(b[0])
    for k in range(1, 4):
        s = s + float(a[k]) * float(b[k])
    with np.errstate(all="ignore"):
        r = float(np.float32(s))
    if math.isfinite(s) and math.isinf(r):
        return math.nan                      # xenia: a finite f64 sum that overflows f32 is a QNaN
    return r


def fctiwz(x):
    if math.isnan(x) or x <= -2147483648.0:
        return -2147483648
    if x >= 2147483647.0:
        return 2147483647
    return int(x)                            # toward zero


def xm_scalar_sincos(v):
    signed_pi = math.copysign(PI, v) if not math.isnan(v) else (-PI if math.copysign(1.0, v) < 0 else PI)
    shifted = add(signed_pi, v)
    q = float(np.float32(float(fctiwz(mul(shifted, RECIP_TWO_PI)))))
    d = fma(q, TWO_PI, -v)
    y = d if math.isnan(d) else -d
    y2 = mul(y, y)
    y3 = mul(y2, y)
    p0 = [mul(1.0, 1.0), mul(y, y), mul(y2, y2), mul(y3, y3)]
    cos_a = vmsum4_rule1(p0, COS_A)
    y8 = mul(p0[3], y2)
    p1 = [mul(p0[k], y) for k in range(4)]
    sin_a = vmsum4_rule1(p1, SIN_A)
    p0b = [mul(p0[k], y8) for k in range(4)]
    p1b = [mul(p1[k], y8) for k in range(4)]
    p0c = [mul(p0b[k], y8) for k in range(4)]
    cos_b = vmsum4_rule1(p0b, COS_B)
    p1c = [mul(p1b[k], y8) for k in range(4)]
    sin_b = vmsum4_rule1(p1b, SIN_B)
    cos_c = vmsum4_rule1(p0c, COS_C)
    sin_c = vmsum4_rule1(p1c, SIN_C)
    return add(add(sin_c, sin_b), sin_a), add(add(cos_c, cos_b), cos_a)


class Rule1CPU(S.CPU):
    """emu64 with vmsum4fp128 as ROUNDING_RULE 1 (the campaign model) instead of the exact sum."""
    def exec_vector(self, a, w, mn):
        if (w >> 26) == 5 and (w & 0x210) != 0 and (w & 0x3D0) == 0x1D0:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            r = vmsum4_rule1(self.vf(va), self.vf(vb))
            self.setvf(vd, [np.float32(r)] * 4)
            return True
        return super().exec_vector(a, w, mn)


def run(v, rule1=True):
    mem = V.Mem()
    mem.add(S.STACK, 0x20000)
    mem.add(S.SELF, 0x100)
    c = (Rule1CPU if rule1 else S.CPU)(mem)
    c.setg(1, S.STACK + 0x10000)
    c.setg(3, S.SELF)
    c.setg(4, S.SELF + 4)
    c.setf(1, v)
    c.run(0x821F0C08)
    return val(c.rd32(S.SELF)), val(c.rd32(S.SELF + 4))


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rng = random.Random(0x5C5C)
    cases = [0.0, -0.0, PI, -PI, TWO_PI, -TWO_PI, 1.0, -1.0, 0.5, 1e-3, -1e-3, 1e-30, 100.0, -100.0, 1e5, -1e5,
             3.0e7, math.inf, -math.inf, math.nan, val(0x40490FDA), val(0x40490FDC)]
    while len(cases) < n:
        cases.append(f32(rng.uniform(-10.0, 10.0)) if len(cases) < n * 3 // 4 else f32(rng.uniform(-1000.0, 1000.0)))
    bad_rule1 = bad_exact = diff_std = 0
    for v in cases:
        ms, mc = xm_scalar_sincos(v)
        es, ec = run(v, True)
        if not (same(ms, es) and same(mc, ec)):
            bad_rule1 += 1
            if bad_rule1 < 5:
                print("RULE1 DIFF %r model %08X %08X emu %08X %08X" % (v, bits(ms), bits(mc), bits(es), bits(ec)))
        xs, xc = run(v, False)
        if not (same(xs, es) and same(xc, ec)):
            bad_exact += 1
        if math.isfinite(v) and not (same(ms, f32(math.sin(v))) and same(mc, f32(math.cos(v)))):
            diff_std += 1
    print("XMScalarSinCos: %d inputs; model vs emu (rule-1 vmsum): %d mismatches; emu exact-vmsum vs rule-1: %d differ;"
          " the console vs correctly rounded sin/cos: %d differ" % (len(cases), bad_rule1, bad_exact, diff_std))
