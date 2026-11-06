import json,pathlib,subprocess
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave30/protossl-native/cert-parser')
key=serialization.load_pem_private_key((p/'test-key.pem').read_bytes(),password=None)
data=bytearray((p/'sha1_bad_signature.der').read_bytes())
def parse_header(b,i):
 n=b[i+1];return (i+2,n) if n<128 else (i+2+(n&127),int.from_bytes(b[i+2:i+2+(n&127)],'big'))
outer,_=parse_header(data,0);tbs_start=outer;tbs_content,tbs_len=parse_header(data,tbs_start);assert tbs_content-tbs_start==4
tbs=bytes(data[tbs_start:tbs_content+tbs_len]);sig=key.sign(tbs,padding.PKCS1v15(),hashes.SHA1());assert len(sig)==128
data[-128:]=sig;(p/'testroot_valid.der').write_bytes(data)
bad=bytearray(data);bad[-1]^=1;(p/'testroot_tampered.der').write_bytes(bad)
cases=[]
for name,buf,expect in [('valid_test_root',data,0),('tampered_signature',bad,-27)]:
 r=subprocess.run([str(p/'host-parser-testroot.exe'),'example.com',bytes(buf).hex()],capture_output=True,text=True,timeout=90)
 cases.append({'name':name,'expected':expect,'actual':r.stdout,'returncode':r.returncode,'passed':r.returncode==0 and r.stdout==str(expect)})
report={'host_only':True,'no_network':True,'trust':'isolated test-only generated 1024-bit RSA modulus, exponent 65537; product trust is unchanged','tbs_bytes':len(tbs),'cases':cases}
(p/'testroot-signature-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert all(c['passed'] for c in cases)
