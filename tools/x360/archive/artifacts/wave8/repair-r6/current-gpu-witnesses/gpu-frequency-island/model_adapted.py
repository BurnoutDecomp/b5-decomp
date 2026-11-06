"""Exact source-extracted refresh branch versus original Main instruction island.

Whole Main was attempted and remains negative with its unchanged errors/guards.
The diagnostic compiles the exact changed branch and genuine inline GPU setter;
actual CPU/GPU provider definitions and StrStream char operations execute. Integer
formatting and assertion synchronization are controlled interfaces. No whole Main,
full startup/frame, GPU hardware or strict-link proof is claimed.
"""
from pathlib import Path
import hashlib, json, random, struct, sys
OUT = Path(__file__).resolve().parent
sys.path.insert(0, 'D:/Reverse/BP-X360-Experimental-Artifacts/perfmon-gpu-native-r4')
from gpu_fixture import F, Obj, Bundle, NativeCPU, M32, sha, branch_target
STACK = 1073741824
MESSAGE = 1342242816

class Stop(Exception):
    pass

def make_bundle():
    rows = json.loads(_r6_report(OUT / 'successor-native-v1/native/compile-results.json').read_text())['results']
    paths = [_r6_object(OUT / 'actual-branch-v1.obj')] + [Path(r['object']) for r in rows if r['passed'] and (not r['source'].endswith('CgsPerfMonGpuPS3.cpp'))]
    old = json.loads(_r6_report(Path('D:/Reverse/BP-X360-Experimental-Artifacts/perfmon-gpu-native-r4/final-native-v1/native/compile-results.json')).read_text())['results']
    paths += [Path(r['object']) for r in old if r['source'].endswith('CgsStrStream.cpp')]
    objects = [Obj(p) for p in paths]
    b = Bundle(objects)
    b.select('ObservedMainFrequencyBranch')
    return (b, objects)

def execute(bundle, compiled, index, rate):
    rng = random.Random(2185014468 + index)
    m = F.M.V.Mem()
    m.add(STACK, 65536, rng.randbytes(65536))
    m.add(MESSAGE, 4096, b'X' * 4096)
    m.add(2197913600, 8192)
    m.add(2196971520, 4096)
    m.add(2222981120, 65536)
    if compiled:
        for a, data in bundle.images.items():
            m.add(a, len(data), data)
    c = NativeCPU(m)
    c.setg(1, STACK + 32768)
    c.max_steps = 50000
    c.steps = 0
    c.atomic_trace = []
    c.fence_trace = []
    c.msr = 32768
    c.reservation = None
    c.failures = 0
    cpu = bundle.addr('?meGameFrequency@PerfMonCpu@') if compiled else 2197914196
    gpu = bundle.addr('?meGameFrequency@PerfMonGpu@') if compiled else 2197914200
    buffer = bundle.ext('?gpcMessageBuffer@') if compiled else 2196972128
    capacity = bundle.ext('?giMessageBufferSize@') if compiled else 2196972132

    def put(a, v, n=4):
        m.wr(a, (v & (1 << 8 * n) - 1).to_bytes(n, 'big'))
    initial_cpu = rng.getrandbits(32)
    initial_gpu = rng.getrandbits(32)
    put(cpu, initial_cpu)
    put(gpu, initial_gpu)
    put(buffer, MESSAGE)
    put(capacity, 256)
    trace = []

    def begin(x):
        trace.append(['begin'])

    def fire(x):
        trace.append(['fire', F.cstring(x, x.g(3) & M32), F.cstring(x, x.g(4) & M32), x.g(5) & M32])

    def end(x):
        trace.append(['end'])

    def cat(x):
        dest, src, limit = (x.g(3) & M32, x.g(4) & M32, x.g(5) & M32)
        a = F.cstring(x, dest).encode()
        z = F.cstring(x, src).encode()[:limit]
        m.wr(dest + len(a), z + bytes([0]))
        x.setg(3, dest)

    def integer(x):
        stream = x.g(3) & M32
        assert int.from_bytes(m.rd(stream + 4, 4), 'big') == 0
        dest = int.from_bytes(m.rd(stream + 8, 4), 'big')
        size = int.from_bytes(m.rd(stream + 12, 4), 'big')
        value = x.g(4) & M32
        if value & 2147483648:
            value -= 4294967296
        previous = F.cstring(x, dest).encode()
        text = str(value).encode()
        assert len(previous) + len(text) < size
        m.wr(dest + len(previous), text + bytes([0]))
        x.setg(3, stream)
    if compiled:
        for a, name in bundle.unresolved.items():
            if name.startswith('?BeginAssert@'):
                c.hooks[a] = begin
            elif name.startswith('?FireAssert@'):
                c.hooks[a] = fire
            elif name.startswith('?EndAssert@'):
                c.hooks[a] = end
            elif name == 'strncat':
                c.hooks[a] = cat
            elif name.startswith(('?gpcMessageBuffer@', '?giMessageBufferSize@')):
                pass
            else:

                def reject(x, name=name):
                    raise AssertionError(('unexpected native service', name))
                c.hooks[a] = reject
        integers = [name for name in bundle.definitions if name.startswith('??6StrStreamBase@') and name.endswith('H@Z')]
        assert len(integers) == 1, integers
        c.hooks[bundle.addr(integers[0])] = integer
        c.setg(3, rate)
        c.run(bundle.entry)
    else:
        c.hooks[2189522248] = begin
        c.hooks[2189559824] = fire
        c.hooks[2189522264] = end
        c.hooks[branch_target(2189516468)] = cat
        c.hooks[2183073360] = integer
        c.setg(28, rate)
        c.setg(29, 0)
        c.setg(27, 1)

        def observe(pc, word):
            if pc == 2185014652:
                raise Stop()
        c.instruction_observer = observe
        try:
            c.run(2185014468)
        except Stop:
            pass
        else:
            raise AssertionError('did not reach native branch end')
    return {'cpu': m.rd(cpu, 4).hex(), 'gpu': m.rd(gpu, 4).hex(), 'message': m.rd(MESSAGE, 256).hex(), 'trace': trace, 'initial_cpu': hex(initial_cpu), 'initial_gpu': hex(initial_gpu), 'rate': rate}

def main():
    b, objects = make_bundle()
    cases = []
    errors = []
    rates = [50, 60, 0, 24, 25, 29, 30, 49, 51, 59, 61, -1, 120, 2147483647, 2147483648, 4294967295]
    for index in range(64):
        rate = rates[index % len(rates)]
        pair = [execute(b, c, index, rate) for c in [False, True]]
        if pair[0] != pair[1]:
            errors.append({'index': index, 'original': pair[0], 'compiled': pair[1]})
            break
        cases.append({'rate': rate, 'cpu': pair[0]['cpu'], 'gpu': pair[0]['gpu'], 'asserts': len(pair[0]['trace']) // 3})
    result = {'cases': len(cases), 'mismatches': errors, 'scope': __doc__, 'actual_objects': {str(o.path): sha(o.path) for o in objects}, 'source_extraction_receipt_sha256': sha(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\named-native-gates\\gate-results.json')), 'fixture_sha256': sha(__file__), 'cases_summary': cases}
    _r6_output(OUT / 'frequency-branch-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Frequency branch', len(cases), 'cases;', len(errors), 'mismatches')
    if errors:
        print(str(errors[0])[:5000])
    raise SystemExit(bool(errors))
if __name__ == '__main__':
    main()