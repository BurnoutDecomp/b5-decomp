"""Actual native objects/raw ARTIST with controlled SDK services and callback scheduling."""
from pathlib import Path
import hashlib
import json
import random
import re
import struct
import sys
OUT = Path(__file__).resolve().parent
sys.path.insert(0, 'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc')
from validate_startup_phase import F, Obj, Bundle
M32 = 4294967295
M64 = 18446744073709551615
STACK = 1073741824
MESSAGE = 1342242816
DEVICE = 1342308352
ENTRIES = {'callback': 2189580728, 'Construct': 2189581256, 'Swap': 2189581512, 'StartProfiling': 2189581672, 'StopProfiling': 2189581824, 'StartMonitor': 2189581944, 'StopMonitor': 2189582488, 'ComputeValueFromTimer': 2189582952, 'GetLastMonitorValue': 2189610016, 'AddMonitor': 2189579696}
ORIGINAL_STATE = {'values': (2197914224, 2680), 'factor': (2197916904, 4), 'monitors': (2197918344, 1848), 'max': (2197914204, 4), 'frequency': (2197914200, 4), 'active': (2197914192, 1), 'running': (2197914194, 1), 'id': (2197914208, 4)}
SYMBOLS = {'values': '?gValues@', 'factor': '?gfCyclesToPercent@', 'monitors': '?maMonitors@PerfMonGpu@', 'max': '?miMaxMonitor@PerfMonGpu@', 'frequency': '?meGameFrequency@PerfMonGpu@', 'active': '?mbActiveMonitor@PerfMonGpu@', 'running': '?mbProfilingRunning@PerfMonGpu@', 'id': '?miActiveMonitorID@PerfMonGpu@'}

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def branch_target(address):
    word = F.M.V.x360rd.u32(address)
    assert word >> 26 == 18
    delta = word & 67108860
    if delta & 33554432:
        delta -= 67108864
    return address + delta & M32

class NativeCPU(F.AtomicCPU):
    """Exact missing integer instructions needed by the actual SDK __u64tod."""

    def step(self, pc, word, nxt, depth=0):
        if pc in self.hooks:
            self.hooks[pc](self)
            raise F.M.V.Return()
        if word >> 26 == 31:
            xo = word >> 1 & 1023
            rs, ra, rb = (word >> 21 & 31, word >> 16 & 31, word >> 11 & 31)
            if xo == 58:
                assert word & 1 == 0
                value = self.g(rs) & M64
                self.setg(ra, 64 if value == 0 else 64 - value.bit_length())
                return nxt
            if xo == 27:
                assert word & 1 == 0
                shift = self.g(rb) & 127
                self.setg(ra, self.g(rs) << shift & M64 if shift < 64 else 0)
                return nxt
        return super().step(pc, word, nxt, depth)

class NativeBundle(Bundle):
    """Relocate all requested actual function roots into one consistent image."""

    def select_roots(self, prefixes):
        pending = []
        for prefix in prefixes:
            found = [(name, value) for name, value in self.definitions.items() if name.startswith(prefix)]
            assert len(found) == 1, (prefix, found)
            _, (oi, value, section) = found[0]
            pending.append((oi, section))
        used = set()
        while pending:
            key = pending.pop()
            if key in used:
                continue
            used.add(key)
            obj = self.objects[key[0]]
            for at, ix, kind in obj.rels(key[1]):
                if kind == 18:
                    continue
                name, value, section = obj.symbols[ix]
                if section > 0:
                    pending.append((key[0], section))
                elif name in self.definitions:
                    oi, value, section = self.definitions[name]
                    pending.append((oi, section))
        self.bases = {}
        address = 2206203904
        for key in sorted(used):
            self.bases[key] = address
            size = self.objects[key[0]].sections[key[1]][3]
            address += max(size, 1) + 65535 & ~65535
        assert address < 2218786816, hex(address)
        self.images, self.unresolved, external = ({}, {}, {})
        for key in sorted(used):
            obj = self.objects[key[0]]
            section = obj.sections[key[1]]
            body = bytearray(obj.data[section[4]:section[4] + section[3]]) if section[4] else bytearray(section[3])
            for at, ix, kind in obj.rels(key[1]):
                if kind == 18:
                    continue
                name, value, target_section = obj.symbols[ix]
                helper = re.fullmatch('__(save|rest)gprlr_(\\d+)', name)
                if target_section > 0:
                    target = self.bases[key[0], target_section] + value
                elif name in self.definitions:
                    oi, value, target_section = self.definitions[name]
                    target = self.bases[oi, target_section] + value
                elif helper:
                    target = (2193657520 if helper[1] == 'save' else 2193657600) + 4 * (int(helper[2]) - 14)
                else:
                    if name not in external:
                        external[name] = 2222981120 + 16 * len(external)
                        self.unresolved[external[name]] = name
                    target = external[name]
                word = struct.unpack_from('>I', body, at)[0]
                if kind in (16, 17):
                    assert word & 65535 == 0
                    immediate = target + 32768 >> 16 if kind == 16 else target
                    word = word & 4294901760 | immediate & 65535
                elif kind == 6:
                    word = word & 4227858435 | target - self.bases[key] - at & 67108860
                elif kind == 2:
                    word = word + target & M32
                else:
                    raise AssertionError((kind, name))
                struct.pack_into('>I', body, at, word)
            self.images[self.bases[key]] = body
        self.entries = {name: self.addr('?GpuTimerCallback@' if name == 'callback' else '?' + name + '@PerfMonGpu@') for name in ENTRIES}

