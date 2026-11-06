import hashlib, json, re, subprocess
from pathlib import Path

work = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\iir2_fma_repair_luna_765')
root = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7')
out = root/'iir2_fma_repair_luna'/'final'
objdir = out/'objects'
callersdir = root/'iir2_fma_repair_luna'/'callers'/'objects'
source_rel = 'vendor/renderware/src/rw/audio/core/Iir2.cpp'
source = work/source_rel
compile_report = json.loads((objdir/'compile-results.json').read_text(encoding='utf-8'))
caller_report = json.loads((callersdir/'compile-results.json').read_text(encoding='utf-8'))
fixture = json.loads((out/'fixture-result.json').read_text(encoding='utf-8'))
asm_path = Path(compile_report['results'][0]['assembly'])
obj_path = Path(compile_report['results'][0]['object'])
asm = asm_path.read_text(encoding='utf-8', errors='replace').splitlines()

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def function_lines(text):
    start = next(i for i,x in enumerate(asm) if 'PROC NEAR' in x and text in x)
    end = next(i for i in range(start+1,len(asm)) if 'ENDP' in asm[i])
    return asm[start:end]
helper = function_lines("`anonymous namespace'::Iir2MulAdd")
filter_fn = function_lines('rw::audio::core::Iir2::Filter')
ops = lambda block, names: [x.strip() for x in block if any((' '+n+' ') in x for n in names)]
orig_export = Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x82B69DD0.json')
orig = json.loads(orig_export.read_text(encoding='utf-8'))
orig_ops = [x for x in orig['assembly'].splitlines() if any(n in x for n in ('fmuls','fmadds','fnmsubs'))]
baseline_obj = root/'full-stage1'/'Iir2-9bd0cbc1.obj'
baseline_report = root/'full-stage1'/'compile-results.json'
fixture_script = root/'validate_iir2_rounding.py'
old_negative = root/'iir2-rounding-765d-negative.json'
old_validator_hash = 'd421aa6c89c5eb6a0db32a1f61838b718d5463afbb81b58ba80a1069bf36881b'
old_negative_hash = '181f433fd258d95fc108a4f9355074c9c83d8bd6f69ba42f4443f4ec4efb9c8f'

receipt = {
  'status':'private narrow repair, not pushed or integrated',
  'workspace':str(work),
  'base':'765d9ce2f097194da48d7180cfdf329476ae6210',
  'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip(),
  'commit':subprocess.check_output(['git','log','-1','--format=%H%n%s%n%(trailers:key=Co-Authored-By,valueonly)'],cwd=work,text=True).strip().splitlines(),
  'changed_paths':subprocess.check_output(['git','show','--format=','--name-only','HEAD'],cwd=work,text=True).splitlines(),
  'clean':not bool(subprocess.check_output(['git','status','--porcelain'],cwd=work,text=True).strip()),
  'source':{'path':source_rel,'sha256':sha(source)},
  'native_whole_cpp_compile':{'report':str(objdir/'compile-results.json'),'report_sha256':sha(objdir/'compile-results.json'),'requested':compile_report['requested'],'passed':compile_report['passed'],'failed':compile_report['failed'],'object':str(obj_path),'object_sha256':sha(obj_path),'assembly':str(asm_path),'assembly_sha256':sha(asm_path),'log':compile_report['results'][0]['log'],'log_sha256':sha(compile_report['results'][0]['log']),'machine':compile_report['results'][0]['machine']},
  'native_instruction_proof':{
    'original_export':str(orig_export),'original_export_sha256':sha(orig_export),'original_name':orig['name'],'original_feedforward_reference':[
      'sample 0: FMULS(src[0],B0), FMADDS(historyX2,B2,rounded-first-product), FMADDS(historyX1,B1,inner-result)',
      'sample 1: FMULS(src[1],B0), FMADDS(src[0],B1,rounded-first-product), FMADDS(historyX1,B2,inner-result)',
      'samples 2..7: FMULS(src[n-1],B1), FMADDS(src[n-2],B2,rounded-middle), FMADDS(src[n],B0,inner-result)'],
    'compiled_helper_function':ops(helper,['fmadds','fmuls','fadds','fmsubs','fnmsubs']),
    'compiled_helper_fused_instruction_count':sum(' fmadds ' in x for x in helper),
    'compiled_filter_direct_feedforward_fmadds':ops(filter_fn,['fmadds'])[:16],
    'compiled_filter_inner_helper_calls':ops(filter_fn,['bl'])[:16],
    'compiled_filter_products_and_feedback':{'fmuls':sum(' fmuls ' in x for x in filter_fn),'fnmsubs':sum(' fnmsubs ' in x for x in filter_fn)},
    'original_fused_and_product_instructions':orig_ops,
  },
  'actual_object_vs_original_fixture':{'script':str(fixture_script),'script_sha256':sha(fixture_script),'prior_negative_report':str(old_negative),'prior_negative_report_sha256':sha(old_negative),'expected_prior_validator_sha256':old_validator_hash,'expected_prior_negative_sha256':old_negative_hash,'result_report':str(out/'fixture-result.json'),'result_report_sha256':sha(out/'fixture-result.json'),'result':fixture},
  'real_callers':{'report':str(callersdir/'compile-results.json'),'report_sha256':sha(callersdir/'compile-results.json'),'requested':caller_report['requested'],'passed':caller_report['passed'],'failed':caller_report['failed'],'artifacts':[]},
  'preserved_wave7_baseline':{'compile_report':str(baseline_report),'compile_report_sha256':sha(baseline_report),'old_iir2_object':str(baseline_obj),'old_iir2_object_sha256':sha(baseline_obj)},
  'limits':['No byte-match claim: the private helper introduces calls to preserve XDK f32 fused rounding.','Compile/object plus software execution of actual original and compiled PPC is evidence only; no physical X360, audio lifecycle, or performance measurement.','Fixture covers finite stable histories/coefficients/input, including alias and in-place buffers; exceptional FP behavior is not covered.'],
}
for row in caller_report['results']:
  receipt['real_callers']['artifacts'].append({'source':row['source'],'object':row['object'],'object_sha256':sha(row['object']),'assembly':row['assembly'],'assembly_sha256':sha(row['assembly']),'log':row['log'],'log_sha256':sha(row['log'])})
path = root/'iir2_fma_repair_luna'/'final'/'receipt.json'
path.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('receipt',path,'sha256',sha(path),'head',receipt['head'],'clean',receipt['clean'])
print('whole CPP',compile_report['passed'],'/',compile_report['requested'],'callers',caller_report['passed'],'/',caller_report['requested'],'fixture mismatches',fixture['mismatch_count'])
print('source sha256',sha(source),'object sha256',sha(obj_path))
