"""Tiny PPC + VMX/VMX128 emulator for a handful of ARTIST functions.

FX-CRASHVFX copy (2026-09-24): extended with scalar FP (lfs/stfs/lfd/fadds/fsubs/fmuls/fdivs/fsel/
frsp/fcfid), 64-bit std, mullw/divwu/insrwi/clrldi, lvlx/stvewx and the VMX ops of
CB4ParticleBank::Render 0x8291E600, ShadedRotatingRenderMethod::BuildQuad 0x8291E150 and
NativeParticleVertex::VertexIterator::Write 0x8291E478. Unmapped reads inside the image range fall
back to x360rd (read-only image bytes).


Executes the REAL instruction words of:
    BrnCrashTriangleCache::AddTriangles              0x8228CDA8
    BrnCrashTriangleCache::CheckForDuplicateTriangles 0x822847B0
    BrnCrashTriangleCache::InsertTriangleIntoCache    0x8227B2D0
    BrnCrashTriangleCache::CollideWithTriangleCache   0x822849B8
Vector ops are decoded from the raw word (VMX128 register fields per tools/re/vmx128.py);
scalar ops from the IDA export text. Floats are f32 (numpy); vmaddfp/vnmsubfp are FUSED
(computed exactly with fractions, rounded once to f32). Estimates (vrefp/vrsqrtefp) are
exact-then-rounded (the NR steps that follow make the result ~correctly rounded anyway).
"""
import json, os, re, struct, sys
from fractions import Fraction
import numpy as np

ROOT = r'D:/Reverse/BP-Decomp_Workflow'
sys.path.insert(0, os.path.join(ROOT, 'tools', 're'))
import x360rd  # noqa

EXPORT = os.path.join(ROOT, '.ida-exports', 'BURNOUT_X360_ARTIST.XEX')

def f2u(f):
    return struct.unpack('>I', struct.pack('>f', np.float32(f)))[0]

def u2f(u):
    return np.float32(struct.unpack('>f', struct.pack('>I', u & 0xFFFFFFFF))[0])

def fr_to_f32(fr):
    # correctly rounded Fraction -> f32 (round-to-nearest-even)
    if fr == 0:
        return np.float32(0.0)
    sign = -1 if fr < 0 else 1
    a = abs(fr)
    # find exponent
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if Fraction(2) ** e > a:
        e -= 1
    # a in [2^e, 2^(e+1))
    if e < -126:
        e = -126  # denormal range: quantum 2^-149
    q = Fraction(2) ** (e - 23)
    m = a / q
    n = m.numerator // m.denominator
    rem = m - n
    if rem > Fraction(1, 2) or (rem == Fraction(1, 2) and (n & 1)):
        n += 1
    val = Fraction(n) * q
    return np.float32(sign * float(val))

def vmax(a, b):
    a = np.float32(a); b = np.float32(b)
    if np.isnan(a): return a
    if np.isnan(b): return b
    return a if a > b else b

def vmin(a, b):
    a = np.float32(a); b = np.float32(b)
    if np.isnan(a): return a
    if np.isnan(b): return b
    return a if a < b else b

def rf32(x):
    return float(np.float32(x))

def ffr(f):
    return Fraction(float(f))

def fma(a, c, b):
    """a*c + b, exact, rounded once; an exact zero is -0 only when the product and b are both -0."""
    a = np.float32(a); c = np.float32(c); b = np.float32(b)
    if not (np.isfinite(a) and np.isfinite(b) and np.isfinite(c)):
        return np.float32(np.float64(a) * np.float64(c) + np.float64(b))
    fr = ffr(a) * ffr(c) + ffr(b)
    if fr == 0:
        product_is_zero = (a == 0 or c == 0)
        product_negative = bool(np.signbit(a)) != bool(np.signbit(c))
        if product_is_zero and b == 0 and product_negative and bool(np.signbit(b)):
            return np.float32(-0.0)
        return np.float32(0.0)
    return fr_to_f32(fr)


def nmsub(a, c, b):
    """vnmsubfp: -(a*c - b), one rounding, the result negated (zeros too); a NaN keeps its sign."""
    r = fma(a, c, -np.float32(b))
    if np.isnan(r):
        return r
    return np.float32(-r)

class Func:
    def __init__(self, addr):
        d = json.load(open(os.path.join(EXPORT, '0x%08X.json' % addr)))
        self.addr = addr
        self.name = d['name']
        self.ins = []   # (addr, word, mnemonic, ops_text)
        for line in d['assembly'].splitlines():
            m = re.match(r'0x([0-9A-F]+)\s+(\S+)\s*(.*)$', line.strip())
            if not m:
                continue
            a = int(m.group(1), 16)
            w = struct.unpack('>I', x360rd.rd(a, 4))[0]
            ops = m.group(3).split('#')[0].strip()
            self.ins.append((a, w, m.group(2), ops))
        self.index = {a: i for i, (a, _, _, _) in enumerate(self.ins)}

