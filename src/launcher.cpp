#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <future>
#include <vector>
#include <nlohmann/json.hpp>
#include "mod_folder.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "ui_language_preference.h"

namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
constexpr int Browse=100, Launch=101;
constexpr auto ModuleId="local.endfield.interaction";

fs::path Path(const std::string& value){return fs::path(std::u8string(value.begin(),value.end()));}
std::string Utf8(const fs::path& path){auto bytes=path.u8string();return {bytes.begin(),bytes.end()};}
std::wstring Wide(const char* message){
 int size=MultiByteToWideChar(CP_UTF8,0,message,-1,nullptr,0);
 if(size<1)return L"啟動設定無法讀取。";
 std::wstring result(size,L'\0');
 MultiByteToWideChar(CP_UTF8,0,message,-1,result.data(),size);
 result.resize(size-1);
 return result;
}

fs::path LocalData(){
 PWSTR raw=nullptr;
 if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw)))
  throw std::runtime_error("無法讀取 Windows 本機資料目錄。");
 std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> location(raw,CoTaskMemFree);
 return fs::path(location.get());
}

Json ReadJson(const fs::path& path,uintmax_t limit){
 if(fs::file_size(path)>limit)throw std::runtime_error("設定檔超過大小限制。");
 std::ifstream input(path,std::ios::binary);
 return Json::parse(input);
}

fs::path EmlData(const fs::path& local){
 const DWORD size=GetEnvironmentVariableW(L"EML_DATA_ROOT",nullptr,0);
 if(!size)return local/L"EndfieldModLoader";
 std::wstring value(size,L'\0');
 const DWORD read=GetEnvironmentVariableW(L"EML_DATA_ROOT",value.data(),size);
 if(!read||read>=size)return local/L"EndfieldModLoader";
 value.resize(read);
 return fs::absolute(value);
}

bool IsGame(const fs::path& path){
 return path.is_absolute()&&_wcsicmp(path.filename().c_str(),L"Endfield.exe")==0&&fs::is_regular_file(path);
}

void SaveLauncherSettings(const fs::path& settings,const fs::path& game,bool guideSeen){
 fs::create_directories(settings.parent_path());
 auto pending=settings;
 pending+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 {
  std::ofstream out(pending,std::ios::binary|std::ios::trunc);
  out<<Json{{"game",Utf8(game)},{"appearance_guide_seen",guideSeen}}.dump(2);
  out.close();
  if(!out)throw std::runtime_error("無法儲存遊戲路徑。");
 }
 if(!MoveFileExW(pending.c_str(),settings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
  fs::remove(pending);
  throw std::runtime_error("無法儲存遊戲路徑。");
 }
}

fs::path BrowseGame(HWND owner,const fs::path& game,bool english){
 std::array<wchar_t,32768> path{};
 OPENFILENAMEW dialog{sizeof(dialog)};
 dialog.hwndOwner=owner;
 dialog.lpstrTitle=english?L"Select Endfield.exe":L"選擇遊戲中的 Endfield.exe";
 dialog.lpstrFilter=L"Endfield.exe\0Endfield.exe\0\0";
 const auto initial=game.parent_path();
 dialog.lpstrInitialDir=initial.empty()?nullptr:initial.c_str();
 dialog.lpstrFile=path.data();
 dialog.nMaxFile=static_cast<DWORD>(path.size());
 dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(GetOpenFileNameW(&dialog))return fs::path(path.data());
 if(CommDlgExtendedError())throw std::runtime_error(english?"Could not open the file picker.":"無法開啟選擇遊戲視窗。");
 return {};
}

#include "standalone_launch.inc"

struct Launcher {
 fs::path settings,languagePath,game,emlGame,official;
 bool english=false,guideSeen=false;
 std::future<Json> launch;

 explicit Launcher(const fs::path& local):settings(local/L"BetterEndfield"/L"Interaction"/L"launcher.json"),
     languagePath(settings.parent_path()/L"ui-language.txt"){
  try{
   const auto eml=ReadJson(EmlData(local)/L"launcher.json",16384);
   emlGame=Path(eml.value("game_executable_path",""));
   official=Path(eml.value("official_parent",""));
  }catch(const std::exception&){}
  try{const auto data=ReadJson(settings,16384);game=Path(data.at("game").get<std::string>());guideSeen=data.value("appearance_guide_seen",false);}
  catch(const std::exception&){game=emlGame;}
  english=UiLanguagePreference::Load(languagePath).value_or(false);
 }


};
#include "launcher_window.inc"
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
 try{
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  Launcher launcher(LocalData());
  return ShowLauncher(instance,launcher);
 }catch(const std::exception& error){
  MessageBoxW(nullptr,Wide(error.what()).c_str(),L"EndfieldAnimationImporter",MB_OK|MB_ICONERROR);
  return 1;
 }
}
