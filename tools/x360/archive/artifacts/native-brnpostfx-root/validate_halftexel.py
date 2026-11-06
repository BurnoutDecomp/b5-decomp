from pathlib import Path
import ast, hashlib, json, random, struct, sys

HERE = Path(__file__).resolve().parent
source = HERE / 'validate_caller.py'
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
stage = sys.argv[1] if len(sys.argv) > 1 else 'native-combined'
compiled_start = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0xF0
compiled_stop = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x154
output_name = sys.argv[4] if len(sys.argv) > 4 else 'halftexel-window-comparison.json'
report = json.loads((HERE / stage / 'compile-results.json').read_text())
object_path = Path(report['results'][0]['object'])
obj = PackedCoff(object_path, '?Render@BrnPostFx@@')
Return = F.CPU.run.__globals__['Return']

class WindowCPU(TintCPU):
    def step(self, pc, word, nxt, depth):
        if pc == self.stop_pc:
            raise Return()
        return super().step(pc, word, nxt, depth)

BASE, RT, MEMORY, SELF = 0x50000000, 0x50001000, 0x50002000, 0x50003000
WRAPPERS = [0x50004000, 0x50004200, 0x50004400]
TARGETS = [0x50005000, 0x50005200, 0x50005400]
TINTS = [0x50006000, 0x50006200]
rng = random.Random(0x8240A638)
errors, witnesses = [], []
for index in range(128):
    image = bytearray(rng.randbytes(0x8000))
    width, height = rng.randrange(1, 8193), rng.randrange(1, 8193)
    def put(address, value):
        struct.pack_into('>I', image, address - BASE, value)
    put(RT + 4, width)
    put(RT + 8, height)
    for offset, wrapper, target in zip((0x10, 0x18, 0x1C), WRAPPERS, TARGETS):
        put(MEMORY + offset, wrapper)
        put(wrapper + 0x108, target)
    put(SELF + 0x5FC, index % 2)
    put(SELF + 0x5C0, TINTS[0])
    put(SELF + 0x5C4, TINTS[1])
    outputs = []
    for compiled in (False, True):
        c = cpu(obj)
        c.__class__ = WindowCPU
        c.mem.add(BASE, len(image), image)
        c.setg(29, RT)
        c.setg(30, MEMORY)
        c.setg(31, SELF)
        c.setg(28, 0)
        c.setg(11, 0 if index % 2 else 0x51000000)
        c.stop_pc = obj.entry + compiled_stop if compiled else 0x8240A63C
        c.run(obj.entry + compiled_start if compiled else 0x8240A54C)
        value = c.mem.rd(c.g(1) + 0xE0, 16) if compiled else struct.pack('>4I', *c.vr[127])
        outputs.append(value)
        assert c.mem.rd(BASE, len(image)) == bytes(image), 'window modified source objects'
    if outputs[0] != outputs[1]:
        errors.append({'index': index, 'width': width, 'height': height,
                       'original': outputs[0].hex(), 'compiled': outputs[1].hex()})
    if index < 4:
        witnesses.append({'width': width, 'height': height, 'output': outputs[1].hex()})

result = {'cases': 128, 'mismatches': errors, 'object': str(object_path), 'object_sha256': sha(object_path),
    'original_window': ['0x8240A54C', '0x8240A63C'],
    'compiled_window': [hex(obj.entry + compiled_start), hex(obj.entry + compiled_stop)], 'witnesses': witnesses,
    'original_window_sha256': hashlib.sha256(F.M.V.x360rd.rd(0x8240A54C, 0xF0)).hexdigest(),
    'original_mask_sha256': hashlib.sha256(F.M.V.x360rd.rd(0x82CDA350, 16)).hexdigest(),
    'verifier_sha256': sha(__file__),
    'limits': 'Actual original and compiled whole-product instruction windows. Same role inputs r29/source dimensions,r30/renderer memory,r31/PostFx,r28zero,r11override; outputs compare original live v127 and final native16-byte local. Both native scalar f32 divisions/multiplies execute; original byte-permute executes. Finite nonzero dimensions1..8192; source object bytes untouched. Stops before compiled first GetTexture call and original subsequent render setup; does not verify whole Render signature, shader ABI, GPU or frame output.'}
with (HERE / output_name).open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': 128, 'mismatches': len(errors)}))
assert not errors
