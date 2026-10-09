"""FBX -> bind-pose-based Endfield animation. Run in Blender background mode.

Fails closed without Unity mesh bindposes. Runtime pose snapshots alone are not
treated as a T-pose. Supports the supplied Chocolate rig and Mixamo naming.
"""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

REQUIRED = ['Bip001_Pelvis', 'Bip001_Spine', 'Bip001_Head', 'Bip001_Neck']
for side in ('L', 'R'):
    REQUIRED += ['Bip001_'+side+'_'+n for n in ('UpperArm', 'Forearm', 'Hand', 'Thigh', 'Calf', 'Foot')]


def matrix(value):
    if len(value) != 4 or any(len(row) != 4 for row in value):
        raise ValueError('Invalid reference matrix')
    if any(not math.isfinite(v) for row in value for v in row):
        raise ValueError('Non-finite reference matrix')
    return Matrix(value)


def rigid(m):
    if m.to_3x3().determinant() <= 1e-9:
        raise ValueError('Mirrored or singular target bone transform')
    scales = m.to_scale()
    if max(scales)-min(scales) > .01 * max(scales):
        raise ValueError('Nonuniform target bone scale requires a separate retarget profile')
    result = m.to_quaternion().normalized().to_matrix().to_4x4() @ Matrix.Diagonal((*scales, 1.))
    result.translation = m.translation
    return result


def target_reference(data, actor=1):
    if data.get('format') != 'endfield-skeleton-pose-reference' or data.get('version') != 1:
        raise ValueError('Unsupported skeleton reference')
    palettes = [p for p in data.get('mesh_bindposes', []) if p['actor'] == actor]
    if not palettes:
        raise ValueError('No partner mesh bindposes; connect and refresh skeleton with the new module')
    nodes = {b['key']: b for b in data['bones'] if b['actor'] == actor}
    # Facial/accessory joints are not part of this body retargeter. Validate
    # only body joints and their ancestors, not unrelated scaled eyelashes.
    needed = {key for key, node in nodes.items() if node['name'].startswith('Bip001')}
    pending = list(needed)
    while pending:
        parent = nodes[pending.pop()].get('parent_key')
        if parent in nodes and parent not in needed:
            needed.add(parent)
            pending.append(parent)
    nodes = {key: node for key, node in nodes.items() if key in needed}
    names = {}
    for key, node in nodes.items():
        if node['name'] in names and node['name'].startswith('Bip001'):
            raise ValueError('Ambiguous target bone '+node['name'])
        names[node['name']] = key
    palettes.sort(key=lambda p: sum(b['name'] in REQUIRED for b in p['bones']), reverse=True)
    model = {}
    inverse_root = matrix(palettes[0]['root_world_to_local'])
    # Unweighted helper ancestors keep their captured local frame. Weighted
    # joints below always take the authoritative mesh bind pose instead.
    for key, node in nodes.items():
        q = node['world_rotation_xyzw']
        current = Quaternion((q[3], q[0], q[1], q[2])).to_matrix().to_4x4()
        current.translation = Vector(node['world_position'])
        if 'world_matrix' in node:
            current = matrix(node['world_matrix'])
        model[key] = rigid(inverse_root @ current)
    weighted = set()
    for palette in palettes:
        to_model = matrix(palette['root_world_to_local']) @ matrix(palette['renderer_local_to_world'])
        for bone in palette['bones']:
            key = bone['key']
            if key not in nodes:
                continue
            if key in weighted:
                continue  # body palette wins over clothing/accessory palettes
            bind = matrix(bone['inverse_bind_matrix'])
            if abs(bind.determinant()) < 1e-12:
                raise ValueError('Singular inverse bind pose')
            model[key] = rigid(to_model @ bind.inverted())
            weighted.add(key)
    missing = [n for n in REQUIRED if n not in names or names[n] not in weighted]
    if missing:
        raise ValueError('Target bind poses missing: '+', '.join(missing))
    return nodes, names, model, weighted


def normalize_name(name):
    return name.split(':')[-1].replace('mixamorig', '').replace('_', '').replace('.', '').lower()


