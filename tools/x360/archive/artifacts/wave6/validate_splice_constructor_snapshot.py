"""Read-only two-constructor source-equivalence witness; no build/report writes.
Usage: python -X utf8 validate_splice_constructor_snapshot.py [actual-SpliceObjects.obj]
The pointer overload is the unchanged delegate target, not a located ARTIST body.
FindSplice/Allocate/assert callback are controlled; returned allocations are null.
"""
import sys, struct, json, hashlib
from pathlib import Path
sys.dont_write_bytecode = True
sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
p = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6/repair-stage1/SpliceObjects-20701adc.obj')
objects = {kind: F.Coff(p, prefix) for kind, prefix in [
    ('type_index', '??0Splice@@QAA@W4SPLICE_TYPE@@'),
    ('pointer', '??0Splice@@QAA@PAUSPLICE_Data@@')]}
OWNER = F.M.S.SELF + 0x1000
DATA, OTHER, REFS, MANAGER, SUBMIX = [F.M.S.AUX + n for n in (0x1000, 0x1100, 0x2000, 0x4000, 0x5000)]
CALLBACK = 0x84803000
results = {}
for mode in ['control', 'allocator_replaces_data', 'assert_callback_replaces_data']:
    results[mode] = {}
    for kind, obj in objects.items():
        c = F.cpu(obj)
        c.mem.wr(OWNER, bytes([0xa5]) * 64)
        data = bytearray(32)
        data[7] = 2
        struct.pack_into('>I', data, 24, REFS)
        c.mem.wr(DATA, data)
        c.mem.wr(OTHER, bytes(32))
        refs = bytearray(88)
        struct.pack_into('>f', refs, 16, 0.0)
        struct.pack_into('>f', refs, 60, -127.0)
        c.mem.wr(REFS, refs)
        c.mem.wr(MANAGER, bytes(0x700))
        F.put(c, MANAGER + 0x610, CALLBACK if mode == 'assert_callback_replaces_data' else 0)
        trace, find_calls = [], []
        def find_splice(x):
            find_calls.append((x.g(4), x.g(5)))
            x.setg(3, DATA)
        def allocate(x):
            trace.append(('allocate', x.g(4), F.cstring(x, x.g(5) & 0xffffffff)))
            if mode == 'allocator_replaces_data' and len(trace) == 1:
                F.put(x, OWNER + 0x3c, OTHER)
            x.setg(3, 0)
        def assert_callback(x):
            trace.append(('assert', F.cstring(x, x.g(3) & 0xffffffff)))
            F.put(x, OWNER + 0x3c, OTHER)
        for address, name in obj.unresolved.items():
            if name.startswith('?gpSpliceManager@@'):
                F.put(c, address, MANAGER)
            elif name.startswith('?FindSplice@SpliceManager@@'):
                c.hooks[address] = find_splice
            elif name.startswith('?Allocate@SpliceManager@@'):
                c.hooks[address] = allocate
        c.hooks[CALLBACK] = assert_callback
        c.setg(3, OWNER)
        submix = 0 if mode == 'assert_callback_replaces_data' else SUBMIX
        if kind == 'type_index':
            c.setg(4, 1)
            c.setg(5, 7)
            c.setg(6, submix)
        else:
            c.setg(4, DATA)
            c.setg(5, submix)
        c.run(obj.entry)
        results[mode][kind] = {
            'trace': trace, 'find_calls': find_calls,
            'data_member': hex(int.from_bytes(c.mem.rd(OWNER + 0x3c, 4), 'big')),
            'sample_slots': c.mem.rd(OWNER, 60).hex()}
    assert results[mode]['type_index']['find_calls'] == [(1, 7)]
    assert results[mode]['pointer']['find_calls'] == []
    results[mode]['equivalent_ignoring_FindSplice'] = all(
        results[mode]['type_index'][key] == results[mode]['pointer'][key]
        for key in ['trace', 'data_member', 'sample_slots'])
print(json.dumps({'object': str(p), 'object_sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
                  'results': results, 'scope': 'Actual two compiled constructors; unchanged pointer overload is the old delegate behavior, not a located ARTIST constructor. Controlled external FindSplice, allocator and assertion callback; no audio lifecycle or whole original constructor parity.'}, indent=2))
