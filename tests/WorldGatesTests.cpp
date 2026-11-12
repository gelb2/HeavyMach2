#include "Core/Simulation/WorldGates.hpp"
#include "Core/UI/Screens.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{++reads;std::ifstream file(root/std::string(name),std::ios::binary);assert(file);return {std::istreambuf_iterator<char>(file),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Texts {
    std::vector<ui::TextRasterRequest> requests;
    ui::PreparedScreenText prepare(const ui::TextRasterRequest& request){
        requests.push_back(request);auto bitmap=std::make_shared<ui::TextBitmap>();bitmap->request=request;bitmap->layout=ui::textRasterLayout(request);bitmap->alpha.resize(std::size_t(bitmap->layout.width)*bitmap->layout.height);
        return {"prepared:"+request.sourceId,bitmap};
    }
    WorldGates::PrepareText callback(){return [this](const auto& request){return prepare(request);};}
};
Frame draw(const WorldGates& gates,const MapCamera* camera=nullptr){VisualLayer layer;gates.collect(layer,camera);auto submitted=layer.drain();assert(submitted.atlases.empty());Frame frame;std::move(submitted).appendBatches(frame);return frame;}
MapGate gate(int number=0,Vec2 position={0,0},bool event=false){return {number,7,position,"Base001",event};}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
void dataCorpus(Files& files,const Maps& maps,const WorldGateNames& names){
    assert(maps.all().size()==43&&names.raw().rowCount()==43&&names.all().size()==43);
    assert(names.find("Base001")=="RuinField Base"&&names.find("Stage01_02")=="wetland"&&names.find("Stage01_04")=="wide planes 01"&&!names.find("unknown-map"));
    const auto replacement=WorldGateNames::parse(DataTable::parseText("MapName\tMapDesc\nX\tfirst\nX\tlast\n"));assert(replacement.find("X")=="last"&&replacement.raw().rowCount()==2);
    unsigned total=0,event=0,closedCount=0,openCount=0,differentDescriptions=0;PreparedTiles tiles("gate-corpus");Texts texts;
    for(const auto& [name,map]:maps.all()){
        total+=unsigned(map.gates.size());for(const auto& row:map.gates){event+=row.eventOnly;const auto& target=maps.map(row.linkMap);assert(names.find(row.linkMap));if(target.description!=*names.find(row.linkMap))++differentDescriptions;
            assert(std::any_of(target.gates.begin(),target.gates.end(),[&](const auto& other){return row.linkGate==other.number;}));}
        WorldGates closed(files,tiles,map.gates,false,names,texts.callback()),opened(files,tiles,map.gates,true,names,texts.callback());
        closedCount+=unsigned(closed.snapshots().size());openCount+=unsigned(opened.snapshots().size());
        std::size_t openIndex=0,closedIndex=0;for(const auto& row:map.gates){
            const auto check=[&](const WorldGateSnapshot& s){assert(s.definition.number==row.number&&s.definition.linkGate==row.linkGate&&s.definition.position.x==row.position.x&&s.definition.position.y==row.position.y&&s.definition.linkMap==row.linkMap&&s.definition.eventOnly==row.eventOnly&&s.destinationName==*names.find(row.linkMap)&&s.angle==0&&!s.registered[0]&&!s.registered[1]&&!s.registered[2]);};
            check(opened.snapshots()[openIndex++]);if(!row.eventOnly)check(closed.snapshots()[closedIndex++]);
        }
    }
    assert(total==90&&event==4&&closedCount==86&&openCount==90&&texts.requests.size()==(86+90)*2&&differentDescriptions>0);
    const auto first=maps.map("Stage01_01").gates;assert(first.size()==3&&first[0].number==0&&first[0].position.x==-275&&first[0].position.y==-462&&first[0].linkMap=="Base001"&&first[1].number==1&&first[1].linkMap=="Stage01_02"&&first[2].number==2&&first[2].linkMap=="Stage01_04");
    const std::pair<const char*,const char*> events[]={{"Stage01_02","Stage01_08"},{"Stage01_07","Stage01_09"},{"Stage02_06","Stage02_07"},{"Stage04_11","Stage04_12"}};
    for(auto [from,to]:events){const auto& rows=maps.map(from).gates;assert(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.eventOnly&&row.linkMap==to;}));}
    std::cout<<"WORLD_GATES corpus43/maps90/event4/filter86/rawOrder/gateIDs/MapList43/lastWins/notMapInfoFallback PASS\n";
}
void boundaryAndPause(Files& files,const WorldGateNames& names){
    PreparedTiles tiles("gate-boundary");Texts texts;
    const std::vector<MapGate> rows{gate()};WorldGates gates(files,tiles,rows,false,names,texts.callback());
    assert(draw(gates).sprites.empty());unsigned links=0;bool paused=false;
    const auto pause=[&]{return paused;};const auto link=[&](const MapGate& s){++links;assert(s.number==0&&s.linkGate==7&&s.linkMap=="Base001");};
    const auto inner=std::nextafter(30.f,0.f),outer=std::nextafter(30.f,100.f);
    for(const auto point:std::vector<Vec2>{{30,0},{-30,0},{0,30},{0,-30},{outer,0},{0,-outer}}){const auto before=links;gates.tick(point,pause,link);assert(links==before);}
    for(const auto point:std::vector<Vec2>{{29,29},{inner,inner},{-inner,-inner},{0,0}}){const auto before=links;gates.tick(point,pause,link);assert(links==before+1);}
    paused=true;float expected=gates.snapshots()[0].angle;const auto before=links;for(unsigned i=0;i<700;++i){expected+=.01f;gates.tick({0,0},pause,link);}assert(links==before&&gates.snapshots()[0].angle==expected&&expected>6.2831853f); // pause still rotates, no wrap.
    // Reading pause only once at entry lets overlapping later gates trigger.
    const std::vector<MapGate> duplicate{gate(4),gate(2),gate(9)};WorldGates ordered(files,tiles,duplicate,false,names,texts.callback());unsigned reads=0;std::vector<int> reached;paused=false;
    ordered.tick({0,0},[&]{++reads;return paused;},[&](const MapGate& g){reached.push_back(g.number);paused=true;});assert(reads==3&&reached==std::vector<int>{4});for(const auto& s:ordered.snapshots())assert(s.angle==.01f);
    paused=false;ordered.tick({0,0},pause,[&](const MapGate& g){reached.push_back(g.number);});assert((reached==std::vector<int>{4,4,2,9})); // no invented one-shot latch.
    const std::vector<MapGate> eventOnly{gate(3,{0,0},true)};const auto oldReads=files.reads,oldProbes=files.probes;const auto oldText=texts.requests.size();WorldGates filtered(files,tiles,eventOnly,false,names,texts.callback());assert(filtered.snapshots().empty()&&filtered.preparationFrame().sprites.empty()&&files.reads==oldReads&&files.probes==oldProbes&&texts.requests.size()==oldText);
    std::cout<<"WORLD_GATES strictAbs30/nextafter/corner29x29/pauseRotation/noWrap/reReadPauseAfterCallback/noInventedLatch/filterBeforePreparation PASS\n";
}
void visualsAndWarmIO(Files& files,const WorldGateNames& names){
    PreparedTiles tiles("gate-visual");Texts texts;const std::vector<MapGate> rows{gate()};WorldGates gates(files,tiles,rows,false,names,texts.callback());MapCamera camera;
    assert(texts.requests.size()==2);for(const auto& r:texts.requests)assert(r.text=="RuinField Base"&&r.fontName=="Trebuchet-BoldItalic"&&r.fontSize==13&&r.dimensions.width==64&&r.dimensions.height==48&&r.alignment==ui::TextAlign::center&&!r.retina);
    assert(gates.snapshots()[0].gate.bounds.left==-32&&gates.snapshots()[0].gate.bounds.top==32&&gates.snapshots()[0].labels[0].bounds.top==32); // Text POT bounds64, draw content48.
    const auto prepared=gates.preparationFrame();assert(prepared.sprites.size()==3&&prepared.sprites[0].image=="LinkGate.png"&&prepared.sprites[0].size.width==64&&prepared.sprites[0].size.height==64);
    gates.tick({5000,5000},[]{return true;},{},&camera);auto f=draw(gates,&camera);assert(f.sprites.size()==3);const auto& image=f.sprites[0];const auto& white=f.sprites[1];const auto& black=f.sprites[2];
    assert(image.image=="LinkGate.png"&&!image.atlasBatch&&image.center.x==384&&image.center.y==512&&image.blend.source==BlendFactor::sourceAlpha&&image.blend.destination==BlendFactor::one&&image.strip.size()==4);
    const float c=std::cos(.01f),s=std::sin(.01f);assert(near(image.strip[0].position.x,384+(-32*c-32*s))&&near(image.strip[0].position.y,512+(-32*s+32*c)));
    assert(white.center.x==384&&white.center.y==518&&black.center.x==385&&black.center.y==517&&white.angle==0&&black.angle==0&&white.size.width==64&&white.size.height==48&&black.size.height==48);
    assert(white.color.r==1&&white.color.g==1&&white.color.b==1&&black.color.r==0&&black.color.g==0&&black.color.b==0&&white.color.a==1&&black.color.a==1);
    assert(white.blend.source==BlendFactor::sourceAlpha&&white.blend.destination==BlendFactor::oneMinusSourceAlpha&&white.strip[0].uv.y==.75f); // one content/POT crop.
    std::array<VisualLayer,14> layers;gates.collect(layers,&camera);assert(layers[1].visualCount()==3);for(unsigned i=0;i<layers.size();++i)if(i!=1)assert(layers[i].visualCount()==0);
    const auto reads=files.reads,probes=files.probes;const auto labels=texts.requests.size();for(unsigned i=0;i<1000;++i){gates.tick({5000,5000},[]{return true;},{},&camera);const auto frame=draw(gates,&camera);assert(frame.sprites.size()==3);(void)gates.preparationFrame();}assert(files.reads==reads&&files.probes==probes&&texts.requests.size()==labels);
    // No one-frame previous-isInScreen lag for non-atlas QobBase registration.
    MapCamera outside=camera;outside.center.x=10000;gates.tick({5000,5000},[]{return true;},{},&outside);assert(draw(gates,&camera).sprites.empty());gates.tick({5000,5000},[]{return true;},{},&camera);assert(draw(gates,&camera).sprites.size()==3);
    // Camera visibility does not gate proximity callbacks or child ticks.
    unsigned linked=0;gates.tick({0,0},[]{return false;},[&](const auto&){++linked;},&outside);assert(linked==1&&draw(gates,&outside).sprites.empty());
    const std::vector<MapGate> edgeRows{gate(0,{-476.125f,0})};WorldGates edge(files,tiles,edgeRows,false,names,texts.callback());edge.tick({5000,5000},[]{return true;},{},&camera);assert(!edge.snapshots()[0].registered[0]&&!edge.snapshots()[0].registered[1]&&edge.snapshots()[0].registered[2]);f=draw(edge,&camera);assert(f.sprites.size()==1&&f.sprites[0].color.r==0); // black text's +1 crosses the exact expanded camera bound independently.
    std::cout<<"WORLD_GATES nonAtlasLayer1Geometry/blend2/whiteThenBlack/translatedUnrotatedText/64x48content64POT/currentVisibility/independentChildCulling/warm1000IO0 PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));const WorldGateNames names(files);dataCorpus(files,maps,names);boundaryAndPause(files,names);visualsAndWarmIO(files,names);}
