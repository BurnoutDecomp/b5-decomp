import hashlib, json, math, struct
from fractions import Fraction
from pathlib import Path

work = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\audio_fma_luna_526adc')
art = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave6\audio_fma_luna')
exports = Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
whole = art / 'final-whole-cpp' / 'objects'
callers = art / 'caller-closure' / 'objects'
curve = art / 'compiler-sign-fold' / 'objects'

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]

def fbits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]

def fused32(a, b, c):
    exact = Fraction.from_float(float(a)) * Fraction.from_float(float(b)) + Fraction.from_float(float(c))
    return f32(float(exact))

def compiled_ops(assembly_path, function_text):
    lines = Path(assembly_path).read_text(encoding='utf-8', errors='replace').splitlines()
    start = next(i for i, line in enumerate(lines) if 'PROC NEAR' in line and function_text in line)
    end = next(i for i in range(start + 1, len(lines)) if 'ENDP' in lines[i])
    ops = []
    for line in lines[start:end]:
        fields = line.split()
        if len(fields) >= 4 and len(fields[0]) == 5 and len(fields[1]) == 8 and fields[1].isalnum():
            op = fields[2].lower()
            if op in ('fmadds', 'fmsubs', 'fnmadds', 'fnmsubs', 'fmuls'):
                ops.append(line.strip())
    return ops

# Compare ARTIST's index route (FMSUBS -> fctiwz -> reverse table offset) to
# whole-CPP XDK output (FNMSUBS -> fctiwz -> direct table offset).
curve_witnesses = []
for bits in [0x00000000, 0x00000001, 0x3f000000, 0x3f7ffffe, 0x3f7fffff, 0x3f800000, 0x80000000]:
    fraction = struct.unpack('<f', struct.pack('<I', bits))[0]
    original = fused32(fraction, 511.0, -511.0)
    emitted = fused32(-fraction, 511.0, 511.0)
    # Original ReadSinTable computes (0u - (u32(index) << 2)) >> 2.
    original_i = math.trunc(original)
    original_table_index = ((0 - ((original_i & 0xffffffff) << 2)) & 0xffffffff) >> 2
    emitted_table_index = math.trunc(emitted)
    assert original_table_index == emitted_table_index, (hex(bits), original, emitted)
    curve_witnesses.append({
        'input_f32_bits': f'0x{bits:08X}',
        'original_fmsubs_result_bits': f'0x{fbits(original):08X}',
        'emitted_fnmsubs_result_bits': f'0x{fbits(emitted):08X}',
        'original_signed_integer': original_i,
        'original_reverse_table_index': original_table_index,
        'emitted_direct_table_index': emitted_table_index,
        'same_lookup': True,
    })

compile_report = json.loads((whole / 'compile-results.json').read_text(encoding='utf-8'))
caller_report = json.loads((callers / 'compile-results.json').read_text(encoding='utf-8'))
curve_report = json.loads((curve / 'compile-results.json').read_text(encoding='utf-8'))
assert compile_report['state'] == 'complete' and compile_report['passed'] == 3 and compile_report['failed'] == 0
assert caller_report['state'] == 'complete' and caller_report['passed'] == 3 and caller_report['failed'] == 0
assert curve_report['state'] == 'complete' and curve_report['passed'] == 1 and curve_report['failed'] == 0
current_sources = {
    row['source']: row for row in compile_report['results']
    if row['source'] in ('vendor/renderware/src/rw/audio/core/Iir2.cpp', 'vendor/renderware/src/rw/audio/core/ReverbFilters.cpp')
}
current_sources.update({row['source']: row for row in curve_report['results']})
assert len(current_sources) == 3 and all(row['passed'] for row in current_sources.values())

