#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "external_mesh.h"
#include "preset_settings.h"
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace ExternalMesh {
inline void WriteBuffer(const std::filesystem::path& path,const Bytes& bytes){
 std::ofstream out(path,std::ios::binary|std::ios::trunc);
 out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
 out.close();
 if(!out)throw std::runtime_error("Cannot write generated mesh buffer");
}

inline constexpr char LiveMagic[]="EAILIVE2";
inline constexpr size_t LiveHeaderSize=sizeof(LiveMagic)-1+sizeof(FILETIME);
inline constexpr size_t LiveLimit=64*1024*1024;
inline std::filesystem::path LivePath(std::filesystem::path source){
 source+=L".eai-live-buffer";
 return source;
}
inline bool IsLiveFile(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);
 char header[sizeof(LiveMagic)-1]{};
 return in.read(header,sizeof(header))&&std::memcmp(header,LiveMagic,sizeof(header))==0;
}
inline std::filesystem::path LiveSettingsPath(const EfmiBridge::Mod& mod){
 auto name=mod.file.filename();
 name+=L".eai-live.json";
 return mod.file.parent_path()/"BEM.GeneratedMeshes"/name;
}
inline std::filesystem::path LiveTemporary(std::filesystem::path path){
 path+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 return path;
}
inline void PublishLiveFile(const std::filesystem::path& temp,const std::filesystem::path& path){
 if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))
  throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),"Cannot update live mesh");
}

class LiveBuffers {
 std::mutex mutex;
 std::condition_variable changed;
 std::jthread worker;
 std::optional<EfmiBridge::Scan> pending;
 std::string error;
 size_t revision=0;
 bool writing=false;

 void Run(std::stop_token stop){
  std::unique_lock lock(mutex);
  while(!stop.stop_requested()){
   changed.wait(lock,[&]{return stop.stop_requested()||pending.has_value();});
   if(stop.stop_requested())break;
   auto scan=std::move(*pending);
   pending.reset();
   writing=true;
   const auto current=revision;
   lock.unlock();
   std::vector<std::pair<std::filesystem::path,std::filesystem::path>> staged;
   try{
    auto buffers=Build(scan);
    for(auto& b:buffers){
     if(b.bytes.size()>LiveLimit)throw std::runtime_error("Live mesh buffer exceeds 64 MB");
     auto path=LivePath(b.source),temp=LiveTemporary(path);
     if(std::filesystem::exists(path)&&!IsLiveFile(path))
      throw std::runtime_error("Live mesh filename is occupied");
     WIN32_FILE_ATTRIBUTE_DATA source{};
     if(!GetFileAttributesExW(b.source.c_str(),GetFileExInfoStandard,&source))
      throw std::runtime_error("Cannot verify the original mesh buffer");
     staged.emplace_back(temp,path);
     std::ofstream out(temp,std::ios::binary|std::ios::trunc);
     out.write(LiveMagic,sizeof(LiveMagic)-1);
     out.write(reinterpret_cast<const char*>(&source.ftLastWriteTime),sizeof(FILETIME));
     out.write(reinterpret_cast<const char*>(b.bytes.data()),b.bytes.size());
     out.close();
     if(!out)throw std::runtime_error("Cannot write live mesh buffer");
    }
    lock.lock();
    if(current==revision&&!stop.stop_requested()){
     for(auto& [temp,path]:staged)PublishLiveFile(temp,path);
     if(!buffers.empty())SaveLiveSettings(scan);
     error.clear();
    }
   }catch(const std::exception& e){
    if(!lock.owns_lock())lock.lock();
    if(current==revision)error=e.what();
   }
   for(auto& [temp,path]:staged){std::error_code ec;std::filesystem::remove(temp,ec);}
   writing=false;
   changed.notify_all();
  }
 }

 static void SaveLiveSettings(const EfmiBridge::Scan& scan){
  for(auto& mod:scan.mods){
   auto values=Presets::Json::object();
   for(auto& mesh:mod.meshes)if(mesh.dirty)values[mesh.name]=mesh.values;
   if(values.empty())continue;
   auto path=LiveSettingsPath(mod),temp=LiveTemporary(path);
   std::filesystem::create_directories(path.parent_path());
   std::ofstream out(temp,std::ios::binary|std::ios::trunc);
   out<<Presets::Json{{"format","eai-live-mesh-settings"},{"values",values}}.dump(2);
   out.close();
   if(!out)throw std::runtime_error("Cannot save live mesh settings");
   PublishLiveFile(temp,path);
  }
 }

public:
 ~LiveBuffers(){Flush();Stop();}

 void Flush(){
  std::unique_lock lock(mutex);
  changed.wait(lock,[&]{return !writing&&!pending.has_value();});
 }

 void Clear(){
  std::lock_guard lock(mutex);
  ++revision;
  pending.reset();
  error.clear();
 }

 void Stop(){
  if(!worker.joinable())return;
  {
   std::lock_guard lock(mutex);
   worker.request_stop();
  }
  changed.notify_all();
  worker.join();
  Clear();
 }

 void Queue(const EfmiBridge::Scan& scan){
  std::lock_guard lock(mutex);
  pending=scan;
  if(!worker.joinable())worker=std::jthread([this](std::stop_token stop){Run(stop);});
  changed.notify_all();
 }

 std::string Error(){
  std::lock_guard lock(mutex);
  return error;
 }
};

inline void LoadLiveSettings(EfmiBridge::Mod& mod){
 auto path=LiveSettingsPath(mod);
 if(!std::filesystem::is_regular_file(path))return;
 auto data=Presets::Json::parse(EfmiBridge::ReadFile(path));
 if(data.value("format",std::string{})!="eai-live-mesh-settings")return;
 auto& values=data.at("values");
 for(auto& mesh:mod.meshes)if(mesh.supported&&values.contains(mesh.name)){
  auto value=values.at(mesh.name).get<std::array<float,12>>();
  if(Valid(value)){mesh.values=value;mesh.dirty=true;}
 }
}

inline void DisableLiveFiles(const EfmiBridge::Scan& scan){
 for(auto& mod:scan.mods){
  for(auto& [name,r]:mod.resources){
   if(r.file.empty())continue;
   std::filesystem::path source;
   try{source=ResourceFile(mod,r);}catch(const std::runtime_error&){continue;}
   auto path=LivePath(source);
   if(IsLiveFile(path))std::filesystem::remove(path);
  }
  auto path=LiveSettingsPath(mod);
  if(std::filesystem::is_regular_file(path)){
   auto data=Presets::Json::parse(EfmiBridge::ReadFile(path),nullptr,false);
   if(data.is_object()&&data.value("format",std::string{})=="eai-live-mesh-settings")
    std::filesystem::remove(path);
  }
 }
}
}