class Mem:
    def __init__(self):
        self.regions = []  # (base, bytearray)
    def add(self, base, size, data=None):
        b = bytearray(size) if data is None else bytearray(data)
        self.regions.append((base, b))
        return b
    def _find(self, a, n):
        for base, b in self.regions:
            if base <= a and a + n <= base + len(b):
                return b, a - base
        raise KeyError('unmapped %08X' % a)
    def rd(self, a, n):
        try:
            b, o = self._find(a, n)
        except KeyError:
            if 0x82000000 <= a < 0x83400000:
                return bytes(x360rd.rd(a, n))
            raise
        return bytes(b[o:o + n])
    def wr(self, a, data):
        b, o = self._find(a, len(data))
        b[o:o + len(data)] = data

VOLATILE_V = list(range(0, 14)) + list(range(32, 64))

class CPU:
    def __init__(self, mem, funcs):
        self.mem = mem
        self.funcs = funcs     # addr -> Func
        self.gpr = [0] * 32
        self.vr = [(0, 0, 0, 0)] * 128
        self.cr = [0] * 8      # 4-bit fields: LT GT EQ SO -> bits 8,4,2,1
        self.fpr = [0.0] * 32  # python floats (doubles); single ops round through np.float32
        self.trace = False
        self.asserts = 0

    # ---------------------------------------------------------------- helpers
    def r(self, name):
        name = name.strip()
        if name == 'r0' and False:
            return 0
        return self.gpr[int(name[1:])]

    def setr(self, name, v):
        self.gpr[int(name.strip()[1:])] = v & 0xFFFFFFFF

    def ea_ra0(self, ra, disp):
        base = 0 if ra == 0 else self.gpr[ra]
        return (base + disp) & 0xFFFFFFFF

    def vf(self, i):
        # VMX non-Java mode: denormal inputs read as (signed) zero
        out = []
        for x in self.vr[i]:
            if (x & 0x7F800000) == 0:
                x &= 0x80000000
            out.append(u2f(x))
        return out

    def setvf(self, i, fl):
        # VMX non-Java mode: denormal results flush to (signed) zero
        out = []
        for x in fl:
            u = f2u(x)
            if (u & 0x7F800000) == 0:
                u &= 0x80000000
            out.append(u)
        self.vr[i] = tuple(out)

    def ldv(self, a):
        a &= ~0xF
        data = self.mem.rd(a, 16)
        return struct.unpack('>4I', data)

    def stv(self, a, v):
        a &= ~0xF
        self.mem.wr(a, struct.pack('>4I', *v))

    def setcr(self, field, lt, gt, eq):
        self.cr[field] = (8 if lt else 0) | (4 if gt else 0) | (2 if eq else 0)

    # ---------------------------------------------------------------- vector decode
    def exec_vector(self, a, w, mn):
        op = w >> 26
        # --- VMX128 loads/stores (opcode 4) -----------------------------------------
        if op == 4 and (w & 0x7F3) in (0x0C3, 0x1C3):
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            ra = (w >> 16) & 0x1F
            rb = (w >> 11) & 0x1F
            ea = ((0 if ra == 0 else self.gpr[ra]) + self.gpr[rb]) & 0xFFFFFFFF
            if (w & 0x7F3) == 0x0C3:
                self.vr[vd] = self.ldv(ea)
            else:
                self.stv(ea, self.vr[vd])
            return True
        if op == 5:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            if (w & 0x210) == 0x000:   # vperm128 vD, vA, vB, vC(3-bit)
                vc = (w >> 6) & 7
                src = struct.pack('>4I', *self.vr[va]) + struct.pack('>4I', *self.vr[vb])
                ctl = struct.pack('>4I', *self.vr[vc])
                self.vr[vd] = struct.unpack('>4I', bytes(src[c & 0x1F] for c in ctl))
                return True
            xo = w & 0x3D0
            if xo == 0x2D0:   # vor128
                self.vr[vd] = tuple(self.vr[va][i] | self.vr[vb][i] for i in range(4))
                return True
            if xo == 0x210:   # vand128
                self.vr[vd] = tuple(self.vr[va][i] & self.vr[vb][i] for i in range(4))
                return True
            A = self.vf(va); B = self.vf(vb); D = self.vf(vd)
            if xo == 0x0D0:   # vmaddfp128 vD = vA*vB + vD (fused)
                self.setvf(vd, [fma(A[i], B[i], D[i]) for i in range(4)])
                return True
            if xo == 0x150:   # vnmsubfp128 vD = vD - vA*vB (fused)
                self.setvf(vd, [nmsub(A[i], B[i], D[i]) for i in range(4)])
                return True
            if xo == 0x090:   # vmulfp128
                self.setvf(vd, [np.float32(A[i] * B[i]) for i in range(4)])
            elif xo == 0x050: # vsubfp128
                self.setvf(vd, [np.float32(A[i] - B[i]) for i in range(4)])
            elif xo == 0x010: # vaddfp128
                self.setvf(vd, [np.float32(A[i] + B[i]) for i in range(4)])
            elif xo == 0x1D0: # vmsum4fp128
                if all(np.isfinite(A)) and all(np.isfinite(B)):
                    s = ffr(A[0]) * ffr(B[0]) + ffr(A[1]) * ffr(B[1]) + ffr(A[2]) * ffr(B[2]) + ffr(A[3]) * ffr(B[3])
                    r = fr_to_f32(s)
                else:
                    r = np.float32(sum(np.float64(A[i]) * np.float64(B[i]) for i in range(4)))
                self.setvf(vd, [r] * 4)
            elif xo == 0x190: # vmsum3fp128
                if all(np.isfinite(A[:3])) and all(np.isfinite(B[:3])):
                    s = ffr(A[0]) * ffr(B[0]) + ffr(A[1]) * ffr(B[1]) + ffr(A[2]) * ffr(B[2])
                    r = fr_to_f32(s)
                else:
                    r = np.float32(sum(np.float64(A[i]) * np.float64(B[i]) for i in range(3)))
                self.setvf(vd, [r] * 4)
            else:
                raise NotImplementedError('%08X op5 %08X %s' % (a, w, mn))
            return True
        if op == 6:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            if (w & 0x730) == 0x210:   # vpermwi128
                perm = ((w >> 16) & 0x1F) | (((w >> 6) & 7) << 5)
                B = self.vr[vb]
                self.vr[vd] = tuple(B[(perm >> (6 - 2 * i)) & 3] for i in range(4))
                return True
            if (w & 0x3D0) == 0x300 or (w & 0x3D0) == 0x340:   # vmrghw128 / vmrglw128
                va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
                A = self.vr[va]; B = self.vr[vb]
                if (w & 0x3D0) == 0x300:
                    self.vr[vd] = (A[0], B[0], A[1], B[1])
                else:
                    self.vr[vd] = (A[2], B[2], A[3], B[3])
                return True
            if (w & 0x7F0) == 0x730:   # vspltw128 vD, vB, UIMM
                u = (w >> 16) & 3
                B = self.vr[vb]
                self.vr[vd] = (B[u],) * 4
                return True
            if (w & 0x7F0) == 0x2B0:   # vcsxwfp128 vD, vB, UIMM (signed word -> float * 2^-UIMM)
                u = (w >> 16) & 0x1F
                B = self.vr[vb]
                self.setvf(vd, [np.float32(struct.unpack('>i', struct.pack('>I', B[i]))[0] / (2 ** u)) for i in range(4)])
                return True
            if (w & 0x3D0) in (0x280, 0x2C0):   # vmaxfp128 / vminfp128
                va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
                Af = self.vf(va); Bf = self.vf(vb)
                if (w & 0x3D0) == 0x280:
                    self.setvf(vd, [vmax(Af[i], Bf[i]) for i in range(4)])
                else:
                    self.setvf(vd, [vmin(Af[i], Bf[i]) for i in range(4)])
                return True
            if (w & 0x7F0) == 0x770:   # vspltisw128
                simm = (w >> 16) & 0x1F
                simm = simm - 32 if simm & 0x10 else simm
                self.vr[vd] = ((simm & 0xFFFFFFFF),) * 4
                return True
            if (w & 0x390) in (0x000, 0x080, 0x100, 0x200) and (w & 0x7F0) not in (0x770,):
                va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
                kind = w & 0x390
                rc = (w >> 6) & 1
                A = self.vr[va]; B = self.vr[vb]
                Af = self.vf(va); Bf = self.vf(vb)
                if kind == 0x000:
                    res = [Af[i] == Bf[i] for i in range(4)]
                elif kind == 0x080:
                    res = [Af[i] >= Bf[i] for i in range(4)]
                elif kind == 0x100:
                    res = [Af[i] > Bf[i] for i in range(4)]
                else:
                    res = [A[i] == B[i] for i in range(4)]
                self.vr[vd] = tuple(0xFFFFFFFF if t else 0 for t in res)
                if rc:
                    self.setcr(6, all(res), False, not any(res))
                return True
            if (w & 0x730) == 0x710:   # vrlimi128
                imm = (w >> 16) & 0x1F
                z = (w >> 6) & 3
                B = self.vr[vb]
                rot = [B[(i + z) & 3] for i in range(4)]
                D = list(self.vr[vd])
                for i in range(4):
                    if (imm >> (3 - i)) & 1:
                        D[i] = rot[i]
                self.vr[vd] = tuple(D)
                return True
            if (w & 0x7F0) == 0x330:   # vrfim128 vD, vB (round toward -inf; NaN / inf / -0 kept)
                Bf = self.vf(vb)
                self.setvf(vd, [np.float32(np.floor(np.float64(Bf[i]))) for i in range(4)])
                return True
            if (w & 0x7F0) == 0x630:   # vrefp128 vD, vB (estimate modelled exact-then-rounded, as vrefp)
                Bf = self.vf(vb)
                out = []
                for i in range(4):
                    x = Bf[i]
                    if np.isnan(x):
                        out.append(x)
                    elif x == 0:
                        out.append(np.float32(np.copysign(np.inf, x)))
                    elif np.isinf(x):
                        out.append(np.float32(np.copysign(0.0, x)))
                    else:
                        out.append(fr_to_f32(Fraction(1) / ffr(x)))
                self.setvf(vd, out)
                if any(np.isnan(Bf)):
                    self.vr[vd] = tuple((self.vr[vb][i] | 0x00400000) if np.isnan(Bf[i]) else self.vr[vd][i]
                                        for i in range(4))
                return True
            raise NotImplementedError('%08X op6 %08X %s' % (a, w, mn))
        if op != 4:
            return False
        # --- classic VMX (opcode 4) ---------------------------------------------------
        vd = (w >> 21) & 0x1F
        va = (w >> 16) & 0x1F
        vb = (w >> 11) & 0x1F
        vc = (w >> 6) & 0x1F
        x6 = w & 0x3F
        if x6 in (42, 43, 44, 46, 47):
            A = self.vr[va]; B = self.vr[vb]; C = self.vr[vc]
            if x6 == 42:  # vsel
                self.vr[vd] = tuple((A[i] & ~C[i] | B[i] & C[i]) & 0xFFFFFFFF for i in range(4))
            elif x6 == 43:  # vperm
                src = struct.pack('>4I', *A) + struct.pack('>4I', *B)
                ctl = struct.pack('>4I', *C)
                out = bytes(src[c & 0x1F] for c in ctl)
                self.vr[vd] = struct.unpack('>4I', out)
            elif x6 == 44:  # vsldoi
                sh = (w >> 6) & 0xF
                src = struct.pack('>4I', *A) + struct.pack('>4I', *B)
                self.vr[vd] = struct.unpack('>4I', src[sh:sh + 16])
            elif x6 == 46:  # vmaddfp  D = A*C + B  (fused)
                Af = self.vf(va); Bf = self.vf(vb); Cf = self.vf(vc)
                self.setvf(vd, [fma(Af[i], Cf[i], Bf[i]) for i in range(4)])
            elif x6 == 47:  # vnmsubfp D = B - A*C (fused)
                Af = self.vf(va); Bf = self.vf(vb); Cf = self.vf(vc)
                self.setvf(vd, [nmsub(Af[i], Cf[i], Bf[i]) for i in range(4)])
            return True
        x10 = w & 0x3FF
        rc = (w >> 10) & 1
        if x10 in (198, 454, 710, 134):
            A = self.vr[va]; B = self.vr[vb]
            Af = self.vf(va); Bf = self.vf(vb)
            if x10 == 198:
                res = [Af[i] == Bf[i] for i in range(4)]
            elif x10 == 454:
                res = [Af[i] >= Bf[i] for i in range(4)]
            elif x10 == 710:
                res = [Af[i] > Bf[i] for i in range(4)]
            else:
                res = [A[i] == B[i] for i in range(4)]
            self.vr[vd] = tuple(0xFFFFFFFF if t else 0 for t in res)
            if rc:
                self.setcr(6, all(res), False, not any(res))
            return True
        x11 = w & 0x7FF
        A = self.vr[va]; B = self.vr[vb]
        if x11 == 10:      # vaddfp
            Af = self.vf(va); Bf = self.vf(vb)
            self.setvf(vd, [np.float32(Af[i] + Bf[i]) for i in range(4)])
        elif x11 == 74:    # vsubfp
            Af = self.vf(va); Bf = self.vf(vb)
            self.setvf(vd, [np.float32(Af[i] - Bf[i]) for i in range(4)])
        elif x11 == 140:   # vmrghw
            self.vr[vd] = (A[0], B[0], A[1], B[1])
        elif x11 == 396:   # vmrglw
            self.vr[vd] = (A[2], B[2], A[3], B[3])
        elif x11 == 652:   # vspltw
            u = va & 3
            self.vr[vd] = (B[u],) * 4
        elif x11 == 908:   # vspltisw
            s = va if va < 16 else va - 32
            self.vr[vd] = ((s & 0xFFFFFFFF),) * 4
        elif x11 == 388:   # vslw
            self.vr[vd] = tuple((A[i] << (B[i] & 31)) & 0xFFFFFFFF for i in range(4))
        elif x11 == 1028:  # vand
            self.vr[vd] = tuple(A[i] & B[i] for i in range(4))
        elif x11 == 1092:  # vandc
            self.vr[vd] = tuple(A[i] & ~B[i] & 0xFFFFFFFF for i in range(4))
        elif x11 == 1156:  # vor
            self.vr[vd] = tuple(A[i] | B[i] for i in range(4))
        elif x11 == 1220:  # vxor
            self.vr[vd] = tuple(A[i] ^ B[i] for i in range(4))
        elif x11 == 1284:  # vnor
            self.vr[vd] = tuple(~(A[i] | B[i]) & 0xFFFFFFFF for i in range(4))
        elif x11 == 842:   # vcfsx
            u = va
            vals = [np.float32(struct.unpack('>i', struct.pack('>I', B[i]))[0] / (2 ** u)) for i in range(4)]
            self.setvf(vd, vals)
        elif x11 == 266:   # vrefp
            Bf = self.vf(vb)
            out = []
            for i in range(4):
                x = Bf[i]
                if np.isnan(x):
                    out.append(x)
                elif x == 0:
                    out.append(np.float32(np.copysign(np.inf, x)))
                elif np.isinf(x):
                    out.append(np.float32(np.copysign(0.0, x)))
                else:
                    out.append(fr_to_f32(Fraction(1) / ffr(x)))
            self.setvf(vd, out)
            if any(np.isnan(Bf)):
                self.vr[vd] = tuple((self.vr[vb][i] | 0x00400000) if np.isnan(Bf[i]) else self.vr[vd][i]
                                    for i in range(4))
        elif x11 == 330:   # vrsqrtefp
            Bf = self.vf(vb)
            out = []
            for i in range(4):
                x = Bf[i]
                if np.isnan(x):
                    out.append(x)
                elif x == 0:
                    out.append(np.float32(np.copysign(np.inf, x)))
                elif x < 0:
                    out.append(np.float32(np.nan))
                elif np.isinf(x):
                    out.append(np.float32(0.0))
                else:
                    out.append(np.float32(1.0 / np.sqrt(np.float64(x))))
            self.setvf(vd, out)
            fix = []
            for i in range(4):
                if np.isnan(Bf[i]):
                    fix.append(self.vr[vb][i] | 0x00400000)
                elif not np.isnan(Bf[i]) and Bf[i] < 0:
                    fix.append(0x7FC00000)
                else:
                    fix.append(self.vr[vd][i])
            self.vr[vd] = tuple(fix)
        elif x11 == 1034:  # vmaxfp
            Af = self.vf(va); Bf = self.vf(vb)
            self.setvf(vd, [vmax(Af[i], Bf[i]) for i in range(4)])
        elif x11 == 1098:  # vminfp
            Af = self.vf(va); Bf = self.vf(vb)
            self.setvf(vd, [vmin(Af[i], Bf[i]) for i in range(4)])
        elif x11 == 906:   # vctuxs vD, vB, UIMM (float * 2^UIMM -> u32, truncate, saturate)
            Bf = self.vf(vb)
            out = []
            for i in range(4):
                x = float(Bf[i]) * (2 ** va)
                if x != x or x <= 0.0:
                    out.append(0)
                elif x >= 4294967295.0:
                    out.append(0xFFFFFFFF)
                else:
                    out.append(int(x))
            self.vr[vd] = tuple(out)
        elif x11 == 714:   # vrfim (round toward -inf)
            Bf = self.vf(vb)
            self.setvf(vd, [np.float32(np.floor(np.float64(Bf[i]))) for i in range(4)])
        elif x11 == 1540:  # vmr / vor alias?  (not used)
            raise NotImplementedError
        else:
            # lvsl: primary 31, handled elsewhere
            raise NotImplementedError('%08X op4 xo11=%d %08X %s' % (a, x11, w, mn))
        return True

    # ---------------------------------------------------------------- scalar
    def cond(self, bo_name, crf):
        f = self.cr[crf]
        lt, gt, eq = bool(f & 8), bool(f & 4), bool(f & 2)
        return {'beq': eq, 'bne': not eq, 'blt': lt, 'bge': not lt, 'bgt': gt, 'ble': not gt}[bo_name]

    def run(self, faddr, depth=0):
        fn = self.funcs[faddr]
        pc = 0
        steps = 0
        while True:
            steps += 1
            if steps > 2000000:
                raise RuntimeError('runaway')
            a, w, mn, ops = fn.ins[pc]
            nxt = pc + 1
            if self.trace:
                print('%s%08X %-10s %s' % ('  ' * depth, a, mn, ops))
            if self.exec_vector(a, w, mn):
                pc = nxt
                continue
            o = [x.strip() for x in ops.split(',')] if ops else []
            if mn in ('mflr', 'mtlr', 'ld', 'stwu'):
                if mn == 'stwu':
                    # stwu r1, back_chain(r1) with frame size from the word
                    d = w & 0xFFFF
                    d = d - 0x10000 if d & 0x8000 else d
                    self.gpr[1] = (self.gpr[1] + d) & 0xFFFFFFFF
                pass
            elif mn == 'bl':
                t = o[0]
                if '__save' in t or '__rest' in t:
                    pass
                elif 'CgsDev__Assert' in t:
                    self.asserts += 1
                else:
                    # resolve target
                    tw = w & 0x03FFFFFC
                    if tw & 0x02000000:
                        tw -= 0x04000000
                    tgt = (a + tw) & 0xFFFFFFFF
                    saved_g = list(self.gpr[13:32])
                    saved_v = {i: self.vr[i] for i in range(128) if i not in VOLATILE_V}
                    saved_cr = list(self.cr)
                    self.run(tgt, depth + 1)
                    self.gpr[13:32] = saved_g
                    for i, v in saved_v.items():
                        self.vr[i] = v
                    # cr2-cr4 are nonvolatile; keep simple: restore all but cr0/cr1/cr5-7? restore cr2-4
                    for i in (2, 3, 4):
                        self.cr[i] = saved_cr[i]
            elif mn == 'b':
                if '__rest' in o[0]:
                    if fn.addr == 0x8228CDA8 or True:
                        # frame pop handled by addi r1 before
                        pass
                    return
                tgt = int(o[0].split('_')[-1], 16)
                nxt = fn.index[tgt]
            elif mn == 'blr':
                return
            elif mn in ('beq', 'bne', 'blt', 'bge', 'bgt', 'ble'):
                crf = int(o[0][2:]) if o[0].startswith('cr') else 0
                tgt = o[-1]
                if self.cond(mn, crf):
                    nxt = fn.index[int(tgt.split('_')[-1], 16)]
            elif mn == 'li':
                self.setr(o[0], self._simm(w))
            elif mn == 'lis':
                self.setr(o[0], (w & 0xFFFF) << 16)
            elif mn == 'addi':
                base = 0 if o[1] == 'r0' else self.r(o[1])
                self.setr(o[0], base + self._simm(w))
            elif mn == 'addis':
                base = 0 if o[1] == 'r0' else self.r(o[1])
                self.setr(o[0], base + (self._imm(o[2]) << 16))
            elif mn == 'mr':
                self.setr(o[0], self.r(o[1]))
            elif mn == 'add':
                self.setr(o[0], self.r(o[1]) + self.r(o[2]))
            elif mn == 'subf':
                self.setr(o[0], self.r(o[2]) - self.r(o[1]))
            elif mn == 'ori':
                self.setr(o[0], self.r(o[1]) | (w & 0xFFFF))
            elif mn == 'xori':
                self.setr(o[0], self.r(o[1]) ^ (w & 0xFFFF))
            elif mn == 'mulhwu':
                self.setr(o[0], (self.r(o[1]) * self.r(o[2])) >> 32)
            elif mn == 'slwi':
                self.setr(o[0], self.r(o[1]) << int(o[2], 0))
            elif mn == 'srwi':
                self.setr(o[0], self.r(o[1]) >> int(o[2], 0))
            elif mn == 'clrlwi':
                n = int(o[2], 0)
                self.setr(o[0], self.r(o[1]) & ((1 << (32 - n)) - 1))
            elif mn == 'extrwi':
                n = int(o[2], 0); b = int(o[3], 0)
                self.setr(o[0], (self.r(o[1]) >> (32 - b - n)) & ((1 << n) - 1))
            elif mn == 'rlwinm':
                sh = int(o[2], 0); mb = int(o[3], 0); me = int(o[4], 0)
                v = self.r(o[1])
                rot = ((v << sh) | (v >> (32 - sh))) & 0xFFFFFFFF if sh else v
                mask = 0
                for bit in range(32):
                    if (mb <= me and mb <= bit <= me) or (mb > me and (bit >= mb or bit <= me)):
                        mask |= 1 << (31 - bit)
                self.setr(o[0], rot & mask)
            elif mn == 'cntlzw':
                v = self.r(o[1])
                self.setr(o[0], 32 - v.bit_length())
            elif mn in ('cmplwi', 'cmpwi', 'cmplw', 'cmpw'):
                if o[0].startswith('cr'):
                    crf = int(o[0][2:]); x = self.r(o[1]); y = o[2]
                else:
                    crf = 0; x = self.r(o[0]); y = o[1]
                if mn == 'cmpwi':
                    yv = self._simm(w) & 0xFFFFFFFF
                elif mn == 'cmplwi':
                    yv = w & 0xFFFF
                else:
                    yv = self.r(y)
                if mn in ('cmpwi', 'cmpw'):
                    xs = x - (1 << 32) if x & 0x80000000 else x
                    ys = yv - (1 << 32) if yv & 0x80000000 else yv
                    self.setcr(crf, xs < ys, xs > ys, xs == ys)
                else:
                    self.setcr(crf, x < yv, x > yv, x == yv)
            elif mn in ('lwz', 'stw', 'lbz', 'stb'):
                m = re.match(r'(.+)\((r\d+)\)', o[1])
                disp = self._disp(m.group(1), w)
                ea = (self.r(m.group(2)) + disp) & 0xFFFFFFFF
                if mn == 'lwz':
                    self.setr(o[0], struct.unpack('>I', self.mem.rd(ea, 4))[0])
                elif mn == 'stw':
                    self.mem.wr(ea, struct.pack('>I', self.r(o[0])))
                elif mn == 'lbz':
                    self.setr(o[0], self.mem.rd(ea, 1)[0])
                else:
                    self.mem.wr(ea, bytes([self.r(o[0]) & 0xFF]))
            elif mn == 'lvsl':
                # lvsl vD, rA, rB
                vd = int(o[0][1:])
                ra = int(o[1][1:]) if o[1] != '0' else 0
                rb = int(o[2][1:])
                ea = ((0 if ra == 0 else self.gpr[ra]) + self.gpr[rb]) & 0xFFFFFFFF
                sh = ea & 0xF
                self.vr[vd] = struct.unpack('>4I', bytes(range(sh, sh + 16)))
            elif mn in ('twllei', 'mfmsr', 'mtmsr', '.long'):
                pass
            elif mn == 'mullw':
                self.setr(o[0], (self.r(o[1]) * self.r(o[2])) & 0xFFFFFFFF)
            elif mn in ('divwu', 'divwu.'):
                d = self.r(o[2])
                q = (self.r(o[1]) // d) if d else 0
                self.setr(o[0], q)
                if mn == 'divwu.':
                    self.setcr(0, False, q != 0, q == 0)
            elif mn == 'insrwi':
                n = int(o[2], 0); b = int(o[3], 0)
                v = self.r(o[1]) & ((1 << n) - 1)
                sh = 32 - b - n
                mask = ((1 << n) - 1) << sh
                self.setr(o[0], (self.r(o[0]) & ~mask) | (v << sh))
            elif mn == 'clrldi':
                self.setr(o[0], self.r(o[1]))   # 32-bit model: the upper half is already zero
            elif mn in ('lfs', 'stfs', 'lfd', 'std', 'ld'):
                m = re.match(r'(.+)\((r\d+)\)', o[1])
                disp = self._disp(m.group(1), w)
                if mn in ('std', 'ld'):
                    disp &= ~3
                ea = (self.r(m.group(2)) + disp) & 0xFFFFFFFF
                if mn == 'lfs':
                    self.fpr[int(o[0][1:])] = float(struct.unpack('>f', self.mem.rd(ea, 4))[0])
                elif mn == 'stfs':
                    self.mem.wr(ea, struct.pack('>f', np.float32(self.fpr[int(o[0][1:])])))
                elif mn == 'lfd':
                    self.fpr[int(o[0][1:])] = struct.unpack('>d', self.mem.rd(ea, 8))[0]
                elif mn == 'std':
                    try:
                        self.mem.wr(ea, struct.pack('>Q', self.r(o[0]) & 0xFFFFFFFF))
                    except KeyError:
                        pass
            elif mn in ('fadds', 'fsubs', 'fmuls', 'fdivs'):
                a_ = self.fpr[int(o[1][1:])]; b_ = self.fpr[int(o[2][1:])]
                if mn == 'fadds':
                    r_ = a_ + b_
                elif mn == 'fsubs':
                    r_ = a_ - b_
                elif mn == 'fmuls':
                    r_ = a_ * b_
                else:
                    r_ = (a_ / b_) if b_ != 0 else (float('inf') if a_ > 0 else float('-inf') if a_ < 0 else float('nan'))
                self.fpr[int(o[0][1:])] = rf32(r_)
            elif mn == 'fsel':
                a_ = self.fpr[int(o[1][1:])]
                self.fpr[int(o[0][1:])] = self.fpr[int(o[2][1:])] if (a_ == a_ and a_ >= 0.0) else self.fpr[int(o[3][1:])]
            elif mn == 'frsp':
                self.fpr[int(o[0][1:])] = rf32(self.fpr[int(o[1][1:])])
            elif mn == 'fcfid':
                bits = struct.unpack('>q', struct.pack('>d', self.fpr[int(o[1][1:])]))[0]
                self.fpr[int(o[0][1:])] = float(bits)
            elif mn == 'fmr':
                self.fpr[int(o[0][1:])] = self.fpr[int(o[1][1:])]
            elif mn == 'lvlx':
                vd = int(o[0][1:])
                ra = 0 if o[1] == 'r0' else int(o[1][1:])
                rb = int(o[2][1:])
                ea = ((0 if ra == 0 else self.gpr[ra]) + self.gpr[rb]) & 0xFFFFFFFF
                n = 16 - (ea & 0xF)
                data = self.mem.rd(ea, n) + bytes(16 - n)
                self.vr[vd] = struct.unpack('>4I', data)
            elif mn == 'stvewx':
                vs = int(o[0][1:])
                ra = 0 if o[1] == 'r0' else int(o[1][1:])
                rb = int(o[2][1:])
                ea = ((0 if ra == 0 else self.gpr[ra]) + self.gpr[rb]) & 0xFFFFFFFC
                self.mem.wr(ea, struct.pack('>I', self.vr[vs][(ea & 0xF) >> 2]))
            elif mn == 'mfocrf':
                fxm = int(o[1], 0)
                v = 0
                for f in range(8):
                    if fxm & (0x80 >> f):
                        v |= self.cr[f] << (4 * (7 - f))
                self.setr(o[0], v)
            else:
                raise NotImplementedError('%08X %s %s' % (a, mn, ops))
            pc = nxt

    def _simm(self, w):
        d = w & 0xFFFF
        return d - 0x10000 if d & 0x8000 else d

    def _disp(self, text, w):
        # use the raw 16-bit displacement from the word (IDA prints stack var names)
        d = w & 0xFFFF
        return d - 0x10000 if d & 0x8000 else d

    def _imm(self, text, sym_ha=False, sym_l=False):
        text = text.strip()
        m = re.match(r'(unk|flt|dword|stru|off)_([0-9A-F]+)@(ha|l)', text)
        if m:
            addr = int(m.group(2), 16)
            if m.group(3) == 'ha':
                return ((addr + 0x8000) >> 16) & 0xFFFF
            lo = addr & 0xFFFF
            return lo - 0x10000 if lo & 0x8000 else lo
        if text.startswith('unk_') or text.startswith('flt_'):
            raise ValueError(text)
        m = re.match(r'0x330\+var_([0-9A-F]+)', text)
        if m:
            return 0x330 - int(m.group(1), 16)
        m = re.match(r'0xD0\+var_([0-9A-F]+)', text)
        if m:
            return 0xD0 - int(m.group(1), 16)
        m = re.match(r'var_([0-9A-F]+)', text)
        if m:
            return -int(m.group(1), 16)
        return int(text, 0)


def lis_fix(cpu, fn):
    pass


FUNCS = {}
def load():
    for a in (0x8291E600, 0x8291E150, 0x8291E478):
        FUNCS[a] = Func(a)
    return FUNCS

# ------------------------------------------------------------------------------------------
# Memory layout for experiments
CACHE = 0x40000000
TRIS = 0x41000000
LINES = 0x42000000
STACK_TOP = 0x43100000

def new_machine():
    mem = Mem()
    mem.add(CACHE, 0x1E10)
    mem.add(TRIS, 0x10000)
    mem.add(LINES, 0x1000)
    mem.add(STACK_TOP - 0x100000, 0x100000)
    # constant tables (runtime values after CRT init)
    t = mem.add(0x8327F240, 0x20)
    t[16:32] = b'\xff' * 16
    g = mem.add(0x8327F110, 0x10)
    g[:] = struct.pack('>4I', *([0x0004080C] * 4))
    p1 = mem.add(0x82CDA400, 0x10, x360rd.rd(0x82CDA400, 16))
    p2 = mem.add(0x82CDA3C0, 0x10, x360rd.rd(0x82CDA3C0, 16))
    e5 = mem.add(0x82FACBE0, 0x10)
    e5[:] = x360rd.rd(0x82004884, 4) * 4
    e8 = mem.add(0x82FACC00, 0x10)
    e8[:] = x360rd.rd(0x8200D5F0, 4) * 4
    cpu = CPU(mem, FUNCS)
    cpu.gpr[1] = STACK_TOP - 0x1000
    return cpu

def write_f32s(cpu, addr, vals):
    cpu.mem.wr(addr, b''.join(struct.pack('>f', np.float32(v)) for v in vals))

def write_u32s(cpu, addr, vals):
    cpu.mem.wr(addr, b''.join(struct.pack('>I', v & 0xFFFFFFFF) for v in vals))

def read_f32s(cpu, addr, n):
    return [np.float32(x) for x in struct.unpack('>%df' % n, cpu.mem.rd(addr, 4 * n))]

def read_u32s(cpu, addr, n):
    return list(struct.unpack('>%dI' % n, cpu.mem.rd(addr, 4 * n)))

if __name__ == '__main__':
    load()
    for a, f in FUNCS.items():
        print(hex(a), f.name, len(f.ins))
