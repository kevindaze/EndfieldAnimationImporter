"""Read-only smoke test of the packaged DLL in the real EML offline host.
Offline success does not verify game visuals or IL2CPP methods.
"""
import argparse,json,secrets,socket,subprocess,time,urllib.request,zipfile
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('--host',type=Path,required=True);parser.add_argument('--zip',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);args=parser.parse_args()
work=args.work.resolve();work.mkdir(parents=True,exist_ok=True);package=work/'package';package.mkdir(exist_ok=True)
with zipfile.ZipFile(args.zip) as archive:
 for item in archive.infolist():
  target=(package/item.filename).resolve()
  if not target.is_relative_to(package):raise ValueError('Package path escapes staging')
 archive.extractall(package)
manifest=json.loads((package/'module.json').read_text(encoding='utf-8'));module=manifest['id']
with socket.socket() as probe:probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
token=secrets.token_hex(24);index=work/'index.json'
index.write_text(json.dumps({'schema':1,'port':port,'token':token,'modules':[{'id':module,'directory':package.as_posix(),'generation':'eai-smoke','enabled':True,'configuration':{}}]}),encoding='utf-8')
def request(path,body=None):
 data=None if body is None else json.dumps(body).encode()
 req=urllib.request.Request('http://127.0.0.1:'+str(port)+path,data=data,headers={'Authorization':'Bearer '+token,'Content-Type':'application/json'})
 with urllib.request.urlopen(req,timeout=3) as response:return json.load(response)
def wait(fn):
 end=time.monotonic()+20
 while time.monotonic()<end:
  try:
   value=fn()
   if value:return value
  except (OSError,ValueError):pass
  time.sleep(.1)
 raise AssertionError('EML host did not reach expected status')
log=(work/'host.log').open('w',encoding='utf-8');host=subprocess.Popen([str(args.host.resolve()),'--index',str(index)],stdin=subprocess.PIPE,stdout=log,stderr=log,text=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
try:
 status=wait(lambda: next((m for m in request('/status')['modules'] if m['id']==module and m['status']=='ready'),None))
 assert any(manifest['version'] in line for line in status['logs'])
 def reply(command,request_id):
  assert request('/send',{'module_id':module,'request_id':request_id,'body':{'command':command}})['accepted']
  return wait(lambda:next((m for m in request('/poll',{'module_id':module})['messages'] if m.get('request_id')==request_id),None))
 assert reply('workflow_get','workflow')['result']==0
 assert reply('presets_get','presets')['result']==0
 connection=reply('connect','connect');assert connection['result']==2 and 'runtime' in connection['body']['detail']
 print('PASS: EML loads packaged EAI '+manifest['version']+', initializes, dispatches native replies, and safely reports offline runtime unavailable')
finally:
 if host.poll() is None:
  host.stdin.write('stop\n');host.stdin.flush()
  try:host.wait(timeout=10)
  except subprocess.TimeoutExpired:host.kill();host.wait();raise
 log.close()
assert host.returncode==0
print('PASS: EML shutdown')
