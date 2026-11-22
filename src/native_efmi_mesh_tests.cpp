#include "native_efmi_mesh.h"
#include <iostream>
#include <windows.h>

namespace {
void Require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}

template<class T> void Write(const std::filesystem::path& path,const std::vector<T>& data){
 std::ofstream file(path,std::ios::binary|std::ios::trunc);
 file.write(reinterpret_cast<const char*>(data.data()),data.size()*sizeof(T));
 Require(bool(file),"Cannot write fixture");
}

struct Fixture {
 std::filesystem::path folder=std::filesystem::temp_directory_path()/("eai-native-efmi-"+std::to_string(GetCurrentProcessId()));
 Fixture(){Require(std::filesystem::create_directory(folder),"Cannot create isolated fixture");}
 ~Fixture(){
  std::error_code error;
  for(auto file:{"mod.ini","profile.json","pos.buf","uv.buf","idx.buf","pos_merged_map.buf","pos_merged_packed_pos_delta.buf"})
   std::filesystem::remove(folder/file,error);
  std::filesystem::remove(folder,error);
 }
};
}

int wmain(int argc,wchar_t** argv){
 try{
  if(argc==2){
   auto geometry=NativeEfmi::ReadGeometry(NativeEfmi::ReadProfile(argv[1]));
   std::cout<<geometry.positions.size()<<" vertices, "<<geometry.triangles.size()/3<<" triangles, "
            <<geometry.deltas.size()<<" shape entries\n";
   return 0;
  }
  Fixture temp;
  std::ofstream(temp.folder/"mod.ini")<<R"ini(
[ResourceIndex]
format = DXGI_FORMAT_R16_UINT
filename = idx.buf
[ResourcePosition]
stride = 16
filename = pos.buf
[ResourceUv]
stride = 12
filename = uv.buf
[CommandList_Draw_Component0]
ib = ref ResourceIndex
vb0 = ref ResourcePosition
; Draw Component 0 piece
drawindexedinstanced = 6, INSTANCE_COUNT, 1, 1, FIRST_INSTANCE
)ini";
  std::ofstream(temp.folder/"profile.json")<<R"json({
   "format":"eai-efmi-component","version":1,"ini":"mod.ini","draw":"Component 0 / piece",
   "actor":0,"character":"chr_fixture","renderer":"body","bone":"pelvis",
   "packed_position_shape":true,"uv_resource":"ResourceUv","uv_offset":4
  })json";
  Write<float>(temp.folder/"pos.buf",{9,9,9,0, 1,0,0,0, 0,1,0,0, 0,0,1,0});
  Write<float>(temp.folder/"uv.buf",{9,0,0, 9,0,1, 9,1,0, 9,1,1});
  Write<uint16_t>(temp.folder/"idx.buf",{65535,2,0,1,2,1,0});
  Write<int32_t>(temp.folder/"pos_merged_map.buf",{-1,-1,0,-1});
  Write<float>(temp.folder/"pos_merged_packed_pos_delta.buf",{0,0,0.25f});
  auto profile=NativeEfmi::ReadProfile(temp.folder/"profile.json");
  auto mesh=NativeEfmi::ReadGeometry(profile);
  Require(mesh.positions.size()==3&&mesh.positions[0]==ExternalMesh::Vec{0,0,1},"Draw start or base vertex incorrect");
  Require(mesh.triangles==std::vector<int32_t>{0,1,2,0,2,1},"Triangle ordering or shared vertices lost");
  Require(mesh.uv==std::vector<std::array<float,2>>{{1,1},{0,1},{1,0}},"UV remapping incorrect");
  Require(mesh.deltas==std::vector<ExternalMesh::Vec>{{},{},{0,0,.25f}},"Shape remapping incorrect");
  profile.shape=false;
  profile.previewScale=ExternalMesh::Vec{1.5f,1,1};
  mesh=NativeEfmi::ReadGeometry(profile);
  Require(mesh.deltas==std::vector<ExternalMesh::Vec>{{-.25f,0,0},{.25f,0,0},{-.25f,0,0}},"Preview average-centered scale incorrect");
  Write<uint16_t>(temp.folder/"idx.buf",{0,65535,0,1,2,1,0});
  bool rejected=false;
  try{NativeEfmi::ReadGeometry(profile);}catch(const std::runtime_error&){rejected=true;}
  Require(rejected,"Out-of-range input index accepted");
  return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
