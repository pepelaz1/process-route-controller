#include "core.hpp"
#include <windows.h>
#include <windowsx.h>
#include <fstream>
#include <filesystem>
#include <memory>
#include <string>

namespace {
auto world=std::make_unique<route::World>(route::initial());
route::Root root;
bool frozen=false;
constexpr int tile=34,left=28,top=110;
template<class F> void change(F f) {++world->sequence;f();++world->sequence;}
void draw(HDC dc) {
    RECT all{0,0,740,590};auto bg=CreateSolidBrush(RGB(16,27,50));FillRect(dc,&all,bg);DeleteObject(bg);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(233,242,255));
    auto font=CreateFontW(21,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Segoe UI");
    auto old=SelectObject(dc,font);
    std::wstring title=L"PROCESS ROUTE CONTROLLER  /  WORLD";
    TextOutW(dc,28,20,title.c_str(),static_cast<int>(title.size()));
    std::wstring status=L"Health: "+std::to_wstring(world->health)+L"%    Buffer: "+std::to_wstring(world->inventory)+L"/"+std::to_wstring(world->capacity)+L"    Moves: "+std::to_wstring(world->moves);
    TextOutW(dc,28,57,status.c_str(),static_cast<int>(status.size()));
    for(int y=0;y<route::height;++y)for(int x=0;x<route::width;++x) {
        bool wall=world->blocked[route::index({x,y})]!=0;
        RECT cell{left+x*tile,top+y*tile,left+x*tile+tile-3,top+y*tile+tile-3};
        COLORREF c=wall?RGB(83,100,127):RGB(29,48,78);
        if(x==18&&y==10)c=RGB(225,171,61);
        if(x==1&&y==1)c=RGB(47,144,124);
        if(x==world->x&&y==world->y)c=RGB(79,216,233);
        auto brush=CreateSolidBrush(c);FillRect(dc,&cell,brush);DeleteObject(brush);
    }
    std::wstring keys=L"H: health  R: relocate  Space: fill buffer  S: freeze  B: obstacle";
    TextOutW(dc,28,535,keys.c_str(),static_cast<int>(keys.size()));
    SelectObject(dc,old);DeleteObject(font);
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_PAINT: {PAINTSTRUCT ps;auto dc=BeginPaint(hwnd,&ps);draw(dc);EndPaint(hwnd,&ps);return 0;}
    case WM_KEYDOWN:
        if(wp=='R') {
            auto replacement=std::make_unique<route::World>(*world);
            ++replacement->relocations;root.world=reinterpret_cast<std::uintptr_t>(replacement.get());
            world=std::move(replacement);
        } else change([&]{
            if(wp=='H')world->health=world->health<35?100:34;
            if(wp==VK_SPACE)world->inventory=world->capacity;
            if(wp=='S')frozen=!frozen;
            if(wp=='B')world->blocked[route::index({2,1})]^=1;
        });
        InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN: {
        int px=GET_X_LPARAM(lp)-left,py=GET_Y_LPARAM(lp)-top;
        if(px<0||py<0)return 0;
        route::Point p{px/tile,py/tile};
        if(!route::inside(p))return 0;
        change([&]{
            if((wp&MK_CONTROL)&&p==route::Point{1,1}&&world->x==1&&world->y==1) {
                world->inventory=0;++world->unloads;
            } else if(!frozen&&route::walkable(*world,p)&&route::distance(p,{world->x,world->y})==1) {
                world->x=p.x;world->y=p.y;++world->moves;
            }
        });
        InvalidateRect(hwnd,nullptr,FALSE);return 0;
    }
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show) {
    root.world=reinterpret_cast<std::uintptr_t>(world.get());
    WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=proc;cls.lpszClassName=L"ProcessRouteWorld";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);
    if(!RegisterClassW(&cls))return 1;
    auto hwnd=CreateWindowW(cls.lpszClassName,L"Process Route World",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,750,620,nullptr,nullptr,instance,nullptr);
    if(!hwnd)return 2;
    ShowWindow(hwnd,show);UpdateWindow(hwnd);
    std::wstring filename=command;
    if(filename.size()>=2&&filename.front()==L'"'&&filename.back()==L'"')filename=filename.substr(1,filename.size()-2);
    if(!filename.empty()) {
        std::ofstream handshake{std::filesystem::path(filename)};
        handshake<<GetCurrentProcessId()<<' '<<reinterpret_cast<std::uintptr_t>(&root)<<' '<<reinterpret_cast<std::uintptr_t>(hwnd)<<'\n';
        if(!handshake)return 3;
    }
    MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return 0;
}
