#include "core.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <chrono>

namespace {
using namespace std::chrono_literals;
struct Handle {
    HANDLE h{};
    explicit Handle(HANDLE value=nullptr):h(value){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
template<class T>T read(HANDLE process,std::uintptr_t address) {
    T result{};SIZE_T bytes=0;
    check(ReadProcessMemory(process,reinterpret_cast<const void*>(address),&result,sizeof(result),&bytes)&&bytes==sizeof(result),"ReadProcessMemory: incomplete read");
    return result;
}
route::World snapshot(HANDLE process,std::uintptr_t rootAddress) {
    for(int attempt=0;attempt<10;++attempt) {
        auto a=read<route::Root>(process,rootAddress);
        check(a.signature==route::magic&&a.protocol==route::version&&a.world!=0,"Root protocol mismatch");
        try {
            auto before=read<std::uint32_t>(process,a.world+offsetof(route::World,sequence));
            if(before&1)continue;
            auto w=read<route::World>(process,a.world);
            auto after=read<std::uint32_t>(process,a.world+offsetof(route::World,sequence));
            auto b=read<route::Root>(process,rootAddress);
            if(a.world==b.world&&before==after&&w.sequence==after&&!(after&1)&&w.protocol==route::version)return w;
        }catch(const std::exception&){std::this_thread::sleep_for(1ms);}
    }
    throw std::runtime_error("Could not acquire a consistent snapshot");
}
void mouse(HWND hwnd,DWORD child,route::Point point,bool control) {
    DWORD owner=0;GetWindowThreadProcessId(hwnd,&owner);
    check(owner==child,"Unexpected target window owner");
    if(GetForegroundWindow()!=hwnd) return;
    POINT position{28+point.x*34+15,110+point.y*34+15};
    check(ClientToScreen(hwnd,&position)!=0,"ClientToScreen failed");
    LONG x0=GetSystemMetrics(SM_XVIRTUALSCREEN),y0=GetSystemMetrics(SM_YVIRTUALSCREEN);
    LONG width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT events[5]{};unsigned count=0;
    if(control){events[count].type=INPUT_KEYBOARD;events[count++].ki.wVk=VK_CONTROL;}
    auto& move=events[count++];move.type=INPUT_MOUSE;
    move.mi.dx=MulDiv(position.x-x0,65535,width-1);move.mi.dy=MulDiv(position.y-y0,65535,height-1);
    move.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK;
    events[count].type=INPUT_MOUSE;events[count++].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    events[count].type=INPUT_MOUSE;events[count++].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    if(control){events[count].type=INPUT_KEYBOARD;events[count].ki.wVk=VK_CONTROL;events[count++].ki.dwFlags=KEYEVENTF_KEYUP;}
    if(SendInput(count,events,sizeof(INPUT))!=count) {
        INPUT release{};release.type=INPUT_KEYBOARD;release.ki.wVk=VK_CONTROL;release.ki.dwFlags=KEYEVENTF_KEYUP;SendInput(1,&release,sizeof(INPUT));
        throw std::runtime_error("SendInput failed; check desktop integrity levels");
    }
}
void testClick(HWND hwnd,route::Point p,bool ctrl=false) {
    SendMessageW(hwnd,WM_LBUTTONDOWN,ctrl?MK_CONTROL:0,MAKELPARAM(28+p.x*34+15,110+p.y*34+15));
}
}
int main(int argc,char** argv) {try {
    const bool integration=argc>1&&std::string(argv[1])=="--integration-test";
    wchar_t executable[32768]{};check(GetModuleFileNameW(nullptr,executable,32768)!=0,"Executable path");
    auto target=std::filesystem::path(executable).parent_path()/L"route-world.exe";
    wchar_t temp[MAX_PATH]{};check(GetTempPathW(MAX_PATH,temp)!=0,"Temp path");
    auto file=std::filesystem::path(temp)/(L"route-handshake-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".txt");
    struct Remove {std::filesystem::path p;~Remove(){std::error_code e;std::filesystem::remove(p,e);}} cleanup{file};
    Handle job(CreateJobObjectW(nullptr,nullptr));check(job.h!=nullptr,"Job creation");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};limit.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    check(SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limit,sizeof(limit))!=0,"Job limits");
    std::wstring command=L"\""+target.wstring()+L"\" \""+file.wstring()+L"\"";
    STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION pi{};
    check(CreateProcessW(target.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,target.parent_path().c_str(),&start,&pi)!=0,"Cannot start route-world.exe");
    Handle child(pi.hProcess),thread(pi.hThread);
    if(!AssignProcessToJobObject(job.h,child.h)){TerminateProcess(child.h,1);throw std::runtime_error("Cannot assign child job");}
    ResumeThread(thread.h);
    DWORD reported=0;std::uintptr_t rootAddress=0,window=0;
    for(int n=0;n<250;++n) {
        std::ifstream handshake(file);if(handshake>>reported>>rootAddress>>window)break;
        check(WaitForSingleObject(child.h,0)==WAIT_TIMEOUT,"Child exited before handshake");
        std::this_thread::sleep_for(20ms);
    }
    check(reported==pi.dwProcessId&&rootAddress&&window,"Invalid child handshake");
    HWND hwnd=reinterpret_cast<HWND>(window);DWORD owner=0;GetWindowThreadProcessId(hwnd,&owner);
    check(owner==pi.dwProcessId,"Window/PID mismatch");
    Handle reader(OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pi.dwProcessId));
    check(reader.h!=nullptr,"Cannot open child for reading");
    auto world=snapshot(reader.h,rootAddress);
    std::cout<<"Connected to owned process "<<pi.dwProcessId<<"; root 0x"<<std::hex<<rootAddress<<std::dec<<'\n';
    if(integration) {
        auto old=read<route::Root>(reader.h,rootAddress).world;
        SendMessageW(hwnd,WM_KEYDOWN,'R',0);
        check(read<route::Root>(reader.h,rootAddress).world!=old,"Relocation did not change pointer");
        check(snapshot(reader.h,rootAddress).relocations==1,"Relocated snapshot");
        SendMessageW(hwnd,WM_KEYDOWN,'B',0);
        world=snapshot(reader.h,rootAddress);check(world.blocked[route::index({2,1})]==1,"Dynamic obstacle");
        route::Planner planner;
        for(int i=0;i<200;++i){world=snapshot(reader.h,rootAddress);auto d=planner.step(world,{18,10});if(d.mode==route::Mode::Arrived)break;check(d.mode==route::Mode::Navigate,"Navigation state");testClick(hwnd,d.next);}
        world=snapshot(reader.h,rootAddress);check(world.x==18&&world.y==10,"Route did not arrive");
        SendMessageW(hwnd,WM_KEYDOWN,VK_SPACE,0);
        for(int i=0;i<200;++i){world=snapshot(reader.h,rootAddress);auto d=planner.step(world,{18,10});if(d.mode==route::Mode::Unload){testClick(hwnd,d.next,true);break;}check(d.mode==route::Mode::Return,"Return state");testClick(hwnd,d.next);}
        world=snapshot(reader.h,rootAddress);check(world.inventory==0&&world.unloads==1,"Unload action");
        SendMessageW(hwnd,WM_KEYDOWN,'H',0);world=snapshot(reader.h,rootAddress);
        check(planner.step(world,{18,10}).mode==route::Mode::Emergency,"Emergency detection");
        PostMessageW(hwnd,WM_CLOSE,0,0);check(WaitForSingleObject(child.h,5000)==WAIT_OBJECT_0,"Emergency close failed");
        std::cout<<"PASS: actual cross-process RAM reads, pointer relocation, dynamic obstacle, A*, return, Ctrl-click message, emergency close\n";
        return 0;
    }
    SetForegroundWindow(hwnd);
    std::cout<<"Keep the world window focused. Escape stops. H: emergency. Space: buffer full. R: pointer relocation. S: freeze.\n";
    route::Planner planner;route::Mode last=route::Mode::Blocked;
    std::mt19937 rng(std::random_device{}());std::uniform_int_distribution<int> interval(100,180);
    auto nextAction=std::chrono::steady_clock::now();
    while(WaitForSingleObject(child.h,0)==WAIT_TIMEOUT) {
        if(GetAsyncKeyState(VK_ESCAPE)&0x8000)break;
        world=snapshot(reader.h,rootAddress);
        if(world.health<35){std::cout<<"EMERGENCY: posting WM_CLOSE to owned child\n";PostMessageW(hwnd,WM_CLOSE,0,0);WaitForSingleObject(child.h,3000);break;}
        if(GetForegroundWindow()!=hwnd){std::this_thread::sleep_for(25ms);continue;}
        if(std::chrono::steady_clock::now()<nextAction){std::this_thread::sleep_for(10ms);continue;}
        auto d=planner.step(world,{18,10});
        if(d.mode!=last){std::cout<<route::name(d.mode)<<" at "<<world.x<<','<<world.y<<'\n';last=d.mode;}
        if(d.mode==route::Mode::Navigate||d.mode==route::Mode::Return||d.mode==route::Mode::Unload)mouse(hwnd,pi.dwProcessId,d.next,d.mode==route::Mode::Unload);
        if(d.mode==route::Mode::Recover)std::cout<<"No progress: route recalculated; waiting for obstruction to clear\n";
        nextAction=std::chrono::steady_clock::now()+std::chrono::milliseconds(interval(rng));
        std::this_thread::sleep_for(10ms);
    }
    return 0;
}catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}}
