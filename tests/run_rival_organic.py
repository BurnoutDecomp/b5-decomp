"""Exercise organic rival collisions using the workflow's existing pad-input harness.
Run from the workflow checkout: python b5-decomp/tests/run_rival_organic.py.
Requires a built game and mounted data. Evidence includes every steering decision,
the game log, and the case report. Physics, collision and scoring are never injected.
"""
import ctypes
import argparse
import math
from pathlib import Path
import re
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--case', default='b5-decomp/tests/RivalOrganic.ps1')
parser.add_argument('--run-name', default='rival_organic')
parser.add_argument('--no-unstick', action='store_true',
                    help='never back out of a wedge (reproduce a wedged-player run as it was)')
args = parser.parse_args()
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_wchar_p]
kernel.CreateEventW.restype = ctypes.c_void_p
kernel.SetEvent.argtypes = kernel.ResetEvent.argtypes = kernel.CloseHandle.argtypes = [ctypes.c_void_p]
events = {name: kernel.CreateEventW(None, True, False, 'Local\\BurnoutPC_Input_' + name)
          for name in ('SteerLeft', 'SteerRight', 'SteerFrac25', 'SteerFrac50')}
assert all(events.values())
# The pedals belong to flow_run's drive schedule (Drive = $true holds Accelerate for the whole
# run and re-applies it only when its schedule CHANGES). The driver borrows them for one thing:
# backing out of a wedge (see UNSTICK below), and hands Accelerate back afterwards.
pedals = {name: kernel.CreateEventW(None, True, False, 'Local\\BurnoutPC_Input_' + name)
          for name in ('Accelerate', 'Brake')}
assert all(pedals.values())
kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
kernel.WaitForSingleObject.restype = ctypes.c_uint32

def is_set(handle):
    return kernel.WaitForSingleObject(handle, 0) == 0   # WAIT_OBJECT_0: the event is signalled

def steer(value):
    # Same ordered fraction/side transitions as flow_run.
    active = set()
    if value:
        active.add('SteerLeft' if value < 0 else 'SteerRight')
        if abs(value) == .25: active.update(('SteerFrac25', 'SteerFrac50'))
        elif abs(value) == .5: active.add('SteerFrac50')
    for n in ('SteerFrac25', 'SteerFrac50'):
        if n in active: kernel.SetEvent(events[n])
    for n in ('SteerLeft', 'SteerRight'):
        if n not in active: kernel.ResetEvent(events[n])
    for n in ('SteerLeft', 'SteerRight'):
        if n in active: kernel.SetEvent(events[n])
    for n in ('SteerFrac25', 'SteerFrac50'):
        if n not in active: kernel.ResetEvent(events[n])

number = r'([-+0-9.eE]+)'
motion_re = re.compile(r'\[motion\] n (\d+) pos ' + ' '.join([number]*3)
                       + ' at ' + ' '.join([number]*3) + ' vel ' + ' '.join([number]*3))
gas_re = re.compile(r'\bgas (-?[0-9.]+)')   # the game's own mfGas on the [motion] line
rival_re = re.compile(r'\[rival\] slot (\d+) global (\d+) pos \(' + ', '.join([number]*3) + r'\)')
log = root / 'build/game/BrnGame.log'
start = time.time()
evidence = root / 'scratch/bugtest/runs' / args.run_name / time.strftime('%Y%m%d_%H%M%S')
evidence.mkdir(parents=True)
output = open(evidence / 'driver.console.log', 'w', encoding='utf-8')
proc = subprocess.Popen(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                         'tools/tests/run_case.ps1', '-Case', args.case,
                         '-RunDir', str(evidence), '-Label', 'rival-pad-pursuit'], cwd=root,
                        stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
