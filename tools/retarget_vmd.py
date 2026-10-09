"""VMD -> selected game skeleton using EAI's built-in standard MMD profile.

Run with Blender background --factory-startup. No external model is required.
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent / 'vendor'))
import bpy
import retarget_fbx as retarget
import standard_mmd


def merge_bone_tracks(tracks, canonical, supported):
    """Merge spelling aliases by frame; neutral placeholder keys lose to motion.

    For conflicting non-neutral keys prefer the actual profile spelling, then
    lexical source order. Keep the chosen key's interpolation unchanged.
    """
    merged, sources, conflicts = {}, {}, []
    def priority(source, name, key):
        moving = any(abs(v) > 1e-7 for v in key.location) or any(abs(v) > 1e-7 for v in key.rotation[:3]) or abs(abs(key.rotation[3])-1) > 1e-7
        return moving, source == name
    for source in sorted(tracks):
        name = canonical(source)
        if name not in supported:
            continue
        frames = merged.setdefault(name, {})
        sources.setdefault(name, []).append(source)
        for key in tracks[source]:
            previous = frames.get(key.frame_number)
            if previous is not None:
                conflicts.append({'bone':name, 'frame':key.frame_number,
                                  'sources':[previous[0], source]})
            if previous is None or priority(source,name,key) > priority(previous[0],name,previous[1]):
                frames[key.frame_number] = (source,key)
    result = {name:[frames[f][1] for f in sorted(frames)] for name,frames in merged.items()}
    report = {'merged_bone_aliases':{name:raw for name,raw in sources.items() if len(raw)>1},
              'overlapping_bone_keys':len(conflicts),
              'bone_alias_conflict_policy':'non-neutral key, then exact profile spelling, then lexical source order'}
    return result, report


def load_motion(args):
    if bpy.app.version < (4, 2, 0):
        raise ValueError('MMD motion import requires Blender 4.2 or newer')
    import mmd_tools
    import logging
    logging.getLogger().setLevel(logging.ERROR)
    mmd_tools.register()
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    from mmd_tools.core import pmx
    args.model = args.output/'standard-skeleton.pmx'
    pmx.save(str(args.model), standard_mmd.create_model(pmx))
    result = bpy.ops.mmd_tools.import_model(filepath=str(args.model.resolve()),
        types={'ARMATURE'}, rename_bones=False, clean_model=False, scale=.08)
    if 'FINISHED' not in result:
        raise ValueError('Built-in MMD skeleton creation failed')
    rigs = [o for o in bpy.data.objects if o.type == 'ARMATURE']
    if len(rigs) != 1:
        raise ValueError('Built-in MMD skeleton must contain one armature')
    bpy.ops.object.select_all(action='DESELECT')
    rigs[0].select_set(True)
    bpy.context.view_layer.objects.active = rigs[0]
    from mmd_tools.core import vmd
    motion = vmd.File()
    motion.load(filepath=str(args.input.resolve()))
    import unicodedata
    pmx_names = {b.mmd_bone.name_j for b in rigs[0].pose.bones}
    normalized = {unicodedata.normalize('NFKC',name):name for name in pmx_names}
    aliases = {'左肘':'左ひじ','右肘':'右ひじ','左膝':'左ひざ','右膝':'右ひざ',
               '左人差指':'左人指','右人差指':'右人指'}
    def canonical(name):
        name=unicodedata.normalize('NFKC',name)
        for old,new in aliases.items():
            name=name.replace(old,new)
        return normalized.get(name,name)
    unknown = sorted(name for name in motion.boneAnimation if canonical(name) not in pmx_names)
    args.motion_report = {
        'reference_profile':standard_mmd.PROFILE,
        'external_model_required':False,
        'vmd_model_name':motion.header.model_name,
        'vmd_bone_tracks':len(motion.boneAnimation),
        'missing_source_bones':unknown,
        'ignored_camera_keys':len(motion.cameraAnimation),
        'ignored_light_keys':len(motion.lightAnimation),
        'ignored_morph_tracks':len(motion.shapeKeyAnimation),
        'ik_evaluated':True,
    }
    if not motion.boneAnimation:
        raise ValueError('VMD has no bone motion (camera-only motions are unsupported)')
    if not any(canonical(name) in pmx_names for name in motion.boneAnimation):
        raise ValueError('VMD has no supported standard MMD body bones')
    renamed = vmd.BoneAnimation()
    merged, merge_report = merge_bone_tracks(motion.boneAnimation,canonical,pmx_names)
    renamed.update(merged)
    args.motion_report.update(merge_report)
    motion.boneAnimation=renamed
    for frame in motion.propertyAnimation:
        frame.ik_states=[(canonical(name),enabled) for name,enabled in frame.ik_states]
    # Only bone and IK data is supplied to the importer, even if VMD includes
    # cameras, lights or facial tracks. Counts remain visible in the report.
    motion.cameraAnimation=vmd.CameraAnimation()
    motion.lightAnimation=vmd.LightAnimation()
    motion.shapeKeyAnimation=vmd.ShapeKeyAnimation()
    normalized_path=args.output/'normalized-motion.vmd'
    motion.save(filepath=str(normalized_path))
    result = bpy.ops.mmd_tools.import_vmd(filepath=str(normalized_path.resolve()),
        bone_mapper='PMX', margin=0, scale=.08, update_scene_settings=True,
        use_pose_mode=False, use_mirror=False, create_new_action=True, log_level='ERROR')
    if 'FINISHED' not in result:
        raise ValueError('VMD bone motion import failed')
    # The add-on creates a hidden mesh for IK-switch animation drivers even
    # with ARMATURE-only import. Keep that helper, never PMX geometry.
    if any(o.type in {'CAMERA', 'LIGHT'} for o in bpy.data.objects):
        raise ValueError('Unexpected camera or light')


def mmd_mapping(rig):
    names = {b.mmd_bone.name_j: b.name for b in rig.pose.bones}
    aliases = {'Bip001_Pelvis':['下半身'], 'Bip001_Spine':['上半身'],
        'Bip001_Spine1':['上半身2'], 'Bip001_Spine2':['上半身3'],
        'Bip001_Neck':['首'], 'Bip001_Head':['頭']}
    for side, jp in [('L','左'), ('R','右')]:
        for target, source in [('Clavicle','肩'),('UpperArm','腕'),
                ('Forearm','ひじ'),('Hand','手首'),('Thigh','足'),
                ('Calf','ひざ'),('Foot','足首'),('Toe0','つま先')]:
            aliases['Bip001_'+side+'_'+target] = [jp+source]
        for digit, finger in enumerate(['親指','人指','中指','薬指','小指']):
            for joint in range(3):
                number = joint if digit == 0 else joint+1
                aliases['Bip001_'+side+'_Finger'+str(digit)+(str(joint) if joint else '')] = [
                    jp+finger+str(number), jp+finger+'０１２３'[number],
                    jp+('人差指' if digit == 1 else finger)+str(number)]
    result = {target: names[next(n for n in choices if n in names)]
              for target, choices in aliases.items() if any(n in names for n in choices)}
    missing = [n for n in retarget.REQUIRED if n not in result]
    if missing:
        raise ValueError('Built-in MMD humanoid bones missing: '+', '.join(missing))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--actor',choices=['partner','controlled'],default='partner')
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    args.output.mkdir(parents=True, exist_ok=True)
    try:
        files = retarget.run(args, load_motion, mmd_mapping, 'vmd')
        result = {'ok':True, 'files':files}
    except Exception as error:
        import traceback
        traceback.print_exc()
        result = {'ok':False, 'error':str(error)}
    (args.output/'result.json').write_text(json.dumps(result, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False), flush=True)
    if not result['ok']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
