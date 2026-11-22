#pragma once
#include <Windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>

// XXMI's public loader ABI, also used by the upstream XXMI Launcher.
class EfmiLoader {
 HMODULE library_=nullptr;
 std::filesystem::path dll_;
 using Inject=int(__cdecl*)(DWORD,const wchar_t*,int);
 Inject inject_=nullptr;
public:
 explicit EfmiLoader(const std::filesystem::path& folder){
  if(folder.empty())return;
  dll_=std::filesystem::absolute(folder)/L"d3d11.dll";
  for(auto name:{L"3dmloader.dll",L"d3d11.dll",L"d3dcompiler_47.dll",L"d3dx.ini",L"Core/EFMI/main.ini"})
   if(!std::filesystem::is_regular_file(folder/name))throw std::runtime_error("EFMI runtime is incomplete");
  library_=LoadLibraryExW((folder/L"3dmloader.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!library_)throw std::runtime_error("Cannot load XXMI loader");
  inject_=reinterpret_cast<Inject>(GetProcAddress(library_,"Inject"));
  if(!inject_){FreeLibrary(library_);library_=nullptr;throw std::runtime_error("XXMI direct loader ABI mismatch");}
 }
 EfmiLoader(const EfmiLoader&)=delete;
 EfmiLoader& operator=(const EfmiLoader&)=delete;
 ~EfmiLoader(){if(library_)FreeLibrary(library_);}
 bool Enabled()const{return library_!=nullptr;}
 int LoadInto(DWORD pid){
  return library_?inject_(pid,dll_.c_str(),15):0;
 }
};
