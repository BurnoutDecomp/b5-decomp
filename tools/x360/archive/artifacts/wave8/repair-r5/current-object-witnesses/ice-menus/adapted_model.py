from pathlib import Path
import json, random, struct, sys, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(P / 'scratch/x360_sdk'))
from native_atomic_fixture import Coff, Bundle, AtomicCPU
import validate_native_math as M
O = Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-geometry-a946-terra')
j = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\ice-menus\\actual-row-selection.json').read_text())
row = next((x for x in j['results'] if x['source'].endswith('ICEControllerMenus.cpp')))
assert row['passed']
obj = Path(row['object'])
b = Bundle([Coff(obj, '?ConstructMenus@ICEController@')])
b.select('?ConstructMenus@ICEController@')
rng = random.Random(2186486128)

def run(compiled, image):
    c = b.cpu()
    c.__class__ = AtomicCPU
    c.mem.add(2222981120, 16384)
    c.mem.wr(M.S.SELF, image)
    c.setg(3, M.S.SELF)
    allocations = []
    bars = []

    def allocate(c):
        allocations.append(c.g(4) & 4294967295)
        c.setg(3, M.S.AUX + 4096 + 256 * (len(allocations) - 1))

    def menu(c):
        c.mem.wr((c.g(3) & 4294967295) + 52, struct.pack('>ff', 12.0, 20.0))

    def width(c):
        c.setf(1, 15.0)

    def bar(c):
        bars.append(((c.g(3) & 4294967295) - M.S.SELF, M.bits(c.f(1)), M.bits(c.f(2)), M.bits(c.f(3)), c.g(7) & 255))
    for address, hook in [(2186464384, allocate), (2186481512, menu), (2186465360, width), (2186481784, bar)]:
        c.hooks[address] = hook
    if compiled:
        for address, name in b.unresolved.items():
            mapping = [('?GetMemory@ICEMemory@', allocate), ('?Construct@ICEWidgetMenu@', menu), ('?GetWidth@ICEWidgetMenu@', width), ('?Construct@ICEWidgetTimeBar@', bar)]
            selected = [f for prefix, f in mapping if name.startswith(prefix)]
            if selected:
                c.hooks[address] = selected[0]
            elif name.startswith('__savefpr_'):
                target = 2193663412 + 4 * (int(name.rsplit('_', 1)[1]) - 23)
                c.hooks[address] = lambda cc, t=target: cc.run(t)
            elif name.startswith('__restfpr_'):
                target = 2193663488 + 4 * (int(name.rsplit('_', 1)[1]) - 23)
                c.hooks[address] = lambda cc, t=target: cc.run(t)
            else:
                raise AssertionError(('unresolved', name))
    c.run(b.entry if compiled else 2186485568)
    return (allocations, bars, c.mem.rd(M.S.SELF + 28996, 36), c.mem.rd(M.S.SELF + 29168, 16))
for i in range(64):
    image = bytearray(rng.randbytes(36864))
    struct.pack_into('>I', image, 3960, M.S.AUX)
    a = run(False, image)
    z = run(True, image)
    if a != z:
        raise AssertionError((i, a, z))
r = {'cases': 64, 'scope': 'Original 82531F40 vs actual whole ICEController::ConstructMenus, compare allocator sequence, both timebar argument/register sets, nine picker floats and four rectangle floats on poisoned state. Allocation/menu/width/timebar are controlled boundaries; original image origin values 32/418 only; later global mutation not tested; other menu state and hardware unproved.', 'object': str(obj), 'sha256': hashlib.sha256(obj.read_bytes()).hexdigest()}
_r5_output(O / 'geometry-menus-comparison-v1.json').write_text(json.dumps(r, indent=2) + '\n')
print(json.dumps(r, indent=2))