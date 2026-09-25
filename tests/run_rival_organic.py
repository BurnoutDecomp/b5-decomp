"""Exercise organic rival collisions using the workflow's existing pad-input harness.
Run from the workflow checkout: python b5-decomp/tests/run_rival_organic.py.
Requires a built game and mounted data. Evidence includes every steering decision,
the game log, and the case report. Physics, collision and scoring are never injected.

Two drivers:
  default         -- this script steers the player car through the harness pad channels toward the
                     nearest rival's straight-line bearing (0 / +-0.25 / +-0.5 at about 12 Hz) while
                     flow_run holds Accelerate; it knows nothing of roads, walls or traffic.
  --ai-pad <mode> -- the game's OWN AI drives the player car through the pad path
                     (BRN_AI_PAD_PLAYER=<mode>, WorldModule::HarnessArmAIPadPlayer; the control word
                     stays 1, so rivals still target the player and takedowns stay player-credited).
                     cruise = the console seat's own driving, race = the same, armed only inside an
                     event, pursuit = route to the nearest attached rival with the game's route planner
                     and ram it inside the console's own slam window. The env is set through the case's
                     DiagEnv (a generated wrapper case in the evidence dir), the Python steering is off,
                     and the UNSTICK below stays only as a logged fallback (the seat lets a held real
                     BRAKE through, which is exactly what the unstick holds).
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
parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('--case', default='b5-decomp/tests/RivalOrganic.ps1')
parser.add_argument('--run-name', default='rival_organic')
parser.add_argument('--no-unstick', action='store_true',
                    help='never back out of a wedge (reproduce a wedged-player run as it was)')
parser.add_argument('--ai-pad', choices=('cruise', 'race', 'pursuit'), default=None,
                    help='the game\'s own AI drives the player car through the pad path (BRN_AI_PAD_PLAYER)')
parser.add_argument('--no-frames', action='store_true',
                    help='force the case\'s Frames off (the disk rule: no frame dumps unless a picture is the witness)')
parser.add_argument('--diag-env', default='',
                    help='extra BRN_* witnesses appended to the case DiagEnv (e.g. BRN_MM_DIAG=1: the rivals\' '
                         'aggression lines, [mm-ai] ... tgt 1 == FindTarget took the player)')
parser.add_argument('--slot', type=int, default=0,
                    help='harness slot for run_case.ps1 (3 == build/game/Memcard_3, the progression copy with rivals); '
                         'a slot run holds the slot-0 box lock for its whole length')
args = parser.parse_args()
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_wchar_p]
kernel.CreateEventW.restype = ctypes.c_void_p
kernel.SetEvent.argtypes = kernel.ResetEvent.argtypes = kernel.CloseHandle.argtypes = [ctypes.c_void_p]
# The harness input channels are slot-suffixed (flow_run's named events); slot 0 has no suffix.
channel_suffix = '' if args.slot == 0 else '_%d' % args.slot
events = {name: kernel.CreateEventW(None, True, False, 'Local\\BurnoutPC_Input_' + name + channel_suffix)
          for name in ('SteerLeft', 'SteerRight', 'SteerFrac25', 'SteerFrac50')}
assert all(events.values())
# The pedals belong to flow_run's drive schedule (Drive = $true holds Accelerate for the whole
# run and re-applies it only when its schedule CHANGES). The driver borrows them for one thing:
# backing out of a wedge (see UNSTICK below), and hands Accelerate back afterwards.
pedals = {name: kernel.CreateEventW(None, True, False, 'Local\\BurnoutPC_Input_' + name + channel_suffix)
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

def wrapper_case(case_path, evidence_dir):
    """The case run_case.ps1 executes: the original, unchanged, plus -- for --ai-pad -- the seat's variable
    through DiagEnv and a check that the seat really armed (a run that never armed is not an AI-pad run),
    -- for --diag-env -- the extra witnesses, and -- for --no-frames -- Frames off. Written into the evidence
    dir so the run documents itself."""
    if not args.ai_pad and not args.no_frames and not args.diag_env:
        return case_path
    source = (root / case_path).resolve() if not Path(case_path).is_absolute() else Path(case_path)
    lines = ["# Generated by b5-decomp/tests/run_rival_organic.py -- the case below, as run.",
             "$case = & '%s'" % str(source).replace("'", "''")]
    if args.ai_pad:
        lines += ["if ($case.DiagEnv) { $case.DiagEnv += ',BRN_AI_PAD_PLAYER=%s' } else { $case.DiagEnv = 'BRN_AI_PAD_PLAYER=%s' }"
                  % (args.ai_pad, args.ai_pad),
                  "$case.Checks += @(@{ Kind = 'LogMatch'; Name = 'the AI PAD seat armed (BRN_AI_PAD_PLAYER=%s)'; "
                  "Pattern = '\\[ai-pad\\] \\*\\*\\*\\*\\* HARNESS-ONLY \\(BRN_AI_PAD_PLAYER=%s\\): armed'; Expect = $true })"
                  % (args.ai_pad, args.ai_pad)]
    if args.diag_env:
        extra = args.diag_env.replace("'", "''")
        lines += ["if ($case.DiagEnv) { $case.DiagEnv += ',%s' } else { $case.DiagEnv = '%s' }" % (extra, extra)]
    if args.no_frames:
        lines += ["$case.Frames = $false"]
    lines += ["$case"]
    path = evidence_dir / 'case_as_run.ps1'
    path.write_text('\r\n'.join(lines) + '\r\n', encoding='utf-8-sig')
    return str(path)

number = r'([-+0-9.eE]+)'
motion_re = re.compile(r'\[motion\] n (\d+) pos ' + ' '.join([number]*3)
                       + ' at ' + ' '.join([number]*3) + ' vel ' + ' '.join([number]*3))
gas_re = re.compile(r'\bgas (-?[0-9.]+)')   # the game's own mfGas on the [motion] line
rival_re = re.compile(r'\[rival\] slot (\d+) global (\d+) pos \(' + ', '.join([number]*3) + r'\)')
log = root / ('build/game/BrnGame.log' if args.slot == 0 else 'build/game_slots/%d/BrnGame.log' % args.slot)
start = time.time()
evidence = root / 'scratch/bugtest/runs' / args.run_name / time.strftime('%Y%m%d_%H%M%S')
evidence.mkdir(parents=True)
case_to_run = wrapper_case(args.case, evidence)
output = open(evidence / 'driver.console.log', 'w', encoding='utf-8')
label = ('ai-pad-' + args.ai_pad) if args.ai_pad else 'rival-pad-pursuit'
if args.slot:
    # A slot run holds the SLOT-0 box lock for its whole length (the lane recipe: no slot-0 run shares
    # the box with it), then run_case takes the slot's own lock. The lock is released when this
    # PowerShell exits (_box_lock.ps1: an abandoned mutex passes to the next waiter).
    def ps_quote(text):
        return "'" + str(text).replace("'", "''") + "'"
    command = ('. ' + ps_quote('tools/diagnostics/_box_lock.ps1') + '; '
               + 'Enter-BoxLock -TimeoutSec 7200 -Label ' + ps_quote('%s-slot%d' % (label, args.slot)) + ' -Slot 0; '
               + '& ' + ps_quote('tools/tests/run_case.ps1') + ' -Case ' + ps_quote(case_to_run)
               + ' -RunDir ' + ps_quote(evidence) + ' -Label ' + ps_quote(label) + ' -Slot ' + str(args.slot)
               + '; exit $LASTEXITCODE')
    run_case_args = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', command]
else:
    run_case_args = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                     'tools/tests/run_case.ps1', '-Case', case_to_run,
                     '-RunDir', str(evidence), '-Label', label]
proc = subprocess.Popen(run_case_args, cwd=root,
                        stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
position = 0
carry = ''
rivals = {}
motion = None
target = None
last_frame = -1
last_note = 0
trace = open(evidence / 'driver.controls.log', 'w', encoding='utf-8', buffering=1)
if args.ai_pad:
    trace.write(f'driver: --ai-pad {args.ai_pad} -- the game\'s own AI drives through the pad path '
                f'(BRN_AI_PAD_PLAYER={args.ai_pad} via the case DiagEnv); the Python steering is OFF and the '
                f'UNSTICK below is only a logged fallback.\n')
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
# With --ai-pad the game's AI owns the throttle ([motion] gas is the AI's), and the seat hands the car
# back to the real pad while its Brake is held, so the same rule backs a wedged AI out.
# Every manoeuvre is logged to driver.controls.log.
WEDGE_SECONDS, WEDGE_METRES, UNSTICK_SECONDS, UNSTICK_MAX = 6.0, 1.0, 2.0, 20
track = []            # (time, x, z, frame, gas) of recent [motion] samples
unstick_until = 0.0   # while time() < this, the driver is backing out
unsticks = 0
last_steer = 0
last_unstick_end = 0.0
motion_gas = 0.0
driving_seen = False  # no unstick before the car has driven once (a start-line hold is not a wedge)
pad_armed = False     # --ai-pad: the seat's own arm / stand-down lines say who drives right now
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
                if line.startswith(('[ai-pad] ***** HARNESS-ONLY', '[ai-pad] re-armed')): pad_armed = True
                elif line.startswith('[ai-pad] stood down'): pad_armed = False
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
                                f'for {UNSTICK_SECONDS} s'
                                + ((' (FALLBACK: the AI pad was wedged)' if pad_armed
                                    else ' (the AI pad is not armed: flow_run\'s held throttle was driving)')
                                   if args.ai_pad else '')
                                + '\n')
            if args.ai_pad:
                pass   # the game's AI steers; this script only watches for a wedge (above)
            elif motion and rivals and motion[0] != last_frame and not unstick_until:
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
    trace.write(f'unsticks {unsticks}\n')
    trace.close()
    output.close()
print('Report:', evidence / 'REPORT.md')
sys.exit(proc.returncode)
