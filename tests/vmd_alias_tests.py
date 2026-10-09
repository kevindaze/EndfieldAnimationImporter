"""Test name-collision handling without Blender or private fixtures."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unicodedata
source=Path(__file__).resolve().parents[1]/'tools/retarget_vmd.py'
tree=ast.parse(source.read_text(encoding='utf-8'))
function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='merge_bone_tracks')
namespace={};exec(compile(ast.Module(body=[function],type_ignores=[]),str(source),'exec'),namespace)
merge=namespace['merge_bone_tracks']
def key(frame,angle=0):
 return SimpleNamespace(frame_number=frame,location=(0,0,0),rotation=(angle,0,0,1),interp=[frame]*64)
canonical=lambda s:unicodedata.normalize('NFKC',s)
active=key(0,.2);neutral=key(0);later=key(60,.3)
tracks={'上半身2':[active], '上半身２':[neutral,later]}
for data in [tracks,dict(reversed(list(tracks.items())))]:
 result,report=merge(data,canonical,{'上半身2'})
 assert result['上半身2']==[active,later]
 assert report['overlapping_bone_keys']==1 and '上半身2' in report['merged_bone_aliases']
# Meaningful aliased motion beats a neutral exact-spelling placeholder.
result,_=merge({'上半身2':[neutral],'上半身２':[active]},canonical,{'上半身2'})
assert result['上半身2']==[active]
# Conflicting authored keys use exact spelling, keeping interpolation intact.
other=key(0,.5)
result,_=merge({'上半身2':[active],'上半身２':[other]},canonical,{'上半身2'})
assert result['上半身2'][0] is active
# Unsupported duplicate aliases are ignored rather than rejecting the file.
result,_=merge({'未知２':[active],'未知2':[neutral]},canonical,{'上半身2'})
assert result=={}
print('PASS: VMD full/half-width aliases, neutral placeholders, disjoint frames, deterministic conflict precedence and ignored unknown bones')
