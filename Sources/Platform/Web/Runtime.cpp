#include "Renderer.hpp"
#include "Core/Audio/OpenALEffects.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include "Core/Data/ResourceName.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace hm2::web {
namespace {
EM_JS(void, message,(const char* value),{ Module.hm2Error(UTF8ToString(value)); });
EM_JS(void, dialog,(int kind,int argument,const char* title,const char* body),{
    Module.hm2Dialog(kind,argument,UTF8ToString(title),UTF8ToString(body));
});
EM_JS(void, musicFile,(const char* name),{ Module.hm2Music(UTF8ToString(name)); });
EM_JS(void, fieldDisplay,(int visible),{ Module.hm2FieldDisplay(visible !== 0); });
EM_JS(int, load,(const char* key,std::uint8_t* bytes,int capacity),{
    const value=Module.hm2Read(UTF8ToString(key));
    if(!value) return 0;
    if(value.length>capacity) return -1;
    HEAPU8.set(value,bytes);
    return value.length;
});
// SessionHost expects a durable acknowledgement. Asyncify suspends the C++
// entry point until the actual IndexedDB or desktop transaction completes.
EM_ASYNC_JS(int, persist,(const char* key,const std::uint8_t* bytes,int length),{
    const name=UTF8ToString(key);
    const value=HEAPU8.slice(bytes,bytes+length);
    try { await Module.hm2Persist(name,value); return 1; }
    catch(error) { Module.hm2StoreError(String(error)); return 0; }
});
EM_JS(void, rasterAlpha,(const char* value,const char* font,float point,int align,
                          float contentWidth,float contentHeight,int w,int h,std::uint8_t* out),{
    Module.hm2Raster(UTF8ToString(value),UTF8ToString(font),point,align,
        contentWidth,contentHeight,w,h,HEAPU8.subarray(out,out+w*h));
});

class Assets final : public ResourceAccess {
    static std::string path(std::string_view name) {
        if(!validResourceName(name)) throw std::runtime_error("Invalid game asset path");
        return "/campaign/"+std::string(name);
    }
public:
    Bytes read(std::string_view name) override {
        std::ifstream stream(path(name),std::ios::binary|std::ios::ate);
        if(!stream) throw std::runtime_error("Missing local game resource: "+std::string(name));
        const auto size=stream.tellg();
        if(size<0) throw std::runtime_error("Unable to measure game resource");
        Bytes bytes(static_cast<std::size_t>(size));
        stream.seekg(0);
        if(!stream.read(reinterpret_cast<char*>(bytes.data()),size))
            throw std::runtime_error("Unable to read game resource: "+std::string(name));
        return bytes;
    }
    bool exists(std::string_view name)const override {
        return validResourceName(name)&&std::ifstream(path(name),std::ios::binary).good();
    }
};

class Rasterizer final : public ui::TextRasterizer {
public:
    ui::TextBitmap rasterize(const ui::TextRasterRequest& request) override {
        ui::TextBitmap result;
        result.request=request;
        result.layout=ui::textRasterLayout(request);
        const auto& lay=result.layout;
        result.alpha.resize(std::size_t(lay.width)*lay.height);
        rasterAlpha(request.text.c_str(),request.fontName.c_str(),lay.fontSize,int(request.alignment),
                    lay.content.width,lay.content.height,lay.width,lay.height,result.alpha.data());
        // Browser draws the requested family with a local sans-serif cascade.
        result.metrics.resolvedFontName=request.fontName;
        result.metrics.osVersion="Web Canvas";
        return result;
    }
};
enum class Dialog {notice,name,deleteSlot,gameOver,recovery};
enum class ScreenLayout {standard,fittedMenu};
struct Touch { Vec2 previous; bool world=false; };

class Runtime final : public SessionHost,public ui::ScreensHost {
    Assets resources_;
    audio::OpenALEffects effects_;
    Rasterizer raster_;
    Renderer renderer_{resources_};
    DarwinWorldRandom random_{1};
    PreparedTiles tiles_{"webScreens"};
    ui::TextPreparationCache text_;
    std::map<const ui::TextBitmap*,std::string> textNames_;
    std::map<std::uintptr_t,Touch> touches_;
    std::unique_ptr<GameSession> game_;
    std::unique_ptr<ui::Screens> screens_;
    DisplayConfiguration display_;
    DisplayTransform worldProjection_;
    ScreenLayout screenLayout_=ScreenLayout::standard;
    std::uint64_t revision_=0,resourcesRevision_=0,screensGeneration_=0,textNumber_=0,ticks_=0;
    CombatScene* scene_=nullptr;
    bool active_=true,graphics_=true,desktopHost_=false;
    bool fieldVisible_=false;

