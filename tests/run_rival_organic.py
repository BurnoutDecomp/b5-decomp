"""Exercise organic rival collisions using the workflow's existing pad-input harness.
Run from the workflow checkout: python b5-decomp/tests/run_rival_organic.py.
Requires a built game and mounted data. Evidence includes every steering decision,
the game log, and the case report. Physics, collision and scoring are never injected.
"""
import ctypes
import math
from pathlib import Path
import re
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[2]
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_wchar_p]
kernel.CreateEventW.restype = ctypes.c_void_p
kernel.SetEvent.argtypes = kernel.ResetEvent.argtypes = kernel.CloseHandle.argtypes = [ctypes.c_void_p]
events = {name: kernel.CreateEventW(None, True, False, 'Local\\BurnoutPC_Input_' + name)
          for name in ('SteerLeft', 'SteerRight', 'SteerFrac25', 'SteerFrac50')}
assert all(events.values())

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
rival_re = re.compile(r'\[rival\] slot (\d+) global (\d+) pos \(' + ', '.join([number]*3) + r'\)')
log = root / 'build/game/BrnGame.log'
start = time.time()
evidence = root / 'scratch/bugtest/runs/rival_organic' / time.strftime('%Y%m%d_%H%M%S')
evidence.mkdir(parents=True)
output = open(evidence / 'driver.console.log', 'w', encoding='utf-8')
proc = subprocess.Popen(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                         'tools/tests/run_case.ps1', '-Case', 'b5-decomp/tests/RivalOrganic.ps1',
                         '-RunDir', str(evidence), '-LockTimeoutSec', '30', '-Label', 'rival-pad-pursuit'], cwd=root,
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
                if m: motion = (int(m[1]), *map(float, m.groups()[1:]))
                r = rival_re.search(line)
                if r: rivals[int(r[2])] = (float(r[3]), float(r[5]), time.time())
            if motion and rivals and motion[0] != last_frame:
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
                    if time.time()-last_note > .5:
                        trace.write(f'{time.time()-start:.2f} frame={last_frame} target={target} distance={distance:.2f} error={error:.3f} steer={value} pos={x:.1f},{z:.1f}\n')
                        last_note = time.time()
                else: steer(0)
        time.sleep(.08)
finally:
    if owned_run:
        steer(0)
    for handle in events.values(): kernel.CloseHandle(handle)
    trace.close()
    output.close()
print('Report:', evidence / 'REPORT.md')
sys.exit(proc.returncode)
