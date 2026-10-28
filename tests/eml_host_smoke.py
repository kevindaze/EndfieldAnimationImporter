"""Isolated smoke test of the packaged DLL in the real EML offline host.
Offline success does not verify game visuals or IL2CPP methods.
"""
import argparse,json,os,secrets,socket,subprocess,time,urllib.request,zipfile
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
# Isolate persistence from the user's real EAI settings and exercise legacy migration.
local=work/'appdata';settings=local/'BetterEndfield'/'Interaction'/'overlay-settings.json';settings.parent.mkdir(parents=True,exist_ok=True)
grip=','.join(map(str,([0]*6+[12,20,12,25,40,25,35,50,30,40,55,30,45,60,35])*2))
slots=[dict(enabled=i<2,name='Migration '+str(i),controlled='chr_0003_endminf',target='chr_0004_pelica' if i!=1 else 'chr_0005_chen',animation='standing',controlled_animation='none',controlled_pose='inherit',controlled_support='none',support_mode='none',grip_values=grip,bone_offsets={}) for i in range(9)]
settings.write_text(json.dumps(dict(base_configuration='{}',preset_bank=dict(schema=2,enabled=True,slots=slots),grip_values=grip,ui_schema=17)),encoding='utf-8')
environment=os.environ.copy();environment['LOCALAPPDATA']=str(local);environment['APPDATA']=str(local)
log=(work/'host.log').open('w',encoding='utf-8');host=subprocess.Popen([str(args.host.resolve()),'--index',str(index)],stdin=subprocess.PIPE,stdout=log,stderr=log,text=True,env=environment,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
try:
 status=wait(lambda: next((m for m in request('/status')['modules'] if m['id']==module and m['status']=='ready'),None))
 assert any(manifest['version'] in line for line in status['logs'])
 def reply(command,request_id,**fields):
  assert request('/send',{'module_id':module,'request_id':request_id,'body':{'command':command,**fields}})['accepted']
  return wait(lambda:next((m for m in request('/poll',{'module_id':module})['messages'] if m.get('request_id')==request_id),None))
 assert reply('workflow_get','workflow')['result']==0
 assert reply('presets_get','presets')['result']==0
 pairs=reply('paired_get','pairs');assert pairs['result']==0 and len(pairs['body']['paired']['items'])==1
 migrated=pairs['body']['preset_bank']['slots'];assert migrated[0]['pair_id']==migrated[1]['pair_id'] and migrated[0]['pair_id']
 assert set(pairs['body']['paired']['items'][0]['targets'])=={'chr_0004_pelica','chr_0005_chen'}
 assert list(settings.parent.glob('overlay-settings.json.before-pairs-*.json'))
 renamed=reply('paired_rename','rename',id=migrated[0]['pair_id'],name='自訂配對名稱');assert renamed['result']==0
 saved_pair=json.loads(settings.read_text(encoding='utf-8'))['paired_animations']['items'][0];assert saved_pair['name']=='自訂配對名稱' and saved_pair['custom_name'] and saved_pair['id']==migrated[0]['pair_id']
 assert reply('paired_rename','rename-invalid',id=migrated[0]['pair_id'],name='   ')['result']!=0
 white=reply('whitelist_set','white',disabled=['chr_0006_wolfgd'],save=True);assert white['result']==0
 assert not next(r for r in white['body']['operator_roster'] if r['id']=='chr_0006_wolfgd')['enabled']
 assert json.loads(settings.read_text(encoding='utf-8'))['disabled_operators']==['chr_0006_wolfgd']
 assert reply('interaction_target_set','invalid-role',target='chr_0006_wolfgd')['result']!=0
 assert reply('paired_assign','assign',index=4,id=migrated[0]['pair_id'])['result']==0
 # Updating an existing original role persists both actors and preserves other variants/slot references.
 assert reply('interaction_target_set','select-original-update',target='chr_0004_pelica')['result']==0
 loaded=reply('paired_select','load-original-update',id=migrated[0]['pair_id'])
 assert loaded['body']['paired']['is_supported'] and loaded['body']['paired']['can_add']
 revised=loaded['body']['workflow'];values=revised['grip_values'].split(',');values[0]='3';values[21]='4';revised['grip_values']=','.join(values)
 assert reply('draft_set','revise-pair-parameters',setting=revised)['result']==0
 assert reply('paired_add','update-supported-role')['result']==0
 updated=json.loads(settings.read_text(encoding='utf-8'))['paired_animations']['items'][0]
 assert updated['base']['grip_values']==revised['grip_values'] and updated['variants']['chr_0004_pelica']['grip_values']==revised['grip_values']
 assert updated['variants']['chr_0005_chen']['grip_values']==grip
 assert reply('paired_select','reload-updated-role',id=migrated[0]['pair_id'])['body']['workflow']['grip_values']==revised['grip_values']
 assert reply('interaction_target_set','select-calibration-partner',target='chr_0025_ardelia')['result']==0
 selected_pair=reply('paired_select','load-keep-partner',id=migrated[0]['pair_id']);assert selected_pair['result']==0 and selected_pair['body']['workflow']['target']=='chr_0025_ardelia' and selected_pair['body']['paired']['can_add']
 assert json.loads(settings.read_text(encoding='utf-8'))['preset_bank']['slots'][4]['pair_id']==migrated[0]['pair_id']
 physics=reply('breast_physics_get','physics');assert physics['result']==0 and len(physics['body']['breast_physics']['characters'])==20
 generated=reply('spawn_character_get','spawn');assert generated['result']==0 and generated['body']['spawn_character']['character']=='chr_0036_jsspsi' and not generated['body']['spawn_character']['spawned']
 assert any(c['id']=='chr_0016_laevat' for c in generated['body']['spawn_character']['characters'])
 assert reply('spawn_character_select','spawn-select',character='chr_0016_laevat')['body']['spawn_character']['character']=='chr_0016_laevat'
 assert reply('spawn_character_select','spawn-invalid',character='../bad')['result']!=0
 assert reply('spawn_character_create','spawn-offline')['result']!=0
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
