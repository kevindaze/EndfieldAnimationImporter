#pragma once
#include <filesystem>
#include <string>
#include <algorithm>
#include <stdexcept>
namespace AppearanceMods {
inline bool Disabled(std::wstring name){
 std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
 return name.starts_with(L"disabled")||name==L"desktop.ini";
}
inline size_t Count(const std::filesystem::path& folder){
 if(!std::filesystem::is_directory(folder))return 0;
 size_t files=0,inis=0;
 for(auto it=std::filesystem::recursive_directory_iterator(folder,std::filesystem::directory_options::skip_permission_denied);
     it!=std::filesystem::recursive_directory_iterator();++it){
  if(++files>50000)throw std::runtime_error("mod 資料夾超過 50,000 個檔案。");
  if(it->is_symlink()||Disabled(it->path().filename().wstring())||it.depth()>16){
   if(it->is_directory())it.disable_recursion_pending();
   continue;
  }
  auto extension=it->path().extension().wstring();
  std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return static_cast<wchar_t>(towlower(c));});
  if(it->is_regular_file()&&extension==L".ini")++inis;
 }
 return inis;
}
inline std::filesystem::path Prepare(const std::filesystem::path& root){
 const auto folder=root/L"mod";
 std::filesystem::create_directories(folder);
 if(std::filesystem::is_symlink(folder))throw std::runtime_error("mod 資料夾不可是符號連結。");
 return folder;
}
}
