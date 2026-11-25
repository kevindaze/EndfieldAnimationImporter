#pragma once
#include <filesystem>
#include <fstream>
#include <optional>
#include <windows.h>

namespace UiLanguagePreference {
inline std::optional<bool> Load(const std::filesystem::path& path){
 try{
  std::ifstream in(path);
  std::string value;
  if(in>>value){
   if(value=="en")return true;
   if(value=="zh-TW")return false;
  }
 }catch(const std::exception&){}
 return {};
}
inline bool Save(const std::filesystem::path& path,bool english){
 try{
  if(path.empty())return false;
  std::filesystem::create_directories(path.parent_path());
  auto pending=path;
  pending+=L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetCurrentThreadId())+L".tmp";
  {
   std::ofstream out(pending,std::ios::binary|std::ios::trunc);
   out<<(english?"en":"zh-TW");
   out.close();
   if(!out)return false;
  }
  if(MoveFileExW(pending.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
  std::filesystem::remove(pending);
 }catch(const std::exception&){}
 return false;
}
}
