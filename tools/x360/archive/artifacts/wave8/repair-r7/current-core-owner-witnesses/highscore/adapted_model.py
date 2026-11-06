from pathlib import Path
import sys, json, random, struct, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
import native_atomic_fixture as F
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/gamestate-c03-r5-terra/final')
row = next((x for x in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-core-owner-witnesses\\highscore\\actual-row-selection.json').read_text())['results'] if x['source'].endswith('BrnGameStateStreetManager_wC_00.cpp')))
obj = F.Coff(Path(row['object']), '?UpdateBufferedHighScores@StreetManager@BrnGameState@@')
b = F.Bundle([obj])
b.select('?UpdateBufferedHighScores@StreetManager@BrnGameState@@')
SELF = F.M.S.SELF
AUX = F.M.S.AUX
STACK = F.M.S.STACK
OUTPUT = AUX + 4096
QUEUE = AUX + 8192
MASK = 4294967295

def branch(pc):
    w = F.M.V.x360rd.u32(pc)
    d = w & 67108860
    return pc + (d - 67108864 if d & 33554432 else d) & MASK

def run(comp, summary, lost, score, seed):
    c = F.cpu(b)
    r = random.Random(seed)
    c.mem.wr(SELF, r.randbytes(8192))
    c.mem.wr(STACK, r.randbytes(131072))
    F.put(c, SELF + 7328, 1)
    c.mem.wr(SELF + 7336, b'\x00' * 8)
    F.put(c, SELF + 7360, 0)
    F.put(c, SELF + 7356, lost)
    F.put(c, SELF + 7352, 1120403456)
    c.mem.wr(SELF + 7364, bytes([summary]))
    events = []
    trace = []

    def gui(x):
        x.setg(3, QUEUE)

    def action(x):
        x.setg(3, QUEUE)

    def roads(x):
        trace.append(['roads', x.g(3) & MASK])
        x.setg(3, score)

    def get(x):
        x.setg(3, SELF + 7168)

    def erase(x):
        F.put(x, SELF + 7328, 0)

    def add(x):
        events.append([x.g(3) & MASK, x.g(5) & MASK, x.g(6) & MASK, x.mem.rd(x.g(4) & MASK, x.g(6) & MASK).hex()])
        x.setg(3, 1)

    def copy(x):
        d = x.g(3) & MASK
        s = x.g(4) & MASK
        n = x.g(5) & MASK
        x.mem.wr(d, x.mem.rd(s, n))
        x.setg(3, d)
    for pc, fn in ((2184484044, roads), (2184484272, roads), (2184484064, gui), (2184484300, action), (2184484340, gui), (2184484080, add), (2184484152, copy), (2184484136, get), (2184484292, erase)):
        c.hooks[branch(pc)] = fn
    c.hooks[F.address(b, '?AddEvent@?$VariableEventQueue')] = add
    c.hooks[F.address(b, '??A?$Array@UBufferedNewHighScore')] = get
    c.hooks[F.address(b, '?Erase@?$Array@UBufferedNewHighScore')] = erase
    for a, n in b.unresolved.items():
        for fragment, fn in (('GetNumberOfRoadsRulesByLocalPlayer', roads), ('GetGuiOutputQueue', gui), ('GetGameActionQueue', action), ('memcpy', copy)):
            if fragment in n:
                c.hooks[a] = fn
    c.setg(3, SELF)
    c.setf(1, 0.25)
    c.setg(5, 0)
    c.setg(6, OUTPUT)
    c.run(b.entry if comp else 2184483824)
    return {'owner': c.mem.rd(SELF, 8192).hex(), 'events': events, 'calls': trace}
errors = []
case = 0
for summary in (0, 1):
    for lost in (0, 3, 4294967295):
        for score in (0, 5, 2147483647):
            for z in range(8):
                a = run(False, summary, lost, score, case)
                c = run(True, summary, lost, score, case)
                case += 1
                if a != c:
                    errors.append({'summary': summary, 'lost': lost, 'score': score, 'fields': [k for k in a if a[k] != c[k]], 'original_events': a['events'], 'native_events': c['events'], 'orig_calls': a['calls'], 'native_calls': c['calls']})
result = {'scope': 'Original vs native actual UpdateBufferedHighScores, emitted summary and single record with seeded stack bytes; output queue, array get/erase, memcpy, and road count controlled; owner bytes, 48-byte payload incl untouched bytes, calls/order compared. No real queue/services or complete StreetManager parity.', 'cases': case, 'mismatches': errors, 'object': row['object'], 'object_sha256': hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()}
_r5_output(out / 'highscore-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('cases', case, 'mismatches', len(errors))
print(json.dumps(errors[:1], indent=2))
assert not errors