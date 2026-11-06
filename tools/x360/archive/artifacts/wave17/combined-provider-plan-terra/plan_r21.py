import json,pathlib,collections,hashlib,struct
P=pathlib.Path;o=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/combined-provider-plan-terra');base=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra');data=json.loads((o/'coff-records-r21-12libs.json').read_text()); rr=data['records'];objs=[x for x in rr if x['kind']=='object'];libs=[x for x in rr if x['kind']!='object']; auditpath=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r21-layout-intake-20261103/composite-artifact-audit.json');audit=json.loads(auditpath.read_text());
paths={'prior17':base/'R19_GAME_PROVIDER_PLAN.json','xgraphics':P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/material-state-provider-terra/XGRAPHICS_COHORT_PROPOSED_POLICY.json'),'xcam':P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/xcam-providers-terra/XCAM_PROVIDER_POLICY.json'),'xaudio':P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/audio-providers-terra/XAUDIO_COHORT_PROPOSED_POLICY.json'),'crt':P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-entry-terra/crt-audit/REPORT.md')}; pol={k:json.loads(v.read_text()) for k,v in paths.items() if k!='crt'}; groups={'prior17':pol['prior17']['excluded'],'xgraphics':pol['xgraphics']['exclude_game_link_only'],'xcam':pol['xcam']['objects'],'xaudio':pol['xaudio']['excluded_game_link_only']};exception=next(x for x in objs if P(x['path']).name=='exception-f6409ca5.obj');groups['crt']=[{'source':exception['source'],'object':exception['path'],'sha256':exception['sha256']}];sha=lambda p:hashlib.sha256(P(p).read_bytes()).hexdigest(); bysrc={x['source']:x for x in objs};excluded={};mismatch=[]
for cat,items in groups.items():
 for x in items:
  s=x['source'];assert s not in excluded,(s,cat);assert s in bysrc,s;z=bysrc[s]; excluded[s]=cat
  if x['sha256']!=z['sha256']:mismatch.append({'source':s,'cohort':cat,'policy_sha256':x['sha256'],'r21_sha256':z['sha256'],'object':z['path']})
assert len(excluded)==102;bad=[x['source'] for x in objs if audit['artifact_manifest'][x['source']]['object_sha256']!=x['sha256']];assert not bad
selected=[x for x in objs if x['source'] not in excluded];
seldefs=collections.defaultdict(list); sdkdefs=collections.defaultdict(list); cohortdefs=collections.defaultdict(list); weak=collections.defaultdict(list)
for x in selected:
 for n,k,s,c,v in x['symbols']:
  if k in ('strong','comdat','common','absolute'):seldefs[n].append({'id':x['id'],'kind':k,'selection':c})
  elif k=='weak':weak[n].append({'id':x['id'],'fallback':v,'search':c})
for x in libs:
 for n,k,s,c,v in x['symbols']:
  if k in ('strong','comdat','common','absolute','import'):sdkdefs[n].append({'id':x['id'],'library':P(x['archive']).name,'member':x['member'],'kind':k,'selection':c})
for x in objs:
 if x['source'] not in excluded:continue
 for n,k,s,c,v in x['symbols']:
  if k in ('strong','comdat','common','absolute'):cohortdefs[n].append({'id':x['id'],'cohort':excluded[x['source']],'source':x['source'],'kind':k,'selection':c})
refs=collections.defaultdict(lambda:{'undefined':[],'relocations':[]})
for r in selected:
 for n,k,s,c,v in r['symbols']:
  if n in cohortdefs and k=='undefined':refs[n]['undefined'].append({'id':r['id'],'source':r['source']})
 b=P(r['path']).read_bytes();_,ns,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',b);st=b[so+nn*18:];sym={};i=0
 while i<nn:
  raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18);off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0;end=st.find(b'\0',off) if off else -1;n=st[off:end].decode('latin1') if off and end>=0 else raw.rstrip(bytes(1)).decode('latin1');sym[i]=(n,sec,cl);i+=1+aux
 for j in range(ns):
  h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*j);assert not h[9]&0x01000000,(r['path'],j)
  for z in range(h[7]):
   addr,idx,typ=struct.unpack_from('<IIH',b,h[5]+10*z);v=sym.get(idx)
   if v and v[0] in cohortdefs:refs[v[0]]['relocations'].append({'id':r['id'],'source':r['source'],'section':j+1,'offset':addr,'type':typ,'target_section':v[1],'storage_class':v[2]})
refout={}; blockers={}
for n,ref in refs.items():
 if not ref['undefined'] and not ref['relocations']:continue
 row={'cohort_definitions':cohortdefs[n],'outside_undefined':ref['undefined'],'outside_relocations':ref['relocations'],'remaining_object_candidates':seldefs[n],'sdk_candidates':sdkdefs[n],'weak_aliases':weak[n]};refout[n]=row
 if ref['undefined'] and not seldefs[n] and not sdkdefs[n] and not weak[n]:blockers[n]=row
libs_rsp=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/sdk-library-providers-terra/libraries-candidate-full-conditional.rsp');rsp=o/'R21_GAME_OBJECTS_102_EXCLUSIONS_PROVISIONAL.rsp';rsp.write_text('\n'.join('"'+x['path']+'"' for x in selected)+'\n');manifest={'head':audit['head'],'report':data['report'],'report_sha256':data['report_sha256'],'audit':str(auditpath),'audit_sha256':sha(auditpath),'selection_scope':'R21 provisional source-preserving SDK provider policy. Later source fixes not included. No link authorized.','r21_pass_objects':len(objs),'selected_count':len(selected),'excluded_count':len(excluded),'cohort_counts':{k:len(v) for k,v in groups.items()},'policy_sha256':{k:sha(v) for k,v in paths.items()},'policy_r21_hash_differences':mismatch,'libraries':data['libraries'],'library_rsp':str(libs_rsp),'library_rsp_sha256':sha(libs_rsp),'object_rsp':str(rsp),'object_rsp_sha256':sha(rsp),'selected':[{'source':x['source'],'object':x['path'],'sha256':x['sha256']} for x in selected],'excluded':[{'source':x['source'],'object':x['path'],'sha256':x['sha256'],'group':excluded[x['source']]} for x in objs if x['source'] in excluded],'cohort_external_references':refout,'no_remaining_candidate_with_outside_undefined':blockers,'all_selected_objects_match_sealed_audit':not bad};(o/'R21_COMBINED_PROVIDER_PROVISIONAL.json').write_text(json.dumps(manifest,indent=2)); print('counts',len(objs),len(selected),len(excluded),'mismatch',len(mismatch),'cohort outside refs',len(refout),'no candidate blockers',len(blockers)); print('blockers',list(blockers)[:30]);print('changed',mismatch)

