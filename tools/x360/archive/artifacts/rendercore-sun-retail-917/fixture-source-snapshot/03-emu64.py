"""FX-CRASHVFX: a PC-driven PPC64 + VMX interpreter for running REAL ARTIST instruction words.

Differences from vmxemu.CPU.run (whose vector decoder this reuses unchanged):
  * the scalar side is decoded from the RAW WORD, not the IDA export text, so it runs any address
    (export holes included) and never trusts a simplified mnemonic (extrwi / inslwi / insrdi ...);
  * GPRs are 64-bit, with the PPC64 semantics of the 32-bit ops (rlwinm/rlwimi work on the low word,
    cmpw/cmplw compare the low word, lwz zero-extends) -- the inlined CgsNumeric::Random LCG needs
    ld / mulld / rldicl / rldimi / std on a real 64-bit register;
  * FPRs hold raw 64-bit patterns, so fctiwz / stfiwx / std-lfd-fcfid move bits, not values;
  * single-precision fused ops (fmadds / fmsubs / fnmadds / fnmsubs) are computed exactly and
    rounded ONCE to f32 (the architecture's definition);
  * calls go to Python hooks by target address, or are interpreted when unhooked; the save/restore
    helpers are no-ops and a `b` into a restore helper returns.
"""
import os, struct, sys
from fractions import Fraction
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vmxemu as V   # noqa: E402

M64 = (1 << 64) - 1
M32 = 0xFFFFFFFF

# __savegprlr_14..31 / __restgprlr_14..31 and __savefpr_14..31 / __restfpr_14..31 (the addresses
# ppcdis names in every prologue: __savegprlr_17 0x82C08EBC, __restgprlr_17 0x82C08F0C,
# __savefpr_21 0x82C0A5AC, __restfpr_21 0x82C0A5F8).
SAVE_HELPERS = [(0x82C08EB0, 0x82C08F00), (0x82C0A590, 0x82C0A5DC)]
REST_HELPERS = [(0x82C08F00, 0x82C08F60), (0x82C0A5DC, 0x82C0A640)]


def in_ranges(a, ranges):
    return any(lo <= a < hi for lo, hi in ranges)


def d2b(x):
    return struct.unpack('>Q', struct.pack('>d', x))[0]


def b2d(u):
    return struct.unpack('>d', struct.pack('>Q', u & M64))[0]


def rotl32(v, n):
    v &= M32
    n &= 31
    return ((v << n) | (v >> (32 - n))) & M32 if n else v


def rotl64(v, n):
    v &= M64
    n &= 63
    return ((v << n) | (v >> (64 - n))) & M64 if n else v


def mask64(mb, me):
    """PPC MASK(mb, me) over bits 0..63 (bit 0 = MSB)."""
    m = 0
    if mb <= me:
        for b in range(mb, me + 1):
            m |= 1 << (63 - b)
    else:
        for b in range(0, me + 1):
            m |= 1 << (63 - b)
        for b in range(mb, 64):
            m |= 1 << (63 - b)
    return m


def s16(x):
    x &= 0xFFFF
    return x - 0x10000 if x & 0x8000 else x


def s32(x):
    x &= M32
    return x - (1 << 32) if x & 0x80000000 else x


def s64(x):
    x &= M64
    return x - (1 << 64) if x >> 63 else x


def round_single(fr_or_float):
    """Round an exact value (Fraction) or a double to f32, as a Python float."""
    if isinstance(fr_or_float, Fraction):
        return float(V.fr_to_f32(fr_or_float))
    return float(np.float32(fr_or_float))


def fused(a, c, b, negate_b=False, negate_result=False):
    """Exact a*c +/- b, one rounding to f32 (non-finite operands fall back to double math)."""
    if not all(np.isfinite([a, b, c])):
        r = a * c + (-b if negate_b else b)
        r = -r if negate_result else r
        return float(np.float32(r))
    fr = Fraction(a) * Fraction(c) + (-Fraction(b) if negate_b else Fraction(b))
    if fr == 0:
        # An exact zero takes IEEE round-to-nearest's sign: -0 only when the exact product and the
        # (possibly negated) addend are both -0; canceling non-zero terms give +0. fnmadds / fnmsubs
        # then NEGATE that result, zero included (PowerPC: 'fmadd/fmsub, then negated').
        addend_negative = (np.signbit(b) != negate_b)
        product_is_zero = (a == 0 or c == 0)
        product_negative = bool(np.signbit(a)) != bool(np.signbit(c))
        zero = -0.0 if (product_is_zero and product_negative and b == 0 and addend_negative) else 0.0
        return -zero if negate_result else zero
    if negate_result:
        fr = -fr
    return round_single(fr)


