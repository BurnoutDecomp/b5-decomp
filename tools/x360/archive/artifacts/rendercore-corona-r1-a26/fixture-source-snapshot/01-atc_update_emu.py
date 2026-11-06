"""FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update @0x82228158 run WHOLE on emu64.

The function's own words run from its first instruction to its return, frame after frame on one memory image. Its
out-of-line callees are handled like this:
  interpreted (the console's real words): CheckForPlayerCarBouncing 0x8220F340, CameraImpactEffect::RegisterImpact
      0x821F3648, XMMatrixRotationY 0x82203560 -> XMScalarSinCos 0x821F0C08, __savevmx_122 / __restvmx_122, and the
      six CRT thunks that fill the .bss constants Update reads (0x82C494B8 .. 0x82C49558, 0x82C6CDC0);
  stubbed, with the SAME single-rounding semantics as the PC fixture (tests/FxGateAftertouchUpdate.cpp):
      PositionLag::Update 0x821F8F08   wAxis += the frame's lag offset (four f32 adds)
      Utils::CreateLookAt 0x8220C4F8   rows (1,0,0,0) / (0,1,0,0) / target - eye / eye
      rw::math::vpu::SLerp 0x82216858  every lane from + (to - from) * amount (f32 sub, mul, add)
      CameraShake::Update 0x82221310   records its arguments; transform.wAxis.x += scale * 0.001
      Camera::ValidateTransformWithDebugInfo 0x8220A850   nothing
      Utils::SineLerp 0x8220CCB0       from + (to - from) * t (f32 sub, mul, add)
      the assert family                counted (emu64.install_assert_hooks)
vmsum3fp128 and vmsum4fp128 are ROUNDING_RULE 1 (f64 products, summed left to right in f64, one cast; a finite f64
sum that overflows f32 is a QNaN); the fused ops are exact-then-rounded-once (emu64 / vmxemu); the estimates are
correctly rounded (vmxemu).
"""
import math
import struct

import numpy as np

import sincos_emu as S
from sincos_emu import bits, val
from scalarsincos import Rule1CPU
import vmxemu as V
import emu64 as E

M32 = 0xFFFFFFFF
UPDATE = 0x82228158
SELF = 0x40000000
PARAMS = 0x40020000
SHARED = 0x40021000
CAM = 0x40022000
RANDOM = 0x40023000
STACK = S.STACK

H_LAG = 0x821F8F08
H_LOOKAT = 0x8220C4F8
H_SLERP = 0x82216858
H_SHAKE = 0x82221310
H_VALIDATE = 0x8220A850
H_SINELERP = 0x8220CCB0
THUNKS = (0x82C494B8, 0x82C494E0, 0x82C49508, 0x82C49530, 0x82C49558, 0x82C6CDC0)

