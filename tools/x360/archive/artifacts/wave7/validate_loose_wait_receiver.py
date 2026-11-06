"""Read-only WaitOn receiver witness. Optional argv[1] is an actual LooseOctree.obj.
Only the newly added call is isolated. Both native and765d active-flag locations
are seeded; query counts remain zero. This is not complete owner-layout proof.
No mkdir, report or bytecode writes are allowed by this fixture.
"""
import sys, json, hashlib
from pathlib import Path
sys.dont_write_bytecode = True
old_mkdir, old_open = Path.mkdir, Path.open
Path.mkdir = lambda *a, **kw: None
def readonly(self, mode='r', *a, **kw):
    assert not any(c in mode for c in 'wax+'), ('fixture write blocked', str(self), mode)
    return old_open(self, mode, *a, **kw)
Path.open = readonly
try:
    sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
    import native_atomic_fixture as F
    root = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7')
    row = next(x for x in json.loads((root/'selected-stage1/compile-results.json').read_text())['results'] if x['source'].endswith('/CgsLooseOctree.cpp'))
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(row['object'])
    obj = F.Coff(path, '?WaitForFrustumTestJobResults@LooseOctree@')
    original_path = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x828B2558.json')
    original = json.loads(original_path.read_text())
    original_wait = int(next(x['address'] for x in original['xrefs_from'] if 'WaitOn' in x['name']), 16)
    OWNER, OUT = 0x45000000, F.M.S.SELF + 0x1000
    results = []
    for index in range(4):
        pair = []
        for compiled in [False, True]:
            c = F.cpu(obj)
            c.mem.add(OWNER, 0xa8000)
            c.mem.wr(OWNER + 0x90b00 + index, bytes([1]))
            c.mem.wr(OWNER + 0x9bf80 + index, bytes([1]))
            c.setg(3, OWNER)
            c.setg(4, OUT)
            trace = []
            def wait(cpu):
                trace.append({'receiver_offset': hex((cpu.g(3) & 0xffffffff) - OWNER),
                              'r4': cpu.g(4) & 0xffffffff, 'r5': cpu.g(5) & 0xffffffff,
                              'r6': hex(cpu.g(6) & 0xffffffff)})
            c.hooks[original_wait] = wait
            for address, name in obj.unresolved.items():
                if name.startswith('?WaitOn@Job@Jobs@EA@@'):
                    c.hooks[address] = wait
            c.run(obj.entry if compiled else 0x828b2558)
            assert len(trace) == 1, ('selected active index must execute exactly one WaitOn', index, compiled, trace)
            pair.append(trace)
        results.append({'active_job_index': index, 'original': pair[0], 'compiled': pair[1],
                        'receiver_and_arguments_equal': pair[0] == pair[1]})
    result = {'object': str(path), 'object_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'original_export': str(original_path), 'original_export_sha256': hashlib.sha256(original_path.read_bytes()).hexdigest(),
              'original_wait_address': hex(original_wait), 'results': results,
              'scope': 'Four controlled zero-query caller windows; seed native90B00 and765d9BF80 flags only to isolate emitted WaitOn receivers. Existing surrounding flag/query/result layout differences are not normalized into a full owner/runtime claim. SDK job wait is a controlled trace hook; no real job execution or physical behavior.'}
finally:
    Path.mkdir, Path.open = old_mkdir, old_open
print(json.dumps(result, indent=2))