class Return(Exception):
    pass


class CPU64(V.CPU):
    def __init__(self, mem):
        super().__init__(mem, {})
        self.gpr = [0] * 32
        self.fbits = [0] * 32
        self.ctr = 0
        self.hooks = {}          # addr -> callable(cpu)
        self.calls = []          # (target) log of unhooked calls
        self.asserts = []        # (line r5, message addr r3) of every FireAssert
        self.steps = 0
        self.max_steps = 5_000_000

    # ---- VMX128 ops the base decoder does not carry -------------------------------------------
    def exec_vector(self, a, w, mn):
        op = w >> 26
        if op == 5 and (w & 0x210) != 0:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            xo = w & 0x3D0
            if xo == 0x110:   # vmaddcfp128 vD = vA * vD + vB (fused)
                A = self.vf(va); B = self.vf(vb); D = self.vf(vd)
                self.setvf(vd, [V.fma(A[i], D[i], B[i]) for i in range(4)])
                return True
            if xo == 0x310:   # vxor128 (IDA-labelled at 0x82298264, word 0x17FF073C)
                self.vr[vd] = tuple(self.vr[va][i] ^ self.vr[vb][i] for i in range(4))
                return True
        if op == 6 and (w & 0x3D0) in (0x0D0, 0x1D0, 0x150):
            # vslw128 / vsrw128 / vsraw128 (tools/re/vmx_table.json: 0x0D0/0x0F0/0x4F0, 0x1D0/0x1F0,
            # 0x170 -- the 0x20 and 0x400 bits are vA's high bits): per word, vA shifted by vB & 31.
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            A = self.vr[va]; B = self.vr[vb]
            x = w & 0x3D0
            out = []
            for i in range(4):
                s = B[i] & 31
                if x == 0x0D0:
                    out.append((A[i] << s) & M32)
                elif x == 0x1D0:
                    out.append((A[i] & M32) >> s)
                else:
                    out.append((s32(A[i]) >> s) & M32)
            self.vr[vd] = tuple(out)
            return True
        return super().exec_vector(a, w, mn)

    # ---- register helpers --------------------------------------------------------------------
    def g(self, i):
        return self.gpr[i] & M64

    def setg(self, i, v):
        self.gpr[i] = v & M64

    def f(self, i):
        return b2d(self.fbits[i])

    def setf(self, i, x):
        self.fbits[i] = d2b(float(x))

    def crbit(self, bi):
        return (self.cr[bi >> 2] >> (3 - (bi & 3))) & 1

    def ea(self, ra, disp):
        return ((0 if ra == 0 else self.g(ra)) + disp) & M32

    def rd32(self, a):
        return struct.unpack('>I', self.mem.rd(a & M32, 4))[0]

    def rd64(self, a):
        return struct.unpack('>Q', self.mem.rd(a & M32, 8))[0]

    def wr32(self, a, v):
        self.mem.wr(a & M32, struct.pack('>I', v & M32))

    def wr64(self, a, v):
        self.mem.wr(a & M32, struct.pack('>Q', v & M64))

    def fetch(self, pc):
        return struct.unpack('>I', self.mem.rd(pc, 4))[0]

    # ---- the call boundary -------------------------------------------------------------------
    def call(self, target, depth):
        if in_ranges(target, SAVE_HELPERS):
            return
        if target in self.hooks:
            self.hooks[target](self)
            return
        saved_g = list(self.gpr[13:32])
        saved_f = list(self.fbits[14:32])
        saved_v = {i: self.vr[i] for i in range(128) if i not in V.VOLATILE_V}
        saved_cr = list(self.cr)
        saved_sp = self.gpr[1]
        self.calls.append(target)
        self.run(target, depth + 1)
        self.gpr[13:32] = saved_g
        self.fbits[14:32] = saved_f
        for i, v in saved_v.items():
            self.vr[i] = v
        for i in (2, 3, 4):
            self.cr[i] = saved_cr[i]
        self.gpr[1] = saved_sp

    # ---- the interpreter ---------------------------------------------------------------------
    def run(self, pc, depth=0):
        try:
            while True:
                self.steps += 1
                if self.steps > self.max_steps:
                    raise RuntimeError('runaway at %08X' % pc)
                w = self.fetch(pc)
                nxt = (pc + 4) & M32
                if self.trace:
                    print('%s%08X %08X' % ('  ' * depth, pc, w))
                if self.exec_vector(pc, w, 'w%08X' % w):
                    pc = nxt
                    continue
                pc = self.step(pc, w, nxt, depth)
        except Return:
            return

    def step(self, pc, w, nxt, depth):
        op = w >> 26
        rd = (w >> 21) & 31
        ra = (w >> 16) & 31
        rb = (w >> 11) & 31
        simm = s16(w)
        uimm = w & 0xFFFF

        if op == 18:                                   # b / bl
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            tgt = (li if (w & 2) else pc + li) & M32
            if w & 1:
                self.call(tgt, depth)
                return nxt
            if in_ranges(tgt, REST_HELPERS):
                raise Return()
            return tgt
        if op == 16:                                   # bc
            bo = rd
            bi = ra
            bd = s16(w & 0xFFFC)
            if not (bo & 4):
                self.ctr = (self.ctr - 1) & M64
            ctr_ok = (bo & 4) or ((self.ctr == 0) == bool(bo & 2))
            cond_ok = (bo & 16) or (self.crbit(bi) == ((bo >> 3) & 1))
            if ctr_ok and cond_ok:
                tgt = (bd if (w & 2) else pc + bd) & M32
                if w & 1:
                    self.call(tgt, depth)
                    return nxt
                return tgt
            return nxt
        if op == 19:
            xo = (w >> 1) & 0x3FF
            if xo == 16:                               # bclr
                bo = rd
                cond_ok = (bo & 16) or (self.crbit(ra) == ((bo >> 3) & 1))
                if not (bo & 4):
                    self.ctr = (self.ctr - 1) & M64
                ctr_ok = (bo & 4) or ((self.ctr == 0) == bool(bo & 2))
                if cond_ok and ctr_ok:
                    raise Return()
                return nxt
            if xo == 528:                              # bcctr (bctr / bctrl): the switch jump tables
                bo = rd
                cond_ok = (bo & 16) or (self.crbit(ra) == ((bo >> 3) & 1))
                if not cond_ok:
                    return nxt
                tgt = self.ctr & 0xFFFFFFFC & M32
                if w & 1:
                    self.call(tgt, depth)
                    return nxt
                return tgt
            raise NotImplementedError('%08X op19 xo=%d %08X' % (pc, xo, w))
        if op == 14:                                   # addi / li
            self.setg(rd, (0 if ra == 0 else self.g(ra)) + simm)
            return nxt
        if op == 15:                                   # addis / lis
            self.setg(rd, (0 if ra == 0 else self.g(ra)) + (simm << 16))
            return nxt
        if op == 7:                                    # mulli
            self.setg(rd, s64(self.g(ra)) * simm)
            return nxt
        if op in (12, 13):                             # addic / addic. (the inlined queue Clear's walk,
            v = (self.g(ra) + simm) & M64              #  LoadFXBundle 0x8229CF60); XER.CA is not modelled
            self.setg(rd, v)
            if op == 13:
                self.setcr(0, s64(v) < 0, s64(v) > 0, v == 0)
            return nxt
        if op == 24:                                   # ori  (rS = rd field, rA = ra)
            self.setg(ra, self.g(rd) | uimm)
            return nxt
        if op == 25:                                   # oris
            self.setg(ra, self.g(rd) | (uimm << 16))
            return nxt
        if op == 26:                                   # xori
            self.setg(ra, self.g(rd) ^ uimm)
            return nxt
        if op == 28:                                   # andi.
            v = self.g(rd) & uimm
            self.setg(ra, v)
            self.setcr(0, s64(v) < 0, s64(v) > 0, v == 0)
            return nxt
        if op in (10, 11):                             # cmpli / cmpi
            crf = rd >> 2
            l64 = rd & 1
            x = self.g(ra)
            if op == 11:
                xv = s64(x) if l64 else s32(x)
                self.setcr(crf, xv < simm, xv > simm, xv == simm)
            else:
                xv = x if l64 else (x & M32)
                self.setcr(crf, xv < uimm, xv > uimm, xv == uimm)
            return nxt
        if op == 21 or op == 20:                       # rlwinm / rlwimi
            sh = rb
            mb = (w >> 6) & 31
            me = (w >> 1) & 31
            r = rotl32(self.g(rd), sh)
            r = (r << 32) | r
            m = mask64(mb + 32, me + 32)
            if op == 21:
                v = r & m
            else:
                v = (r & m) | (self.g(ra) & ~m & M64)
            self.setg(ra, v)
            if w & 1:
                self.setcr(0, s64(v) < 0, s64(v) > 0, v == 0)
            return nxt
        if op == 30:                                   # MD-form rotates
            sh = rb | (((w >> 1) & 1) << 5)
            mbf = (w >> 5) & 0x3F
            mb = ((mbf & 1) << 5) | (mbf >> 1)
            xo = (w >> 2) & 7
            r = rotl64(self.g(rd), sh)
            if xo == 0:                                # rldicl
                v = r & mask64(mb, 63)
            elif xo == 1:                              # rldicr (mb field is me)
                v = r & mask64(0, mb)
            elif xo == 2:                              # rldic
                v = r & mask64(mb, 63 - sh)
            elif xo == 3:                              # rldimi
                m = mask64(mb, 63 - sh)
                v = (r & m) | (self.g(ra) & ~m & M64)
            else:
                raise NotImplementedError('%08X op30 xo=%d' % (pc, xo))
            self.setg(ra, v)
            return nxt
        if op in (32, 33, 34, 35, 40, 41, 42, 43):     # lwz lwzu lbz lbzu lhz lhzu lha lhau
            a = self.ea(ra, simm)
            if op in (32, 33):
                v = self.rd32(a)
            elif op in (34, 35):
                v = self.mem.rd(a, 1)[0]
            elif op in (40, 41):
                v = struct.unpack('>H', self.mem.rd(a, 2))[0]
            else:
                v = s16(struct.unpack('>H', self.mem.rd(a, 2))[0]) & M64
            self.setg(rd, v)
            if op & 1:
                self.setg(ra, a)
            return nxt
        if op in (36, 37, 38, 39, 44, 45):             # stw stwu stb stbu sth sthu
            a = self.ea(ra, simm)
            v = self.g(rd)
            if op in (36, 37):
                self.wr32(a, v)
            elif op in (38, 39):
                self.mem.wr(a, bytes([v & 0xFF]))
            else:
                self.mem.wr(a, struct.pack('>H', v & 0xFFFF))
            if op & 1:
                self.setg(ra, a)
            return nxt
        if op == 58:                                   # ld / ldu / lwa
            ds = s16(w & 0xFFFC)
            a = self.ea(ra, ds)
            sub = w & 3
            if sub == 2:
                self.setg(rd, s32(self.rd32(a)) & M64)
            else:
                self.setg(rd, self.rd64(a))
                if sub == 1:
                    self.setg(ra, a)
            return nxt
        if op == 62:                                   # std / stdu
            ds = s16(w & 0xFFFC)
            a = self.ea(ra, ds)
            self.wr64(a, self.g(rd))
            if (w & 3) == 1:
                self.setg(ra, a)
            return nxt
        if op in (48, 49):                             # lfs / lfsu
            a = self.ea(ra, simm)
            self.setf(rd, struct.unpack('>f', self.mem.rd(a, 4))[0])
            if op == 49:
                self.setg(ra, a)
            return nxt
        if op in (50, 51):                             # lfd / lfdu
            a = self.ea(ra, simm)
            self.fbits[rd] = self.rd64(a)
            if op == 51:
                self.setg(ra, a)
            return nxt
        if op in (52, 53):                             # stfs / stfsu
            a = self.ea(ra, simm)
            self.mem.wr(a, struct.pack('>f', np.float32(self.f(rd))))
            if op == 53:
                self.setg(ra, a)
            return nxt
        if op in (54, 55):                             # stfd / stfdu
            a = self.ea(ra, simm)
            self.wr64(a, self.fbits[rd])
            if op == 55:
                self.setg(ra, a)
            return nxt
        if op == 59:                                   # single-precision arithmetic (A-form)
            frt, fa, fb, fc = rd, ra, rb, (w >> 6) & 31
            xo = (w >> 1) & 31
            A, B, C = self.f(fa), self.f(fb), self.f(fc)
            # For two f32 operands the double result rounded to f32 IS the correctly rounded f32
            # result for + - * / (the double carries >= 2p+2 bits), so double math then one frsp.
            if xo == 21:                               # fadds
                r = float(np.float32(A + B))
            elif xo == 20:                             # fsubs
                r = float(np.float32(A - B))
            elif xo == 25:                             # fmuls
                r = float(np.float32(A * C))
            elif xo == 18:                             # fdivs
                r = float(np.float32(np.float64(A) / np.float64(B))) if B != 0 else float(np.float32(np.float64(A) / np.float64(B)))
            elif xo == 29:                             # fmadds
                r = fused(A, C, B)
            elif xo == 28:                             # fmsubs
                r = fused(A, C, B, negate_b=True)
            elif xo == 31:                             # fnmadds
                r = fused(A, C, B, negate_result=True)
            elif xo == 30:                             # fnmsubs
                r = fused(A, C, B, negate_b=True, negate_result=True)
            elif xo == 22:                             # fsqrts (correctly rounded: the double root of an f32
                if B != B:                             # rounds to the f32 root; -0 stays -0; < 0 is a QNaN)
                    r = B
                elif B < 0.0:
                    r = float('nan')
                else:
                    r = float(np.float32(np.sqrt(np.float64(B))))
            else:
                raise NotImplementedError('%08X op59 xo=%d' % (pc, xo))
            self.setf(frt, r)
            return nxt
        if op == 63:
            frt, fa, fb, fc = rd, ra, rb, (w >> 6) & 31
            xo5 = (w >> 1) & 31
            xo10 = (w >> 1) & 0x3FF
            if xo5 == 23:                              # fsel
                A = self.f(fa)
                self.fbits[frt] = self.fbits[fc] if (A == A and A >= 0.0) else self.fbits[fb]
                return nxt
            if xo5 == 21:                              # fadd (double)
                self.setf(frt, self.f(fa) + self.f(fb))
                return nxt
            if xo5 == 20:                              # fsub (double)
                self.setf(frt, self.f(fa) - self.f(fb))
                return nxt
            if xo5 == 25:                              # fmul (double)
                self.setf(frt, self.f(fa) * self.f(fc))
                return nxt
            if xo5 == 18:                              # fdiv (double)
                A, B = self.f(fa), self.f(fb)
                self.setf(frt, float(np.float64(A) / np.float64(B)))
                return nxt
            if xo5 == 29:                              # fmadd (double, fused)
                A, B, C = self.f(fa), self.f(fb), self.f(fc)
                self.setf(frt, float(Fraction(A) * Fraction(C) + Fraction(B)) if all(np.isfinite([A, B, C])) else A * C + B)
                return nxt
            if xo10 == 12:                             # frsp
                self.setf(frt, float(np.float32(self.f(fb))))
                return nxt
            if xo10 == 15:                             # fctiwz
                x = self.f(fb)
                if x != x:
                    v = 0x80000000
                elif x >= 2147483647.0:
                    v = 0x7FFFFFFF if x >= 2147483648.0 else int(x)
                elif x <= -2147483648.0:
                    v = 0x80000000
                else:
                    v = int(x) & M32                  # int() truncates toward zero
                self.fbits[frt] = 0xFFF8000000000000 | (v & M32)
                return nxt
            if xo10 == 815:                            # fctidz
                x = self.f(fb)
                if x != x:
                    v = 0x8000000000000000
                elif x >= 9223372036854775808.0:
                    v = 0x7FFFFFFFFFFFFFFF
                elif x < -9223372036854775808.0:
                    v = 0x8000000000000000
                else:
                    v = int(x) & M64                  # int() truncates toward zero
                self.fbits[frt] = v
                return nxt
            if xo10 == 846:                            # fcfid
                self.setf(frt, float(s64(self.fbits[fb])))
                return nxt
            if xo10 == 72:                             # fmr
                self.fbits[frt] = self.fbits[fb]
                return nxt
            if xo10 == 40:                             # fneg
                self.fbits[frt] = self.fbits[fb] ^ (1 << 63)
                return nxt
            if xo10 == 264:                            # fabs
                self.fbits[frt] = self.fbits[fb] & ~(1 << 63) & M64
                return nxt
            if xo10 == 0 or xo10 == 32:                # fcmpu / fcmpo
                A, B = self.f(fa), self.f(fb)
                crf = rd >> 2
                if A != A or B != B:
                    self.cr[crf] = 1
                else:
                    self.setcr(crf, A < B, A > B, A == B)
                return nxt
            raise NotImplementedError('%08X op63 xo=%d %08X' % (pc, xo10, w))
        if op == 31:
            xo = (w >> 1) & 0x3FF
            if xo == 83:                               # mfmsr (the job code's assert trap reads it)
                self.setg(rd, 0)
                return nxt
            if xo in (146, 178):                       # mtmsr / mtmsrd -- a debugger-break toggle: no-op here
                return nxt
            if xo == 444:                              # or / mr
                self.setg(ra, self.g(rd) | self.g(rb))
                return nxt
            if xo == 28:                               # and
                self.setg(ra, self.g(rd) & self.g(rb))
                return nxt
            if xo == 60:                               # andc
                self.setg(ra, self.g(rd) & ~self.g(rb) & M64)
                return nxt
            if xo == 124:                              # nor
                self.setg(ra, ~(self.g(rd) | self.g(rb)) & M64)
                return nxt
            if xo == 316:                              # xor
                self.setg(ra, self.g(rd) ^ self.g(rb))
                return nxt
            xo9 = (w >> 1) & 0x1FF
            if xo9 == 266:                             # add
                self.setg(rd, self.g(ra) + self.g(rb))
                return nxt
            if xo9 == 40:                              # subf
                self.setg(rd, self.g(rb) - self.g(ra))
                return nxt
            if xo9 == 104:                             # neg
                self.setg(rd, -self.g(ra))
                return nxt
            if xo9 == 233:                             # mulld
                self.setg(rd, s64(self.g(ra)) * s64(self.g(rb)))
                return nxt
            if xo9 == 235:                             # mullw
                self.setg(rd, s32(self.g(ra)) * s32(self.g(rb)))
                return nxt
            if xo9 == 11:                              # mulhwu (the compiler's divide-by-constant; low word kept)
                self.setg(rd, ((self.g(ra) & M32) * (self.g(rb) & M32)) >> 32)
                return nxt
            if xo9 == 75:                              # mulhw
                self.setg(rd, ((s32(self.g(ra)) * s32(self.g(rb))) >> 32) & M32)
                return nxt
            if xo9 == 459:                             # divwu
                d = self.g(rb) & M32
                self.setg(rd, ((self.g(ra) & M32) // d) if d else 0)
                return nxt
            if xo9 == 491:                             # divw
                d = s32(self.g(rb))
                n = s32(self.g(ra))
                self.setg(rd, int(n / d) if d else 0)
                return nxt
            if xo == 986:                              # extsw
                self.setg(ra, s32(self.g(rd)) & M64)
                return nxt
            if xo == 954:                              # extsb
                v = self.g(rd) & 0xFF
                self.setg(ra, (v - 0x100 if v & 0x80 else v) & M64)
                return nxt
            if xo == 922:                              # extsh
                self.setg(ra, s16(self.g(rd)) & M64)
                return nxt
            if xo == 26:                               # cntlzw
                v = self.g(rd) & M32
                self.setg(ra, 32 - v.bit_length())
                return nxt
            if xo == 24 or xo == 536:                  # slw / srw
                n = self.g(rb) & 63
                v = self.g(rd) & M32
                r = 0 if n > 31 else ((v << n) & M32 if xo == 24 else v >> n)
                self.setg(ra, r)
                return nxt
            if xo == 824:                              # srawi
                n = rb
                v = s32(self.g(rd))
                self.setg(ra, (v >> n) & M64)
                return nxt
            if xo == 0 or xo == 32:                    # cmp / cmpl
                crf = rd >> 2
                l64 = rd & 1
                x, y = self.g(ra), self.g(rb)
                if xo == 0:
                    xv, yv = (s64(x), s64(y)) if l64 else (s32(x), s32(y))
                else:
                    xv, yv = (x, y) if l64 else (x & M32, y & M32)
                self.setcr(crf, xv < yv, xv > yv, xv == yv)
                return nxt
            if xo == 19:                               # mfcr / mfocrf
                fxm = (w >> 12) & 0xFF
                v = 0
                for fld in range(8):
                    if ((w >> 20) & 1) == 0 or (fxm & (0x80 >> fld)):
                        v |= self.cr[fld] << (4 * (7 - fld))
                self.setg(rd, v)
                return nxt
            if xo == 339:                              # mfspr (mflr / mfctr)
                spr = ((w >> 16) & 31) | (((w >> 11) & 31) << 5)
                self.setg(rd, self.ctr if spr == 9 else 0)
                return nxt
            if xo == 467:                              # mtspr (mtlr / mtctr)
                spr = ((w >> 16) & 31) | (((w >> 11) & 31) << 5)
                if spr == 9:
                    self.ctr = self.g(rd)
                return nxt
            idx = (0 if ra == 0 else self.g(ra)) + self.g(rb)
            a = idx & M32
            if xo == 23:                               # lwzx
                self.setg(rd, self.rd32(a))
                return nxt
            if xo == 87:                               # lbzx
                self.setg(rd, self.mem.rd(a, 1)[0])
                return nxt
            if xo == 279:                              # lhzx
                self.setg(rd, struct.unpack('>H', self.mem.rd(a, 2))[0])
                return nxt
            if xo == 21:                               # ldx
                self.setg(rd, self.rd64(a))
                return nxt
            if xo == 151:                              # stwx
                self.wr32(a, self.g(rd))
                return nxt
            if xo == 215:                              # stbx
                self.mem.wr(a, bytes([self.g(rd) & 0xFF]))
                return nxt
            if xo == 407:                              # sthx
                self.mem.wr(a, struct.pack('>H', self.g(rd) & 0xFFFF))
                return nxt
            if xo == 149:                              # stdx
                self.wr64(a, self.g(rd))
                return nxt
            if xo == 535:                              # lfsx
                self.setf(rd, struct.unpack('>f', self.mem.rd(a, 4))[0])
                return nxt
            if xo == 599:                              # lfdx
                self.fbits[rd] = self.rd64(a)
                return nxt
            if xo == 663:                              # stfsx
                self.mem.wr(a, struct.pack('>f', np.float32(self.f(rd))))
                return nxt
            if xo == 727:                              # stfdx
                self.wr64(a, self.fbits[rd])
                return nxt
            if xo == 983:                              # stfiwx
                self.wr32(a, self.fbits[rd] & M32)
                return nxt
            if xo in (598, 854, 86, 982):              # sync / eieio / dcbf / icbi
                return nxt
            if xo == 103 or xo == 359:                 # lvx / lvxl
                self.vr[rd] = self.ldv(a)
                return nxt
            if xo == 231 or xo == 487:                 # stvx / stvxl
                self.stv(a, self.vr[rd])
                return nxt
            if xo == 519:                              # lvlx: the bytes from EA to the end of its quadword, left-justified
                n = 16 - (a & 0xF)
                data = self.mem.rd(a, n) + bytes(16 - n)
                self.vr[rd] = struct.unpack('>4I', data)
                return nxt
            if xo == 551:                              # lvrx: the bytes before EA in its quadword, right-justified
                n = a & 0xF
                data = bytes(16 - n) + (self.mem.rd(a & ~0xF, n) if n else b'')
                self.vr[rd] = struct.unpack('>4I', data)
                return nxt
            if xo == 6:                                # lvsl
                sh = a & 0xF
                self.vr[rd] = struct.unpack('>4I', bytes(range(sh, sh + 16)))
                return nxt
            raise NotImplementedError('%08X op31 xo=%d %08X' % (pc, xo, w))
        if op == 3:                                    # twi (the compiler's trap for an assert)
            return nxt
        if op == 2:                                    # tdi
            return nxt
        raise NotImplementedError('%08X op=%d %08X' % (pc, op, w))


# ---- the assert family: counted, never fatal ---------------------------------------------------
ASSERT_BEGIN = 0x82817548
ASSERT_FIRE = 0x82820810
ASSERT_END = 0x82817558


def install_assert_hooks(cpu):
    cpu.hooks[ASSERT_BEGIN] = lambda c: c.setg(3, 0)
    cpu.hooks[ASSERT_END] = lambda c: c.setg(3, 0)

    def fire(c):
        c.asserts.append((c.g(5) & M32, c.g(3) & M32))
        c.setg(3, 0)
    cpu.hooks[ASSERT_FIRE] = fire


def vreg_f32(cpu, i):
    return [float(x) for x in cpu.vf(i)]


def vreg_bits(cpu, i):
    return list(cpu.vr[i])