    static void show(Dialog kind,int argument,std::string_view title,std::string_view body) {
        dialog(int(kind),argument,std::string(title).c_str(),std::string(body).c_str());
    }
    static std::optional<Bytes> stored(const std::string& key,std::size_t limit) {
        Bytes bytes(limit);
        const int count=load(key.c_str(),bytes.data(),bytes.size());
        if(count<0) throw std::runtime_error("Saved data exceeds its size limit: "+key);
        if(!count) return {};
        bytes.resize(count);
        return bytes;
    }
    static bool write(const char* key,std::span<const std::uint8_t> bytes) {
        return persist(key,bytes.data(),bytes.size())!=0;
    }
    static std::string hudKey(std::string_view profile) {
        if(profile!="compact"&&profile!="regular") throw std::invalid_argument("Invalid HUD profile");
        return "hud/"+std::string(profile);
    }

    std::optional<DisplayConfiguration> portraitFieldDisplay() const {
        constexpr auto hd=DisplayConfiguration{}.canvas;
        if(desktopHost_||display_.device==DisplayDevice::phone||display_.compact||
           display_.canvas.width>display_.canvas.height||
           display_.canvas.height*hd.width>=display_.canvas.width*hd.height) return {};
        // A short portrait surface can expose the map margin beside the HUD. Select
        // the shared map-bound camera without enabling its distance zoom.
        auto field=display_;
        field.compact=true;
        field.focusZoom=false;
        return field;
    }

    void updateScreenDisplay(ui::ScreenId screen,bool resized=false) {
        const bool fieldVisible=screen==ui::ScreenId::field;
        if(fieldVisible_!=fieldVisible) {
            fieldVisible_=fieldVisible;
            fieldDisplay(fieldVisible);
        }
        auto field=fieldVisible ? portraitFieldDisplay() : std::nullopt;
        worldProjection_={};
        if(auto* battle=game_->battle(); field&&battle) {
            // Keep the compact camera's field-to-HUD proportion at the regular
            // HD HUD size. The world uses one uniform scale and the inverse
            // pointer transform below; no texture or HUD coordinates are changed.
            const float regular=DisplayLayout(display_).ui(DisplayRegion::bottom).scale;
            const float compact=DisplayLayout(*field).ui(DisplayRegion::bottom).scale;
            worldProjection_.scale=regular/compact;
            if(!battle->cameraPresentation().fillSurface) {
                field->canvas={field->canvas.width/worldProjection_.scale,
                               field->canvas.height/worldProjection_.scale};
                field->insets={};
                battle->setDisplay(*field);
            }
        }
        constexpr auto hd=DisplayConfiguration{}.canvas;
        // The HD exit button is centered at y=52 and its 64-unit frame starts at y=20.
        constexpr float exitBottom=20;
        const bool cropsExit=display_.canvas.height*hd.width<display_.canvas.width*(hd.height-2*exitBottom);
        const bool fit=display_.device==DisplayDevice::tablet&&cropsExit&&
            (screen==ui::ScreenId::base||screen==ui::ScreenId::shop||screen==ui::ScreenId::command);
        const auto layout=fit ? ScreenLayout::fittedMenu : ScreenLayout::standard;
        if(!resized&&layout==screenLayout_) return;
        screenLayout_=layout;
        auto screens=display_;
        // Reuse the existing fit policy for both HD artwork and inverse hit testing.
        if(fit) {
            screens.device=DisplayDevice::desktop;
            screens.compact=false;
        }
        screens_->setDisplay(screens);
    }

