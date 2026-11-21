// Reuse the existing concurrent game's real CPU UI adapter, not its main.
// This diagnostic runs the same120 sessions and fingerprints actual frames,
// camera, simulation, UI and working-save state. No device or save-file writes.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#define main hm2ExistingConcurrentTestMain
#include "../tests/ConcurrentValidationTests.cpp"
#undef main
#pragma clang diagnostic pop
#include <bit>

namespace {
struct Digest {
    std::uint64_t value=14695981039346656037ull;
    void word(std::uint64_t n){for(unsigned j=0;j<8;++j){value^=(n>>(8*j))&255;value*=1099511628211ull;}}
    void scalar(float n){word(std::bit_cast<std::uint32_t>(n));}
    void scalar(double n){word(std::bit_cast<std::uint64_t>(n));}
    void point(Vec2 p){scalar(p.x);scalar(p.y);}
    void size(Size p){scalar(p.width);scalar(p.height);}
    void text(std::string_view s){word(s.size());for(unsigned char c:s)word(c);}
    void color(Color c){scalar(c.r);scalar(c.g);scalar(c.b);scalar(c.a);}
    void clip(const std::optional<ClipRect>& c){word(c.has_value());if(c){scalar(c->x);scalar(c->y);scalar(c->width);scalar(c->height);}}
    void vertices(const std::vector<StripVertex>& v){word(v.size());for(const auto& x:v){point(x.position);point(x.uv);color(x.color);}}
    void frame(const Frame& f){
        scalar(f.width);scalar(f.height);word(f.sprites.size());
        for(const auto& s:f.sprites){text(s.image);point(s.center);size(s.size);scalar(s.angle);word(s.tile);word(s.columns);word(s.rows);word(s.reverseY);color(s.color);word(unsigned(s.blend.source));word(unsigned(s.blend.destination));clip(s.clip);vertices(s.strip);text(s.textureSource.image);word(unsigned(s.textureSource.variant));word(s.visualLayer.value_or(~0u));word(bool(s.atlasBatch));
            if(s.atlasBatch){const auto& b=*s.atlasBatch;text(b.tileIdentity);text(b.image);word(b.blendType);word(b.pixelFormat);size(b.content);size(b.storage);clip(b.clip);word(b.quads.size());for(const auto& q:b.quads)for(const auto& x:q.vertices){point(x.position);point(x.uv);word(x.color.r);word(x.color.g);word(x.color.b);word(x.color.a);}for(auto i:b.indices)word(i);}
        }
    }
    void session(GameSession& g,const validation::ScenarioObservation& o){
        scalar(g.renderTime());word(unsigned(g.state().screen));word(g.state().systemVisible);word(g.state().credits);word(g.state().level);word(o.saves);word(o.sounds);word(o.buttonRequests);word(o.buttonFailures);word(o.emissions);word(o.damage);word(o.kills);
        for(auto x:g.workingSlot())word(x);
        if(auto* s=g.battle()){word(1);const auto& c=s->camera();point(c.center);point(c.offset);size(c.viewport);point(c.backgroundWorld);scalar(c.inputMinimumY);scalar(c.lowerOverscan);scalar(c.focusMinimumY);word(c.fitWorld);word(c.keepFocusVisible);word(c.fillSurface);point(s->player().position);point(s->player().destination);word(s->playerHP());word(s->selectedEnemy());word(s->isPaused());scalar(s->clock().worldTime());word(s->metrics().randomCalls);word(s->metrics().emissions);word(s->metrics().damageCalls);for(const auto& e:s->enemies()){word(e.id);text(e.name);point(e.position);word(e.hp);word(e.corpse);}for(const auto& b:s->bots()){word(b.id);point(b.position);word(b.hp);word(b.target);}}
        else word(0);
    }
};
}
int main(int argc,char** argv){
    if(argc<2)return 64;const bool mixed=argc>2&&std::string_view(argv[2])=="--mixed";
    Files files(argv[1]);Host host;host.nativeUI=true;validation::ConcurrentValidation run(files,host);host.runtime=&run;
    run.setDisplay(DisplayConfiguration::surface({1024,1366},{0,20,0,24}));
    if(mixed)for(unsigned i=0;i<55;i+=2)run.session(i).setDisplay(DisplayConfiguration::surface({912,420},{68,20,68,0}));
    host.prepareNativeUI();host.prepareHUDRegressionUI();std::vector<Digest> frames(run.scenarioCount()),states(run.scenarioCount());
    for(unsigned tick=0;tick<1200;++tick){run.tick();(void)run.drawFrame();for(unsigned i=0;i<run.scenarioCount();++i){if(mixed&&i<55&&i%2==0)continue;frames[i].frame(run.scenarioFrame(i));states[i].session(run.session(i),run.observations()[i]);}}
    for(unsigned i=0;i<run.scenarioCount();++i){if(mixed&&i<55&&i%2==0)continue;const auto& o=run.observations()[i];std::cout<<"{\"case\":"<<i<<",\"frames\":1200,\"frameDigest\":\""<<std::hex<<frames[i].value<<"\",\"stateDigest\":\""<<states[i].value<<std::dec<<"\",\"buttonFailures\":"<<o.buttonFailures<<",\"emissions\":"<<o.emissions<<",\"kills\":"<<o.kills<<",\"saves\":"<<o.saves<<"}\n";}
}
