from pathlib import Path
import json, random, struct, hashlib
P = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
s = (P / 'scratch/parallel-native/native_render_states/validate_hierarchy.py').read_text(encoding='utf8')
s = s[:s.index('\nrows=')] + s[s.index('\ndef data_symbol'):s.index('\n# Decode')]
__file__ = str(P / 'scratch/parallel-native/native_render_states/validate_hierarchy.py')
exec(compile(s, __file__, 'exec'))
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/ice-space-guard-terra')
obj = _r5_object(out / 'ICECameraSpaceHandler.obj')
co = Coff(obj, '?GetTransformToWorld@CameraSpaceHandler@ICE@@')
rng = random.Random(2186484640)
for i in range(128):
    matrix = rng.randbytes(64)
    handle = [1, 4097, 2014711313, 305418240][i % 4]
    results = []
    for native in (False, True):
        c = co.cpu()
        calls = []
        c.mem.wr(M.S.SELF + 512, struct.pack('>I', handle))
        c.mem.wr(M.S.SELF + 2048, matrix)
        c.mem.wr(M.S.SELF + 1280, bytes([165]) * 64)
        c.setg(3, M.S.SELF + 1280)
        c.setg(4, M.S.SELF)
        c.setg(5, 9)

        def getcamera(cpu):
            calls.append(('getcamera', cpu.g(3) & 4294967295))
            cpu.setg(3, M.S.SELF + 2048)

        def gettransform(cpu):
            calls.append(('gettransform', cpu.g(3) & 4294967295))
            cpu.setg(3, M.S.SELF + 2048)
        c.hooks[2183209608] = getcamera
        c.hooks[data_symbol(co, '?GetProducedCamera@')] = getcamera
        for a, n in co.unresolved.items():
            if n.startswith('?GetTransform@Camera@'):
                c.hooks[a] = gettransform
        c.run(co.entry if native else 2186484640)
        assert calls == ([('getcamera', handle), ('gettransform', M.S.SELF + 2048)] if native else [('getcamera', handle)]), calls
        results.append((c.mem.rd(M.S.SELF + 1280, 64), c.g(3) & 4294967295))
    assert results[0] == results[1] == (matrix, M.S.SELF + 1280), i
report = {'state': 'pass', 'cases': 128, 'object': str(obj), 'sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'Original and compiled gameplay-space matrix copy, exact handle forwarding and output bytes; native GetProducedCamera/GetTransform and original sub_82212288 are controlled. Odd and low nonnull handles demonstrate guard absence only; controlled handlers do not dereference them. Null assertion and real camera lifecycle not exercised.'}
_r5_output(out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))