    Frame drawWorld() {
        if(worldProjection_.identity()) return game_->drawWorld();
        auto& battle=*game_->battle();
        auto frame=battle.drawFrame();
        const auto camera=battle.cameraPresentation().worldToCanvas;
        const DisplayTransform projection{camera.scale*worldProjection_.scale,worldProjection_.map(camera.offset)};
        transformFrame(frame,projection,display_.canvas,DisplayLayout(display_).clip(DisplayRegion::world));
        return frame;
    }

    void synchronize(bool force=false) {
        const auto& state=game_->state();
        updateScreenDisplay(state.screen);
        const bool rebuild=revision_!=game_->presentationRevision();
        if(rebuild) {
            screens_->refresh(state,game_->renderTime());
            revision_=game_->presentationRevision();
        } else screens_->updateHUD(state);
        if(!graphics_) return;
        if(!force&&!rebuild&&scene_==game_->battle()&&resourcesRevision_==game_->resourceRevision()&&
           screensGeneration_==screens_->resourceGeneration()) return;
        scene_=game_->battle();
        resourcesRevision_=game_->resourceRevision();
        screensGeneration_=screens_->resourceGeneration();
        auto owners=game_->preparationFrame();
        screens_->appendPreparation(owners);
        renderer_.prepare(owners);
        for(const auto* expired:text_.discardUnused()) textNames_.erase(expired);
        for(const auto& [bitmap,key]:textNames_) {
            Sprite prepared;
            prepared.image=key;
            owners.sprites.push_back(std::move(prepared));
        }
        renderer_.retain(owners);
    }
public:
    Runtime() {
        if(!effects_.valid()) throw std::runtime_error(effects_.error());
        renderer_.initialize();
        std::optional<SaveData> data;
        if(const auto bytes=stored("save",SaveData::byteCount))
            data=SaveData::decode(*bytes,random_,false);
#if HM2_WEB_VALIDATION
        else {
            data.emplace();
            data->initializeSlot(0,"Web QA","kr",1794268800);
        }
#endif
        game_=std::make_unique<GameSession>(resources_,*this,random_,std::move(data));
        for(const auto& name:game_->orderedStartupSounds()) prepareSound(name);
        screens_=std::make_unique<ui::Screens>(resources_,tiles_,*this);
        synchronize(true);
    }
    void sound(std::string_view name,Vec2 point) override {
        if(!active_||name.empty()) return;
        prepareSound(name);
        if(!effects_.play(name,point.x,point.y)) throw std::runtime_error(effects_.error());
    }
    void soundID(unsigned id,Vec2 point) override { sound(audio::soundFilename(id),point); }
    void sound(std::string_view name) override { sound(name,{}); }
    void sfx(unsigned id) override { soundID(id,{}); }
    void prepareSound(std::string_view name) override {
        if(name.empty()||effects_.contains(name)) return;
        if(!effects_.prepare(std::string(name),audio::decodeWave(resources_.read(name))))
            throw std::runtime_error("Cannot prepare original PCM effect "+std::string(name)+": "+effects_.error());
    }
    void bgm(std::string_view name) override { musicFile(std::string(name).c_str()); }
    void alert(std::string_view key) override { alert(key,"HeavyMach 2"); }
    void alert(std::string_view key,std::string_view title) override {
        show(Dialog::notice,0,title,game_->originalText(key));
    }
    void notice(std::string_view title,std::string_view body) override { show(Dialog::notice,0,title,body); }
    void requestName(unsigned slot) override { show(Dialog::name,slot,"새 프로필",""); }
    void requestDelete(unsigned slot) override {
        show(Dialog::deleteSlot,slot,"프로필 삭제",game_->originalText("DeleteSlot"));
    }
    void requestGameOver(std::uint64_t token) override {
        show(Dialog::gameOver,static_cast<int>(token),"게임 종료",game_->originalText("DestroyMach"));
    }
    bool supportsRecoveryCode()const override { return true; }
    void requestRecoveryCode() override {
        show(Dialog::recovery,0,"저장 이동","");
    }
    bool backupSaveForRecovery(std::span<const std::uint8_t> bytes) override {
        return write("backup",bytes);
    }
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason) override {
        return write("save",bytes);
    }
    std::optional<Bytes> loadHUDLayout(std::string_view profile) override {
        return stored(hudKey(profile),65536);
    }
    bool saveHUDLayout(std::string_view profile,std::span<const std::uint8_t> bytes) override {
        return write(hudKey(profile).c_str(),bytes);
    }
    std::uint32_t absoluteTimeLow() override {
#if HM2_WEB_VALIDATION
        return static_cast<std::uint32_t>(ticks_+731);
#else
        return static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }
    std::uint32_t unixTime() override { return static_cast<std::uint32_t>(std::time(nullptr)); }

    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& request) override {
        auto bitmap=text_.prepare(raster_,request);
        auto [it,inserted]=textNames_.try_emplace(bitmap.get());
        if(inserted) {
            it->second="@text/web/"+std::to_string(textNumber_++);
            if(graphics_) renderer_.prepareText(it->second,*bitmap);
        }
        return {it->second,std::move(bitmap)};
    }
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& request) override {
        return prepareText(request);
    }
    void action(const ui::ScreenAction& action) override {
        game_->action(action);
        synchronize();
    }
    void resize(float width,float height,float left,float bottom,float right,float top,int kind) {
        auto configuration=DisplayConfiguration::surface({width,height},{left,bottom,right,top});
        configuration.device=kind==0?DisplayDevice::phone:kind==1?DisplayDevice::tablet:DisplayDevice::desktop;
        desktopHost_=kind==3;
        // Desktop shares the large-surface layout even in a reduced-size window.
        if(configuration.device==DisplayDevice::desktop) configuration.compact=configuration.focusZoom=false;
        cancel();
        display_=configuration;
        game_->setDisplay(display_);
        updateScreenDisplay(game_->state().screen,true);
        synchronize(true);
    }
    void cancel() {
        for(const auto& [id,touch]:touches_)
            if(touch.world) game_->inputWorld(PointerPhase::cancel,worldProjection_.inverse(touch.previous),id);
        touches_.clear();
        screens_->discardInputs();
    }
    void activity(bool active) {
        if(!active) cancel();
        active_=active;
        effects_.setActive(active);
        game_->setActive(active);
    }
    void lost() { cancel(); graphics_=false; renderer_.abandonContext(); }
    void recovered() {
        renderer_.initialize();
        graphics_=true;
        for(const auto& [bitmap,key]:textNames_) renderer_.prepareText(key,*bitmap);
        synchronize(true);
    }
    void input(PointerPhase phase,Vec2 point,std::uintptr_t id) {
        if(!active_||!id) return;
        if(phase==PointerPhase::begin) touches_.insert_or_assign(id,Touch{point});
        auto found=touches_.find(id);
        if(found==touches_.end()) return;
        auto& touch=found->second;
        const bool sameMove=phase==PointerPhase::move&&point.x==touch.previous.x&&point.y==touch.previous.y;
        touch.previous=point;
        const bool ui=game_->inputOverlay(phase,point,id)||screens_->input(phase,point,id,game_->renderTime());
        if(!ui&&!sameMove) {
            const auto captured=game_->inputWorld(phase,worldProjection_.inverse(point),id);
            if(phase==PointerPhase::begin) touch.world=captured;
        }
        if(phase==PointerPhase::end||phase==PointerPhase::cancel) {
            if(ui&&touch.world) game_->inputWorld(PointerPhase::cancel,worldProjection_.inverse(point),id);
            touches_.erase(found);
        }
        synchronize();
    }
    void step(int count) {
        if(!active_||!graphics_) return;
        for(int i=0;i<count;++i) {
            game_->tick([this] {
                synchronize();
                screens_->tick(game_->renderTime(),random_);
            });
            synchronize();
            ++ticks_;
        }
        Frame frame=drawWorld();
        const auto worldEnd=frame.sprites.size();
        screens_->append(frame,game_->renderTime());
        const bool desktop=display_.device==DisplayDevice::desktop;
        const HelpUnderlay help{worldEnd,desktop&&game_->help()?frame.sprites.size():0};
        game_->appendOverlays(frame);
        int width=0,height=0;
        emscripten_get_canvas_element_size("#game",&width,&height);
        auto artwork=MenuArtwork::native;
        if(screenLayout_==ScreenLayout::fittedMenu) artwork=MenuArtwork::desktop;
        else if(desktop) artwork=game_->state().screen==ui::ScreenId::maintenance?
            MenuArtwork::desktopBackdrop:MenuArtwork::desktop;
        renderer_.draw(frame,width,height,artwork,help);
    }
    void confirm(Dialog kind,int argument,std::string_view value) {
        if(kind==Dialog::name&&argument>=0&&static_cast<unsigned>(argument)<SaveData::slotCount) {
            if(value.empty()||value.size()>31||value.find('\0')!=value.npos) alert("IncorrectName");
            else game_->createSlot(static_cast<unsigned>(argument),value,"kr");
        } else if(kind==Dialog::deleteSlot&&argument>=0&&static_cast<unsigned>(argument)<SaveData::slotCount)
            game_->deleteSlot(static_cast<unsigned>(argument));
        else if(kind==Dialog::gameOver&&argument>0) game_->confirmGameOver(argument);
        synchronize();
    }
    std::string recoveryCode()const { return game_->recoveryCode(); }
    void validateRecovery(std::string_view code)const { game_->validateRecoveryCode(code); }
    bool restore(std::string_view code) {
        const bool result=game_->restoreRecoveryCode(code);
        if(result) synchronize();
        return result;
    }
