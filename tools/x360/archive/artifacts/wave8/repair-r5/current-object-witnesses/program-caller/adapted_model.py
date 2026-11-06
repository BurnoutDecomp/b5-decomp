import sys, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
p = _r5_object(Path('D:\\Reverse\\programbuffer-terra-build\\final2\\rwgpfxhelper-36a0ea04.obj'))
o = F.Coff(p, '?CreateProgram@PfxHelper@postfx@graphics@rw@@')
S, A = (F.M.S.SELF, F.M.S.AUX)
ALLOC = A + 4096
CALL = A + 20480
payload = A + 24576
results = []
for index in range(128):
    shader = index % 2
    size = 16 + index
    pair = []
    for original in [True, False]:
        c = F.cpu(o)
        c.mem.wr(A, bytes(65536))
        F.put(c, ALLOC, A + 8192)
        F.put(c, A + 8208, CALL)
        trace = []

        def param(z, a):
            vals = struct.unpack('>10I', z.mem.rd(a, 40))
            return [vals[k] for k in [0, 1, 2, 4, 6, 8, 9]]

        def descriptor(z):
            dst = z.g(3) & 4294967295
            par = z.g(4) & 4294967295
            trace.append(('describe', param(z, par)))
            z.mem.wr(dst, struct.pack('>10I', 32, 16, 0, 1, 48, 32, 0, 1, 0, 1))
            z.setg(3, dst)

        def allocate(z):
            dst = z.g(3) & 4294967295
            alloc = z.g(4) & 4294967295
            desc = z.g(5) & 4294967295
            flags = z.g(6) & 4294967295
            trace.append(('allocate', alloc, flags, z.mem.rd(desc, 40).hex()))
            z.mem.wr(dst, struct.pack('>5I', A + 32768, 286331153, A + 36864, 858993459, 1145324612))
            z.setg(3, dst)

        def initialize(z):
            res = z.g(3) & 4294967295
            par = z.g(4) & 4294967295
            trace.append(('init', z.mem.rd(res, 20).hex(), param(z, par)))
            z.setg(3, A + 32768)

        def base_ctor(z):
            dst = z.g(3) & 4294967295
            z.mem.wr(dst, struct.pack('>II', 0, 1))
            z.setg(3, dst)
        c.hooks[2192979120] = descriptor
        c.hooks[2192974968] = initialize
        c.hooks[CALL] = allocate
        for at, n in o.unresolved.items():
            if n.startswith('?GetResourceDescriptor@ProgramBuffer@'):
                c.hooks[at] = descriptor
            elif n.startswith('?Initialize@ProgramBuffer@'):
                c.hooks[at] = initialize
            elif n.startswith('??0BaseResourceDescriptor@'):
                c.hooks[at] = base_ctor
        c.setg(3, shader)
        c.setg(4, payload)
        c.setg(5, size)
        c.setg(6, ALLOC)
        c.run(2185225344 if original else o.entry)
        pair.append((trace, c.g(3) & 4294967295))
    assert pair[0] == pair[1], (index, pair)
    results.append(index)
report = {'result': 'pass', 'cases': len(results), 'original': '0x823FE480', 'object_sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'scope': 'actual PfxHelper CreateProgram with explicit allocator, original vs native call trace and all five resource lanes, controlled descriptor/allocator/Initialize hooks; default allocator path not covered'}
_r5_output(Path('D:\\Reverse\\programbuffer-terra-build\\final2\\caller-witness.json')).write_text(json.dumps(report, indent=2))
print(json.dumps(report))