# ---- the shared schema (tests/FxGateAftertouchUpdate.cpp walks the same lists in the same order) -------------------
RIG_FIELDS = [
    ("mbIsPrepared", 0x008, "b"),
    ("mCurrentTargetPos", 0x2B0, "v"),
    ("mWorldSpaceNormalizedVectorFromCar", 0x2C0, "v"),
    ("mDesiredWorldSpaceNormalizedVectorFromCar", 0x2D0, "v"),
    ("mCrashPoint", 0x2E0, "v"),
    ("mCameraPositionLastFrame", 0x2F0, "v"),
    ("mImpactEffect.mfImpactFactor", 0x370, "f"),
    ("mfHeight", 0x384, "f"),
    ("mfDistance", 0x388, "f"),
    ("mfBlendFactor", 0x38C, "f"),
    ("mfTimeSinceLastDecision", 0x390, "f"),
    ("mfTimeSinceLastManualControl", 0x394, "f"),
    ("mManualCameraDirection", 0x3A0, "v"),
    ("mfManualHeightAdjustment", 0x3B0, "v"),
    ("mbManualCameraControl", 0x3C0, "b"),
    ("mbWasFallingDownwards", 0x3C1, "b"),
    ("mbIsTempDebugCrashCamera", 0x3C3, "b"),
    ("mbIsRandomStartTempDebugCrashCamera", 0x3C4, "b"),
    ("mfDebugCrashCameraParam0to1", 0x3C8, "f"),
    ("mfBounceShakeMultiplier", 0x3CC, "f"),
    ("mfRollAngleRads", 0x3D0, "f"),
    ("mfCloseupAmount0To1", 0x3D4, "f"),
]
PARAM_FIELDS = [
    ("mShakeParams.mfXYShakeMagnitudeDegs", 0x08), ("mShakeParams.mfZShakeMagnitudeDegs", 0x0C),
    ("mShakeParams.mfXYWobbleMagnitudeDegs", 0x10), ("mShakeParams.mfWobbleCenteringFactor", 0x14),
    ("mfSlowDistance", 0x2C), ("mfSlowHeight", 0x30), ("mfFastDistance", 0x34), ("mfFastHeight", 0x38),
    ("mfPitch", 0x3C), ("mfFOV", 0x40), ("mfBlendFactorBlendFactor", 0x44), ("mfMinimumBlendFactor", 0x48),
    ("mfMaximumBlendFactor", 0x4C), ("mfManualBlendFactor", 0x50), ("mfHeightDistanceBlendFactor", 0x54),
    ("mfHeightDistanceVelocityRange", 0x58), ("mfTimeBetweenDecisions", 0x6C),
]
RIG_WORDS = sum(4 if k == "v" else 1 for _, _, k in RIG_FIELDS)
PARAM_WORDS = len(PARAM_FIELDS)
CAMERA_IN_WORDS = 17          # transform (16) + FOV
RANDOM_WORDS = 11             # ring (8) + seed hi + seed lo + index
FRAME_IN_WORDS = 2 + 2 + 16 + 4 + 3 + 4   # held stick, orbit stick, car transform, velocity, 3 timesteps, lag offset
CAMERA_OUT_WORDS = 18         # transform (16) + FOV + the E_FLAG_VALID bit
RECORD_WORDS = 3 + 33 + 3 + 17 + 4 + 1
FRAME_OUT_WORDS = RIG_WORDS + RANDOM_WORDS + CAMERA_OUT_WORDS + RECORD_WORDS


def f32op(x):
    return float(np.float32(x))


def add32(a, b):
    with np.errstate(all="ignore"):
        return float(np.float32(a) + np.float32(b))


def sub32(a, b):
    with np.errstate(all="ignore"):
        return float(np.float32(a) - np.float32(b))


def mul32(a, b):
    with np.errstate(all="ignore"):
        return float(np.float32(a) * np.float32(b))


def vmsum_rule1(a, b, n):
    s = float(a[0]) * float(b[0])
    for k in range(1, n):
        s = s + float(a[k]) * float(b[k])
    with np.errstate(all="ignore"):
        r = float(np.float32(s))
    if math.isfinite(s) and math.isinf(r):
        return math.nan                      # xenia DOT_PRODUCT_3/4: a finite f64 sum that overflows f32 is a QNaN
    return r


class UpdateCPU(Rule1CPU):
    """Rule1CPU (vmsum4fp128 = ROUNDING_RULE 1) plus vmsum3fp128 = ROUNDING_RULE 1."""
    def exec_vector(self, a, w, mn):
        if (w >> 26) == 4 and (w & 0x7F3) == 0x403:   # lvlx128: emu64's op-31 lvlx, with the VMX128 register field
            vd = ((w >> 21) & 0x1F) | ((w & 3) << 5)
            ra, rb = (w >> 16) & 0x1F, (w >> 11) & 0x1F
            ea = ((0 if ra == 0 else self.g(ra)) + self.g(rb)) & M32
            n = 16 - (ea & 0xF)
            self.vr[vd] = struct.unpack(">4I", self.mem.rd(ea, n) + bytes(16 - n))
            return True
        if (w >> 26) == 5 and (w & 0x3D0) == 0x350:   # vsel128 vD, vA, vB: vD = (vA & ~vD) | (vB & vD) (vD is the mask)
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            A, B, D = self.vr[va], self.vr[vb], self.vr[vd]
            self.vr[vd] = tuple(((A[i] & ~D[i]) | (B[i] & D[i])) & M32 for i in range(4))
            return True
        if (w >> 26) == 5 and (w & 0x210) != 0 and (w & 0x3D0) == 0x190:
            vd = ((w >> 21) & 0x1F) | (((w >> 2) & 3) << 5)
            va = ((w >> 16) & 0x1F) | (((w >> 5) & 1) << 5) | (((w >> 10) & 1) << 6)
            vb = ((w >> 11) & 0x1F) | ((w & 3) << 5)
            r = vmsum_rule1(self.vf(va), self.vf(vb), 3)
            self.setvf(vd, [np.float32(r)] * 4)
            return True
        return super().exec_vector(a, w, mn)


