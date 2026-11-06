from pathlib import Path
import hashlib, json, random, struct, sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
EXPORT = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
sys.path.insert(0, str(PARENT / 'scratch/x360_sdk'))
# Import the existing fixture without permitting its legacy output-directory hooks.
mkdir = Path.mkdir
Path.mkdir = lambda *args, **kwargs: None
try:
    import native_atomic_fixture as F
finally:
    Path.mkdir = mkdir

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
report = json.loads((HERE / 'native/compile-results.json').read_text())
path = Path(next(row['object'] for row in report['results']
                 if row['source'].endswith('OSGlobalManager.cpp')))
obj = F.Coff(path, '??__Es_autoinitOSGlobalManager@')
original_globals = (0x8327F024, 0x8327F020, 0x8327F05C)
specs = {'ctor': (0x82BBC878, '??0OSGlobalManager@'),
         'allocate': (0x82BBC8D8, '?AllocateAndConstructOSGlobalManager@'),
         'init': (0x82BBD228, '?InitOSGlobalSystem@'),
         'shutdown': (0x82BBC918, '?ShutdownOSGlobalSystem@'),
         'crt': (0x82C73EB0, '??__Es_autoinitOSGlobalManager@')}
contexts, entries = {}, {}
for name, (original, prefix) in specs.items():
    bundle = F.Bundle([obj])
    bundle.select(prefix)
    globals_ = ((F.address(bundle, '?s_pOSGlobalManager@'),
                 F.address(bundle, '?s_hGlobalSemaphore@'),
                 F.address(bundle, '?s_initRefCount@'))
                if name in ('init', 'shutdown', 'crt') else original_globals)
    contexts[name] = (bundle, globals_)
    entries[name] = (original, bundle.entry)

# The actual object must register this initializer in the native CRT table.
crt_slots = [(sec, obj.relocations(sec)) for sec, header in obj.sections.items()
             if header[0].rstrip(b'\0') == b'.CRT$XCU']
assert len(crt_slots) == 1 and len(crt_slots[0][1]) == 1
at, index, kind = crt_slots[0][1][0]
assert at == 0 and kind == 2 and obj.symbols[index][0] == obj.fn[0]

class NullManagerAtomic(Exception):
    pass

class CheckedCPU(F.AtomicCPU):
    def step(self, pc, word, nxt, depth):
        if word >> 26 == 31 and ((word >> 1) & 1023) in (20, 150):
            ra, rb = (word >> 16) & 31, (word >> 11) & 31
            address = ((self.g(ra) if ra else 0) + self.g(rb)) & 0xFFFFFFFF
            if address == 0x10:
                raise NullManagerAtomic()
        return super().step(pc, word, nxt, depth)

BASE, MANAGER, HEAP, MUTEX, SEMAPHORE = 0x45000000, 0x45001000, 0x45004000, 0x51001000, 0x51002000
kernel_addresses = {'RtlInitializeCriticalSection': 0x82C75E8C,
                    'GetProcessHeap': 0x829384F8, 'RtlAllocateHeap': 0x82936B40,
                    'RtlFreeHeap': 0x82937430, 'CloseHandle': 0x829278B8,
                    'CreateMutexA': 0x82C20720, 'WaitForSingleObject': 0x8292C808,
                    'CreateSemaphoreA': 0x82C20420, 'GetLastError': 0x8292C7D0,
                    'ReleaseSemaphore': 0x82C204B8, 'ReleaseMutex': 0x82C207A8,
                    'atexit': 0x82C09120}
groups, failures = {}, []

