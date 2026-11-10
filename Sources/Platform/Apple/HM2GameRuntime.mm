#include "Platform/Apple/HM2GameRuntime.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Platform/Apple/HM2TextRasterizer.h"
#import "Platform/Apple/HM2Audio.h"
#import "Platform/Apple/HM2Renderer.h"
#include "Core/Audio/SoundIDs.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <mach/mach_time.h>
#include <ctime>
#include <map>

namespace hm2::apple {
namespace {
NSString* string(std::string_view value){return [[NSString alloc]initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];}
}
struct GameRuntime::Impl:SessionHost,ui::ScreensHost {
    NSURL* saveURL;HM2Renderer* renderer;HM2Audio* audio;
    std::function<void(const NativeRequest&)> request;
    Files files;DarwinWorldRandom random{1};PreparedTiles tiles{"screens"};
    std::unique_ptr<ui::TextRasterizer> rasterizer=makeTextRasterizer();ui::TextPreparationCache textCache;
    std::map<const ui::TextBitmap*,std::string> textNames;
    std::unique_ptr<GameSession> game;std::unique_ptr<ui::Screens> ui;
    std::uint64_t revision=0;CombatScene* preparedWorld=nullptr;bool active=true;
    Impl(NSURL* root,NSURL* save,std::function<void(const NativeRequest&)> callback):saveURL(save),request(std::move(callback)),files(root.fileSystemRepresentation){
        if(!root.isFileURL||!save.isFileURL)throw std::runtime_error("HM2 runtime requires local resource/save URLs");
        renderer=[[HM2Renderer alloc]initWithResourceRoot:root];audio=[[HM2Audio alloc]initWithResourceRoot:root];
        if(![audio prepareSounds])throw std::runtime_error(audio.lastError.UTF8String);
        std::optional<SaveData> data;
        if([NSFileManager.defaultManager fileExistsAtPath:saveURL.path]){
            NSError* error=nil;NSData* bytes=[NSData dataWithContentsOfURL:saveURL options:0 error:&error];
            if(!bytes)throw std::runtime_error(error.localizedDescription.UTF8String);
            data=SaveData::decode({static_cast<const std::uint8_t*>(bytes.bytes),bytes.length},random);
        }
        game=std::make_unique<GameSession>(files,*this,random,std::move(data));ui=std::make_unique<ui::Screens>(files,tiles,*this);synchronize();
    }
    void notify(NativeRequest value){if(request)request(value);}
    void sound(std::string_view n,Vec2 p)override{if(!n.empty()&&![audio play:string(n) x:p.x y:p.y])throw std::runtime_error(audio.lastError.UTF8String);}
    void soundID(unsigned id,Vec2 p)override{sound(audio::soundFilename(id),p);}
    void sound(std::string_view n)override{sound(n,{});}
    void sfx(unsigned id)override{soundID(id,{});}
    void bgm(std::string_view n)override{if(![audio playMusic:string(n)])throw std::runtime_error(audio.lastError.UTF8String);}
    void alert(std::string_view key)override{notify({NativeRequest::Kind::alert,0,std::string(key),key=="IncorrectName"?"Incorrect Name":"Alert!"});}
    void alert(std::string_view key,std::string_view title)override{notify({NativeRequest::Kind::alert,0,std::string(key),std::string(title)});}
    void requestName(unsigned slot)override{notify({NativeRequest::Kind::name,slot,{},"New User"});}
    void requestDelete(unsigned slot)override{notify({NativeRequest::Kind::deleteSlot,slot,"DeleteSlot","Delete Slot"});}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason)override{
        NSData* contents=[NSData dataWithBytes:bytes.data() length:bytes.size()];NSError* error=nil;
        const BOOL success=[contents writeToURL:saveURL options:NSDataWritingAtomic error:&error];
        if(!success)notify({NativeRequest::Kind::saveError,0,std::string(error.localizedDescription.UTF8String)});
        return success;
    }
    std::uint32_t absoluteTimeLow()override{return std::uint32_t(mach_absolute_time());}
    std::uint32_t unixTime()override{return std::uint32_t(std::time(nullptr));}
    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& request)override{
        const auto bitmap=textCache.prepare(*rasterizer,request);auto i=textNames.find(bitmap.get());
        if(i==textNames.end()){
            const auto name="@text/"+std::to_string(textNames.size());[renderer prepareText:*bitmap identifier:name];i=textNames.emplace(bitmap.get(),name).first;
        }return {i->second,bitmap};
    }
    void action(const ui::ScreenAction& a)override{game->action(a);synchronize();}
    void synchronize(){
        const auto& state=game->state();
        const bool rebuilt=revision!=game->presentationRevision();
        if(rebuilt){
            ui->refresh(state,game->renderTime());revision=game->presentationRevision();Frame prepared;ui->append(prepared,game->renderTime());tiles.appendPreparation(prepared);[renderer prepareFrame:prepared];
        }else ui->updateHUD(state);
        if(rebuilt||game->battle()!=preparedWorld){preparedWorld=game->battle();if(preparedWorld){const auto prepared=game->preparationFrame();[renderer prepareFrame:prepared];}}
    }
};
GameRuntime::GameRuntime(NSURL* root,NSURL* save,std::function<void(const NativeRequest&)> callback):impl_(std::make_unique<Impl>(root,save,std::move(callback))){}
GameRuntime::~GameRuntime()=default;
void GameRuntime::tick(){auto& p=*impl_;if(!p.active)return;p.game->tick();p.synchronize();p.ui->tick(p.game->renderTime(),p.random);}
void GameRuntime::draw(CAEAGLLayer* layer){auto& p=*impl_;Frame frame=p.game->drawWorld();p.ui->append(frame,p.game->renderTime());[p.renderer drawFrame:frame layer:layer];}
bool GameRuntime::input(PointerPhase phase,Vec2 point,std::uintptr_t id){auto& p=*impl_;if(!p.active)return false;bool used=p.ui->input(phase,point,id,p.game->renderTime());if(!used)used=p.game->inputWorld(phase,point,id);p.synchronize();return used;}
bool GameRuntime::provideName(unsigned slot,std::string_view name,std::string_view country){
    NSString* value=string(name);if(!value||value.length<1||value.length>10){impl_->alert("IncorrectName");return false;}
    const bool result=impl_->game->createSlot(slot,name,country);impl_->synchronize();return result;
}
bool GameRuntime::confirmDelete(unsigned slot){const bool result=impl_->game->deleteSlot(slot);impl_->synchronize();return result;}
bool GameRuntime::logicalPoint(CGPoint point,Vec2& logical)const{return [impl_->renderer logicalPoint:point result:&logical];}
std::string GameRuntime::nativeText(std::string_view key)const{return impl_->game->originalText(key);}
void GameRuntime::setActive(bool active){if(!active)impl_->ui->discardInputs();impl_->active=active;impl_->game->setActive(active);[impl_->audio setActive:active];}
void GameRuntime::synchronize(){impl_->synchronize();}
GameSession& GameRuntime::session(){return *impl_->game;}
ui::Screens& GameRuntime::screens(){return *impl_->ui;}
std::uint64_t GameRuntime::resourceReads()const{std::uint64_t total=0;for(const auto& [name,count]:impl_->files.reads){(void)name;total+=count;}return total;}
std::size_t GameRuntime::preparedTextCount()const{return impl_->textCache.size();}
NSUInteger GameRuntime::textureUploads()const{return impl_->renderer.textureUploads;}
NSDictionary<NSString*,NSNumber*>* GameRuntime::audioMetrics()const{return impl_->audio.metrics;}
NSData* GameRuntime::pixels(){return impl_->renderer.readRGBA;}
void GameRuntime::captureNextFrame(){[impl_->renderer captureNextFrame];}
CGSize GameRuntime::drawableSize()const{return impl_->renderer.drawableSize;}
}