#if HM2_WEB_VALIDATION
    std::string status() {
        const auto& state=game_->state();
        const auto& metrics=game_->metrics();
        const auto audioMetrics=effects_.metrics();
        const auto playback=effects_.playbackSnapshot();
        const auto playing=std::count_if(playback.begin(),playback.end(),[](const auto& effect){
            return effect.playing&&effect.sampleOffset>0;
        });
        std::ostringstream out;
        out<<"{\"screen\":"<<int(state.screen)<<",\"slot\":"<<state.selectedSlot
           <<",\"ticks\":"<<ticks_<<",\"saves\":"<<metrics.saveCompletions<<",\"credits\":"<<state.credits
           <<",\"level\":"<<state.level<<",\"device\":"<<int(display_.device)
           <<",\"compact\":"<<(display_.compact?"true":"false")
           <<",\"transition\":"<<game_->screenTransition().phase
           <<",\"help\":"<<(game_->help()?"true":"false")
           <<",\"active\":"<<(active_?"true":"false")
           <<",\"effects\":{\"contexts\":"<<audioMetrics.contextsCreated
           <<",\"sources\":"<<audioMetrics.sourcesCreated<<",\"buffers\":"<<audioMetrics.buffersCreated
           <<",\"plays\":"<<audioMetrics.playRequests<<",\"sampling\":"<<playing<<'}'
           <<",\"textures\":{\"reads\":"<<renderer_.assetReads()<<",\"uploads\":"<<renderer_.uploads()<<'}'
           <<",\"canvas\":["<<display_.canvas.width<<','<<display_.canvas.height<<"],\"buttons\":[";
        bool first=true;
        for(const auto& key:screens_->buttons()) {
            const auto button=screens_->button(key);
            if(!button||!button->visible||!button->active) continue;
            if(!first) out<<',';
            first=false;
            const auto point=screens_->buttonScreenPosition(key);
            out<<'['<<key.id<<','<<key.intData<<','<<point.x<<','<<point.y<<']';
        }
        out<<"],\"portalTargets\":[";
        first=true;
        const auto* battle=game_->battle();
        if(battle) {
            const auto player=battle->player().position;
            for(const auto& portal:battle->gates()) {
                const auto& gate=portal.definition;
                const auto point=worldProjection_.map(game_->worldCanvasPoint(
                    battle->camera().toScreen({gate.position.x+(player.x-gate.position.x)*.24f,
                                               gate.position.y+(player.y-gate.position.y)*.24f})));
                if(point.x<0||point.x>display_.canvas.width||point.y<0||point.y>display_.canvas.height) continue;
                if(!first) out<<',';
                first=false;
                out<<'['<<point.x<<','<<point.y<<','<<(gate.linkMap.starts_with("Base")?"true":"false")<<']';
            }
        }
        out<<"],\"path\":";
        if(battle) {
            const auto path=battle->dashState();
            out<<"{\"points\":"<<path.pathCount
               <<",\"held\":"<<(path.movePointer?"true":"false")<<'}';
        } else out<<"null";
        out<<'}';
        return out.str();
    }
    bool field(std::string_view map) {
        if(game_->state().selectedSlot<0&&!game_->selectSlot(0)) return false;
        const bool result=game_->enterField(map,0);
        synchronize();
        return result;
    }