position = 0
carry = ''
rivals = {}
motion = None
target = None
last_frame = -1
last_note = 0
trace = open(evidence / 'driver.controls.log', 'w', encoding='utf-8', buffering=1)
owned_run = False
# UNSTICK (2026-09-24, crash-parity FX-AIRESET finding). The driver only steers above 3 m/s, so a
# wall contact that leaves the player nose-in ended the pursuit for the rest of the run in about
# half the organic runs -- 100 s of a parked car, and in the section-2943 dead end under the
# 2927 ramp every rival reset then fell back to ResetAwayFromPlayer (the console's own
# far-away pose). A car that has moved less than WEDGE_METRES over WEDGE_SECONDS while the drive
# holds Accelerate is backed out: Accelerate released, Brake (reverse) held with half lock for
# UNSTICK_SECONDS, then Accelerate handed back. Never before the car has driven once (a start-line
# hold is not a wedge), and only while the game itself applies the throttle for the whole window
# (the [motion] line's mfGas > 0.5): a crashed car reads gas 0 while the wreck settles, so a
# crash is never taken for a wedge. Replayed over six banked runs: it fires only at the known
# wedges (fxgeometric/20260924_164556 from t=104 s; two fxemitter pursuits), never in a crash.
# Every manoeuvre is logged to driver.controls.log.
WEDGE_SECONDS, WEDGE_METRES, UNSTICK_SECONDS, UNSTICK_MAX = 6.0, 1.0, 2.0, 20
track = []            # (time, x, z, frame, gas) of recent [motion] samples
unstick_until = 0.0   # while time() < this, the driver is backing out
unsticks = 0
last_steer = 0
last_unstick_end = 0.0
motion_gas = 0.0
driving_seen = False  # no unstick before the car has driven once (a start-line hold is not a wedge)
try:
    while proc.poll() is None:
        if not owned_run:
            owned_run = '[flow] pid=' in (evidence / 'driver.console.log').read_text(encoding='utf-8', errors='replace')
        if owned_run and log.exists() and log.stat().st_mtime >= start:
            with log.open('rb') as f:
                if f.seek(0, 2) < position: position = 0; carry = ''; rivals.clear(); motion = None
                f.seek(position)
                new = f.read()
                position = f.tell()
            lines = (carry + new.decode('utf-8', errors='replace')).split('\n')
            carry = lines.pop()
            for line in lines:
                m = motion_re.search(line)
                if m:
                    motion = (int(m[1]), *map(float, m.groups()[1:]))
                    g = gas_re.search(line)
                    motion_gas = float(g[1]) if g else 0.0
                    if math.hypot(motion[7], motion[9]) > 3: driving_seen = True
                r = rival_re.search(line)
                if r: rivals[int(r[2])] = (float(r[3]), float(r[5]), time.time())
            now = time.time()
            if unstick_until and now >= unstick_until:
                kernel.ResetEvent(pedals['Brake'])
                kernel.SetEvent(pedals['Accelerate'])
                steer(0)
                unstick_until = 0.0
                last_unstick_end = now
                track.clear()
                trace.write(f'{now-start:.2f} UNSTICK end #{unsticks}: brake released, accelerate handed back\n')
            # Only a NEW [motion] frame is a sample: a paused game (an assert pauses the sim) or a
            # log that stops printing must not read as a car standing still.
            if motion and not unstick_until and (not track or track[-1][3] != motion[0]):
                track.append((now, motion[1], motion[3], motion[0], motion_gas))
                while track and now - track[0][0] > WEDGE_SECONDS:
                    track.pop(0)
                # The game must be APPLYING throttle the whole window (mfGas > 0.5): a crashed car
                # reads gas 0 while the wreck settles, so a long crash is never taken for a wedge.
                if (not args.no_unstick and track and driving_seen and now - track[0][0] >= WEDGE_SECONDS * 0.9 and unsticks < UNSTICK_MAX
                        and all(s[4] > 0.5 for s in track)
                        and now - last_unstick_end > WEDGE_SECONDS
                        and max(math.hypot(s[1] - motion[1], s[2] - motion[3]) for s in track) < WEDGE_METRES
                        and is_set(pedals['Accelerate'])):
                    unsticks += 1
                    kernel.ResetEvent(pedals['Accelerate'])
                    kernel.SetEvent(pedals['Brake'])
                    steer(.5 if last_steer <= 0 else -.5)
                    unstick_until = now + UNSTICK_SECONDS
                    trace.write(f'{now-start:.2f} UNSTICK #{unsticks} at pos={motion[1]:.1f},{motion[3]:.1f}: '
                                f'moved < {WEDGE_METRES} m in {WEDGE_SECONDS} s -- reverse with half lock '
                                f'for {UNSTICK_SECONDS} s\n')
            if motion and rivals and motion[0] != last_frame and not unstick_until:
                last_frame, x, y, z, hx, hy, hz, vx, vy, vz = motion
                candidates = [(i, rx, rz, math.hypot(rx-x, rz-z)) for i, (rx, rz, t) in rivals.items()
                              if time.time()-t < 3 and i != 0]
                if candidates and math.hypot(vx, vz) > 3:
                    # Prefer a car in front; retain that target through the impact.
                    ahead = [c for c in candidates if (c[1]-x)*hx+(c[2]-z)*hz > 0]
                    selected = next((c for c in candidates if c[0] == target and c[3] < 35), None)
                    if selected is None: selected = min(ahead or candidates, key=lambda c:c[3])
                    target, rx, rz, distance = selected
                    dx, dz = rx-x, rz-z
                    error = math.atan2(hz*dx-hx*dz, hx*dx+hz*dz)
                    magnitude = .5 if abs(error) > .6 else .25 if abs(error) > .035 else 0
                    value = -math.copysign(magnitude, error)
                    steer(value)
                    if value: last_steer = value
                    if time.time()-last_note > .5:
                        trace.write(f'{time.time()-start:.2f} frame={last_frame} target={target} distance={distance:.2f} error={error:.3f} steer={value} pos={x:.1f},{z:.1f}\n')
                        last_note = time.time()
                else: steer(0)
        time.sleep(.08)
finally:
    if owned_run:
        steer(0)
        kernel.ResetEvent(pedals['Brake'])   # never leave reverse held; Accelerate stays flow_run's
    for handle in pedals.values(): kernel.CloseHandle(handle)
    for handle in events.values(): kernel.CloseHandle(handle)
    trace.close()
    output.close()
print('Report:', evidence / 'REPORT.md')
sys.exit(proc.returncode)
