"""Console model of the car-car contact-impulse chain, run over [carcar-dv] v1 lines.

For every applied race-car/race-car contact the witness recorded (inputs before ApplyCarCarImpulse,
the rigid-body arrivals, and both bodies' velocities after the two CalculateNewVelocity drains), this
recomputes -- in float32, from the ARTIST asm formulas, independently of the PC source -- what the
console bodies produce on the same inputs:
  ApplyCarCarImpulse @0x82624C08: point velocities v + w x r, rel = pvA - pvB, closing = rel . -n
    (0 >= closing -> no apply), CalculateCollisionImpulseWithBody @0x8259CAE8 with e = 0 and -n,
    non-showtime x (0 + 1) * 0.5, traffic x30 arm, unit direction + length;
  ApplySensorImpulse @0x826078B0 per body: budget (owner 2 | crashing | showtime -> 1.0, else the
    drive-time row 0.2 when kbAllowDriveTimeDeformation), six signed body axes rotated by the
    transform rows, projection > 0, showtime-victim x15 arm, head sensor ApplyLocalImpulse
    (sub_825E1320): frac = a(1-p) + a p min(vN/sfm, 1), base = min(budget, frac),
    absorbed = mag * base^(60 dt), remaining = mag - absorbed -> VehicleRigidBody @0x8260DFA0
    (showtime -> dropped) -> ApplyCarContactImpulse @0x825D4C10 (not crashing: linear Up removed,
    At x CarCarResponse removed; angular roll x1.0, yaw x0.75, pitch x1.0 removed) or
    ApplyCrashedContactImpulse @0x825D4D50 (angular x CarAngularImpulseScale);
    block (5): rows 0 unless crashing (and not aftertouch-outside-showtime); car-car impulse row
    20 when the other car is crashing too, the tangential bank x dt, x CarAngularImpulseScale;
  CalculateNewVelocity @0x825A1B10: v += (F dt + J) / m, w += Iinv (T dt + L).
Why it exists (crash parity FX-LADDER, 2026-09-25): the owner's "rivals react too little / deform less /
fly more" named the contact-impulse chain as the prime suspect. Every function in it was re-read against
ARTIST (lane log scratch/CRASHPARITY_0922/fixes/FX-LADDER.md) and this checker closes the loop on LIVE
inputs: a run with --diag-env BRN_CARCAR_DV=1 prints one [carcar-dv] line per applied race-car contact
(BrnDeformableObject_Update.cpp), and the model below must reproduce the build's post-contact
velocities. First run: fxladder_dv/20260925_150432 (exe bd53eed5008e), 634 applies, max |model - PC|
3.81e-6 on v' / w'.

A body-space arrival whose projection is within float rounding of zero (the near-vertical axis of a
horizontal contact) can land on either side of the `> 0` skip test; it carries ~1e-6 of impulse and is
reported, not failed.

Usage: python b5-decomp/tests/fxladder_carcar_dv_model.py <BrnGame.log> [--verbose]
Exit code 0 when every record agrees within 1e-4 (m/s, rad/s)."""
import sys, struct, math
import numpy as np

f32 = np.float32


def F(u):
    return f32(struct.unpack('<f', struct.pack('<I', u))[0])


def V(w, i):
    return np.array([F(w[i]), F(w[i + 1]), F(w[i + 2])], dtype=np.float32)


def dot(a, b):
    return f32(f32(a[0] * b[0]) + f32(a[1] * b[1]) + f32(a[2] * b[2]))


def cross(a, b):
    return np.array([a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]],
                    dtype=np.float32)


def rows_mul(R, x):  # row0 * x.x + row1 * x.y + row2 * x.z
    return (R[0] * x[0] + R[1] * x[1] + R[2] * x[2]).astype(np.float32)


AXES = [np.array(a, dtype=np.float32) for a in
        ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0), (0, 0, 1), (0, 0, -1))]


def parse(line):
    toks = line.split()
    i = toks.index('v1')
    w = [int(t, 16) for t in toks[i + 1:]]
    assert len(w) == 142, len(w)
    r = dict(step=w[0], present=w[1], gidA=w[2], gidB=w[3], sensorA=w[4], sensorB=w[5], kbAllow=w[6],
             dt=F(w[7]), pA=V(w, 8), pB=V(w, 11), n=V(w, 14), time=F(w[17]), bodies=[])
    for o in (18, 80):
        fl = w[o]
        b = dict(flags=fl, crashing=bool(fl & 1), showtime=bool(fl & 2), aftertouch=bool(fl & 4),
                 actual=bool(fl & 8), bounced=bool(fl & 16), owner=(fl >> 8) & 0xFF, set=(fl >> 16) & 0xFF,
                 level=(fl >> 24) & 0xFF,
                 T=[V(w, o + 1), V(w, o + 4), V(w, o + 7), V(w, o + 10)],
                 v=V(w, o + 13), w=V(w, o + 16), m=F(w[o + 19]),
                 I=[V(w, o + 20), V(w, o + 23), V(w, o + 26)],
                 J=V(w, o + 29), L=V(w, o + 32), Fo=V(w, o + 35), Tq=V(w, o + 38),
                 ccr=F(w[o + 41]), ang=F(w[o + 42]), a=F(w[o + 43]), p=F(w[o + 44]),
                 sfm=[F(w[o + 45 + k]) for k in range(6)],
                 arrivals=w[o + 51], routes=w[o + 52], arrSum=V(w, o + 53),
                 v1=V(w, o + 56), w1=V(w, o + 59))
        r['bodies'].append(b)
    return r