source_paths = [
    'vendor/renderware/src/rw/audio/core/Iir2.cpp',
    'vendor/renderware/src/rw/audio/core/ReverbFilters.cpp',
    'src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp',
    'vendor/renderware/src/rw/audio/core/CompressorLimiter1.cpp',
]
original_addresses = ['0x82B69DD0','0x82B64560','0x82B64C20','0x82B64DB0','0x82689698','0x826897F0','0x8268F2D0','0x826D5068']
report = {
    'status': 'private validation; no push or integration',
    'workspace': str(work),
    'base': '526adc27662c0c2aedf417980dcf824c989bb035',
    'head': __import__('subprocess').check_output(['git','rev-parse','HEAD'], cwd=work, text=True).strip(),
    'commits': __import__('subprocess').check_output(['git','log','--format=%H%x09%s%x09%(trailers:key=Co-Authored-By,valueonly)', '-3'], cwd=work, text=True).splitlines(),
    'sources': {p: sha(work/p) for p in source_paths},
    'whole_cpp_current_source_set': {
        'requested': 3, 'passed': 3, 'failed': 0,
        'source_to_report': {
            source: {'report':str(whole/'compile-results.json') if source != 'src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp' else str(curve/'compile-results.json'),
                     'report_sha256':sha(whole/'compile-results.json') if source != 'src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp' else sha(curve/'compile-results.json'),
                     'object':row['object'], 'object_sha256':sha(row['object']),
                     'assembly':row['assembly'], 'assembly_sha256':sha(row['assembly']),
                     'log':row['log'], 'log_sha256':sha(row['log'])}
            for source,row in current_sources.items()
        },
    },
    'caller_closure_report': {'path': str(callers/'compile-results.json'), 'sha256': sha(callers/'compile-results.json'), 'requested': caller_report['requested'], 'passed': caller_report['passed'], 'failed': caller_report['failed']},
    'soundutils_sign_fold_report': {'path': str(curve/'compile-results.json'), 'sha256': sha(curve/'compile-results.json'), 'requested': curve_report['requested'], 'passed': curve_report['passed'], 'failed': curve_report['failed']},
    'whole_cpp_artifacts': {},
    'caller_artifacts': {},
    'artist_exports': {},
    'compiled_fused_sites': {
        'Iir2::Filter': compiled_ops(whole/'Iir2-9bd0cbc1.asm', 'rw::audio::core::Iir2::Filter'),
        'AllPassFilterFunc': compiled_ops(whole/'ReverbFilters-ae9b3b3d.asm', 'rw::audio::core::AllPassFilterFunc'),
        'CombFilterFunc': compiled_ops(whole/'ReverbFilters-ae9b3b3d.asm', 'rw::audio::core::CombFilterFunc'),
        'Curve::GetOutput': compiled_ops(curve/'CgsSoundUtils-82d0fe85.asm', 'CgsSound::Utils::Curve::GetOutput'),
        'Slope::GetValue': compiled_ops(curve/'CgsSoundUtils-82d0fe85.asm', 'CgsSound::Utils::Slope::GetValue'),
        'PathLine<2>::Update': compiled_ops(curve/'CgsSoundUtils-82d0fe85.asm', 'CgsSound::Utils::PathLine<2>::Update'),
        'PathLine<3>::Update': compiled_ops(curve/'CgsSoundUtils-82d0fe85.asm', 'CgsSound::Utils::PathLine<3>::Update'),
    },
    'soundutils_case3_equivalence': {
        'claim': 'semantic lookup equivalence for finite f32 inputs in [0,1], not opcode or byte equivalence',
        'original': 'FMSUBS computes round32(fraction*511 - 511); FCTIWZ truncates to i32; ReadSinTable reverses the signed index into a table offset.',
        'emitted': 'FNMSUBS computes round32(511 - fraction*511); FCTIWZ truncates to i32; optimized code indexes the same table directly.',
        'proof': 'IEEE round-to-nearest is sign symmetric, so emitted value is the negation of the original fused value (both use the same positive 511 f32 operands). For [0,1] the original result is in [-511,0]; truncation toward zero preserves negation, and the original reverse-table index equals the emitted direct index, including f=0, f=1 and signed zero.',
        'nan': 'The ARTIST assert comparisons let NaN continue. Both fused forms yield NaN; ARTIST/XDK listing then uses FCTIWZ. Power ISA v2.07B specifies NaN conversion result 0x80000000, so both select the same integer/table path.',
        'out_of_range': 'Inputs outside [0,1] take the existing assertion path before the case. No behavior claim is made if that assertion returns and execution continues.',
        'witnesses': curve_witnesses,
    },
    'limitations': [
        'Compile-only COFF/codegen evidence; no X360 execution, hardware, gameplay, or audio parity claim.',
        'CompressorLimiter1 remains held: its scalar envelope site can contract, but whole CPP compile fails on pre-existing std::exp2/std::log2 API gaps. ARTIST Process uses VMX vlogefp/vexptefp estimates; no substitute was added.',
    ],
    'reference_spec': 'https://powerpc.dev/general/PowerISA_V2.07B.pdf',
}
baseline_path = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave6\full-stage1\compile-results.json')
baseline_results = json.loads(baseline_path.read_text(encoding='utf-8'))
compressor_row = next(row for row in baseline_results['results'] if row['source'] == 'vendor/renderware/src/rw/audio/core/CompressorLimiter1.cpp')
report['held_compressor'] = {
    'baseline_report': str(baseline_path),
    'baseline_report_sha256': sha(baseline_path),
    'baseline_log': compressor_row['log'],
    'baseline_log_sha256': sha(compressor_row['log']),
    'baseline_diagnostics': Path(compressor_row['log']).read_text(encoding='utf-8', errors='replace').splitlines(),
    'artist_export': str(exports/'0x82B64DB0.json'),
    'artist_export_sha256': sha(exports/'0x82B64DB0.json'),
}
for row in current_sources.values():
    for key in ('object','assembly','log'):
        report['whole_cpp_artifacts'][row['source']+'_'+key] = {'path':row[key], 'sha256':sha(row[key])}
for row in caller_report['results']:
    for key in ('object','assembly','log'):
        report['caller_artifacts'][row['source']+'_'+key] = {'path':row[key], 'sha256':sha(row[key])}
for address in original_addresses:
    path = exports/(address+'.json')
    data = json.loads(path.read_text(encoding='utf-8'))
    lines = data['assembly'].splitlines()
    relevant = [line for line in lines if any(op in line for op in ('fmadds','fnmsubs','fmsubs','fmuls'))]
    report['artist_exports'][address] = {'path':str(path), 'sha256':sha(path), 'name':data.get('name'), 'fused_or_mul_ops':relevant[:40]}

out = art/'delivery.json'
out.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
print('delivery', out, 'sha256', sha(out))
print('whole CPP', compile_report['passed'], '/', compile_report['requested'], 'caller closure', caller_report['passed'], '/', caller_report['requested'])
print('case-3 table-index witnesses', len(curve_witnesses), 'all matched')
