// Unity Matrix4x4 stores columns consecutively. JSON uses explicit row arrays.
struct ReferenceMatrix {float m[16]{};};
Presets::Json MatrixJson(const ReferenceMatrix& matrix){
 auto result=Presets::Json::array();for(int row=0;row<4;++row){auto values=Presets::Json::array();for(int col=0;col<4;++col){float value=matrix.m[col*4+row];if(!std::isfinite(value))throw std::runtime_error("Non-finite bind matrix");values.push_back(value);}result.push_back(values);}return result;
}
void CaptureMeshBindposes(Presets::Json& data,const std::vector<Pose::FullBone>& bones){
 if(!skinnedRenderer.type_object)throw std::runtime_error("SkinnedMeshRenderer metadata unavailable");
 for(auto key:{SharedMesh,MeshBones,BindPoses,LocalToWorld,WorldToLocal})if(!methods[key].resolved.method_info)throw std::runtime_error("Bind pose contract unavailable");
 Pins pins;std::map<void*,std::string> keys;for(auto& b:bones)keys[b.bone]=b.info.key;
 data["mesh_bindposes"]=Presets::Json::array();
 for(size_t i=0;i<bones.size();++i){ReferenceMatrix world{};if(!Value(LocalToWorld,bones[i].bone,world))throw std::runtime_error("Bone world matrix unavailable");data["bones"][i]["world_matrix"]=MatrixJson(world);}

 for(int actor=0;actor<2;++actor){auto root=std::find_if(bones.begin(),bones.end(),[&](auto& b){return b.info.actor==actor&&b.info.name=="@root";});if(root==bones.end())throw std::runtime_error("Reference root missing");
  ReferenceMatrix rootInverse{};if(!Value(WorldToLocal,root->bone,rootInverse))throw std::runtime_error("Root matrix unavailable");
  // Root must be the Animator model root, so querying its owning GameObject is unnecessary:
  // traverse the already verified model hierarchy through its model GameObject.
  auto entity=pins.Keep(Call(Main));auto model=entity?pins.Keep(Call(Model,entity)):nullptr;auto go=model?pins.Keep(Call(ModelGo,model)):nullptr;
  if(actor==1){int count=0;auto manager=Squad(pins,count);go=nullptr;for(int i=0;manager&&i<count;++i){void* args[]{&i};auto member=pins.Keep(Call(Member,manager,args));auto m=member?pins.Keep(Call(Model,member)):nullptr;auto g=m?pins.Keep(Call(ModelGo,m)):nullptr;if(g&&Presets::Identity(ObjectName(g))==workflow.draft.target){if(go)throw std::runtime_error("Ambiguous partner model");go=g;}}}
  if(!Living(go))throw std::runtime_error("Reference model unavailable");bool inactive=true;void* args[]{skinnedRenderer.type_object,&inactive};auto renderers=pins.Keep(Call(Components,go,args));int dim=0,count=0;void* lengthArgs[]{&dim};if(!renderers||!Value(Length,renderers,count,lengthArgs)||count<1||count>256)throw std::runtime_error("Skinned renderers unavailable");
  for(int i=0;i<count;++i){void* indexArgs[]{&i};auto renderer=pins.Keep(Call(Item,renderers,indexArgs));auto mesh=renderer?pins.Keep(Call(SharedMesh,renderer)):nullptr;auto palette=renderer?pins.Keep(Call(MeshBones,renderer)):nullptr;auto bind=mesh?pins.Keep(Call(BindPoses,mesh)):nullptr;if(!palette||!bind)continue;
   int n=0,m=0;if(!Value(Length,palette,n,lengthArgs)||!Value(Length,bind,m,lengthArgs)||n!=m||n<1||n>1200)continue;
   auto transform=pins.Keep(Call(ComponentTransform,renderer));ReferenceMatrix rendererWorld{};if(!transform||!Value(LocalToWorld,transform,rendererWorld))continue;
   auto record=Presets::Json{{"actor",actor},{"renderer",ObjectName(renderer)},{"root_world_to_local",MatrixJson(rootInverse)},{"renderer_local_to_world",MatrixJson(rendererWorld)},{"bones",Presets::Json::array()}};
   for(int j=0;j<n;++j){void* a[]{&j};auto bone=pins.Keep(Call(Item,palette,a));auto found=keys.find(bone);if(found==keys.end())continue;ReferenceMatrix matrix{};if(!Value(Item,bind,matrix,a))throw std::runtime_error("Bind matrix unavailable");record["bones"].push_back({{"key",found->second},{"name",ObjectName(bone)},{"inverse_bind_matrix",MatrixJson(matrix)}});}
   if(!record["bones"].empty())data["mesh_bindposes"].push_back(record);
  }
 }
 if(data["mesh_bindposes"].empty())throw std::runtime_error("No mesh bind pose palettes");
 data["bind_pose_source"]="Unity.Mesh.bindposes";
}