def sensor_apply(X, Y, dirv, mag, closing, pos_contact, n_contact, rel, dt, kbAllow, notes):
    """ApplySensorImpulse on body X (other body Y); returns (J world, L world, arrivals body-sum, count)."""
    J = np.zeros(3, np.float32)
    L = np.zeros(3, np.float32)
    arr = np.zeros(3, np.float32)
    count = 0
    routes = 0
    R = X['T'][:3]
    pos = X['T'][3]
    if X['showtime']:
        notes.append('UNMODELLED showtime pre-apply')
    budget = f32(1.0) if (X['owner'] == 2 or X['crashing'] or X['showtime']) else (f32(0.2) if kbAllow else f32(0.0))
    imp = (dirv * mag).astype(np.float32)
    for d in range(6):
        wax = rows_mul(R, AXES[d])
        proj = dot(imp, wax)
        if not (proj > 0):
            continue
        shaped = proj
        if X['showtime']:
            notes.append('UNMODELLED showtime shaping')
        elif Y is not None and (Y['showtime'] or Y['bounced']):
            shaped = f32(shaped * f32(15.0))
        # head sensor ApplyLocalImpulse (sub_825E1320)
        ratio = f32(closing / X['sfm'][d])
        if ratio > 1:
            ratio = f32(1.0)
        frac = f32(f32(X['a'] * f32(1 - X['p'])) + f32(f32(X['a'] * X['p']) * ratio))
        base = budget if budget < frac else frac
        expo = f32(f32(60.0) * dt)
        fac = f32(np.power(np.float64(base), np.float64(expo)))
        absorbed = f32(shaped * fac)
        remaining = f32(shaped - absorbed)
        if X['showtime']:
            continue  # VehicleRigidBody::RecievePassedOnImpulse drops it (vtable +0x10 gate)
        Jb = (AXES[d] * remaining).astype(np.float32)
        arr += Jb
        count += 1
        Jw = rows_mul(R, Jb)
        r = (pos_contact - pos).astype(np.float32)
        Lw = cross(r, Jw)
        if X['crashing']:
            routes |= 1
            Lw = (Lw * X['ang']).astype(np.float32)
        else:
            routes |= 4
            right, up, at = R[0], R[1], R[2]
            Jw = (Jw - up * dot(Jw, up)).astype(np.float32)
            Jw = (Jw - at * f32(dot(Jw, at) * X['ccr'])).astype(np.float32)
            Lw = (Lw - at * f32(dot(Lw, at) * f32(1.0))).astype(np.float32)
            Lw = (Lw - up * f32(dot(Lw, up) * f32(0.75))).astype(np.float32)
            Lw = (Lw - right * f32(dot(Lw, right) * f32(1.0))).astype(np.float32)
        J += Jw
        L += Lw
    # block (5): the tangential bank
    if X['aftertouch'] and not X['actual']:
        row = f32(0)
    elif not X['crashing']:
        row = f32(0)
    else:
        row = f32(20.0) if (Y is not None and Y['crashing']) else f32(0.0)
    if row != 0:
        t = (rel - n_contact * dot(rel, n_contact)).astype(np.float32)
        tsq = dot(t, t)
        if tsq >= f32(1e-4):
            tl = f32(math.sqrt(tsq))
            negdir = (-t / tl).astype(np.float32)
            cl = min(max(tl, f32(0)), f32(1))
            smag = f32(mag * cl)
            bank = (negdir * smag * row * dt).astype(np.float32)
            Lt = cross((pos_contact - pos).astype(np.float32), bank)
            J += (bank * X['ang']).astype(np.float32)
            L += (Lt * X['ang']).astype(np.float32)
            notes.append('tangential bank row 20')
    return J, L, arr, count, routes


