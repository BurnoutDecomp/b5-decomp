from pathlib import Path
import sys, json, random, struct, hashlib, math
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/gamestate-c03-r5-terra/final')
row = next((x for x in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-core-owner-witnesses\\completion\\actual-row-selection.json').read_text())['results'] if x['source'].endswith('BrnProgressionManager_Completion.cpp')))
obj = F.Coff(Path(row['object']), '?SendGameCompletionResults@ProgressionManager@BrnProgression@@')
b = F.Bundle([obj])
b.select('?SendGameCompletionResults@ProgressionManager@BrnProgression@@')
SELF = F.M.S.SELF
AUX = F.M.S.AUX
STACK = F.M.S.STACK
MODE = AUX + 65536
QUEUE = AUX + 126976
MASK = 4294967295

def branch(pc):
    w = F.M.V.x360rd.u32(pc)
    d = w & 67108860
    return pc + (d - 67108864 if d & 33554432 else d) & MASK

def run(comp, pct, seen, dated, mode, seed):
    c = F.cpu(b)
    r = random.Random(seed)
    c.mem.wr(SELF, r.randbytes(131072))
    c.mem.wr(AUX, r.randbytes(65536))
    c.mem.wr(STACK, r.randbytes(131072))
    F.put(c, SELF + 133436, MODE)
    F.put(c, MODE + 3476, mode)
    c.mem.wr(SELF + 118400, bytes([seen]))
    c.mem.wr(SELF + 118405, bytes([dated]))
    events = []
    calls = []

    def compute(x):
        x.setf(1, pct)

    def getter(x):
        x.setg(3, x.mem.rd(SELF + 118400, 1)[0])

    def update(x):
        calls.append(['date', x.g(3) & MASK])

    def add(x):
        events.append([x.g(3) & MASK, x.g(5) & MASK, x.g(6) & MASK, x.mem.rd(x.g(4) & MASK, x.g(6) & MASK).hex()])
        x.setg(3, 1)
    c.hooks[branch(2184797264)] = compute
    c.hooks[F.address(b, '?ComputeCompletionPercentage@ProgressionManager')] = compute
    c.hooks[branch(2184797340)] = update
    c.hooks[branch(2184797388)] = add
    c.hooks[F.address(b, '?AddEvent@?$VariableEventQueue')] = add
    for a, n in b.unresolved.items():
        if '?GetSeen100PercentCompletionSequence@' in n:
            c.hooks[a] = getter
        if '?Update@DateAndTime@' in n:
            c.hooks[a] = update
    c.setg(3, SELF)
    c.setg(4, QUEUE)
    c.run(b.entry if comp else 2184797224)
    return {'owner': (c.mem.rd(SELF, 131072) + c.mem.rd(AUX, 2560)).hex(), 'events': events, 'calls': calls}
errors = []
cases = 0
for p in (0.0, 99.0, 100.0, 101.0, float('nan')):
    for seen in (0, 1):
        for dated in (0, 1):
            for mode in (0, 4, 18, 4294967295):
                a = run(False, p, seen, dated, mode, cases)
                c = run(True, p, seen, dated, mode, cases)
                cases += 1
                if a != c:
                    errors.append({'pct': str(p), 'seen': seen, 'dated': dated, 'mode': mode, 'fields': [k for k in a if a[k] != c[k]], 'events_original': a['events'], 'events_native': c['events'], 'calls_original': a['calls'], 'calls_native': c['calls']})
result = {'scope': 'Original versus native actual SendGameCompletionResults; seeded owner and entire event bytes incl untouched padding; ComputeCompletionPercentage, Profile flag getter, DateAndTime Update and AddEvent controlled; compare owner, 8-byte payload, date/update order; real services excluded.', 'cases': cases, 'mismatches': errors, 'object': row['object'], 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()}
_r5_output(out / 'completion-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('cases', cases, 'mismatches', len(errors))
print(json.dumps(errors[:3], indent=2))
assert not errors