def source_mapping(rig):
    normalized = {}
    for bone in rig.data.bones:
        key = normalize_name(bone.name)
        if key in normalized:
            raise ValueError('Ambiguous source bone '+key)
        normalized[key] = bone.name
    aliases = {'Bip001_Pelvis': ['Hips', 'Pelvis'], 'Bip001_Spine': ['Spine'],
               'Bip001_Spine1': ['Spine1'], 'Bip001_Spine2': ['Spine2', 'Chest'],
               'Bip001_Neck': ['Neck'], 'Bip001_Head': ['Head']}
    for side, long in [('L', 'Left'), ('R', 'Right')]:
        for target, choices in [('Clavicle', ['Shoulder.'+side, long+'Shoulder']),
                                 ('UpperArm', ['UpperArm.'+side, long+'Arm']),
                                 ('Forearm', ['LowerArm.'+side, long+'ForeArm']),
                                 ('Hand', ['Hand.'+side, long+'Hand']),
                                 ('Thigh', ['UpperLeg.'+side, long+'UpLeg']),
                                 ('Calf', ['LowerLeg.'+side, long+'Leg']),
                                 ('Foot', ['Foot.'+side, long+'Foot']),
                                 ('Toe0', ['Toe.'+side, long+'ToeBase'])]:
            aliases['Bip001_'+side+'_'+target] = choices
        for digit, finger in enumerate(['Thumb', 'Index', 'Middle', 'Ring', 'Little']):
            for joint, suffix in enumerate(['Proximal', 'Intermediate', 'Distal']):
                mixamo_finger = 'Pinky' if finger == 'Little' else finger
                aliases['Bip001_'+side+'_Finger'+str(digit)+(str(joint) if joint else '')] = [
                    finger+suffix+'.'+side, long+'Hand'+mixamo_finger+str(joint+1)]
    result = {}
    for target, choices in aliases.items():
        for choice in choices:
            if normalize_name(choice) in normalized:
                result[target] = normalized[normalize_name(choice)]
                break
    missing = [n for n in REQUIRED if n not in result]
    if missing:
        raise ValueError('Source humanoid bones missing: '+', '.join(missing))
    return result


def body_basis(transforms):
    right = transforms['Bip001_R_Thigh'].translation-transforms['Bip001_L_Thigh'].translation
    up = transforms['Bip001_Head'].translation-transforms['Bip001_Pelvis'].translation
    if right.length < 1e-6 or up.length < 1e-6:
        raise ValueError('Degenerate humanoid reference')
    up.normalize()
    right -= up*right.dot(up)
    right.normalize()
    forward = right.cross(up).normalized()
    return Matrix((right, up, forward)).transposed()


def retarget_frame(source_bind, source_pose, target_bind, parents, mapping, source_basis, target_basis, height_scale, align_directions=True, initial_source_position=None):
    # Blender is right handed, Unity is left handed. Apply the reflection to
    # ROTATION MATRICES, not by guessing quaternion sign flips per bone.
    reflection = Matrix.Diagonal((1., 1., -1.))
    conversion = target_basis @ reflection @ source_basis.transposed()
    rotations = {}
    for key, source in mapping.items():
        bind_q = source_bind[source].to_quaternion().normalized()
        pose_q = source_pose[source].to_quaternion().normalized()
        delta = (pose_q @ bind_q.inverted()).to_matrix()
        rotations[key] = (conversion @ delta @ conversion.transposed()) @ target_bind[key].to_quaternion().to_matrix()
    # Match anatomical segment directions, not only rotation deltas: an A-pose
    # source and T-pose target otherwise keep their different reference angles.
    semantic_children = {}
    for side in ('L','R'):
        prefix='Bip001_'+side+'_'
        semantic_children.update({prefix+'Clavicle':prefix+'UpperArm',
            prefix+'UpperArm':prefix+'Forearm',prefix+'Forearm':prefix+'Hand',
            prefix+'Hand':prefix+'Finger2',prefix+'Thigh':prefix+'Calf',
            prefix+'Calf':prefix+'Foot',prefix+'Foot':prefix+'Toe0'})
    by_semantic={semantic:key for key,semantic in mapping.items()}
    if align_directions:
        for key,semantic in mapping.items():
            child=semantic_children.get(semantic)
            child_key=by_semantic.get(child)
            if child_key is None or child not in source_pose:
                continue
            source_direction=conversion @ (source_pose[child].translation-source_pose[semantic].translation)
            bind_direction=target_bind[child_key].translation-target_bind[key].translation
            local_axis=target_bind[key].to_quaternion().inverted() @ bind_direction
            observed=rotations[key] @ local_axis
            if source_direction.length<1e-6 or observed.length<1e-6:
                raise ValueError('Degenerate anatomical segment '+semantic)
            correction=observed.normalized().rotation_difference(source_direction.normalized())
            rotations[key]=correction.to_matrix() @ rotations[key]
    pelvis = next(k for k in mapping if mapping[k] == 'Bip001_Pelvis')
    # Anchor motion to the first evaluated pose, rather than the source rig's
    # authored scene location. Keep subsequent XYZ motion, including crouches.
    origin = initial_source_position if initial_source_position is not None else source_bind['Bip001_Pelvis'].translation
    displacement = conversion @ (source_pose['Bip001_Pelvis'].translation-origin) * height_scale
    posed = {}
    locals = {}
    visiting = set()
    def visit(key):
        if key in posed:
            return posed[key]
        if key in visiting:
            raise ValueError('Cyclic target hierarchy')
        visiting.add(key)
        parent = parents.get(key)
        bind_parent = target_bind[parent] if parent in target_bind else Matrix.Identity(4)
        pose_parent = visit(parent) if parent in target_bind else Matrix.Identity(4)
        local_bind = bind_parent.inverted() @ target_bind[key]
        world = pose_parent @ local_bind
        if key in rotations:
            world = rotations[key].to_4x4() @ Matrix.Diagonal((*target_bind[key].to_scale(), 1.))
            world.translation = (pose_parent @ local_bind).translation
        if key == pelvis:
            world.translation = target_bind[key].translation+displacement
        posed[key] = world
        locals[key] = pose_parent.inverted() @ world
        visiting.remove(key)
        return world
    for key in target_bind:
        visit(key)
    return locals, pelvis


