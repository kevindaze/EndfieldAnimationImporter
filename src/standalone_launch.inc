// Uses the bundled EML launcher and host under the upstream AGPL-3.0 license.
std::wstring Quote(const fs::path& path){
 // Windows paths cannot contain quotes. Double trailing backslashes before the closing quote.
 auto value=path.wstring();
 if(value.find(L'"')!=value.npos)throw std::runtime_error("路徑含有無效字元。");
 size_t trailing=0;
 for(auto it=value.rbegin();it!=value.rend()&&*it==L'\\';++it)++trailing;
 return L"\""+value+std::wstring(trailing,L'\\')+L"\"";
}

fs::path PackageRoot(){
 std::array<wchar_t,32768> path{};
 auto length=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
 if(!length||length>=path.size())throw std::runtime_error("無法讀取 EAI 程式路徑。");
 return fs::canonical(fs::path(path.data()).parent_path());
}

fs::path StandaloneFolder(const fs::path& local,const fs::path& version,const fs::path& root){
 const auto& path=root.native();
 std::array<UCHAR,32> digest{};
 BCRYPT_ALG_HANDLE algorithm=nullptr;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
  throw std::runtime_error("無法初始化獨立宿主索引。");
 const auto status=BCryptHash(algorithm,nullptr,0,
  reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(path.data())),
  static_cast<ULONG>(path.size()*sizeof(wchar_t)),digest.data(),static_cast<ULONG>(digest.size()));
 BCryptCloseAlgorithmProvider(algorithm,0);
 if(status<0)throw std::runtime_error("無法辨識獨立宿主索引位置。");
 constexpr wchar_t digits[]=L"0123456789abcdef";
 std::wstring key;
 for(auto byte:digest){key+=digits[byte>>4];key+=digits[byte&15];}
 return local/L"BetterEndfield/Interaction/Standalone"/version/key;
}

