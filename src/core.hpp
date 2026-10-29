#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

namespace route {
constexpr int width=20, height=12, count=width*height;
constexpr std::uint32_t magic=0x52544331, version=1;
struct Point { int x{},y{}; bool operator==(const Point&) const = default; };
struct World {
    std::uint32_t sequence{};
    std::uint32_t protocol{version};
    std::array<std::uint8_t,count> blocked{};
    int x{1},y{1},health{100},inventory{},capacity{8};
    std::uint32_t moves{},unloads{},relocations{};
};
struct Root { std::uint32_t signature{magic}, protocol{version}; std::uintptr_t world{}; };
inline bool inside(Point p) { return p.x>=0&&p.y>=0&&p.x<width&&p.y<height; }
inline int index(Point p) { return p.y*width+p.x; }
inline bool walkable(const World& w,Point p) { return inside(p)&&!w.blocked[index(p)]; }
inline int distance(Point a,Point b) { return std::abs(a.x-b.x)+std::abs(a.y-b.y); }
inline World initial() {
    World w;
    for(int y=0;y<height;++y) if(y!=8) w.blocked[index({9,y})]=1;
    for(int x=12;x<18;++x) w.blocked[index({x,5})]=1;
    return w;
}
inline std::vector<Point> path(const World& w,Point start,Point goal) {
    if(!walkable(w,start)||!walkable(w,goal)) return {};
    struct Node { int f,g,id; bool operator<(const Node& b) const { return f>b.f; } };
    std::priority_queue<Node> open;
    std::array<int,count> cost,parent;
    cost.fill(std::numeric_limits<int>::max()); parent.fill(-1);
    cost[index(start)]=0; open.push({distance(start,goal),0,index(start)});
    constexpr Point dirs[]={{1,0},{0,1},{-1,0},{0,-1}};
    while(!open.empty()) {
        auto n=open.top();open.pop();
        if(n.g!=cost[n.id]) continue;
        Point p{n.id%width,n.id/width};
        if(p==goal) {
            std::vector<Point> out;
            for(int at=n.id;at!=-1;at=parent[at]) out.push_back({at%width,at/width});
            std::reverse(out.begin(),out.end()); return out;
        }
        for(auto d:dirs) {
            Point next{p.x+d.x,p.y+d.y};
            if(!walkable(w,next)) continue;
            int id=index(next),g=n.g+1;
            if(g>=cost[id]) continue;
            cost[id]=g;parent[id]=n.id;open.push({g+distance(next,goal),g,id});
        }
    }
    return {};
}
enum class Mode { Navigate,Return,Unload,Emergency,Arrived,Blocked,Recover };
inline const char* name(Mode m) {
    switch(m) {
    case Mode::Navigate:return "NAVIGATE"; case Mode::Return:return "RETURN";
    case Mode::Unload:return "UNLOAD"; case Mode::Emergency:return "EMERGENCY";
    case Mode::Arrived:return "ARRIVED"; case Mode::Blocked:return "BLOCKED";
    case Mode::Recover:return "RECOVER";
    } return "UNKNOWN";
}
struct Decision { Mode mode; Point next; };
class Planner {
    Point last_{-1,-1};int stalled_{};bool returning_{};
public:
    Decision step(const World& w,Point goal) {
        Point p{w.x,w.y};
        if(w.protocol!=version||!inside(p)||w.health<0||w.health>100||w.capacity<=0)
            throw std::runtime_error("Invalid world snapshot");
        if(w.health<35) return {Mode::Emergency,p};
        if(w.inventory>=w.capacity) returning_=true;
        if(returning_&&w.inventory==0) returning_=false;
        Point target=returning_?Point{1,1}:goal;
        if(p==target) { last_=p;stalled_=0;return {returning_?Mode::Unload:Mode::Arrived,p}; }
        stalled_=p==last_?stalled_+1:0;last_=p;
        if(stalled_>=5) { stalled_=0;return {Mode::Recover,p}; }
        auto result=path(w,p,target);
        if(result.size()<2) return {Mode::Blocked,p};
        return {returning_?Mode::Return:Mode::Navigate,result[1]};
    }
};
}
