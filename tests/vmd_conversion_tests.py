"""Optional real-Blender integration test; private fixtures stay outside the repo.

python tests/vmd_conversion_tests.py --blender ... --motion ...
    --reference ... --output ... --validator ...
"""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import subprocess

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
for name in ['blender','motion','reference','output','validator']:
    parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--actor',choices=['controlled','partner'],default='partner')
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
# The upstream binary parser is independent of bpy. Add cameras and lights to
# a copy of the real motion and verify they have no influence on output tracks.
spec=importlib.util.spec_from_file_location('vmd_format',root/'tools/vendor/mmd_tools/core/vmd/__init__.py')
vmd=importlib.util.module_from_spec(spec)
spec.loader.exec_module(vmd)
motion=vmd.File()
motion.load(filepath=str(args.motion))
# The supplied motion keeps its arms/fingers still. Inject known movements to
# exercise those mappings independently of which limbs the sample animates.
for name in ['左ひじ','右ひじ','左人指１','右人指１']:
    keys=[]
    for frame,angle in [(0,0),(60,.5),(120,0)]:
        key=vmd.BoneFrameKey()
        key.frame_number=frame;key.location=(0,0,0)
        key.rotation=(0,math.sin(angle/2),0,math.cos(angle/2))
        key.interp=[20]*16+[107]*16+[20]*16+[107]*16
        keys.append(key)
    motion.boneAnimation[name]=keys
camera=vmd.CameraKeyFrameKey()
camera.location=(100,200,300);camera.rotation=(1,2,3)
camera.distance=99;camera.angle=45;camera.interp=[20,20,107,107]*6
camera.frame_number=10000
motion.cameraAnimation.append(camera)
light=vmd.LightKeyFrameKey()
light.color=(1,1,1);light.direction=(0,-1,0);light.frame_number=10000
motion.lightAnimation.append(light)
motion_file=args.output/'motion-with-camera.vmd'
motion.save(filepath=str(motion_file))
out=args.output/'converted'
command=[str(args.blender),'--background','--factory-startup','--python-exit-code','1',
    '--python',str(root/'tools/retarget_vmd.py'),'--',str(motion_file),str(out),
    '--reference',str(args.reference),'--actor',args.actor]
with (args.output/'blender.log').open('w',encoding='utf-8') as log:
    subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
report=json.loads((out/'conversion-report.json').read_text(encoding='utf-8'))
assert report['ignored_camera_keys'] >= 1
assert report['ignored_light_keys'] >= 1
assert report['reference_profile']=='eai-standard-mmd-v1'
assert report['external_model_required'] is False
assert 'reference_model' not in report
assert report['duration'] < 10000/30, 'Camera timeline must not extend motion duration'
result=json.loads((out/'result.json').read_text(encoding='utf-8'))
assert result['ok'] and len(result['files'])==1
clip_path=out/result['files'][0]
clip=json.loads(clip_path.read_text(encoding='utf-8'))
assert clip['target_character']==report['target_character']
reference=json.loads(args.reference.read_text(encoding='utf-8-sig'))
assert clip['actors'][0]['role']==args.actor
assert clip['target_character']==reference['controlled' if args.actor=='controlled' else 'partner']
tracks=clip['actors'][0]['tracks']
assert len([t for t in tracks if 'Finger' in t['bone']]) == 30
for track in tracks:
    times=[k['time'] for k in track['keys']]
    assert times[0]==0 and abs(times[-1]-clip['duration'])<1e-5
    assert times==sorted(times)
    for key in track['keys']:
        assert all(math.isfinite(x) for x in key['rotation'])
        assert abs(sum(x*x for x in key['rotation'])-1)<1e-4
        assert all(math.isfinite(x) for x in key.get('position',[]))
for bone in ['Bip001_L_Forearm','Bip001_R_Forearm','Bip001_L_Calf','Bip001_R_Calf',
             'Bip001_L_Finger1','Bip001_R_Finger1']:
    track=next(t for t in tracks if t['bone']==bone)
    assert len({tuple(k['rotation']) for k in track['keys']}) > 1, bone+' did not animate'
subprocess.run([str(args.validator),str(clip_path)],check=True)
print('PASS: built-in MMD profile, all 30 finger joints, finite normalized output; camera/light ignored')

# An IK-only motion has no thigh/calf FK keys. Moving IK must still bend legs.
ik_motion=vmd.File();ik_motion.header=vmd.Header()
ik_motion.header.model_name='IK-only test'
ik_motion.boneAnimation=vmd.BoneAnimation()
for name in ['左足IK','右足IK']:
    for frame,height in [(0,0),(30,1),(60,0)]:
        key=vmd.BoneFrameKey();key.frame_number=frame
        key.location=(.3 if name.startswith('左') else -.3,height,0)
        key.rotation=(0,0,0,1);key.interp=[20]*64
        ik_motion.boneAnimation[name].append(key)
ik_file=args.output/'ik-only.vmd';ik_motion.save(filepath=str(ik_file))
ik_out=args.output/'ik-only'
ik_command=command[:]
ik_command[ik_command.index('--')+1:]=[str(ik_file),str(ik_out),'--reference',str(args.reference),'--actor',args.actor]
with (args.output/'ik-blender.log').open('w',encoding='utf-8') as log:
    subprocess.run(ik_command,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=300)
ik_result=json.loads((ik_out/'result.json').read_text(encoding='utf-8'))
ik_clip=ik_out/ik_result['files'][0]
ik_data=json.loads(ik_clip.read_text(encoding='utf-8'))
for bone in ['Bip001_L_Calf','Bip001_R_Calf']:
    track=next(t for t in ik_data['actors'][0]['tracks'] if t['bone']==bone)
    assert len({tuple(k['rotation']) for k in track['keys']})>1, 'IK-only did not bend '+bone
subprocess.run([str(args.validator),str(ik_clip)],check=True)
print('PASS: half-width IK aliases and IK-only motion drives both knees without FK tracks')

camera_only=vmd.File();camera_only.header=vmd.Header()
camera_only.cameraAnimation=vmd.CameraAnimation();camera_only.cameraAnimation.append(camera)
camera_file=args.output/'camera-only.vmd';camera_only.save(filepath=str(camera_file))
negative=args.output/'camera-only'
bad_command=command[:]
bad_command[bad_command.index('--')+1:]=[str(camera_file),str(negative),'--reference',str(args.reference),'--actor',args.actor]
with (args.output/'camera-only.log').open('w',encoding='utf-8') as log:
    completed=subprocess.run(bad_command,stdout=log,stderr=subprocess.STDOUT,timeout=300)
assert completed.returncode!=0
bad=json.loads((negative/'result.json').read_text(encoding='utf-8'))
assert not bad['ok'] and 'no bone motion' in bad['error']
print('PASS: camera-only VMD rejects clearly')
