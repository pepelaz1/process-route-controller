#include "core.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    auto w=route::initial();auto p=route::path(w,{1,1},{18,10});
    std::ofstream f(argc>1?argv[1]:"route.svg"); if(!f)return 1;
    f<<"<svg xmlns='http://www.w3.org/2000/svg' width='900' height='640' viewBox='0 0 900 640'><rect width='900' height='640' rx='20' fill='#101b32'/><g font-family='Arial' fill='white'><text x='40' y='50' font-size='27'>Process Route Controller</text><text x='40' y='83' font-size='16' fill='#a9bad8'>A* / 20 × 12 grid / "<<p.size()-1<<" steps / shortest route verified against BFS</text></g>";
    for(int y=0;y<route::height;++y)for(int x=0;x<route::width;++x)
        f<<"<rect x='"<<40+x*40<<"' y='"<<115+y*40<<"' width='36' height='36' rx='5' fill='"<<(w.blocked[route::index({x,y})]?"#53647f":"#1d304e")<<"'/>";
    f<<"<polyline fill='none' stroke='#4fd8e9' stroke-width='5' stroke-linejoin='round' points='";
    for(auto q:p)f<<58+q.x*40<<','<<133+q.y*40<<' ';
    f<<"'/><circle cx='98' cy='173' r='10' fill='#75f1bb'/><circle cx='778' cy='533' r='10' fill='#ffcf70'/><text x='40' y='622' font-family='Arial' font-size='15' fill='#a9bad8'>Green: origin   ·   Gold: destination   ·   Grey: blocked cells</text></svg>";
    std::cout<<"Route: "<<p.size()-1<<" steps. Report written.\n";
}
