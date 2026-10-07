#include "language_catalog.h"
#include <iostream>
int main(){using namespace EaiLanguage;int failures=0;
 auto check=[&](bool value){if(!value)++failures;};
 english=false;check(Translate(L"保存動畫")==L"保存動畫");
 english=true;check(Translate(L"保存動畫")==L"Save Animation");
 check(Translate(L"標記起點：83.5")==L"Mark Start: 83.5");
 check(Translate(L"☑ 區間循環")==L"☑ Loop Range");
 check(Translate(L"左膝固定")==L"Left Knee");
 check(Translate(L"Bip001_L_Forearm")==L"Bip001_L_Forearm");
 check(Translate(L"保存動畫：選取區間加入動畫庫；另存：所選區間另存 JSON。")==L"Save adds the range to the library. Save As exports the range as JSON.");
 english=false;check(Translate(L"保存動畫")==L"保存動畫");
 std::cout<<(failures?"FAIL":"PASS")<<": native language catalog\n";return failures?1:0;
}
