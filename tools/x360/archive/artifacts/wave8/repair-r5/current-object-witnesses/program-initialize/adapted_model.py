import sys, json, struct, hashlib, random
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
p = _r5_object(Path('D:\\Reverse\\programbuffer-terra-build\\final2\\programbuffer-1503d257.obj'))
o = F.Coff(p, '?Initialize@ProgramBuffer@renderengine@@')
b = F.Bundle([o])
b.select('?Initialize@ProgramBuffer@renderengine@@')
table = F.address(b, '?Xbox2CreateConstantTable@ProgramBuffer@renderengine@@')
S, A = (F.M.S.SELF, F.M.S.AUX)
P = S + 256
D = A
H = A + 4096
C = A + 8192
M = A + 12288
T = A + 16384
FUNC = A + 20480
rng = random.Random(360)
results = []
for function in [0, FUNC]:
    for shader in [0, 1]:
        for cooked in [0, 1]:
            for idx in range(16):
                initial = rng.randbytes(256)
                physical = rng.randbytes(256)
                cached = rng.randbytes(64)
                micro = rng.randbytes(64)
                constants = rng.randbytes(32)
                trace_pair = []
                for original in [True, False]:
                    c = F.cpu(b)
                    c.mem.wr(S, bytes(512))
                    c.mem.wr(D, initial)
                    c.mem.wr(H, physical)
                    c.mem.wr(C, cached)
                    c.mem.wr(M, micro)
                    c.mem.wr(T, constants)
                    resource = [D, 3735928559, H, 2291772091, 3437096703]
                    c.mem.wr(S, struct.pack('>5I', *resource))
                    params = [function, shader, 4369, C, 24, M, 32, T, 16 if cooked else 0, 7]
                    c.mem.wr(P, struct.pack('>10I', *params))
                    trace = []

                    def copy(z):
                        dst = z.g(3) & 4294967295
                        src = z.g(4) & 4294967295
                        n = z.g(5) & 4294967295
                        trace.append(('copy', dst, src, n))
                        z.mem.wr(dst, z.mem.rd(src, n))
                        z.setg(3, dst)

                    def get(z):
                        fn = z.g(3) & 4294967295
                        parts = z.g(4) & 4294967295
                        trace.append(('parts', fn))
                        z.mem.wr(parts, struct.pack('>4I', C, 24, M, 32))

                    def setheader(kind):

                        def f(z):
                            dst = z.g(3) & 4294967295
                            n = z.g(4) & 4294967295
                            parts = z.g(5) & 4294967295
                            trace.append((kind, dst, n, z.mem.rd(parts, 16).hex()))
                            z.mem.wr(dst, bytes([65 if kind == 'vertex' else 81]) * n)
                        return f

                    def reg(kind):

                        def f(z):
                            trace.append((kind, z.g(3) & 4294967295, z.g(4) & 4294967295))
                        return f

                    def tbl(z):
                        fn = z.g(3) & 4294967295
                        dst = z.g(4) & 4294967295
                        size = z.g(5) & 4294967295
                        trace.append(('table', fn, dst, size != 0))
                        z.mem.wr(dst, b'TABLE123')
                        if size:
                            F.put(z, size, 8)
                        z.setg(3, 3)
                    for addr, fn in [(2190751184, get), (2193663536, copy), (2190633888, copy), (2190754088, setheader('vertex')), (2190753744, setheader('pixel')), (2190752104, reg('regv')), (2190751480, reg('regp')), (2192967848, tbl), (table, tbl)]:
                        c.hooks[addr] = fn
                    for at, name in b.unresolved.items():
                        m = {'XGGetMicrocodeShaderParts': get, 'memcpy': copy, 'XMemCpy': copy, 'XGSetVertexShaderHeader': setheader('vertex'), 'XGSetPixelShaderHeader': setheader('pixel'), 'XGRegisterVertexShader': reg('regv'), 'XGRegisterPixelShader': reg('regp')}
                        if name in m:
                            c.hooks[at] = m[name]
                    c.setg(3, S)
                    c.setg(4, P)
                    c.run(2192974968 if original else b.entry)
                    trace_pair.append((trace, c.mem.rd(D, 256).hex(), c.mem.rd(H, 256).hex(), c.mem.rd(S, 20).hex(), c.g(3) & 4294967295))
                assert trace_pair[0] == trace_pair[1], (function, shader, cooked, idx, trace_pair[0][0], trace_pair[1][0])
                results.append((function != 0, shader, cooked, idx))
out = {'result': 'pass', 'cases': len(results), 'original': '0x82B62478', 'object_sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'scope': 'original and native Initialize, distinct five resource lanes, vertex/pixel, function/inline, cooked/rebuilt table, controlled XG and copy hooks; no GPU execution'}
_r5_output(Path('D:\\Reverse\\programbuffer-terra-build\\final2\\initialize-witness.json')).write_text(json.dumps(out, indent=2))
print(json.dumps(out))