# ---- memory helpers ------------------------------------------------------------------------------------------------
def wvec_bits(c, a, us):
    for k in range(4):
        c.wr32(a + 4 * k, us[k] & M32)


def rvec_bits(c, a):
    return [c.rd32(a + 4 * k) for k in range(4)]


def w8(c, a, b):
    c.mem.wr(a & M32, bytes([b & 0xFF]))


def r8(c, a):
    return c.mem.rd(a & M32, 1)[0]


class Records:
    def __init__(self):
        self.reset()

    def reset(self):
        self.lag = []
        self.lookat = []
        self.slerp = []
        self.shake = []
        self.sinelerp = []

    def words(self, asserts):
        out = [len(self.lag)]
        lag = self.lag[0] if self.lag else (0, 0)
        out += [lag[0], lag[1]]
        out += [len(self.lookat)]
        for i in range(4):
            eye, tgt = self.lookat[i] if i < len(self.lookat) else ([0] * 4, [0] * 4)
            out += list(eye) + list(tgt)
        out += [len(self.slerp)] + [self.slerp[i] if i < len(self.slerp) else 0 for i in range(2)]
        out += [len(self.shake)]
        for i in range(2):
            if i < len(self.shake):
                which, params, f1, f2, rnd = self.shake[i]
                out += [which] + list(params) + [f1, f2, rnd]
            else:
                out += [0] * 8
        sl = self.sinelerp[0] if self.sinelerp else (0, 0, 0)
        out += [len(self.sinelerp)] + list(sl)
        out += [asserts]
        assert len(out) == RECORD_WORDS, len(out)
        return [u & M32 for u in out]


