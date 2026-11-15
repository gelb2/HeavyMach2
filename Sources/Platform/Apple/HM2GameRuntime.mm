#include "Platform/Apple/HM2GameRuntime.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Platform/Apple/HM2TextRasterizer.h"
#include "Platform/Apple/HM2HUDStorage.hpp"
#include "Platform/Apple/HM2Profiling.hpp"
#import "Platform/Apple/HM2Audio.h"
#import "Platform/Apple/HM2Renderer.h"
#include "Core/Audio/SoundIDs.hpp"
#include "Core/Presentation/DisplayLayout.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <mach/mach_time.h>
#include <ctime>
#include <map>
#include <array>

namespace hm2::apple {
namespace {
NSString* string(std::string_view value){return [[NSString alloc]initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];}
}
struct GameRuntime::Impl:SessionHost,ui::ScreensHost {
    NSURL* saveURL;HM2Renderer* renderer;HM2Audio* audio;
    std::function<void(const NativeRequest&)> request;
    std::function<std::uint32_t()> absoluteTime;
    Files files;DarwinWorldRandom random{1};PreparedTiles tiles{"screens"};
    std::unique_ptr<ui::TextRasterizer> rasterizer=makeTextRasterizer();ui::TextPreparationCache textCache;
    std::map<const ui::TextBitmap*,std::string> textNames;
    std::unique_ptr<GameSession> game;std::unique_ptr<ui::Screens> ui;
    std::uint64_t revision=0,resourceRevision=0,uiResourceGeneration=0,nextTextIdentity=0,hudLayoutWrites=0;CombatScene* preparedWorld=nullptr;bool active=true,audioActive=true;
    std::array<std::uintptr_t,16> worldPointers{};
    std::array<InputObservation,256> inputRing{};std::size_t inputCount=0;
    Impl(NSURL* root,NSURL* save,std::function<void(const NativeRequest&)> callback,std::function<std::uint32_t()> time):saveURL(save),request(std::move(callback)),absoluteTime(std::move(time)),files(root.fileSystemRepresentation){
        if(!root.isFileURL||!save.isFileURL)throw std::runtime_error("HM2 runtime requires local resource/save URLs");
        renderer=[[HM2Renderer alloc]initWithResourceRoot:root];audio=[[HM2Audio alloc]initWithResourceRoot:root];
        if(audio.lastError.length)throw std::runtime_error(audio.lastError.UTF8String);
        std::optional<SaveData> data;
        if([NSFileManager.defaultManager fileExistsAtPath:saveURL.path]){
            NSError* error=nil;NSData* bytes=[NSData dataWithContentsOfURL:saveURL options:0 error:&error];
            if(!bytes)throw std::runtime_error(error.localizedDescription.UTF8String);
            data=SaveData::decode({static_cast<const std::uint8_t*>(bytes.bytes),bytes.length},random);
        }
        game=std::make_unique<GameSession>(files,*this,random,std::move(data));
        for(const auto& name:game->orderedStartupSounds())prepareSound(name);
        ui=std::make_unique<ui::Screens>(files,tiles,*this);synchronize();
    }
    void notify(NativeRequest value){if(request)request(value);}
    void sound(std::string_view n,Vec2 p)override{if(!active||!audioActive)return;if(!n.empty()&&![audio playEffect:n x:p.x y:p.y])throw std::runtime_error(audio.lastError.UTF8String);}
    void soundID(unsigned id,Vec2 p)override{sound(audio::soundFilename(id),p);}
    void sound(std::string_view n)override{sound(n,{});}
    void sfx(unsigned id)override{soundID(id,{});}
    void prepareSound(std::string_view name)override{if(![audio prepareEffect:name])throw std::runtime_error(audio.lastError.UTF8String?:"Cannot prepare original sound reference");}
    void bgm(std::string_view n)override{if(![audio playMusic:string(n)])throw std::runtime_error(audio.lastError.UTF8String);}
    void alert(std::string_view key)override{notify({NativeRequest::Kind::alert,0,std::string(key),key=="IncorrectName"?"Incorrect Name":"Alert!"});}
    void alert(std::string_view key,std::string_view title)override{notify({NativeRequest::Kind::alert,0,std::string(key),std::string(title)});}
    void notice(std::string_view title,std::string_view message)override{notify({NativeRequest::Kind::notice,0,std::string(message),std::string(title)});}
    void requestName(unsigned slot)override{notify({NativeRequest::Kind::name,slot,{},"New User"});}
    void requestDelete(unsigned slot)override{notify({NativeRequest::Kind::deleteSlot,slot,"DeleteSlot","Delete Slot"});}
    void requestGameOver(std::uint64_t token)override{notify({NativeRequest::Kind::gameOver,0,"DestroyMach","Destroy Your Mach.",token});}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason)override{
        NSData* contents=[NSData dataWithBytes:bytes.data() length:bytes.size()];NSError* error=nil;
        const BOOL success=[contents writeToURL:saveURL options:NSDataWritingAtomic error:&error];
        if(!success)notify({NativeRequest::Kind::saveError,0,std::string(error.localizedDescription.UTF8String)});
        return success;
    }
    std::uint32_t absoluteTimeLow()override{return absoluteTime?absoluteTime():std::uint32_t(mach_absolute_time());}
    std::uint32_t unixTime()override{return std::uint32_t(std::time(nullptr));}
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& request)override{return prepareText(request);}
    std::optional<Bytes> loadHUDLayout(std::string_view profile)override{return readHUDLayout(saveURL.URLByDeletingLastPathComponent,profile);}
    bool saveHUDLayout(std::string_view profile,std::span<const std::uint8_t> bytes)override{const bool saved=writeHUDLayout(saveURL.URLByDeletingLastPathComponent,profile,bytes);if(saved)++hudLayoutWrites;return saved;}
    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& request)override{
        const auto bitmap=textCache.prepare(*rasterizer,request);auto i=textNames.find(bitmap.get());
        if(i==textNames.end()){
            const auto name="@text/"+std::to_string(nextTextIdentity++);[renderer prepareText:*bitmap identifier:name];i=textNames.emplace(bitmap.get(),name).first;
        }return {i->second,bitmap};
    }
    void action(const ui::ScreenAction& a)override{game->action(a);synchronize();}
    void synchronize(){
        const auto& state=game->state();
        const bool rebuilt=revision!=game->presentationRevision();
        if(rebuilt){
            ui->refresh(state,game->renderTime());revision=game->presentationRevision();Frame prepared;ui->appendPreparation(prepared);[renderer prepareFrame:prepared];
        }else ui->updateHUD(state);
        const bool resourcesChanged=rebuilt||game->battle()!=preparedWorld||resourceRevision!=game->resourceRevision()||uiResourceGeneration!=ui->resourceGeneration();
        if(resourcesChanged){
            preparedWorld=game->battle();resourceRevision=game->resourceRevision();uiResourceGeneration=ui->resourceGeneration();
            const auto prepared=game->preparationFrame();[renderer prepareFrame:prepared];
            // New nodes are built and old nodes released before reclaiming the
            // cache-only bitmaps. Shared live GUI/world text remains owned.
            for(const auto* discarded:textCache.discardUnused())textNames.erase(discarded);
            Frame owned=prepared;ui->appendPreparation(owned);
            for(const auto& [bitmap,name]:textNames){(void)bitmap;Sprite sprite;sprite.image=name;owned.sprites.push_back(std::move(sprite));}
            [renderer retainPreparedTextures:owned];
        }
    }
};
GameRuntime::GameRuntime(NSURL* root,NSURL* save,std::function<void(const NativeRequest&)> callback,std::function<std::uint32_t()> time):impl_(std::make_unique<Impl>(root,save,std::move(callback),std::move(time))){}
GameRuntime::~GameRuntime()=default;
void GameRuntime::tick(){auto& p=*impl_;if(!p.active)return;ProfileSpan phase(ProfileSpan::Phase::tick);p.game->tick([&p]{p.synchronize();p.ui->tick(p.game->renderTime(),p.random);});p.synchronize();}
void GameRuntime::draw(CAEAGLLayer* layer){auto& p=*impl_;Frame frame;{ProfileSpan phase(ProfileSpan::Phase::frameBuild);frame=p.game->drawWorld();p.ui->append(frame,p.game->renderTime());p.game->appendOverlays(frame);}{ProfileSpan phase(ProfileSpan::Phase::present);[p.renderer drawFrame:frame layer:layer];}}
bool GameRuntime::input(PointerPhase phase,Vec2 point,std::uintptr_t id){
    auto& p=*impl_;if(!p.active)return false;InputObservation observation;observation.phase=phase;observation.canvas=point;observation.pointer=id;
    if(const auto* scene=p.game->battle()){
        const auto before=scene->dashState();observation.movePointerBefore=before.movePointer;observation.pathBefore=before.pathCount;observation.tapBefore=before.tapTime;observation.specialBefore=before.special;
        observation.worldTimeBefore=scene->clock().worldTime();observation.dashStartsBefore=scene->metrics().dashStarts;observation.playerBefore=scene->player().position;
        const auto logical=p.game->display().enabled?scene->cameraPresentation().worldToCanvas.inverse(point):point;
        const auto& camera=scene->camera();const Vec2 base{camera.center.x-camera.viewport.width*.5f,camera.center.y-camera.viewport.height*.5f};
        const Vec2 shifted{base.x+camera.offset.x,base.y+camera.offset.y};observation.worldPoint={shifted.x+logical.x,shifted.y+logical.y};
    }
    bool used=observation.overlayConsumed=p.game->inputOverlay(phase,point,id);
    if(!used)used=observation.uiConsumed=p.ui->input(phase,point,id,p.game->renderTime());
    if(!used){used=observation.worldConsumed=p.game->inputWorld(phase,point,id);
        if(phase==PointerPhase::begin&&used&&id)for(auto& pointer:p.worldPointers)if(!pointer||pointer==id){pointer=id;break;}
    }
    if(phase==PointerPhase::end||phase==PointerPhase::cancel){
        for(auto& pointer:p.worldPointers)if(pointer==id&&id){
            // A modal/HUD can consume the terminal event after this pointer
            // began in the World. Cancel only that prior owner's capture;
            // never forward a new move/selection through a menu or help.
            if(observation.overlayConsumed||observation.uiConsumed){p.game->inputWorld(PointerPhase::cancel,point,id);observation.terminalCancelled=true;}
            pointer=0;
        }
    }
    if(const auto* scene=p.game->battle()){const auto after=scene->dashState();observation.movePointerAfter=after.movePointer;observation.pathAfter=after.pathCount;observation.tapAfter=after.tapTime;observation.specialAfter=after.special;observation.worldTimeAfter=scene->clock().worldTime();observation.dashStartsAfter=scene->metrics().dashStarts;}
    p.inputRing[p.inputCount++%p.inputRing.size()]=observation;p.synchronize();return used;
}
bool GameRuntime::provideName(unsigned slot,std::string_view name,std::string_view country){
    NSString* value=string(name);if(!value||value.length<1||value.length>10){impl_->alert("IncorrectName");return false;}
    const bool result=impl_->game->createSlot(slot,name,country);impl_->synchronize();return result;
}
bool GameRuntime::confirmDelete(unsigned slot){const bool result=impl_->game->deleteSlot(slot);impl_->synchronize();return result;}
bool GameRuntime::confirmGameOver(std::uint64_t token){const bool result=impl_->game->confirmGameOver(token);impl_->synchronize();return result;}
bool GameRuntime::logicalPoint(CGPoint point,Vec2& logical)const{return [impl_->renderer logicalPoint:point result:&logical];}
std::string GameRuntime::nativeText(std::string_view key)const{return impl_->game->originalText(key);}
void GameRuntime::setActive(bool active){if(!active){impl_->ui->discardInputs();impl_->worldPointers={};}impl_->active=active;impl_->game->setActive(active);[impl_->audio setActive:active&&impl_->audioActive];}
void GameRuntime::setAudioActive(bool active){
    auto& p=*impl_;if(p.audioActive==active)return;p.audioActive=active;
    if(!active){
        p.ui->discardInputs();p.worldPointers={};
        // Use the same common interruption pause/menu, then restore only the
        // foreground UI clock. Restoring audio never presses World Resume.
        p.game->setActive(false);p.game->setActive(p.active);
    }
    [p.audio setActive:p.active&&active];
    // Background callbacks must not issue GL preparation commands.
    if(p.active)p.synchronize();
}
void GameRuntime::synchronize(){impl_->synchronize();}
GameSession& GameRuntime::session(){return *impl_->game;}
ui::Screens& GameRuntime::screens(){return *impl_->ui;}
std::uint64_t GameRuntime::resourceReads()const{std::uint64_t total=0;for(const auto& [name,count]:impl_->files.reads){(void)name;total+=count;}return total;}
std::map<std::string,unsigned> GameRuntime::resourceReadCounts()const{return impl_->files.reads;}
std::size_t GameRuntime::preparedTextCount()const{return impl_->textCache.size();}
std::vector<std::string> GameRuntime::preparedTextIdentities()const{std::vector<std::string> names;names.reserve(impl_->textNames.size());for(const auto& [bitmap,name]:impl_->textNames){(void)bitmap;names.push_back(name);}return names;}
NSUInteger GameRuntime::textureUploads()const{return impl_->renderer.textureUploads;}
NSDictionary<NSString*,NSNumber*>* GameRuntime::audioMetrics()const{return impl_->audio.metrics;}
NSDictionary<NSString*,NSNumber*>* GameRuntime::resourceMetrics()const{
    auto& p=*impl_;return @{@"residentTextures":@(p.renderer.residentTextures),@"residentTextureBytes":@(p.renderer.residentTextureBytes),@"textureUploads":@(p.renderer.textureUploads),@"imageTextureUploads":@(p.renderer.imageTextureUploads),@"textureDeletes":@(p.renderer.textureDeletes),@"imageResourceReads":@(p.renderer.imageResourceReads),@"imageDecodes":@(p.renderer.imageDecodeCalls),@"textBitmaps":@(p.textCache.size()),@"textIdentities":@(p.textNames.size()),@"worldResourceRevision":@(p.game->resourceRevision()),@"uiResourceGeneration":@(p.ui->resourceGeneration()),@"hudLayoutWrites":@(p.hudLayoutWrites),@"thermalState":@(NSProcessInfo.processInfo.thermalState)};
}
std::vector<InputObservation> GameRuntime::inputHistory()const{const auto& p=*impl_;std::vector<InputObservation> result;const auto count=std::min(p.inputCount,p.inputRing.size());result.reserve(count);for(std::size_t i=p.inputCount-count;i<p.inputCount;++i)result.push_back(p.inputRing[i%p.inputRing.size()]);return result;}
NSData* GameRuntime::pixels(){return impl_->renderer.readRGBA;}
void GameRuntime::captureNextFrame(){[impl_->renderer captureNextFrame];}
CGSize GameRuntime::drawableSize()const{return impl_->renderer.drawableSize;}
}

namespace hm2::apple {void GameRuntime::setDisplay(DisplayConfiguration display){auto& p=*impl_;p.worldPointers={};p.ui->discardInputs();p.game->setDisplay(display);p.ui->setDisplay(display);Frame frame;p.ui->appendPreparation(frame);[p.renderer prepareFrame:frame];[p.renderer prepareFrame:p.game->preparationFrame()];}}
