"""Check authored profile and binary round-trip without Blender/private models."""
import importlib.util
import math
from pathlib import Path
import tempfile

root=Path(__file__).resolve().parents[1]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module
pmx=load('pmx_format',root/'tools/vendor/mmd_tools/core/pmx/__init__.py')
profile=load('standard_mmd',root/'tools/standard_mmd.py')
model=profile.create_model(pmx)
assert not any([model.vertices,model.faces,model.textures,model.materials,model.rigids,model.joints])
names=[b.name for b in model.bones]
assert len(names)==len(set(names))
assert len([n for n in names if any(f in n for f in ['親指','人指','中指','薬指','小指'])])==30
for i,b in enumerate(model.bones):
    assert b.parent<i and b.parent>=-1
    assert all(math.isfinite(x) for x in b.location)
    if b.isIK:
        assert 0<=b.target<len(names) and b.ik_links
        for link in b.ik_links: assert 0<=link.target<len(names)
for jp in ['左','右']:
    foot=model.bones[names.index(jp+'足ＩＫ')]
    assert foot.target==names.index(jp+'足首')
    assert [link.target for link in foot.ik_links]==[names.index(jp+'ひざ'),names.index(jp+'足')]
    assert foot.ik_links[0].maximumAngle[0]<0
    assert jp+'腕捩' in names and jp+'手捩' in names
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)/'standard.pmx'
    pmx.save(str(path),model)
    restored=pmx.load(str(path))
    assert [b.name for b in restored.bones]==names
    assert not restored.vertices and not restored.materials
print('PASS: built-in MMD hierarchy, 30 finger joints, twists, knee/toe IK and mesh-free PMX round-trip')
