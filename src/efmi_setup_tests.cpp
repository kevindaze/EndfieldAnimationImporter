#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../third_party/eml/src/native/efmi_loader.h"
#include "mod_folder.h"
#include <iostream>
int wmain(int argc,wchar_t** argv){
 try{
  if(argc!=2)return 2;
  const std::filesystem::path root(argv[1]);
  const auto mods=AppearanceMods::Prepare(root);
  if(AppearanceMods::Count(mods)!=1)throw std::runtime_error("Expected one active INI fixture");
  bool rejected=false;
  try{EfmiLoader invalid(root/L"missing");}catch(const std::runtime_error&){rejected=true;}
  if(!rejected)throw std::runtime_error("Incomplete runtime accepted");
  for(int i=0;i<2;++i){
   EfmiLoader loader(root/L"runtime/efmi");
   if(!loader.Enabled()||loader.LoadInto(0)==0)throw std::runtime_error("Invalid PID accepted by direct loader");
  }
  std::cout<<"PASS: EFMI direct loader ABI, invalid PID rejection, setup/teardown and mod discovery\n";
  return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