class Machine:
    """One console instance: the rig, its parameters, the shared info, the camera and the shared random."""

    def __init__(self):
        mem = V.Mem()
        mem.add(STACK, 0x20000)
        mem.add(SELF, 0x1000)
        mem.add(PARAMS, 0x100)
        mem.add(SHARED, 0x800)
        mem.add(CAM, 0x200)
        mem.add(RANDOM, 0x40)
        mem.add(0x82FAA700, 0x300)            # .bss: the five KF VecFloat splats (0x82FAA710 .. 0x82FAA9C0)
        mem.add(0x83017FA0, 0x10)             # .bss: CgsCore::K_V3D_YAXIS
        self.c = c = UpdateCPU(mem)
        E.install_assert_hooks(c)
        c.setg(1, STACK + 0x10000)
        for thunk in THUNKS:                   # the CRT fills them from the image's own constants
            c.run(thunk)
        self.rec = Records()
        self.lag_offset = [0.0, 0.0, 0.0, 0.0]
        c.hooks[H_LAG] = self.h_lag
        c.hooks[H_LOOKAT] = self.h_lookat
        c.hooks[H_SLERP] = self.h_slerp
        c.hooks[H_SHAKE] = self.h_shake
        c.hooks[H_VALIDATE] = lambda cpu: None
        c.hooks[H_SINELERP] = self.h_sinelerp

    # ---- the stubs ------------------------------------------------------------------------------------------------
    def h_lag(self, c):
        self.rec.lag.append((bits(c.f(1)), 1 if (c.g(4) & M32) == PARAMS + 0x18 else 0))
        frame = c.g(6) & M32
        w = [val(u) for u in rvec_bits(c, frame + 0x30)]
        wvec_bits(c, frame + 0x30, [bits(add32(w[k], self.lag_offset[k])) for k in range(4)])

    def h_lookat(self, c):
        eye = list(c.vr[1])
        tgt = list(c.vr[2])
        self.rec.lookat.append((eye, tgt))
        out = c.g(3) & M32
        wvec_bits(c, out, [bits(1.0), 0, 0, 0])
        wvec_bits(c, out + 0x10, [0, bits(1.0), 0, 0])
        wvec_bits(c, out + 0x20, [bits(sub32(val(tgt[k]), val(eye[k]))) for k in range(4)])
        wvec_bits(c, out + 0x30, eye)

    def h_slerp(self, c):
        out, frm, to = c.g(3) & M32, c.g(4) & M32, c.g(5) & M32
        t = val(c.vr[1][0])
        self.rec.slerp.append(c.vr[1][0] & M32)
        rows = []
        for r in range(4):
            a = [val(u) for u in rvec_bits(c, frm + 16 * r)]
            b = [val(u) for u in rvec_bits(c, to + 16 * r)]
            rows.append([bits(add32(a[k], mul32(sub32(b[k], a[k]), t))) for k in range(4)])
        for r in range(4):
            wvec_bits(c, out + 16 * r, rows[r])

    def h_shake(self, c):
        this = c.g(3) & M32
        which = {SELF + 0x360: 1, SELF + 0x374: 2}.get(this, 9)
        params = [c.rd32((c.g(5) & M32) + 4 * k) for k in range(4)]
        rnd = {SELF + 0x330: 1, RANDOM: 2}.get(c.g(6) & M32, 9)
        self.rec.shake.append((which, params, bits(c.f(1)), bits(c.f(2)), rnd))
        tr = c.g(4) & M32
        x = val(c.rd32(tr + 0x30))
        c.wr32(tr + 0x30, bits(add32(x, mul32(c.f(2), 0.001))))

    def h_sinelerp(self, c):
        a, b, t = c.f(1), c.f(2), c.f(3)
        self.rec.sinelerp.append((bits(a), bits(b), bits(t)))
        c.setf(1, add32(a, mul32(sub32(b, a), t)))

    # ---- state -----------------------------------------------------------------------------------------------------
    def write_rig(self, words):
        c, i = self.c, 0
        for _, off, kind in RIG_FIELDS:
            if kind == "b":
                w8(c, SELF + off, words[i]); i += 1
            elif kind == "f":
                c.wr32(SELF + off, words[i]); i += 1
            else:
                wvec_bits(c, SELF + off, words[i:i + 4]); i += 4
        c.wr32(SELF + 0x3D8, PARAMS)           # mpParameters

    def read_rig(self):
        c, out = self.c, []
        for _, off, kind in RIG_FIELDS:
            if kind == "b":
                out.append(r8(c, SELF + off))
            elif kind == "f":
                out.append(c.rd32(SELF + off))
            else:
                out += rvec_bits(c, SELF + off)
        return out

    def write_params(self, words):
        for (_, off), u in zip(PARAM_FIELDS, words):
            self.c.wr32(PARAMS + off, u)

    def write_camera(self, words):
        c = self.c
        for k in range(16):
            c.wr32(CAM + 4 * k, words[k])
        c.wr32(CAM + 0x58, words[16])
        c.wr64(CAM + 0x140, 0)

    def read_camera(self):
        c = self.c
        return [c.rd32(CAM + 4 * k) for k in range(16)] + [c.rd32(CAM + 0x58), (c.rd64(CAM + 0x140) >> 1) & 1]

    def write_random(self, words):
        c = self.c
        for k in range(8):
            c.wr32(RANDOM + 4 * k, words[k])
        c.wr64(RANDOM + 0x20, (words[8] << 32) | words[9])
        c.wr32(RANDOM + 0x28, words[10])
        c.wr32(SHARED + 0x5D4, RANDOM)

    def read_random(self):
        c = self.c
        seed = c.rd64(RANDOM + 0x20)
        return [c.rd32(RANDOM + 4 * k) for k in range(8)] + [seed >> 32, seed & M32, c.rd32(RANDOM + 0x28)]

    def frame(self, words):
        """One Update: words = FRAME_IN_WORDS; returns FRAME_OUT_WORDS."""
        c = self.c
        wvec_bits(c, SHARED + 0x000, [words[0], words[1], 0, 0])
        wvec_bits(c, SHARED + 0x030, [words[2], words[3], 0, 0])
        for k in range(16):
            c.wr32(SHARED + 0x250 + 4 * k, words[4 + k])
        wvec_bits(c, SHARED + 0x390, words[20:24])
        c.wr32(SHARED + 0x580, words[24])      # E_WORLD
        c.wr32(SHARED + 0x584, words[25])      # E_WORLD_NO_SLOMO
        c.wr32(SHARED + 0x588, words[26])      # E_GAME
        self.lag_offset = [val(u) for u in words[27:31]]
        c.wr64(CAM + 0x140, 0)                 # the E_FLAG_VALID bit is checked per frame
        self.rec.reset()
        c.asserts.clear()
        c.setg(1, STACK + 0x10000)
        c.setg(3, SELF)
        c.setg(4, CAM)
        c.setg(5, SHARED)
        c.run(UPDATE)
        ret = c.g(3) & M32
        assert ret == 1, "Update returned %d" % ret
        out = self.read_rig() + self.read_random() + self.read_camera() + self.rec.words(len(c.asserts))
        assert len(out) == FRAME_OUT_WORDS
        return [u & M32 for u in out]