#endif
};

std::unique_ptr<Runtime> runtime;
std::string resultText;
template<class Operation> int invoke(Operation&& operation) {
    try {
        if(!runtime) throw std::runtime_error("Game is not initialized");
        return int(operation(*runtime));
    } catch(const std::exception& error) {
        message(error.what());
        return -1;
    }
}
}
}

using namespace hm2;
using namespace hm2::web;
extern "C" {
EMSCRIPTEN_KEEPALIVE int hm2_start() {
    try {
        EmscriptenWebGLContextAttributes attributes;
        emscripten_webgl_init_context_attributes(&attributes);
        attributes.majorVersion=2;
        attributes.alpha=false;
        attributes.depth=false;
        attributes.stencil=false;
        attributes.antialias=false;
        const auto context=emscripten_webgl_create_context("#game",&attributes);
        if(context<=0||emscripten_webgl_make_context_current(context)!=EMSCRIPTEN_RESULT_SUCCESS)
            throw std::runtime_error("WebGL 2 is unavailable");
        runtime=std::make_unique<Runtime>();
        return 1;
    } catch(const std::exception& error) { message(error.what()); return -1; }
}
EMSCRIPTEN_KEEPALIVE int hm2_resize(float width,float height,float left,float bottom,float right,float top,int kind) {
    return invoke([=](auto& app){app.resize(width,height,left,bottom,right,top,kind);return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_step(int ticks) {
    return invoke([=](auto& app){app.step(std::clamp(ticks,0,6));return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_input(int phase,float x,float y,int pointer) {
    return invoke([=](auto& app){app.input(PointerPhase(phase),{x,y},pointer);return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_activity(int active) {
    return invoke([=](auto& app){app.activity(active!=0);return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_context(int restored) {
    return invoke([=](auto& app){if(restored) app.recovered(); else app.lost();return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_confirm(int type,int argument,const char* value) {
    return invoke([=](auto& app){app.confirm(Dialog(type),argument,value?value:"");return true;});
}
EMSCRIPTEN_KEEPALIVE const char* hm2_recovery_code() {
    resultText.clear();
    invoke([](auto& app){resultText=app.recoveryCode();return true;});
    return resultText.c_str();
}
EMSCRIPTEN_KEEPALIVE int hm2_validate_recovery(const char* code) {
    return invoke([=](auto& app){app.validateRecovery(code?code:"");return true;});
}
EMSCRIPTEN_KEEPALIVE int hm2_restore(const char* code) {
    return invoke([=](auto& app){return app.restore(code?code:"");});
}
#if HM2_WEB_VALIDATION
EMSCRIPTEN_KEEPALIVE const char* hm2_status() {
    resultText.clear();
    invoke([](auto& app){resultText=app.status();return true;});
    return resultText.c_str();
}
EMSCRIPTEN_KEEPALIVE int hm2_validation_field(const char* map) {
    return invoke([=](auto& app){return app.field(map?map:"");});
}
#endif
}
