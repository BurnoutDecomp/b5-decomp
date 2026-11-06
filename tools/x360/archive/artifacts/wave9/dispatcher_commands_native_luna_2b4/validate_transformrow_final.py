import ast, hashlib, importlib.util, json, math, random, re, struct, sys
from pathlib import Path
sys.path.insert(0, r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
V = M.V
# Calibrate the exact selected ARTIST reader/image before extracting executable bytes.
reader_path = Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\x360rd.py')
spec = importlib.util.spec_from_file_location('x360rd_dispatch', reader_path)
XR = importlib.util.module_from_spec(spec); spec.loader.exec_module(XR)
assert XR.u32(0x82815570) == 0x7C8A2378
assert XR.u32(0x82F264A8) == 4

# Reuse the established COFF section/relocation loader without importing the
# validation script's build-directory side effects.
coff_source = Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\validate_render_states.py')
coff_text = coff_source.read_text(encoding='utf-8')
tree = ast.parse(coff_text)
ns = {'M': M, 'struct': struct, 're': re}
for node in tree.body:
    if isinstance(node, ast.ClassDef) and node.name in ('CPU', 'Coff'):
        code = ast.get_source_segment(coff_text, node)
        if node.name == 'Coff':
            code = code.replace('def __init__(self, path, prefix):',
                                'def __init__(self, path, prefix, load_base=0x83800000):\n        self.load_base = load_base')
            code = code.replace('0x83800000 + i*0x10000', 'self.load_base + i*0x10000')
        exec(compile(code, str(coff_source), 'exec'), ns)
Coff = ns['Coff']
obj = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\dispatcher_commands_native_luna_2b4\final-compile\CgsDispatcherCommands-727fe626.obj')
compiled = Coff(obj, '?TransformRow@?A0x7638ae42@CgsGraphics@@')

# ARTIST 0x827EE66C..0x827EE72C is the actual four-row transform body from
# InterpretOcclusionQuery. The final instruction is replaced only by a return
# at 0x827EE730 so we can compare the rows before the following OOBB operation.
original = bytearray(XR.rd(0x827EE66C, 0xC4)) + bytearray(struct.pack('>I', 0x4E800020))
ORIG_CODE, WORLD, VIEW, STACK = 0x86000000, 0x86100000, 0x86100100, 0x86100200
ROW_OUTPUTS = (0x50, 0x60, 0x70, 0x80)  # var_B0,row0; var_A0,row1; var_90,row2; var_80,row3

# Raw float words ensure signed zero, subnormals, and cancellation values are included.
def words_to_bytes(words): return struct.pack('>' + str(len(words)) + 'I', *words)
def floats_from_words(words): return [struct.unpack('>f', struct.pack('>I', w))[0] for w in words]
rng = random.Random(0x827EE66C)
seed_words = [0x00000000, 0x80000000, 0x00000001, 0x80000001, 0x3F800000,
              0xBF800000, 0x3EAAAAAB, 0xBEAAAAAB, 0x4B000001, 0xCB000001,
              0x3F000001, 0xBF000001, 0x00800000, 0x80800000]
rows=[]
for case in range(64):
    words = [seed_words[(case*7+i*3) % len(seed_words)] for i in range(32)]
    if case >= len(seed_words):
        words = [rng.getrandbits(32) for _ in range(32)]
        # Keep randomized arithmetic finite while retaining a broad exponent/sign range.
        words = [(w & 0x807fffff) | (rng.randrange(1,254) << 23) for w in words]
    vals = floats_from_words(words)
    original_cpu = M.new_cpu()
    original_cpu.mem.add(ORIG_CODE, len(original), original)
    original_cpu.mem.add(WORLD, 64, words_to_bytes(words[:16]))
    original_cpu.mem.add(VIEW, 64, words_to_bytes(words[16:]))
    original_cpu.mem.add(STACK, 0x200, bytearray(0x200))
    original_cpu.setg(1, STACK); original_cpu.setg(11, WORLD); original_cpu.setg(10, VIEW)
    original_cpu.setg(6, 32); original_cpu.setg(8, 16); original_cpu.setg(9, 48)
    original_cpu.run(ORIG_CODE)
    expected = [original_cpu.mem.rd(STACK+off, 16) for off in ROW_OUTPUTS]
    actual=[]
    for row in range(4):
        cpu = compiled.cpu()
        cpu.mem.add(WORLD, 64, words_to_bytes(words[:16]))
        cpu.mem.add(VIEW, 64, words_to_bytes(words[16:]))
        cpu.setg(3, WORLD+row*16); cpu.setg(4, VIEW)
        cpu.run(compiled.entry)
        # PPC vector return value is vr1 in the compiler's actual listing.
        actual.append(struct.pack('>4I', *cpu.vr[1]))
    equal = expected == actual
    rows.append({'case':case,'equal':equal,'expected_words':[x.hex() for x in expected],
                 'actual_words':[x.hex() for x in actual]})

report={'original':'ARTIST CgsGraphics::DrawRenderableMesh::InterpretOcclusionQuery @0x827EE550, transform instructions 0x827EE66C..0x827EE72C',
        'compiled_symbol':compiled.fn[0], 'compiled_object':str(obj),
        'compiled_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),
        'original_reader_sentinels':{'0x82815570':'0x7C8A2378','0x82F264A8':'0x00000004'},
        'cases':len(rows),'matrix_rows':4,'mismatches':[r for r in rows if not r['equal']],
        'limits':'64 finite bit-pattern matrix pairs including signed zero, subnormals, cancellation-adjacent values and randomized exponents; original ARTIST code through its four transform stores, return inserted before the following PackedOobb::ToMatrix call; compiled actual TransformRow object function; no full dispatcher, renderer, link, hardware or game claim.'}
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\dispatcher_commands_native_luna_2b4\transformrow-original-comparison-final.json')
out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':len(rows),'mismatches':len(report['mismatches']),'report':str(out)},indent=2))
assert not report['mismatches']



