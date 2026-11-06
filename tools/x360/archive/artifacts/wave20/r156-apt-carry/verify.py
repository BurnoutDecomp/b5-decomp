import hashlib,json,runpy,subprocess,struct
from pathlib import Path
W=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20'); O=W/'r156-apt-carry'; L=W/'r156-link'
HEAD='8157850d32d5d15c6187b22969e939f5ca7c201d'; TREE='f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a'
expected={'seal':'6376515b0c2ab708d8cd8e6d3ec4eeb635826fa8a7f40bbee77799ec8c43f1c4','xex':'e1dd2fa9321932520dc561f491eb9e32f19029c6cd599a38f0a8a1302a083ed2','pe':'d75e7d429422a174df8788ba55bd979460a348ebd7f18339e65a54225a3873c8','map':'1dfdb006eff111042c95e4ce77de3d2c8e605c1fa748540c0c737189c0f8688e'}
sha=lambda b:hashlib.sha256(b).hexdigest()
def load(p):return json.loads(p.read_bytes())
v=runpy.run_path(str(W/'r145-linked-proof/verify_linked.py')); old=v['old']; priorpath=W/'r155-apt-regression/R155_APT_LINKED.json'; prior=load(priorpath); assert sha(priorpath.read_bytes())=='e1e18fcd2d17311307dba745b34fb3b11ec8cd3175015648dcd89f3e442b1b07'
viewpath=W/'r155-apt-view-linked/R155_APT_VIEW_LINKED_ROOT.json'; view=load(viewpath); receipt_path=viewpath.parent/'RECEIPT.json'; receipt=load(receipt_path); assert sha(receipt_path.read_bytes())=='666d248ed86276620c5875fe8383f311fa02fc2a65a4c971df6e4564edd2b218'; assert sha(viewpath.read_bytes())==receipt['artifacts'][viewpath.name] and receipt['state']=='pass'
seal,objects,entries,read,hashes=v['sealed_link'](L,HEAD)
assert all(hashes[k]==x for k,x in expected.items())
assert subprocess.check_output(['git','--no-optional-locks','-C',seal['source_root'],'rev-parse',HEAD+'^{tree}'],text=True).strip()==TREE
assert prior['head']==view['head']=='1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1'
assert set(objects)==set(prior['selected_objects'])
frozen={}; comparisons={}
for stem,row in prior['selected_objects'].items():
    data=Path(row['object']).read_bytes(); assert sha(data)==row['sha256']; frozen[stem]=data
    path,new,newrow=objects[stem]; assert row['source']==newrow['source']
    comparisons[stem]={'source':newrow['source'],'prior_object':row['object'],'prior_sha256':row['sha256'],'selected_object':str(path),'selected_sha256':sha(new),'identical':sha(new)==sha(data)}
linked,proofs,only=v['bind'](objects,frozen,v['FUNCTIONS'],entries,read)
assert set(proofs)==set(prior['functions']) and set(only)==set(prior['compiled_only']) and len(proofs)==48 and len(only)==3
slots=old['verify_dispatch'](linked,proofs,entries); paths=old['verify_paths'](linked,proofs,entries,read); classes=old['verify_class_paths'](linked,proofs)
composed=v['load'](W/'r145-composed-proof/R145_COMPOSED_COFF.json',v['COMPOSED_SHA']); native=v['check_new_paths'](linked,proofs,only,composed,entries,read)
# Load the pure view helpers without executing the previous verifier's main body.
helper={}; source=(viewpath.parent/'verify_r155_apt_view.py').read_text(); exec(source.split('\nprevious = json.loads')[0],helper)
selected={r['source']:r for r in seal['selected']}; viewowners={}
for src in ['src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp','src/GameShared/GameClasses/Module/VariableEventQueue_256_16.cpp']:
 row=selected[src]; p=Path(row['object']); assert sha(p.read_bytes())==row['sha256']; assert sum(1 for x in load(L/'invocation.json')['objects'] if x==row)==1
 assert row['sha256']==view['owner_objects_unchanged'][Path(src).stem]
 rsp=(L/'r156-strict.rsp').read_text().splitlines(); assert rsp.count(f'"{p}"')==1
 viewowners[src]=row
funcs={}; maps=helper['map_symbols'](L/'native-game.map')
for role,prev in view['functions'].items():
 symbol=prev['symbol']; src='src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp'; obj=Path(viewowners[src]['object']); va=old['map_address'](entries,symbol)
 check=helper['mask_linked'](L/'native-game.pe',va,prev['bytes'],obj,symbol)
 assert check['masked_sha256']==prev['relocation_masked_sha256'] and check['nonpair_relocations']==prev['nonpair_relocation_count']
 binds=helper['resolve_targets'](obj,symbol,va,maps); assert binds['counts']['unresolved']==0
 # Reconstruct and verify each PPC relocation, not merely masked bytes.
 key='view_carry_'+str(len(funcs)); old['coff_functions'].__globals__['PREFIX'][key]=symbol
 _,code,refs=old['coff_functions'](obj.read_bytes(),[key])[key]; actual,proof=old['verify_function'](entries,read,va,code,refs)
 funcs[role]={'symbol':symbol,'address':hex(va),'prior_address':prev['r155_address'],'bytes':len(code),'masked_sha256':check['masked_sha256'],'bindings':proof['bindings'],'image_sha256':proof['sha256'],'target_counts':binds['counts']}
assert funcs['queue:CgsGuiViewModule']['address']==funcs['queue:VariableEventQueue_256_16']['address']
loadva=int(funcs['ProcessIncomingLoadNotification']['address'],16); data=read(loadva,funcs['ProcessIncomingLoadNotification']['bytes']); word=lambda n:struct.unpack_from('>I',data,n)[0]
assert (word(0xA0),word(0xB0))==(0x39400001,0x91440014)
b=word(0xB4); disp=b&0x03fffffc; disp=disp-(1<<26) if disp&(1<<25) else disp
add=old['map_address'](entries,'?AddAptData@AptDataHandler@CgsGui@@QAAXPAUAptDataHeader@2@@Z'); assert loadva+0xB4+disp==add
out={'schema':'r156-apt-carry-v1','state':'pass','head':HEAD,'tree':TREE,'link_hashes':hashes,'prior_apt':{'path':str(priorpath),'sha256':sha(priorpath.read_bytes())},'prior_view':{'path':str(viewpath),'sha256':sha(viewpath.read_bytes())},'apt_owners':comparisons,'apt_functions':proofs,'compiled_only':only,'dispatch_slots':slots,'retained_paths':paths,'class_paths':classes,'native_paths':native,'view_owners':viewowners,'view_functions':funcs,'loaded_store':{'function':hex(loadva),'load_one':hex(word(0xA0)),'store':hex(word(0xB0)),'field_offset':'0x14','call_offset':'0xB4','branch_word':hex(b),'add_apt_data':hex(add)},'runtime':'not run'}
p=O/'R156_APT_CARRY.json'; p.write_text(json.dumps(out,indent=2)+'\n'); print('PASS',p,sha(p.read_bytes()),len(proofs),len(only),len(funcs),hex(loadva),hex(add))