def quaternion_error(a, b):
    norm=math.sqrt(math.fsum(v*v for v in a)*math.fsum(v*v for v in b))
    return 2*math.acos(min(1, abs(math.fsum(x*y for x,y in zip(a,b)))/norm))


def simplify(samples, positional):
    keep = {0, len(samples)-1}
    pending = [(0, len(samples)-1)]
    while pending:
        first, last = pending.pop()
        worst, index = 1., None
        t0, q0, p0 = samples[first]
        t1, q1, p1 = samples[last]
        if q0.dot(q1) < 0:
            q1 = -q1
        for i in range(first+1, last):
            t, q, p = samples[i]
            u = (t-t0)/(t1-t0)
            estimate = Quaternion(tuple(a*(1-u)+b*u for a,b in zip(q0,q1))).normalized()
            score = quaternion_error(q,estimate)/math.radians(.5)
            if positional:
                score = max(score, (p-p0.lerp(p1,u)).length/.001)
            if score > worst:
                worst, index = score, i
        if index is not None:
            keep.add(index)
            pending.extend([(first,index),(index,last)])
    return [samples[i] for i in sorted(keep)]


def run(args, loader=None, mapper=None, kind='fbx'):
    reference_bytes = args.reference.read_bytes()
    reference = json.loads(reference_bytes)
    actor = getattr(args, 'actor', 'partner')
    character = reference['controlled' if actor == 'controlled' else 'partner']
    nodes, names, target_bind, weighted = target_reference(reference, 0 if actor == 'controlled' else 1)
    if loader:
        loader(args)
    else:
        bpy.ops.import_scene.fbx(filepath=str(args.input.resolve()))
    rigs = [o for o in bpy.data.objects if o.type=='ARMATURE' and o.animation_data and o.animation_data.action]
    if len(rigs) != 1:
        raise ValueError('Expected one animated humanoid armature')
    rig = rigs[0]
    if not loader and any(b.constraints for b in rig.pose.bones):
        raise ValueError('Bake source constraints before importing')
    source_names = (mapper or source_mapping)(rig)
    # Optional joints require authoritative target bind poses too.
    source_names = {target:source for target,source in source_names.items()
                    if target in names and names[target] in weighted}
    first, last = map(float,rig.animation_data.action.frame_range)
    scene = bpy.context.scene
    source_fps = scene.render.fps/scene.render.fps_base
    duration = (last-first)/source_fps
    if not .2 <= duration <= 3600:
        raise ValueError('Source duration must be 0.2 seconds to 1 hour')
    scene.frame_set(math.floor(first),subframe=first-math.floor(first))
    source_bind = {target:rig.matrix_world @ rig.data.bones[source].matrix_local for target,source in source_names.items()}
    source_basis = body_basis(source_bind)
    target_named = {name:target_bind[key] for name,key in names.items()}
    target_basis = body_basis(target_named)
    def leg_height(transforms):
        hips = transforms['Bip001_Pelvis'].translation
        return sum((hips-transforms['Bip001_'+side+'_Foot'].translation).length for side in ['L','R'])/2
    height_scale = leg_height(target_named)/leg_height(source_bind)
    mapping = {names[target]:target for target in source_names}
    parents = {key:node['parent_key'] for key,node in nodes.items()}
    selected = set(mapping)
    for key in list(selected):
        parent = parents.get(key)
        while parent in nodes and nodes[parent]['name'] != '@root':
            selected.add(parent)
            parent = parents.get(parent)
    if len(selected)>64:
        raise ValueError('Mapped hierarchy exceeds 64 tracks')
    # FBX needs only the armature. MMD keeps helpers for IK animation drivers.
    for obj in list(bpy.data.objects):
        if obj!=rig and not loader:
            bpy.data.objects.remove(obj,do_unlink=True)
    boundaries=[0.]
    while boundaries[-1]+30<duration:
        boundaries.append(boundaries[-1]+30)
    if duration-boundaries[-1]<.2 and len(boundaries)>1:
        boundaries.pop()
    boundaries.append(duration)
    sample_fps = 30 if kind == 'vmd' else 10
    times=sorted(set([min(i/sample_fps,duration) for i in range(math.ceil(duration*sample_fps)+1)]+boundaries+[min(10.,duration)]))
    samples={key:[] for key in sorted(selected)}
    pelvis_key=None
    initial_source_position=None
    for i,t in enumerate(times):
        f=first+t*source_fps
        scene.frame_set(math.floor(f),subframe=f-math.floor(f))
        source_pose={target:rig.matrix_world @ rig.pose.bones[source].matrix for target,source in source_names.items()}
        if initial_source_position is None:
            initial_source_position=source_pose['Bip001_Pelvis'].translation.copy()
        local,pelvis_key=retarget_frame(source_bind,source_pose,target_bind,parents,mapping,source_basis,target_basis,height_scale,initial_source_position=initial_source_position)
        for key in samples:
            q=local[key].to_quaternion().normalized()
            if samples[key] and samples[key][-1][1].dot(q)<0:
                q.negate()
            samples[key].append((t,q,local[key].translation.copy()))
        if i%500==0:
            print('Retarget',i,'/',len(times),flush=True)
    source_hash=hashlib.sha256(args.input.read_bytes()).hexdigest()
    ref_hash=hashlib.sha256(reference_bytes).hexdigest()
    model_hash = hashlib.sha256(args.model.read_bytes()).hexdigest()[:10]+'_' if kind == 'vmd' else ''
    prefix=kind+'_'+source_hash[:10]+'_'+model_hash+character+('_main' if actor == 'controlled' else '')
    files=[]
    def export(start,end,suffix,label):
        clip_id=prefix+'_'+suffix
        tracks=[]
        for key in samples:
            part=[(round(t-start,6),q,p) for t,q,p in samples[key] if start-1e-7<=t<=end+1e-7]
            part[0]=(0.,part[0][1],part[0][2])
            part[-1]=(round(end-start,6),part[-1][1],part[-1][2])
            keys=[]
            for t,q,p in simplify(part,key==pelvis_key):
                k={'time':t,'rotation':[round(q.x,6),round(q.y,6),round(q.z,6),round(q.w,6)]}
                if key==pelvis_key:
                    k['position']=[round(v,6) for v in p]
                keys.append(k)
            tracks.append({'bone':nodes[key]['name'],'keys':keys})
        data={'format':'endfield-interaction-animation','version':2,'rotation_space':'absolute_local',
              'target_character':character,'id':clip_id,'name':label,
              'duration':round(end-start,6),'distance':1.2,
              'actors':[{'role':actor,'tracks':tracks}]}
        file=args.output/(clip_id+'.interaction-animation.json')
        file.write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
        files.append(file.name)
    title=args.input.stem[:25]
    export(0,duration,'full',title+' 完整動畫')
    report={'source':str(args.input.resolve()),'source_sha256':source_hash,
            'reference':str(args.reference.resolve()),'reference_sha256':ref_hash,
            'target_character':character,'target_role':actor,'source_mapping':source_names,
            'duration':duration,'height_scale':height_scale,'tracks':len(samples),
            'algorithm':'mesh bind poses, body-frame handedness conversion, anatomical segment alignment, target-parent local reconstruction',
            'root_motion':'pelvis translation relative to first evaluated source pose; model anchors remain fixed',
            'source_initial_pelvis_position':list(initial_source_position),
            'not_transferred':['facial shapes','hair','cloth','props'],
            'verified_in_game':False, 'source_format':kind, 'sample_fps':sample_fps}
    if kind == 'vmd':
        report.update(args.motion_report)
    (args.output/'conversion-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    return files


def main():
    p=argparse.ArgumentParser()
    p.add_argument('input',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--reference',type=Path,required=True)
    p.add_argument('--actor',choices=['partner','controlled'],default='partner')
    args=p.parse_args(sys.argv[sys.argv.index('--')+1:])
    args.output.mkdir(parents=True,exist_ok=True)
    try:
        files=run(args)
        result={'ok':True,'files':files}
    except Exception as e:
        result={'ok':False,'error':str(e)}
    (args.output/'result.json').write_text(json.dumps(result,ensure_ascii=False),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False),flush=True)
    if not result['ok']:
        raise SystemExit(1)


if __name__=='__main__':
    main()