def model(r):
    notes = []
    A, B = r['bodies']
    dt = r['dt']
    posA, posB = A['T'][3], B['T'][3]
    rA = (r['pA'] - posA).astype(np.float32)
    rB = (r['pB'] - posB).astype(np.float32)
    pvA = (A['v'] + cross(A['w'], rA)).astype(np.float32)
    pvB = (B['v'] + cross(B['w'], rB)).astype(np.float32)
    rel = (pvA - pvB).astype(np.float32)
    nn = (-r['n']).astype(np.float32)
    closing = dot(rel, nn)
    if not (closing > 0):
        notes.append('MODEL SAYS NO APPLY (closing %.4f)' % closing)
    Apart = cross(rows_mul(A['I'], cross(rA, nn)), rA)
    Bpart = cross(rows_mul(B['I'], cross(rB, nn)), rB)
    den = f32(f32(f32(f32(1) / A['m']) + f32(f32(1) / B['m'])) + f32(dot(nn, Apart) + dot(nn, Bpart)))
    j = f32(-dot(rel, nn) / den)
    imp = (nn * f32(-abs(j))).astype(np.float32)
    if A['showtime']:
        if B['owner'] == 2:
            notes.append('UNMODELLED showtime bounce')
    else:
        imp = (imp * f32(0.5)).astype(np.float32)
        if A['owner'] == 2 and (B['showtime'] or B['bounced']):
            imp = (imp * f32(30.0)).astype(np.float32)
    mag = f32(math.sqrt(dot(imp, imp)))
    dirv = (imp / mag).astype(np.float32) if mag > 0 else imp
    JA, LA, arrA, cA, rtA = sensor_apply(A, B, dirv, mag, closing, r['pA'], r['n'], rel, dt, r['kbAllow'], notes)
    JB, LB, arrB, cB, rtB = sensor_apply(B, A, (-dirv).astype(np.float32), mag, closing, r['pB'],
                                         (-r['n']).astype(np.float32), (-rel).astype(np.float32), dt,
                                         r['kbAllow'], notes)
    out = []
    for X, Jx, Lx in ((A, JA, LA), (B, JB, LB)):
        Jtot = (X['Fo'] * dt + X['J'] + Jx).astype(np.float32)
        Ltot = (X['Tq'] * dt + X['L'] + Lx).astype(np.float32)
        v1 = (X['v'] + Jtot / X['m']).astype(np.float32)
        w1 = (X['w'] + rows_mul(X['I'], Ltot)).astype(np.float32)
        out.append((v1, w1))
    return dict(closing=closing, j=j, mag=mag, out=out, arr=(arrA, arrB), cnt=(cA, cB), routes=(rtA, rtB),
                notes=notes, rel=rel)


def main():
    path = sys.argv[1]
    verbose = '--verbose' in sys.argv
    recs = [parse(line) for line in open(path, encoding='utf-8', errors='replace') if '[carcar-dv] v1' in line]
    print('%d [carcar-dv] records' % len(recs))
    if not recs:
        print('no [carcar-dv] line: was the run armed with BRN_CARCAR_DV=1?')
        return 2
    worst = 0.0
    worst_arr = 0.0
    count_notes = 0
    removed = []
    harder = []
    for k, r in enumerate(recs):
        m = model(r)
        A, B = r['bodies']
        for side in (0, 1):
            X = r['bodies'][side]
            v1m, w1m = m['out'][side]
            worst = max(worst, float(np.max(np.abs(v1m - X['v1']))), float(np.max(np.abs(w1m - X['w1']))))
            scale = max(1.0, float(np.max(np.abs(X['arrSum']))))
            worst_arr = max(worst_arr, float(np.max(np.abs(m['arr'][side] - X['arrSum']))) / scale)
            if m['cnt'][side] != X['arrivals']:
                count_notes += 1
        rA = (r['pA'] - A['T'][3]).astype(np.float32)
        rB = (r['pB'] - B['T'][3]).astype(np.float32)
        after = dot((A['v1'] + cross(A['w1'], rA)) - (B['v1'] + cross(B['w1'], rB)), -r['n'])
        removed.append(1.0 - float(after) / float(m['closing']))
        dvA = A['v1'] - A['v']
        dvB = B['v1'] - B['v']
        harder.append(max(float(np.linalg.norm(dvA)), float(np.linalg.norm(dvB))))
        if verbose:
            print('#%d step %d ent %d/%d sensor %d/%d closing %.2f j %.0f shaped %.0f m %.0f/%.0f set %d/%d lvl %d/%d '
                  'crash %d/%d | dvA %s dvB %s %s'
                  % (k, r['step'], r['gidA'] >> 10 & 0x3FFF, r['gidB'] >> 10 & 0x3FFF, r['sensorA'], r['sensorB'],
                     m['closing'], m['j'], m['mag'], A['m'], B['m'], A['set'], B['set'], A['level'], B['level'],
                     A['crashing'], B['crashing'], np.round(dvA, 3), np.round(dvB, 3), ';'.join(sorted(set(m['notes'])))))
    removed = np.array(removed)
    harder = np.array(harder)
    print('closing-speed fraction one apply removes at the contact point: median %.3f (p10 %.3f, p90 %.3f)'
          % (np.median(removed), np.percentile(removed, 10), np.percentile(removed, 90)))
    print('per-apply |dv| of the harder-hit car: median %.3f, p90 %.3f, max %.3f m/s'
          % (np.median(harder), np.percentile(harder, 90), harder.max()))
    print("max |model - pc| on v' / w' = %.3g ; max relative arrival-sum error = %.3g ; "
          'sides whose arrival COUNT differs by a near-zero projection = %d' % (worst, worst_arr, count_notes))
    ok = worst <= 1e-4 and worst_arr <= 1e-4
    print('PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
