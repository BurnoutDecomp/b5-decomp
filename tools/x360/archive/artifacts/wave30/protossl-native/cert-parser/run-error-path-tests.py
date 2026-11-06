import json,pathlib,subprocess
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-parser')
exec((p/'run-parser-tests.py').read_text().split('fixtures=')[0])
def build(f):
 issuer0=name(trusted);subject0=name([(3,'example.com')]);valid1=tlv(0x17,b'240101000000Z');valid2=tlv(0x17,b'300101000000Z')
 serial=tlv(2,bytes(33) if f==4 else b'\1')
 inneralg=tlv(0x31,b'') if f==5 else seq(tlv(5,b'')) if f==6 else alg(99 if f==7 else 5)
 issuer=tlv(0x31,b'') if f==8 else issuer0
 validity=tlv(0x31,b'') if f==9 else seq(tlv(0x18,b'') if f==10 else valid1,tlv(0x18,b'') if f==11 else valid2)
 subject=tlv(0x31,b'') if f==12 else subject0
 modulus=bytes([0])+bytes([0x81])+bytes(126)+bytes([1])
 pub=tlv(0x31,b'') if f==21 else seq(tlv(2,b'\1\2\3' if f==22 else modulus),tlv(2,b'' if f==23 else bytes.fromhex('010001')))
 spkialg=tlv(0x31,b'') if f==14 else seq(tlv(5,b'')) if f==15 else alg(1)
 spki=tlv(0x31,b'') if f==13 else seq(spkialg,tlv(4,b'') if f==16 else tlv(3,b'\0'+pub))
 tbs=seq(serial,inneralg,issuer,validity,subject,spki)
 outeralg=tlv(0x31,b'') if f==18 else seq(tlv(5,b'')) if f==19 else alg(5)
 sig=tlv(4,b'') if f==20 else tlv(3,b'\0'+bytes(128))
 return seq(tbs,outeralg,sig)
cases=[]
for fault in [3,4,5,6,7,8,9,10,11,12,13,14,15,16,18,19,20,21,22,23]:
 data=bytes.fromhex('30033001a0') if fault==3 else build(fault)
 (p/('error_%02d.der'%fault)).write_bytes(data)
 r=subprocess.run([str(p/'host-parser.exe'),'example.com',data.hex()],capture_output=True,text=True,timeout=20)
 cases.append({'error':-fault,'actual':r.stdout,'returncode':r.returncode,'passed':r.returncode==0 and r.stdout==str(-fault)})
(p/'native-error-path-tests.json').write_text(json.dumps({'host_only':True,'no_network':True,'cases':cases},indent=2)+'\n')
print(json.dumps(cases,indent=2));assert all(x['passed'] for x in cases)