std::string RandomHex(size_t count){
 std::vector<unsigned char> bytes(count);
 if(BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(count),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
  throw std::runtime_error("無法產生宿主連線憑證。");
 std::string result;
 constexpr char digits[]="0123456789abcdef";
 for(auto byte:bytes){result+=digits[byte>>4];result+=digits[byte&15];}
 return result;
}

fs::path PrepareStandalone(const fs::path& game,const fs::path& root,const fs::path& local){
 if(!IsGame(game))throw std::runtime_error("請選擇遊戲目錄中的 Endfield.exe。");
 const auto manifest=ReadJson(root/L"module.json",1048576);
 if(manifest.at("id")!=ModuleId)throw std::runtime_error("EAI 套件識別不符。");
 for(const auto& name:{L"runtime/EModLoader.Launcher.exe",L"runtime/EModLoader.Host.dll",L"native/windows-x64/Endfield.Interaction.dll"})
  if(!fs::is_regular_file(root/name))throw std::runtime_error("EAI 套件不完整，請重新完整解壓。");
 const auto folder=StandaloneFolder(local,Path(manifest.at("version").get<std::string>()),root);
 fs::create_directories(folder);
 const auto indexPath=folder/L"index.json";
 // An older EML proxy would direct the game into a different module library.
 const auto receipt=game.parent_path()/L"eml-xinput.json";
 if(fs::exists(receipt)&&fs::exists(game.parent_path()/L"xinput1_4.dll")){
  const auto installed=ReadJson(receipt,16384);
  if(installed.value("product","")=="EndfieldModLoader"&&
     Path(installed.at("index_path").get<std::string>()).lexically_normal()!=indexPath.lexically_normal())
   throw std::runtime_error("此遊戲已安裝舊 EML 自啟動。請先在 EML 卸載 DLL 自啟動，再使用獨立 EAI。");
 }
 Json index;
 if(fs::exists(indexPath)){
  index=ReadJson(indexPath,1048576);
  if(index.at("schema")!=1||!index.at("modules").is_array()||index.at("modules").size()!=1||
     index.at("modules").at(0).at("id")!=ModuleId)
   throw std::runtime_error("EAI 專用宿主設定無效。");
  if(index["modules"][0].at("directory")!=Utf8(root))
   throw std::runtime_error("EAI 專用宿主索引的套件路徑不符。");
  return indexPath;
 }
 WSADATA winsock{};
 if(WSAStartup(MAKEWORD(2,2),&winsock))throw std::runtime_error("無法初始化本機宿主連線。");
 auto socketHandle=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 int addressLength=sizeof(address);
 const bool ok=socketHandle!=INVALID_SOCKET&&bind(socketHandle,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0&&
               getsockname(socketHandle,reinterpret_cast<sockaddr*>(&address),&addressLength)==0;
 if(socketHandle!=INVALID_SOCKET)closesocket(socketHandle);
 WSACleanup();
 if(!ok)throw std::runtime_error("無法配置本機宿主連線。");
 index={{"schema",1},{"port",ntohs(address.sin_port)},{"token",RandomHex(32)},
        {"modules",Json::array({{{"id",ModuleId},{"directory",Utf8(root)},{"generation",manifest.at("version")},
          {"enabled",true},{"configuration",Json::object()}}})}};
 // Publish exclusively so simultaneous launcher instances cannot overwrite credentials.
 auto serialized=index.dump(2);
 auto pending=indexPath;pending+=L"."+std::to_wstring(GetCurrentProcessId())+L".pending";
 HANDLE file=CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("EAI 設定已由另一個啟動器建立，請重試。");
 DWORD written=0;
 const bool saved=WriteFile(file,serialized.data(),static_cast<DWORD>(serialized.size()),&written,nullptr)&&written==serialized.size()&&FlushFileBuffers(file);
 CloseHandle(file);
 if(!saved){DeleteFileW(pending.c_str());throw std::runtime_error("無法儲存獨立宿主設定。");}
 if(!MoveFileExW(pending.c_str(),indexPath.c_str(),MOVEFILE_WRITE_THROUGH)){
  DeleteFileW(pending.c_str());throw std::runtime_error("另一個啟動器已建立設定，請重試。");
 }
 return indexPath;
}

class LaunchHandle {
 HANDLE handle_=INVALID_HANDLE_VALUE;
public:
 explicit LaunchHandle(HANDLE handle):handle_(handle){}
 ~LaunchHandle(){if(handle_&&handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
 HANDLE Get()const{return handle_;}
};

Json RunStandalone(HWND owner,const fs::path& game,const fs::path& official,bool elevated=false){
 const auto root=PackageRoot(),index=PrepareStandalone(game,root,LocalData());
 const auto pipeName=L"EML.Launch."+Path(RandomHex(16)).wstring();
 const auto pipePath=L"\\\\.\\pipe\\"+pipeName;
 LaunchHandle pipe(CreateNamedPipeW(pipePath.c_str(),PIPE_ACCESS_INBOUND|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
                     PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,16384,16384,0,nullptr));
 if(pipe.Get()==INVALID_HANDLE_VALUE)throw std::runtime_error("無法建立啟動結果通道。");
 LaunchHandle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));
 if(!event.Get())throw std::runtime_error("無法建立啟動通道事件。");
 OVERLAPPED operation{};operation.hEvent=event.Get();
 if(!ConnectNamedPipe(pipe.Get(),&operation)&&GetLastError()!=ERROR_IO_PENDING)
  throw std::runtime_error("無法準備啟動結果通道。");
 auto arguments=L"--game "+Quote(game)+L" --index "+Quote(index)+L" --host "+Quote(root/L"runtime/EModLoader.Host.dll")+
                L" --result-pipe "+pipeName+L" --allow-xinput-conflict true";
 const auto mods=AppearanceMods::Prepare(root);
 if(AppearanceMods::Count(mods)>0){
  const auto efmi=root/L"runtime/efmi";
  for(auto name:{L"3dmloader.dll",L"d3d11.dll",L"d3dcompiler_47.dll",L"d3dx.ini",L"Core/EFMI/main.ini"})
   if(!fs::is_regular_file(efmi/name))throw std::runtime_error("EFMI 外型載入元件不完整，請重新完整解壓。");
  fs::create_directories(efmi/L"ShaderCache");
  fs::create_directories(efmi/L"ShaderFixes");
  arguments+=L" --efmi "+Quote(efmi);
 }
 if(official.is_absolute()&&fs::is_regular_file(official))arguments+=L" --official-parent "+Quote(official);
 const auto helper=root/L"runtime/EModLoader.Launcher.exe";
 SHELLEXECUTEINFOW start{sizeof(start)};
 start.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;start.hwnd=owner;start.lpVerb=elevated?L"runas":L"open";
 start.lpFile=helper.c_str();start.lpParameters=arguments.c_str();start.nShow=SW_HIDE;
 const auto cancelConnection=[&](){
  CancelIoEx(pipe.Get(),&operation);
  DWORD ignored=0;GetOverlappedResult(pipe.Get(),&operation,&ignored,TRUE);
 };
 if(!ShellExecuteExW(&start)){cancelConnection();throw std::runtime_error("啟動已取消或 Windows 無法啟動載入元件。");}
 LaunchHandle process(start.hProcess);
 HANDLE waits[]{event.Get(),process.Get()};
 auto wait=WaitForMultipleObjects(2,waits,FALSE,90000);
 if(wait!=WAIT_OBJECT_0){cancelConnection();throw std::runtime_error("啟動元件尚未回傳結果，請查看遊戲狀態與宿主日誌。");}
 DWORD connected=0;
 if(!GetOverlappedResult(pipe.Get(),&operation,&connected,FALSE))throw std::runtime_error("啟動通道連線失敗。");
 ULONG client=0;
 if(!GetNamedPipeClientProcessId(pipe.Get(),&client)||client!=GetProcessId(process.Get()))
  throw std::runtime_error("啟動結果的程序身份不符。");
 auto read=[&](void* output,DWORD size){
  auto bytes=static_cast<unsigned char*>(output);
  while(size){
   ResetEvent(event.Get());OVERLAPPED readOperation{};readOperation.hEvent=event.Get();DWORD received=0;
   bool ready=ReadFile(pipe.Get(),bytes,size,&received,&readOperation)!=FALSE;
   if(!ready&&GetLastError()==ERROR_IO_PENDING){
    if(WaitForSingleObject(event.Get(),10000)!=WAIT_OBJECT_0){CancelIoEx(pipe.Get(),&readOperation);WaitForSingleObject(event.Get(),INFINITE);throw std::runtime_error("讀取啟動結果逾時。");}
    ready=GetOverlappedResult(pipe.Get(),&readOperation,&received,FALSE)!=FALSE;
   }
   if(!ready||!received)throw std::runtime_error("啟動元件中斷連線。");
   bytes+=received;size-=received;
  }
 };
 uint32_t size=0;read(&size,sizeof(size));
 if(!size||size>16384)throw std::runtime_error("啟動結果超過大小限制。");
 std::string report(size,'\0');read(report.data(),size);
 auto result=Json::parse(report);
 if(result.at("protocol")=="eml.launch.v1"&&result.at("win32_error")==ERROR_ELEVATION_REQUIRED&&!elevated)
  return RunStandalone(owner,game,official,true);
 if(result.at("protocol")!="eml.launch.v1"||result.at("exit_code")!=0)
  throw std::runtime_error(result.value("error","遊戲啟動失敗。"));
 return result;
}
