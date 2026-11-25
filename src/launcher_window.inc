constexpr int OpenData=102,OpenLog=103,OpenMods=104;
constexpr COLORREF Background=RGB(15,21,29),Sidebar=RGB(22,30,40),Panel=RGB(19,27,37);
constexpr COLORREF Foreground=RGB(234,240,246),Muted=RGB(158,178,196),Accent=RGB(249,174,108);

struct LauncherWindow {
 Launcher& model;
 HWND path=nullptr,launchButton=nullptr,browseButton=nullptr,log=nullptr;
 HBRUSH background=CreateSolidBrush(Background),panel=CreateSolidBrush(Panel);
 HFONT regular=nullptr,heading=nullptr,brand=nullptr;
 int dpi=96;
 size_t modCount=0;
 std::chrono::steady_clock::time_point lastModScan{};
 std::wstring status=L"準備就緒",messages=L"選擇遊戲後，按「啟動遊戲」。",hostLog;
 explicit LauncherWindow(Launcher& value):model(value){}
 ~LauncherWindow(){DeleteObject(background);DeleteObject(panel);DeleteObject(regular);DeleteObject(heading);DeleteObject(brand);}
 int Scale(int value)const{return MulDiv(value,dpi,96);}
 void Fonts(HWND window){
  dpi=GetDpiForWindow(window);
  DeleteObject(regular);DeleteObject(heading);DeleteObject(brand);
  regular=CreateFontW(-Scale(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft JhengHei UI");
  heading=CreateFontW(-Scale(28),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft JhengHei UI");
  brand=CreateFontW(-Scale(46),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
  for(auto control:{path,launchButton,browseButton,log})if(control)SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(regular),TRUE);
 }
 HWND Button(HWND parent,const wchar_t* label,int id){
  return CreateWindowW(L"BUTTON",label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,0,0,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
 }
 void Layout(HWND window){
  RECT r{};GetClientRect(window,&r);
  const int x=Scale(252),right=r.right-Scale(28),width=right-x;
  MoveWindow(path,x,Scale(148),width-Scale(116),Scale(38),TRUE);
  MoveWindow(browseButton,right-Scale(104),Scale(148),Scale(104),Scale(38),TRUE);
  MoveWindow(launchButton,x,Scale(210),Scale(142),Scale(42),TRUE);
  MoveWindow(GetDlgItem(window,OpenData),Scale(20),Scale(238),Scale(180),Scale(38),TRUE);
  MoveWindow(GetDlgItem(window,OpenLog),Scale(20),Scale(286),Scale(180),Scale(38),TRUE);
  MoveWindow(GetDlgItem(window,OpenMods),Scale(20),Scale(334),Scale(180),Scale(38),TRUE);
  const int logTop=std::max(Scale(390),int(r.bottom)-Scale(225));
  MoveWindow(log,x+Scale(16),logTop+Scale(43),width-Scale(32),std::max(Scale(70),int(r.bottom)-logTop-Scale(80)),TRUE);
  InvalidateRect(window,nullptr,TRUE);
 }
 void Text(HDC dc,const std::wstring& text,int x,int y,int width,int height,HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_WORDBREAK){
  SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
  RECT rect{Scale(x),Scale(y),Scale(x+width),Scale(y+height)};
  DrawTextW(dc,text.c_str(),-1,&rect,flags);
 }
 void Paint(HWND window){
  PAINTSTRUCT ps{};auto dc=BeginPaint(window,&ps);
  RECT rect{};GetClientRect(window,&rect);FillRect(dc,&rect,background);
  const int width=MulDiv(rect.right,96,dpi),height=MulDiv(rect.bottom,96,dpi);
  auto side=CreateSolidBrush(Sidebar);RECT sideRect{0,0,Scale(220),rect.bottom};FillRect(dc,&sideRect,side);DeleteObject(side);
  Text(dc,L"EAI",20,42,180,70,brand,Accent);
  Text(dc,L"EndfieldAnimation\nImporter",20,119,185,52,regular,Foreground);
  auto selected=CreateSolidBrush(RGB(43,55,70));RECT nav{Scale(20),Scale(182),Scale(200),Scale(220)};
  FillRect(dc,&nav,selected);DeleteObject(selected);
  Text(dc,L"遊戲啟動",34,191,155,24,regular,Accent);
  Text(dc,L"版本 " EAI_VERSION_W,20,height-64,180,24,regular,Muted);
  Text(dc,L"Windows x64",20,height-40,180,24,regular,Muted);
  Text(dc,L"啟動遊戲",252,30,width-390,40,heading,Foreground);
  Text(dc,L"EndfieldAnimationImporter  ·  " EAI_VERSION_W,252,79,width-300,24,regular,Muted);
  Text(dc,L"遊戲路徑",252,120,300,24,regular,Foreground);
  Text(dc,status,width-190,40,160,40,regular,model.launch.valid()?Accent:Muted,DT_RIGHT|DT_WORDBREAK);
  Text(dc,L"進入遊戲後按 Num0 開啟 EAI 面板，再連接遊戲入口。",252,280,width-290,56,regular,Muted);
  Text(dc,L"外型 Mod  ·  "+std::to_wstring(modCount)+L" 個 INI 檔案",252,340,width-290,32,regular,Muted);
  const int logTop=std::max(390,height-225);
  RECT logRect{Scale(252),Scale(logTop),rect.right-Scale(28),rect.bottom-Scale(28)};
  FillRect(dc,&logRect,panel);
  Text(dc,L"啟動與宿主日誌",268,logTop+14,width-310,24,regular,Muted);
  EndPaint(window,&ps);
 }
 void RefreshLog(){
  std::wstring combined=messages;
  try{
   static const auto directory=StandaloneFolder(LocalData(),EAI_VERSION_W,PackageRoot());
   for(auto name:{L"launch.log",L"host.log"}){
   const auto file=directory/name;
   if(fs::is_regular_file(file)){
    std::ifstream input(file,std::ios::binary);
    const auto size=fs::file_size(file);
    if(size>32768)input.seekg(static_cast<std::streamoff>(size-32768));
    std::string text{std::istreambuf_iterator<char>(input),{}};
    if(size>32768){auto newline=text.find('\n');if(newline!=text.npos)text.erase(0,newline+1);}
    combined+=L"\r\n\r\n"+std::wstring(name)+L"\r\n"+Wide(text.c_str());
   }
   }
  }catch(const std::exception&){}
  if(combined!=hostLog){hostLog=std::move(combined);SetWindowTextW(log,hostLog.c_str());SendMessageW(log,EM_SETSEL,hostLog.size(),hostLog.size());SendMessageW(log,EM_SCROLLCARET,0,0);}
 }
 void Command(HWND window,int id){
  if(model.launch.valid())return;
  if(id==Browse){
   auto selected=BrowseGame(window,model.game);
   if(!selected.empty()){
    if(!IsGame(selected))throw std::runtime_error("請選擇遊戲目錄中的 Endfield.exe。");
    SaveGame(model.settings,selected);model.game=selected;SetWindowTextW(path,selected.c_str());
    messages=L"已儲存遊戲路徑。";status=L"準備就緒";
   }
  }else if(id==Launch){
   if(!IsGame(model.game))throw std::runtime_error("請先選擇有效的 Endfield.exe。");
   const auto selected=model.game,parent=model.game==model.emlGame?model.official:fs::path{};
   model.launch=std::async(std::launch::async,[window,selected,parent]{return RunStandalone(window,selected,parent);});
   EnableWindow(launchButton,FALSE);EnableWindow(browseButton,FALSE);
   status=L"啟動中";messages=L"正在啟動遊戲與 EAI 宿主…";
  }else if(id==OpenMods){
   const auto folder=AppearanceMods::Prepare(PackageRoot());
   if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
    throw std::runtime_error("Windows 無法開啟 mod 資料夾。");
  }else if(id==OpenData||id==OpenLog){
   auto directory=LocalData()/L"BetterEndfield/Interaction";
   if(id==OpenLog)directory=StandaloneFolder(LocalData(),EAI_VERSION_W,PackageRoot());
   fs::create_directories(directory);
   if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
    throw std::runtime_error("Windows 無法開啟資料夾。");
  }
  RefreshLog();InvalidateRect(window,nullptr,FALSE);
 }
 void Tick(HWND window){
  if(std::chrono::steady_clock::now()-lastModScan>std::chrono::seconds(2)){
   lastModScan=std::chrono::steady_clock::now();
   const auto count=AppearanceMods::Count(PackageRoot()/L"mod");
   if(count!=modCount){modCount=count;InvalidateRect(window,nullptr,FALSE);}
  }
  if(model.launch.valid()&&model.launch.wait_for(std::chrono::seconds(0))==std::future_status::ready){
   try{
    auto result=model.launch.get();status=L"宿主已載入";
    messages=L"遊戲已啟動，EAI 宿主已載入。PID "+std::to_wstring(result.at("process_id").get<unsigned>());
   }catch(const std::exception& error){status=L"啟動失敗";messages=Wide(error.what());}
   EnableWindow(launchButton,TRUE);EnableWindow(browseButton,TRUE);InvalidateRect(window,nullptr,FALSE);
  }
  RefreshLog();
 }
};

LRESULT CALLBACK LauncherProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
 auto state=reinterpret_cast<LauncherWindow*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){
  state=static_cast<LauncherWindow*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
  SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
 }
 if(!state)return DefWindowProcW(window,message,wparam,lparam);
 try{
  switch(message){
   case WM_CREATE:
    state->path=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",state->model.game.c_str(),WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_READONLY|ES_AUTOHSCROLL,0,0,0,0,window,nullptr,nullptr,nullptr);
    state->browseButton=state->Button(window,L"選擇遊戲…",Browse);
    state->launchButton=state->Button(window,L"啟動遊戲",Launch);
    state->Button(window,L"開啟資料目錄",OpenData);state->Button(window,L"開啟日誌目錄",OpenLog);
    state->Button(window,L"開啟 mod 資料夾",OpenMods);
    AppearanceMods::Prepare(PackageRoot());
    state->log=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL,0,0,0,0,window,nullptr,nullptr,nullptr);
    state->Fonts(window);state->Layout(window);SetTimer(window,1,300,nullptr);return 0;
   case WM_SIZE:state->Layout(window);return 0;
   case WM_DPICHANGED:{
    auto r=reinterpret_cast<RECT*>(lparam);state->Fonts(window);
    SetWindowPos(window,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;
   }
   case WM_GETMINMAXINFO:{
    auto info=reinterpret_cast<MINMAXINFO*>(lparam);
    info->ptMinTrackSize={state->Scale(850),state->Scale(650)};return 0;
   }
   case WM_PAINT:state->Paint(window);return 0;
   case WM_ERASEBKGND:return 1;
   case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:{
    auto dc=reinterpret_cast<HDC>(wparam);SetTextColor(dc,Foreground);SetBkColor(dc,Panel);return reinterpret_cast<LRESULT>(state->panel);
   }
   case WM_DRAWITEM:{
    const auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
    bool primary=item->CtlID==Launch,disabled=(item->itemState&ODS_DISABLED)!=0;
    COLORREF color=disabled?RGB(40,46,54):primary?Accent:RGB(43,55,70);
    if((item->itemState&ODS_SELECTED)&&!disabled)color=primary?RGB(227,145,77):RGB(60,75,91);
    auto brush=CreateSolidBrush(color);FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);
    wchar_t text[128]{};GetWindowTextW(item->hwndItem,text,128);
    SelectObject(item->hDC,state->regular);SetBkMode(item->hDC,TRANSPARENT);
    SetTextColor(item->hDC,disabled?Muted:primary?Background:Foreground);
    RECT rect=item->rcItem;DrawTextW(item->hDC,text,-1,&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(item->itemState&ODS_FOCUS){InflateRect(&rect,-4,-4);DrawFocusRect(item->hDC,&rect);}return TRUE;
   }
   case WM_COMMAND:if(HIWORD(wparam)==BN_CLICKED)state->Command(window,LOWORD(wparam));return 0;
   case WM_TIMER:state->Tick(window);return 0;
   case WM_CLOSE:
    if(state->model.launch.valid()){state->messages=L"啟動進行中，請等待載入結果。";state->RefreshLog();return 0;}
    DestroyWindow(window);return 0;
   case WM_DESTROY:KillTimer(window,1);PostQuitMessage(0);return 0;
  }
 }catch(const std::exception& error){
  state->status=L"操作失敗";state->messages=Wide(error.what());state->RefreshLog();InvalidateRect(window,nullptr,FALSE);
 }
 return DefWindowProcW(window,message,wparam,lparam);
}

int ShowLauncher(HINSTANCE instance,Launcher& launcher){
 LauncherWindow state(launcher);
 WNDCLASSW wc{};wc.lpfnWndProc=LauncherProc;wc.hInstance=instance;wc.lpszClassName=L"EAI.Standalone.Launcher";
 wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hIcon=LoadIconW(nullptr,MAKEINTRESOURCEW(32512));
 if(!RegisterClassW(&wc))throw std::runtime_error("無法註冊啟動器視窗。");
 auto window=CreateWindowW(wc.lpszClassName,L"EndfieldAnimationImporter  " EAI_VERSION_W,
                          WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1100,760,nullptr,nullptr,instance,&state);
 if(!window)throw std::runtime_error("無法建立啟動器視窗。");
 ShowWindow(window,SW_SHOW);UpdateWindow(window);
 MSG message{};
 while(GetMessageW(&message,nullptr,0,0)>0){
  if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}
 }
 return static_cast<int>(message.wParam);
}
