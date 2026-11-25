#pragma once
#include "external_mesh.h"
#include <nlohmann/json.hpp>

namespace EfmiBridge {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Control,name,binding,low,high,value,integer,dirty)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Resource,name,file,format,stride)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Mesh,name,condition,section,resource,indexResource,count,start,base,values,supported,dirty,reason)
}

namespace EfmiScanCache {
namespace fs=std::filesystem;
using Json=nlohmann::json;
constexpr int Schema=1;
constexpr uintmax_t MaxBytes=16*1024*1024;

inline Json Stamp(const fs::path& file){
 std::error_code error;
 const auto state=fs::status(file,error);
 if(error&&error!=std::errc::no_such_file_or_directory&&error!=std::errc::not_a_directory)
  throw fs::filesystem_error("Could not check cached resource",file,error);
 if(!fs::is_regular_file(state))return nullptr;
 return Json::array({fs::file_size(file),fs::last_write_time(file).time_since_epoch().count()});
}

inline Json Dependencies(const EfmiBridge::Mod& mod){
 std::set<std::string> names;
 for(const auto& mesh:mod.meshes){
  names.insert(mesh.resource);
  names.insert(mesh.indexResource);
 }
 auto result=Json::object();
 for(const auto& name:names){
  const auto resource=mod.resources.find(name);
  if(resource==mod.resources.end()||resource->second.file.empty())continue;
  try{
   const auto path=ExternalMesh::ResourceFile(mod,resource->second);
   result[EfmiBridge::Utf8(path)]=Stamp(path);
  }catch(const std::runtime_error&){
   // A missing or invalid dependency is checked again on the next scan.
   result[name]=nullptr;
  }
 }
 return result;
}

class Scan {
 Json prior_=Json::object(),next_=Json::object();
 fs::path file_;
public:
 size_t hits=0,scanned=0;
 explicit Scan(fs::path file):file_(std::move(file)){
  if(file_.empty())return;
  try{
   if(!fs::is_regular_file(file_)||fs::file_size(file_)>MaxBytes)return;
   std::ifstream input(file_,std::ios::binary);
   auto document=Json::parse(input);
   if(document.value("schema",0)==Schema&&document.at("entries").is_object())
    prior_=std::move(document.at("entries"));
  }catch(const std::exception&){}
 }

 EfmiBridge::Mod ReadMod(const fs::path& file,const fs::path& root){
  const auto key=EfmiBridge::Utf8(file),namespaceRoot=EfmiBridge::Utf8(root);
  Json stamp;
  try{
   if(!file_.empty())stamp=Stamp(file);
   const auto cached=prior_.find(key);
   if(!stamp.is_null()&&cached!=prior_.end()&&cached->at("ini")==stamp&&cached->at("root")==namespaceRoot){
    EfmiBridge::Mod mod{file,cached->at("name").get<std::string>(),cached->at("space").get<std::string>(),
     cached->at("controls").get<std::vector<EfmiBridge::Control>>(),cached->at("meshes").get<std::vector<EfmiBridge::Mesh>>(),
     cached->at("resources").get<std::map<std::string,EfmiBridge::Resource>>()};
    if(cached->at("dependencies")==Dependencies(mod)){
     next_[key]=*cached;
     ++hits;
     return mod;
    }
   }
  }catch(const std::exception&){}

  auto mod=EfmiBridge::Parse(file,EfmiBridge::ReadFile(file),root);
  Json dependencies;
  if(!stamp.is_null())dependencies=Dependencies(mod);
  for(auto& mesh:mod.meshes)ExternalMesh::Probe(mod,mesh);
  ++scanned;
  if(!stamp.is_null()){
   next_[key]={{"ini",stamp},{"root",namespaceRoot},{"dependencies",std::move(dependencies)},
    {"name",mod.name},{"space",mod.space},{"controls",mod.controls},{"meshes",mod.meshes},{"resources",mod.resources}};
  }
  return mod;
 }

 bool Changed()const{return !file_.empty()&&next_!=prior_;}
 bool Write(const fs::path& pending)const{
  auto data=Json{{"schema",Schema},{"entries",next_}}.dump();
  if(data.size()>MaxBytes)return false;
  std::ofstream output(pending,std::ios::binary|std::ios::trunc);
  output.write(data.data(),static_cast<std::streamsize>(data.size()));
  output.close();
  return bool(output);
 }
};
}
