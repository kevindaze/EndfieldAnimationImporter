#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <BetterEndfield/ThirdPartyModule.h>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>

extern "C" __declspec(dllimport) const BE_ThirdPartyModuleV1* BE_CALL BetterEndfield_GetThirdPartyModuleV1();

namespace {
using Json=nlohmann::json;
namespace fs=std::filesystem;
constexpr char ModuleId[]="local.endfield.interaction";
constexpr size_t MaxRequestBytes=1024*1024;

class OfflineHost {
 const BE_ThirdPartyModuleV1* module_=nullptr;
 BE_ThirdPartyHostV1 host_{};
 std::string packageRoot_;
 std::mutex outputMutex_;
 bool initialized_=false,replied_=false;

 void Write(const Json& data){
  std::lock_guard lock(outputMutex_);
  std::cout<<data.dump()<<'\n'<<std::flush;
 }

 static void BE_CALL Log(void* context,const char* message){
  auto& host=*static_cast<OfflineHost*>(context);
  std::lock_guard lock(host.outputMutex_);
  std::cerr<<ModuleId<<": "<<(message?message:"")<<'\n';
 }

 static BE_Result BE_CALL Reply(void* context,const char* request,BE_Result result,const char* body){
  auto& host=*static_cast<OfflineHost*>(context);
  try{
   host.Write({{"kind","reply"},{"request_id",request?request:""},{"result",result},{"body",Json::parse(body?body:"{}")}});
   host.replied_=true;
   return BE_Result_Ok;
  }catch(...){return BE_Result_InvalidArgument;}
 }

 static BE_Result BE_CALL Emit(void* context,const char* body){
  try{
   static_cast<OfflineHost*>(context)->Write({{"kind","event"},{"body",Json::parse(body?body:"{}")}});
   return BE_Result_Ok;
  }catch(...){return BE_Result_InvalidArgument;}
 }

public:
 explicit OfflineHost(const fs::path& root){
  const auto bytes=root.u8string();
  packageRoot_.assign(bytes.begin(),bytes.end());
  module_=BetterEndfield_GetThirdPartyModuleV1();
  if(!module_||module_->struct_size<sizeof(*module_)||module_->version!=BETTER_ENDFIELD_THIRD_PARTY_ABI_V1||
     !module_->id||std::string_view(module_->id)!=ModuleId||
     !module_->initialize||!module_->on_message||!module_->shutdown)
   throw std::runtime_error("EAI module contract mismatch");
  host_.struct_size=sizeof(host_);
  host_.version=BETTER_ENDFIELD_THIRD_PARTY_ABI_V1;
  host_.module_id=ModuleId;
  host_.platform="eai-offline";
  host_.context=this;
  host_.package_root=packageRoot_.c_str();
  host_.log=Log;
  host_.reply=Reply;
  host_.emit=Emit;
  if(module_->initialize(&host_,"{}")!=BE_Result_Ok)
   throw std::runtime_error("EAI initialization failed");
  initialized_=true;
  Write({{"kind","ready"},{"module_id",ModuleId},{"game_runtime",false}});
 }

 ~OfflineHost(){if(initialized_)module_->shutdown();}

 void Dispatch(const std::string& line){
  std::string id;
  try{
   if(line.size()>MaxRequestBytes)throw std::runtime_error("Request exceeds 1 MiB");
   const auto request=Json::parse(line);
   id=request.at("request_id").get<std::string>();
   if(id.empty()||id.size()>128)throw std::runtime_error("Invalid request ID");
   const auto& body=request.at("body");
   if(!body.is_object()||!body.contains("command")||!body.at("command").is_string())
    throw std::runtime_error("Missing EAI command");
   const auto payload=body.dump();
   replied_=false;
   const auto result=module_->on_message(id.c_str(),payload.c_str());
   if(!replied_)Write({{"kind","reply"},{"request_id",id},{"result",result},{"body",Json::object()}});
  }catch(const std::exception& error){
   Write({{"kind","reply"},{"request_id",id},{"result",BE_Result_InvalidArgument},{"body",{{"detail",error.what()}}}});
  }
 }
};
}

int wmain(int argc,wchar_t** argv){
 if(argc!=3||std::wstring_view(argv[1])!=L"--profile"||!*argv[2]){
  std::cerr<<"Usage: EAI.OfflineHost.exe --profile <directory>\n";
  return 2;
 }
 try{
  const auto profile=fs::absolute(argv[2])/"EAI-Offline";
  fs::create_directories(profile);
  if(!SetEnvironmentVariableW(L"LOCALAPPDATA",profile.c_str()))
   throw std::runtime_error("Cannot set the isolated profile");
  std::wstring executable(32768,L'\0');
  const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
  if(!length||length>=executable.size())throw std::runtime_error("Cannot locate the EAI package");
  executable.resize(length);
  const auto native=fs::path(executable).parent_path();
  OfflineHost host(native.parent_path().parent_path());
  std::string line;
  while(std::getline(std::cin,line)){
   if(line=="stop")break;
   if(!line.empty())host.Dispatch(line);
  }
  return 0;
 }catch(const std::exception& error){
  std::cerr<<error.what()<<'\n';
  return 1;
 }
}