def execute(kind, compiled, config, poison):
    bundle, native_globals = contexts[kind]
    cpu = F.cpu(bundle)
    cpu.__class__ = CheckedCPU
    cpu.mem.add(BASE, 0x10000, poison)
    globals_ = native_globals if compiled else original_globals
    pointer = MANAGER if config.get('present') else 0
    for address, value in zip(globals_, (pointer, SEMAPHORE if config.get('sem') else 0,
                                         config.get('count', 0))):
        F.put(cpu, address, value)
    F.put(cpu, MANAGER - 4, HEAP)
    F.put(cpu, MANAGER + 0x10, config.get('shares', 1))
    cpu.setg(3, MANAGER)
    cpu.failures = config.get('retries', 0)
    trace, callbacks = [], []
    raw = HEAP + config.get('alignment', 0)
    def g(n):
        return cpu.g(n) & 0xFFFFFFFF
    def hook(name):
        def called(c):
            if name == 'GetProcessHeap':
                trace.append((name,))
                c.setg(3, 0x51003000)
            elif name == 'RtlAllocateHeap':
                trace.append((name, g(3), g(4), g(5)))
                c.setg(3, raw)
            elif name == 'RtlInitializeCriticalSection':
                trace.append((name, g(3), c.mem.rd(g(3) - 0x14, 0x14).hex()))
                c.mem.wr(g(3), bytes(range(28)))
                c.setg(3, 0)
            elif name == 'CreateMutexA':
                trace.append((name, g(3), g(4), F.cstring(c, g(5))))
                c.setg(3, 0 if config.get('mode') == 'mutex-fail' else MUTEX)
            elif name == 'WaitForSingleObject':
                trace.append((name, g(3), g(4)))
                c.setg(3, 0xFFFFFFFF if config.get('mode') == 'wait-fail' else 0)
            elif name == 'CreateSemaphoreA':
                trace.append((name, g(3), g(4), g(5), F.cstring(c, g(6))))
                c.setg(3, 0 if config.get('mode') == 'sem-fail' else SEMAPHORE)
            elif name == 'GetLastError':
                trace.append((name,))
                c.setg(3, 183 if config.get('mode') in ('existing', 'release-fail') else 0)
            elif name == 'ReleaseSemaphore':
                trace.append((name, g(3), g(4), bool(g(5))))
                if g(5):
                    F.put(c, g(5), MANAGER >> 4)
                c.setg(3, int(config.get('mode') != 'release-fail'))
            elif name == 'atexit':
                trace.append((name, bool(g(3))))
                callbacks.append(g(3))
                c.setg(3, 0)
            elif name == 'RtlFreeHeap':
                trace.append((name, g(3), g(4), g(5)))
                c.setg(3, 1)
            else:
                trace.append((name, g(3)))
                c.setg(3, 1)
        return called
    for name, address in kernel_addresses.items():
        cpu.hooks[address] = hook(name)
    for address, name in bundle.unresolved.items():
        if name in kernel_addresses:
            cpu.hooks[address] = hook(name)
    fault = False
    try:
        cpu.run(entries[kind][int(compiled)])
        if kind == 'crt':
            assert len(callbacks) == 1
            cpu.run(callbacks[0])
    except NullManagerAtomic:
        fault = True
    ret = g(3) if kind in ('ctor', 'allocate', 'init') and not fault else None
    state = tuple(int.from_bytes(cpu.mem.rd(address, 4), 'big') for address in globals_)
    return {'return': ret, 'fault': fault, 'state': state, 'trace': trace,
            'manager': cpu.mem.rd(MANAGER - 4, 0x84).hex(),
            'heap': cpu.mem.rd(HEAP, 0x100).hex(),
            'atomics': cpu.atomic_trace}

def check(kind, config, index):
    poison = random.Random(0x360000 + index).randbytes(0x10000)
    pair = [execute(kind, compiled, config, poison) for compiled in (False, True)]
    groups[kind] = groups.get(kind, 0) + 1
    if pair[0] != pair[1]:
        failures.append({'kind': kind, 'config': config, 'original': pair[0], 'compiled': pair[1]})

for i in range(128):
    check('ctor', {'retries': i % 4}, i)
    check('allocate', {'alignment': i % 32, 'retries': i % 4}, i + 128)
for i, mode in enumerate(('new', 'existing', 'mutex-fail', 'wait-fail', 'sem-fail', 'release-fail')):
    for n in range(32):
        check('init', {'mode': mode, 'shares': n + 1, 'alignment': n % 16,
                       'retries': n % 3}, 256 + 32 * i + n)
for i in range(32):
    check('init', {'present': True, 'sem': True, 'count': i % 4, 'shares': i + 1}, 448 + i)
for n in range(128):
    check('shutdown', {'present': bool(n & 1), 'sem': bool(n & 2),
                       'count': (0, 1, 2, 0xFFFFFFFF)[(n >> 2) % 4],
                       'shares': (0, 1, 2, 0xFFFFFFFF)[(n >> 4) % 4],
                       'retries': (n >> 6) % 2}, 480 + n)
for n in range(64):
    check('crt', {'mode': ('new', 'existing', 'mutex-fail', 'wait-fail')[n % 4],
                  'shares': n + 1, 'alignment': n % 16}, 608 + n)

result = {'object': str(path), 'object_sha256': sha(path), 'cases': sum(groups.values()),
          'groups': groups, 'mismatches': failures, 'native_crt_slot': crt_slots,
          'original_exports': {str(EXPORT / ('0x%08X.json' % address)): sha(EXPORT / ('0x%08X.json' % address))
                               for address, _ in entries.values()},
          'shared_fixture': str(PARENT / 'scratch/x360_sdk/native_atomic_fixture.py'),
          'shared_fixture_sha256': sha(PARENT / 'scratch/x360_sdk/native_atomic_fixture.py'),
          'verifier_sha256': sha(__file__),
          'limits': 'Actual original and compiled PPC execute in the shared software model. Kernel mutex/semaphore/heap/critical-section/atexit services are controlled trace hooks. Preserve original null-manager atomic fault paths using an explicit observer, without making the null page writable behavior part of the claim. Poisoned manager bytes, cookie, state, calls and reservation attempts compared. CRT initialization and registered shutdown are composed. No hardware, real contention, whole game, link or launch claim.'}
with (HERE / 'native-comparison.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({'cases': result['cases'], 'groups': groups, 'mismatches': len(failures)}, indent=2))
assert not failures
