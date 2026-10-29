#include "core.hpp"
#include <cstdlib>
#include <iostream>
#include <queue>
#include <random>
using namespace route;
void require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
int bfs(const World& w,Point a,Point b) {
    if(!walkable(w,a)||!walkable(w,b)) return -1;
    std::array<int,count> d;d.fill(-1);std::queue<Point> q;q.push(a);d[index(a)]=0;
    while(!q.empty()) {auto p=q.front();q.pop();if(p==b)return d[index(p)];
        for(auto v:std::array<Point,4>{{{1,0},{-1,0},{0,1},{0,-1}}}) {
            Point n{p.x+v.x,p.y+v.y};
            if(walkable(w,n)&&d[index(n)]<0){d[index(n)]=d[index(p)]+1;q.push(n);}
        }
    }return -1;
}
int main() {try {
    auto w=initial();auto p=path(w,{1,1},{18,10});
    require(!p.empty()&&p.front()==Point{1,1}&&p.back()==Point{18,10},"Endpoints");
    for(std::size_t i=1;i<p.size();++i) require(walkable(w,p[i])&&distance(p[i-1],p[i])==1,"Illegal step");
    require(path(w,{-1,0},{18,10}).empty(),"Bounds");
    require(path(w,{9,0},{18,10}).empty(),"Blocked start");
    require(path(w,{1,1},{1,1}).size()==1,"Same point");
    std::mt19937 rng(42);
    for(int i=0;i<200;++i) {
        World random;for(auto& cell:random.blocked)cell=static_cast<std::uint8_t>(rng()%4==0);
        random.blocked[index({1,1})]=0;random.blocked[index({18,10})]=0;
        auto r=path(random,{1,1},{18,10});auto shortest=bfs(random,{1,1},{18,10});
        require(r.empty()?shortest==-1:static_cast<int>(r.size())-1==shortest,"A* differs from BFS");
    }
    Planner a;w.health=34;w.inventory=w.capacity;require(a.step(w,{18,10}).mode==Mode::Emergency,"Emergency priority");
    w.health=35;require(a.step(w,{18,10}).mode==Mode::Unload,"35 percent boundary");
    w.x=3;w.y=1;require(a.step(w,{18,10}).mode==Mode::Return,"Return persistence");
    w.x=1;w.inventory=0;require(a.step(w,{18,10}).mode==Mode::Navigate,"Resume after unload");
    Planner b; bool recovery=false;for(int i=0;i<8;++i) recovery|=b.step(w,{18,10}).mode==Mode::Recover;
    require(recovery,"Stall recovery");
    w.blocked.fill(1);w.blocked[index({1,1})]=0;Planner c;
    require(c.step(w,{18,10}).mode==Mode::Blocked,"No route");
    w.protocol=0;bool rejected=false;try{c.step(w,{18,10});}catch(...){rejected=true;}
    require(rejected,"Reject version mismatch");
    std::cout<<"PASS: 200 randomized A*/BFS cases, path validity, boundaries, emergency, return/unload, recovery and protocol checks\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