def make_bundle():
    rows = json.loads(_r6_report(OUT / 'final-native-v1/native/compile-results.json').read_text())['results']
    assert all((row['passed'] for row in rows))
    objects = [Obj(row['object']) for row in rows] + [Obj(_r6_object(OUT / 'sdk-u64tod.obj'))]
    bundle = NativeBundle(objects)
    bundle.select_roots(['?GpuTimerCallback@'] + ['?' + name + '@PerfMonGpu@' for name in ENTRIES if name != 'callback'])
    return bundle

class Runner:

    def __init__(self, bundle, compiled, index):
        self.bundle, self.compiled, self.index = (bundle, compiled, index)
        rng = random.Random(2189580728 + index)
        memory = F.M.V.Mem()
        memory.add(STACK, 131072, rng.randbytes(131072))
        memory.add(MESSAGE, 4096, b'X' * 4096)
        memory.add(2197913600, 8192)
        memory.add(2200375296, 4096)
        memory.add(2196971520, 4096)
        memory.add(2222981120, 65536)
        memory.add(1342373888, 8192)
        if compiled:
            for address, data in bundle.images.items():
                memory.add(address, len(data), data)
        self.memory = memory
        cpu = self.cpu = NativeCPU(memory)
        cpu.setg(1, STACK + 65536)
        cpu.max_steps = 1000000
        cpu.atomic_trace, cpu.fence_trace = ([], [])
        cpu.msr, cpu.reservation, cpu.failures = (32768, None, 0)
        self.state = {name: (bundle.addr(SYMBOLS[name]) if compiled else address, size) for name, (address, size) in ORIGINAL_STATE.items()}
        for name, (address, size) in self.state.items():
            memory.wr(address, rng.randbytes(size))
        self.put('max', 32)
        self.put('frequency', index % 4)
        self.put('active', index % 2)
        self.put('running', index // 2 % 2)
        self.put('id', index % 32)
        values = bytearray(rng.randbytes(2680))
        struct.pack_into('>ii', values, 2672, index % 5, index // 5 % 5)
        values[2640:2672] = bytes(((index + i) % 2 for i in range(32)))
        memory.wr(self.state['values'][0], values)
        for i in range(33):
            name = ('GPU monitor %02d' % i).encode() + b'\x00'
            memory.wr(self.state['monitors'][0] + i * 56, name)
        self.put('factor', 850120175)
        self.qpf = [50000000, 3300000000, 10000019, 4294967553, 9007199254740993, 18446744073709551615, 3][index % 7]
        self.qpf_success = 0 if index % 13 == 0 else 1
        self.counter = 8589934560 + index * 4294967297 & M64
        self.counter_step = 4294967299 + index
        self.trace, self.queue = ([], [])
        self.message_pointer = bundle.ext('?gpcMessageBuffer@') if compiled else 2196972128
        self.message_size = bundle.ext('?giMessageBufferSize@') if compiled else 2196972132
        self.device_pointer = bundle.ext('?gpD3DDevice@') if compiled else 2200376840
        self.write(self.message_pointer, MESSAGE)
        self.write(self.message_size, 64 if index % 2 else 256)
        self.write(self.device_pointer, DEVICE)
        self.install_hooks()

    def write(self, address, value, size=4):
        self.memory.wr(address, (value & (1 << size * 8) - 1).to_bytes(size, 'big'))

    def put(self, name, value):
        address, size = self.state[name]
        self.write(address, value, size)

    def get(self, name):
        address, size = self.state[name]
        return int.from_bytes(self.memory.rd(address, size), 'big')

    def snapshot(self):
        return {name: self.memory.rd(address, size).hex() for name, (address, size) in self.state.items()}

    def short_state(self):
        values = self.state['values'][0]
        return [self.get('active'), self.get('running'), self.get('id'), self.memory.rd(values + 2640, 32).hex()]

    def install_hooks(self):

        def memset(cpu):
            dst, value, length = (cpu.g(3) & M32, cpu.g(4) & 255, cpu.g(5) & M32)
            roles = {address: name for name, (address, size) in self.state.items()}
            if dst in roles:
                self.trace.append(['memset', roles[dst], value, length])
            self.memory.wr(dst, bytes([value]) * length)
            cpu.setg(3, dst)

        def qpf(cpu):
            self.write(cpu.g(3) & M32, self.qpf, 8)
            self.trace.append(['QPF', self.qpf, self.qpf_success])
            cpu.setg(3, self.qpf_success)

        def qpc(cpu):
            self.write(cpu.g(3) & M32, self.counter, 8)
            self.trace.append(['QPC', self.counter])
            self.counter = self.counter + self.counter_step & M64
            cpu.setg(3, 1)

        def begin(cpu):
            self.trace.append(['assert-begin'])

        def fire(cpu):
            self.trace.append(['assert-fire', F.cstring(cpu, cpu.g(3) & M32), F.cstring(cpu, cpu.g(4) & M32), cpu.g(5) & M32])

        def end(cpu):
            self.trace.append(['assert-end'])

        def predication(cpu):
            assert cpu.g(3) & M32 == DEVICE
            self.trace.append(['predication', cpu.g(4) & M32, self.short_state()])

        def callback(cpu):
            assert cpu.g(3) & M32 == DEVICE and cpu.g(4) & M32 == 0
            expected = self.bundle.entries['callback'] if self.compiled else ENTRIES['callback']
            assert cpu.g(5) & M32 == expected
            context = cpu.g(6) & M32
            mark_type, monitor_id = (context >> 16, context & 65535)
            self.trace.append(['callback', mark_type, monitor_id if mark_type >= 2 else 'unused-unwritten', self.short_state()])
            self.queue.append(context)

        def strlen(cpu):
            cpu.setg(3, len(F.cstring(cpu, cpu.g(3) & M32).encode()))

        def strcmp(cpu):
            left = F.cstring(cpu, cpu.g(3) & M32).encode()
            right = F.cstring(cpu, cpu.g(4) & M32).encode()
            cpu.setg(3, (left > right) - (left < right))

        def strcpy(cpu):
            dst = cpu.g(3) & M32
            data = F.cstring(cpu, cpu.g(4) & M32).encode()
            self.memory.wr(dst, data + bytes([0]))
            cpu.setg(3, dst)

        def strncat(cpu):
            dst, src, limit = (cpu.g(3) & M32, cpu.g(4) & M32, cpu.g(5) & M32)
            original = F.cstring(cpu, dst).encode()
            extra = F.cstring(cpu, src).encode()[:limit]
            self.memory.wr(dst + len(original), extra + b'\x00')
            cpu.setg(3, dst)
        services = {'memset': memset, 'QueryPerformanceFrequency': qpf, 'QueryPerformanceCounter': qpc, 'D3DDevice_SetPredication': predication, 'D3DDevice_InsertCallback': callback, 'strlen': strlen, 'strncat': strncat, 'strcmp': strcmp, 'strcpy': strcpy}
        if self.compiled:
            for address, name in self.bundle.unresolved.items():
                if name in services:
                    self.cpu.hooks[address] = services[name]
                elif name.startswith('?BeginAssert@'):
                    self.cpu.hooks[address] = begin
                elif name.startswith('?FireAssert@'):
                    self.cpu.hooks[address] = fire
                elif name.startswith('?EndAssert@'):
                    self.cpu.hooks[address] = end
                elif name.startswith(('?gpD3DDevice@', '?gpcMessageBuffer@', '?giMessageBufferSize@')):
                    pass
                else:

                    def reject(cpu, name=name):
                        raise AssertionError(('unexpected native service', name, hex(getattr(cpu, 'last_pc', 0))))
                    self.cpu.hooks[address] = reject
        else:
            self.cpu.hooks[branch_target(2189581336)] = memset
            self.cpu.hooks[branch_target(2189581392)] = qpf
            self.cpu.hooks[branch_target(2189580900)] = qpc
            self.cpu.hooks[branch_target(2189581748)] = predication
            self.cpu.hooks[branch_target(2189581772)] = callback
            self.cpu.hooks[2193343704] = begin
            self.cpu.hooks[2189559824] = fire
            self.cpu.hooks[2193343728] = end
            self.cpu.hooks[branch_target(2189516468)] = strncat

    def run(self, name, argument=None):
        if argument is not None:
            self.cpu.setg(3, argument)
        address = self.bundle.entries[name] if self.compiled else ENTRIES[name]
        self.cpu.steps = 0
        try:
            self.cpu.run(address)
        except Exception as exc:
            raise RuntimeError((self.compiled, self.index, name, hex(getattr(self.cpu, 'last_pc', 0)))) from exc

    def drain(self):
        contexts, self.queue = (self.queue, [])
        for context in contexts:
            self.run('callback', context)

    def register_monitors(self):
        for monitor in range(32):
            name = ('Registered GPU %02d' % monitor).encode() + bytes([0])
            address = 1342373888 + monitor * 40
            colour = 1342377984 + monitor * 4
            self.memory.wr(address, name)
            self.write(colour, 270544960 + monitor)
            self.cpu.setg(4, colour)
            self.cpu.setf(1, 10.0 + monitor)
            self.run('AddMonitor', address)
            assert self.cpu.g(3) & M32 == monitor
        assert self.get('max') == 32