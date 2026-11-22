#include <Windows.h>
#include <fstream>
#include <filesystem>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int) {
    wchar_t loadForeign[2]{};
    if(GetEnvironmentVariableW(L"EML_TEST_LOAD_FOREIGN_XINPUT",loadForeign,2)) {
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        auto* last=wcsrchr(executable,L'\\');if(!last)return 3;last[1]=0;
        if(wcscat_s(executable,L"xinput1_4.dll"))return 4;
        HMODULE foreign=LoadLibraryW(executable);
        auto marker=foreign?reinterpret_cast<int(*)()>(GetProcAddress(foreign,"EMLForeignXInputMarker")):nullptr;
        if(!marker||marker()!=314)return 5;
    }
    wchar_t name[256]{};if(!GetEnvironmentVariableW(L"EML_TEST_STOP",name,256))return 1;
    HANDLE event=CreateEventW(nullptr,TRUE,FALSE,name);if(!event)return 2;
    wchar_t ready[32768]{};
    if(GetEnvironmentVariableW(L"EML_TEST_READY",ready,32768)){
        std::ofstream out{std::filesystem::path(ready)};
        out<<GetCurrentProcessId()<<'\n'<<(wcsstr(GetCommandLineW(),L"-force-d3d11")?1:0);
    }
    WaitForSingleObject(event,60000);CloseHandle(event);return 0;
}
