#include "Core/UI/Screens.hpp"
#include "Core/Data/DataTable.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
#include "Core/Presentation/MenuGridLayout.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace hm2::ui {
namespace {
Vec2 plus(Vec2 a,Vec2 b){return {a.x+b.x,a.y+b.y};}
Owner owner(ScreenId screen,Panel panel){return (Owner(screen)<<16)|Owner(panel);}
const ItemView emptyItem;
const ItemView& itemAt(const std::vector<ItemView>& items,std::size_t index){return index<items.size()?items[index]:emptyItem;}
using Region=DisplayRegion;
std::shared_ptr<const std::array<Sprite,hud::count>> croppedHUDBackground(VisualImage image){
    // Resolve the original Tile2D non-atlas geometry/UV/blend once when its
    // owner is built. A moved HUD crops only that existing texture's quad.
    VisualLayer original(1);original.submitImage(image);const auto resolved=original.drain();
    if(resolved.nonAtlas.size()!=1||resolved.nonAtlas.front().strip.size()!=4)throw std::logic_error("Original HUD background requires one Tile2D quad");
    const auto& full=resolved.nonAtlas.front();auto pieces=std::make_shared<std::array<Sprite,hud::count>>();
    const auto interpolate=[](const StripVertex& a,const StripVertex& b,float t){auto result=a;result.position={std::lerp(a.position.x,b.position.x,t),std::lerp(a.position.y,b.position.y,t)};result.uv={std::lerp(a.uv.x,b.uv.x,t),std::lerp(a.uv.y,b.uv.y,t)};return result;};
    for(unsigned group=0;group<hud::count;++group){auto& piece=(*pieces)[group];piece=full;const auto slice=hud::backgroundSlices[group];const float left=slice.x/image.tile.content.width,right=(slice.x+slice.width)/image.tile.content.width;
        piece.strip={interpolate(full.strip[0],full.strip[2],left),interpolate(full.strip[1],full.strip[3],left),interpolate(full.strip[0],full.strip[2],right),interpolate(full.strip[1],full.strip[3],right)};
    }
    return pieces;
}
struct ImageNode {VisualImage image;unsigned layer=7;std::string tag;std::shared_ptr<Button> parent{};Vec2 local{};Region region=Region::content;int hudGroup=-1;std::shared_ptr<const std::array<Sprite,hud::count>> hudBackgroundParts;std::shared_ptr<const Sprite> menuPanel;};
struct ButtonNode {std::shared_ptr<Button> button;unsigned layer=7;Region region=Region::content;int hudGroup=-1;float angle=0;};
struct TextNode {std::shared_ptr<Text> text;unsigned layer=7;std::shared_ptr<Button> parent{};Vec2 local{};Region region=Region::content;int hudGroup=-1;};
struct NumberNode {std::shared_ptr<ImageFont> font;TileImage* tile;unsigned layer=7;std::string tag;Region region=Region::content;int hudGroup=-1;};
struct PreviewNode {Frame frame;Vec2 origin;unsigned slot;std::uint64_t revision;unsigned layer=7;Region region=Region::content;int hudGroup=-1;};
struct RadarNode {VisualTile tile;unsigned layer=9;Region region=Region::content;int hudGroup=-1;};
struct BaseDescriptionNode {
    VisualImage background;
    std::shared_ptr<Text> text;
    float localX;
    float targetX;
    unsigned layer=7;Region region=Region::content;int hudGroup=-1;
};
enum class DetailKind {shop,inventory,maintenance};
struct DetailPresentation {
    DetailKind kind=DetailKind::shop;std::uint64_t revision=0;int absolute=-1;
    ItemView entry;Vec2 origin,descPos;float x=.01f,y=.01f,phoneScale=1;bool vertical=false,up=false,phoneArtwork=false;
    unsigned layer=9;std::shared_ptr<Button> background;
    std::shared_ptr<Text> title,description;std::optional<VisualImage> large;
};
struct DetailNode {std::shared_ptr<DetailPresentation> detail;unsigned layer=9;Region region=Region::content;int hudGroup=-1;};
struct FieldPresentation {
    std::uint64_t identity=0,enemyRevision=0;bool range[2]={false,false};
    std::shared_ptr<Text> enemyName;
    std::int32_t displayedCredits=0;
};
enum class CommandPart {middle,top,bottom,row,detail,accept};
struct CommandNode {
    std::size_t index;
    CommandPart part;
    float baseY;
    int row;
};
struct CommandPresentation {
    float size=0,displayedSize=0,detailY=0;
    int displayedSelection=-1,detailSelection=-1;
};
using Node=std::variant<ImageNode,ButtonNode,TextNode,NumberNode,PreviewNode,DetailNode,RadarNode,BaseDescriptionNode>;
struct MessagePresentation {
    VisualImage background;
    std::shared_ptr<Text> text;
    std::uint64_t revision=0;
    float start=0;
    bool initialized=false,hiding=false,autoHide=false;
};
struct PlaceNamePresentation {
    std::uint64_t revision=0;
    VisualImage background;
    std::shared_ptr<Text> shadow,label;
};
struct TitlePresentation {
    float start=0,bgScale=1.8f,bgRotate=-.05f,titleScale=50,episodeScale=40,bigX=1024;
    Vec2 bgPosition{100,-10};
    bool bigVisible=true,titleVisible=false,episodeVisible=false,startVisible=false;
    bool titleSound=false,episodeSound=false,startSound=false;
};
}
struct Screens::HUDState {
    std::map<std::string,hud::Configuration> saved;
    std::string profile,error;
    hud::Configuration draft;
    bool editing=false,rebuild=false;
    std::uintptr_t dragPointer=0;unsigned selected=hud::gauges;Vec2 dragStart,dragOrigin;
    void selectProfile(const DisplayConfiguration& display,ScreensHost& host){
        const std::string next=display.compact?"compact":"regular";
        if(next==profile)return;
        editing=false;dragPointer=0;profile=next;error.clear();
        if(saved.contains(profile))return;
        hud::Configuration config;
        try{if(auto data=host.loadHUDLayout(profile))config=hud::upgrade(hud::decode(*data),DisplayLayout(display));}
        catch(const std::exception&){error="HUD 설정을 읽지 못했습니다. 기본 배치를 표시합니다.";}
        saved.emplace(profile,config);
    }
    const hud::Configuration& active()const{return editing?draft:saved.at(profile);}
    void begin(){draft=saved.at(profile);editing=true;dragPointer=0;rebuild=true;}
    void cancel(){editing=false;dragPointer=0;error.clear();rebuild=true;}
    void reset(){if(editing){draft={};dragPointer=0;error.clear();rebuild=true;}}
    bool apply(ScreensHost& host){
        if(!editing)return false;
        bool success=false;try{success=host.saveHUDLayout(profile,hud::encode(draft));}catch(const std::exception&){}
        if(!success){error="HUD 배치를 저장하지 못했습니다. 다시 적용해 주세요.";rebuild=true;return false;}
        saved[profile]=draft;cancel();return true;
    }
};
struct Screens::Impl {
    std::shared_ptr<std::uint64_t> resourceGeneration;
    ResourceAccess& resources;PreparedTiles& tiles;std::shared_ptr<PreparedTiles> tileOwner;ScreensHost& host;ScreensState state;
    std::vector<Node> nodes;
    DisplayLayout layout;
    std::shared_ptr<HUDState> hud;int currentHUDGroup=-1;
    std::map<const Button*,int> buttonHUDGroups;
    Region region=Region::content;
    float verticalPan=0,verticalOrigin=0;Vec2 verticalStart;std::uintptr_t verticalPointer=0;bool verticalDragging=false;
    float shopPan=0,panOrigin=0;Vec2 panStart;std::uintptr_t panPointer=0;
    bool phoneEquipment=false;unsigned stockWindow=0,inventoryWindow=0,commandWindow=0;bool pageRebuild=false;
    float listTop=1024,listBottom=0;
    std::map<const Button*,Region> buttonRegions;
    struct RegionScope {Impl& self;Region before;RegionScope(Impl& p,Region value):self(p),before(p.region){p.region=value;}~RegionScope(){self.region=before;}};
    template<class T> void pushNode(T node){
        node.region=region;node.hudGroup=currentHUDGroup;
        if(menu::phone(layout.configuration())&&menu::menuRegion(region)){
            if constexpr(std::is_same_v<T,ImageNode>)node.image.scale={node.image.scale.x*1.5f,node.image.scale.y*1.5f};
            if constexpr(std::is_same_v<T,ButtonNode>){node.button->scale={node.button->scale.x*1.5f,node.button->scale.y*1.5f};}
            if constexpr(std::is_same_v<T,TextNode>)node.text->scale={node.text->scale.x*1.5f,node.text->scale.y*1.5f};
            if constexpr(std::is_same_v<T,NumberNode>){node.font->scale={1.5f,1.5f};node.font->tick();}
        }
        if constexpr(std::is_same_v<T,ImageNode>){
            if(node.tag=="hud.background"&&splitHUDBackground())node.hudBackgroundParts=croppedHUDBackground(node.image);
            if(node.tag=="title.bg")node.region=menu::phone(layout.configuration())?Region::titleBackdrop:Region::backdrop;
            else if(layout.configuration().compact&&(node.tag=="menu.backdrop"||node.image.tile.image=="SelSlot_BG.png"||node.image.tile.image=="CmdCenterBG.png"))node.region=Region::backdrop;
            else if(node.tag=="title.big")node.region=menu::phone(layout.configuration())?Region::artBanner:Region::banner;
            else if(node.tag.starts_with("hud.enemy"))node.region=Region::top;
            else if(state.screen==ScreenId::shop&&node.image.tile.image=="ShopBG.png")node.region=Region::shopBackdrop;
        }
        if constexpr(std::is_same_v<T,ButtonNode>){
            const auto id=node.button->key().id;
            if(phoneMenu()){
                if(id==2100)node.region=Region::phoneFooter;
                if(id==9204)node.region=Region::phoneStockFooter;
                if(id==9205)node.region=Region::phoneEquipmentFooter;
            }
            if(phoneMenu()&&id==6093)node.region=Region::phoneHeader;
            if(state.screen==ScreenId::slots&&id!=1009)node.region=Region::bottom;
            if(state.screen==ScreenId::shop&&id==2100&&!phoneMenu())node.region=Region::shopFooter;
            if(state.screen==ScreenId::shop&&id==2103&&!phoneMenu())node.region=Region::shopInventory;
            if(state.screen==ScreenId::command&&(id==5009||id==2100)&&!menu::phone(layout.configuration()))node.region=Region::bottom;
            if(menu::phone(layout.configuration())&&state.screen==ScreenId::base&&id==2004){const auto b=node.button->bounds();node.button->position=layout.keepControlVisible(node.region,node.button->position,{b.left,b.bottom,b.right-b.left,b.top-b.bottom});}
            buttonRegions[node.button.get()]=node.region;buttonHUDGroups[node.button.get()]=node.hudGroup;
        }
        if(node.region==Region::top)node.hudGroup=-1;
        nodes.push_back(std::move(node));
    }
    static constexpr unsigned stockCells(){return menu::phoneShopRows*4;}
    static constexpr unsigned inventoryCells(){return menu::phoneShopRows*5;}
    std::pair<unsigned,unsigned> phoneInventoryRange()const{
        const auto begin=std::min(25u,inventoryWindow*inventoryCells());
        return {begin,std::min(25u,begin+inventoryCells())};
    }
    bool phoneMenu()const{return menu::phone(layout.configuration())&&(state.screen==ScreenId::shop||state.screen==ScreenId::maintenance);}
    bool phoneNodeVisible(Region r)const{
        if(!phoneMenu())return true;
        if(r==Region::phoneEquipment||r==Region::phoneEquipmentFooter)return phoneEquipment;
        if(r==Region::phoneStock||r==Region::phoneStockHeader||r==Region::phoneCategoryHeader||r==Region::phoneStockFooter)return !phoneEquipment;
        return true;
    }
    void setPhoneEquipment(bool value){
        phoneEquipment=value;
        for(const auto& [button,r]:buttonRegions)const_cast<Button*>(button)->visible=phoneNodeVisible(r);
    }
    Region scrollingRegion()const{return state.weaponChoiceType?Region::chooser:Region::list;}
    float verticalLimit()const{if(menu::phone(layout.configuration())&&(state.screen==ScreenId::base||state.screen==ScreenId::shop||state.screen==ScreenId::maintenance||state.screen==ScreenId::command))return 0;if(!layout.configuration().enabled)return 0;const auto r=scrollingRegion();if(!layout.configuration().compact&&r==Region::list)return 0;if(r==Region::list&&state.screen!=ScreenId::command&&!state.missionVisible)return 0;const float height=r==Region::chooser?std::max(1024.f,float(state.weaponChoices.size())*36+200):listTop-listBottom+12;return std::max(0.f,height*layout.ui(r).scale-layout.clip(r)->height);}
    bool tabletMenu()const{return layout.configuration().enabled&&!menu::phone(layout.configuration())&&(state.screen==ScreenId::base||state.screen==ScreenId::shop||state.screen==ScreenId::command);}
    DisplayTransform presentation(Region r)const{
        if(tabletMenu())return layout.ui(Region::tabletMenu);
        if(phoneMenu()&&(r==Region::phoneHeader||r==Region::phoneFooter))
            return menu::phoneReferenceUI(layout.configuration());
        auto transform=layout.ui(r,r==Region::artScene||menu::menuRegion(r)?0.f:r==Region::list||r==Region::chooser?verticalPan:shopPan);
        if(r==Region::list&&layout.configuration().enabled&&layout.configuration().compact)transform.offset.y+=(1024-listTop)*transform.scale-8;
        return transform;
    }
    DisplayTransform presentation(Region r,int group)const{
        auto t=presentation(r);if(group<0||!hud)return t;
        const auto base=layout.ui(Region::bottom),custom=hud::transform(hud->active(),unsigned(group),layout);
        // A normalized setting roundtrip can introduce subpixel float noise
        // at its exact original position. Keep identity geometry bit-exact.
        const auto delta=[](float a,float b){return std::abs(a-b)<.0001f?0.f:a-b;};
        t.offset.x+=delta(custom.offset.x,base.offset.x);t.offset.y+=delta(custom.offset.y,base.offset.y);return t;
    }
    std::optional<ClipRect> clipping(Region r,int group=-1)const{
        if(tabletMenu())return layout.clip(Region::tabletMenu);
        if(group>=0&&hud&&hud->active().customized()){
            return hud::originalClip(hud->active(),unsigned(group),layout);
        }
        return layout.clip(r,r==Region::artScene||menu::menuRegion(r)?0.f:shopPan);
    }
    int buttonHUDGroup(const Button& b)const{const auto i=buttonHUDGroups.find(&b);return i==buttonHUDGroups.end()?-1:i->second;}

    Region buttonRegion(const Button& button)const{const auto found=buttonRegions.find(&button);return found==buttonRegions.end()?Region::content:found->second;}
    bool applyHUDEdit(){
        if(!hud->editing)return false;
        struct Hit {int group;ClipRect rect;};std::vector<Hit> hits;
        for(const auto& key:keys)if(const auto button=dispatcher.find(key);button&&button->visible){
            const int group=buttonHUDGroup(*button);if(group<0)continue;
            const auto t=presentation(buttonRegion(*button),group);const auto b=button->bounds();const auto p=button->screenPosition();
            const auto low=t.map({p.x+b.left,p.y+b.bottom});hits.push_back({group,{low.x,low.y,(b.right-b.left)*t.scale,(b.top-b.bottom)*t.scale}});
        }
        for(std::size_t i=0;i<hits.size();++i)for(std::size_t j=i+1;j<hits.size();++j){
            const auto& a=hits[i];const auto& b=hits[j];if(a.group==b.group||(!hud->draft.positions[a.group]&&!hud->draft.positions[b.group]))continue;
            if(std::min(a.rect.x+a.rect.width,b.rect.x+b.rect.width)>std::max(a.rect.x,b.rect.x)&&std::min(a.rect.y+a.rect.height,b.rect.y+b.rect.height)>std::max(a.rect.y,b.rect.y)){
                hud->error="버튼이 겹칩니다. HUD 조각을 옮겨 주세요.";hud->rebuild=true;return false;
            }
        }
        return hud->apply(host);
    }

    ButtonDispatcher dispatcher;
    std::vector<ButtonKey> keys;
    std::map<Owner,Panel> panels;
    std::shared_ptr<std::array<VisualLayer,14>> layers;
    std::shared_ptr<Impl> previous;
    TitlePresentation title;
    MessagePresentation message;
    PlaceNamePresentation placeName;
    std::vector<std::pair<std::shared_ptr<Button>,Vec2>> chooserMoves;
    float now=0;
    std::shared_ptr<DetailPresentation> detail;
    std::optional<std::uint64_t> dismissedSaleDetail;
    FieldPresentation fieldPresentation;
    CommandPresentation command;
    std::vector<CommandNode> commandNodes;
    float commandSelectedRowY=0;
    bool baseNoticeVisible=true;
    Impl(ResourceAccess& r,PreparedTiles& t,ScreensHost& h,const ScreensState& s,float time,std::shared_ptr<Impl> old,DisplayConfiguration display,std::shared_ptr<HUDState> hudState,std::shared_ptr<PreparedTiles> ownerTiles,std::shared_ptr<std::uint64_t> generation)
        :resourceGeneration(std::move(generation)),resources(r),tiles(t),tileOwner(std::move(ownerTiles)),host(h),state(s),layout(display),hud(std::move(hudState)),previous(std::move(old)),now(time){
        ++*resourceGeneration;
        if(previous&&previous->state.screen==s.screen){if(menu::phone(previous->layout.configuration())==menu::phone(display))title=previous->title;else{const auto art=menu::title(display);title.bigX=art.bigStart;title.bgPosition=art.bgStart;title.start=time;}commandWindow=previous->commandWindow;phoneEquipment=previous->phoneEquipment;stockWindow=previous->state.shopCategory==s.shopCategory?previous->stockWindow:0;inventoryWindow=previous->inventoryWindow;message=previous->message;layers=previous->layers;shopPan=phoneMenu()?0.f:std::min(previous->shopPan,layout.shopPanLimit());if(previous->state.weaponChoiceType==s.weaponChoiceType&&previous->state.missionVisible==s.missionVisible)verticalPan=std::min(previous->verticalPan,verticalLimit());}
        else{const auto art=menu::title(display);title.bigX=art.bigStart;title.bgPosition=art.bgStart;title.start=time;layers=std::make_shared<std::array<VisualLayer,14>>();}
        if(phoneMenu()&&s.screen==ScreenId::maintenance)phoneEquipment=true;
        if(previous){
            if(previous->state.baseId==state.baseId&&phoneMenu()&&previous->phoneMenu()){
                if(previous->state.screen==ScreenId::maintenance&&state.screen==ScreenId::shop&&state.inventoryPage==previous->state.inventoryPage)
                    inventoryWindow=previous->inventoryWindow;
            }
            fieldPresentation=previous->fieldPresentation;placeName=previous->placeName;
        }
        // accept5004 only invalidates Command.curState. Keep its existing rows
        // until the following Command tick actually refreshes the list.
        if(phoneMenu()){
            const auto offset=std::size_t(std::max(0,state.shopPage))*24;
            const auto remaining=state.shopItems.size()-std::min(offset,state.shopItems.size());
            const auto count=unsigned(std::min<std::size_t>(24,remaining));
            stockWindow=std::min(stockWindow,count?(count-1)/stockCells():0u);
        }
        if(previous&&s.screen==ScreenId::command&&previous->state.screen==s.screen&&s.commandListRevision&&s.commandListRevision==previous->state.commandListRevision){
            state.availableMissions=previous->state.availableMissions;state.acceptedMissions=previous->state.acceptedMissions;
            state.selectedMission=-1;
            const auto select=[&](const std::vector<ItemView>& list,int first){for(std::size_t i=0;i<list.size();++i)if(!s.selectedMissionDataId.empty()&&list[i].dataId==s.selectedMissionDataId&&state.selectedMission<0)state.selectedMission=first+int(i);};
            select(state.availableMissions,0);select(state.acceptedMissions,int(state.availableMissions.size()));
        }
        if(fieldPresentation.identity!=s.fieldUIRevision){fieldPresentation={};fieldPresentation.identity=s.fieldUIRevision;}
    }
    ~Impl(){++*resourceGeneration;}
    void hideMessage(){message.hiding=true;if(message.text){message.text.reset();++*resourceGeneration;}}
    void syncMessage(){
        if(state.screen!=ScreenId::field)return;
        auto& m=message;
        if(!m.initialized){
            if(auto* prepared=tile("MessageBox.png",1,2))m.background.tile=tiles.visual(*prepared);
            m.background.screenPosition={405,158};m.background.show=false;m.initialized=true;
        }
        if(m.revision==state.messageRevision)return;m.revision=state.messageRevision;
        if(!state.messageVisible){hideMessage();return;}
        m.background.tileNo=unsigned(state.messageBackground);m.background.scale={1,1};m.background.show=true;m.hiding=false;m.autoHide=state.messageAutoHide;
        m.text.reset();++*resourceGeneration;TextRasterRequest request{"GuiGame.message",state.message,"TrebuchetMS-Bold",{512,24},16,TextAlign::center,false,0};
        auto prepared=host.prepareText(request);if(!prepared.bitmap||!(prepared.bitmap->request==request))throw std::runtime_error("Message text preparation changed the original request");
        m.text=std::make_shared<Text>(std::move(prepared.image),std::move(prepared.bitmap));m.text->setColor(255,255,255);m.text->position=m.background.screenPosition;
        m.start=float(state.messageWorldTime);host.sfx(10);
    }
    void tickMessage(){
        auto& m=message;if(state.selectedSlot<0||m.start==0)return;
        const float age=float(state.messageWorldTime-double(m.start));
        if(age>.6f){m.background.screenPosition.y=158;if(m.autoHide&&age>10.f)hideMessage();}
        else{const float q=age/.6f;const float angle=float(double(q)*std::numbers::pi*3.0);const float wave=std::sin(angle);const float amplitude=(.6f-age)*16.f;m.background.screenPosition.y=158.f+std::abs(wave*amplitude);}
        if(m.hiding){
            if(m.background.scale.y>.1f){m.background.scale.y+=(.09f-m.background.scale.y)*.25f;if(m.background.scale.y<=.1f)m.background.scale.y=.1f;}
            else{m.background.scale.x+=(.04f-m.background.scale.x)*.25f;if(m.background.scale.x<=.05f){m.background.show=false;m.start=0;m.hiding=false;}}
        }
        if(m.text)m.text->position=m.background.screenPosition;
    }
    TileImage* tile(std::string_view name,unsigned columns=1,unsigned rows=1){
        if(name.starts_with("PhoneMenu/")&&!menu::phone(layout.configuration()))return nullptr;
        auto* t=tiles.prepare(resources,name);if(t){t->columns=columns;t->rows=rows;if(name.starts_with("PhoneMenu/"))t->forRetina=menu::retina(name);else if(name=="PhoneShopPanel.png"||name=="PhoneEquipPanel.png")t->forRetina=true;}return t;
    }
    void image(std::string_view name,Vec2 position,unsigned columns=1,unsigned rows=1,unsigned index=0,
               Vec2 scale={1,1},bool atlas=false,unsigned blend=0,float alpha=1,unsigned layer=7,std::string tag={}){
        auto* t=tile(name,columns,rows);if(!t)return;
        VisualImage visual;visual.tile=tiles.visual(*t);visual.tileNo=index;visual.screenPosition=position;visual.scale=scale;
        visual.useAtlas=atlas;visual.blendType=blend;visual.alpha=alpha;
        pushNode(ImageNode{std::move(visual),layer,std::move(tag)});
    }
    std::shared_ptr<Button> addButton(Panel panel,unsigned id,std::string_view name,Vec2 position,
        unsigned columns=1,unsigned rows=2,unsigned normal=0,unsigned pressed=1,std::optional<Size> bounds={},
        int intData=0,std::string dataId={},Vec2 scale={1,1},bool enabled=true,unsigned layer=9,bool scaleBeforeBounds=true,bool reuse=true){
        ButtonKey key{owner(state.screen,panel),id,intData,std::move(dataId)};
        std::optional<ButtonTile> buttonTile;
        if(auto* t=tile(name,columns,rows))buttonTile=ButtonTile{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalTileBlend(0,t->pixelFormat)};
        std::shared_ptr<Button> button;
        if(reuse&&previous)button=previous->dispatcher.find(key);
        if(!button)button=std::make_shared<Button>(key,buttonTile,normal);
        else{button->setDefaultTile(0);button->setTile(buttonTile);}
        button->setDefaultTile(normal);button->setReleaseTile(pressed);button->setDeactiveTile(normal);button->positionSnap=PositionSnap::none;
        button->position=position;button->scale=scaleBeforeBounds?scale:Vec2{1,1};button->visible=true;button->active=enabled;
        if(bounds)button->setBoundWidthHeight(bounds->width,bounds->height);
        button->scale=scale;dispatcher.add(button);keys.push_back(key);panels[key.owner]=panel;
        pushNode(ButtonNode{button,layer});return button;
    }
    std::shared_ptr<Text> prepareText(const TextRasterRequest& request){
        auto prepared=host.prepareText(request);
        if(!prepared.bitmap||!(prepared.bitmap->request==request))throw std::runtime_error("UI text preparation changed the original request");
        auto result=std::make_shared<Text>(std::move(prepared.image),std::move(prepared.bitmap));++*resourceGeneration;return result;
    }
    void syncPlaceName(){
        if(state.screen!=ScreenId::field||placeName.revision==state.fieldPlaceRevision)return;
        placeName={};++*resourceGeneration;
        placeName.revision=state.fieldPlaceRevision;
        if(!placeName.revision)return;
        if(auto* background=tile("PlaceNameBG.png")){
            placeName.background.tile=tiles.visual(*background);
            placeName.background.screenPosition={-1024,512};
        }
        if(!state.fieldPlaceName.empty()){
            placeName.label=prepareText({"GuiGame.placeName",state.fieldPlaceName,"Arial",{256,40},32,TextAlign::center,false,0});
            placeName.shadow=std::make_shared<Text>(*placeName.label);
            placeName.shadow->setColor(0,0,0);
        }
    }
    void tickPlaceName(){
        auto& background=placeName.background;
        if(!state.hasWorkingSlot||!background.show||background.tile.image.empty())return;
        // GuiGame.tick0x29508 approaches the center, then the right edge.
        // QobBase.easyOutTo0x36b48 snaps within the supplied minimum.
        auto& x=background.screenPosition.x;
        const float destination=x<384.f?384.f:900.f;
        const float distance=destination-x;
        x=distance>.1f?x+distance/12.f:destination;
        if(x>=900.f){
            background.show=false;
            if(placeName.shadow||placeName.label)++*resourceGeneration;
            placeName.shadow.reset();
            placeName.label.reset();
        }
    }
    void text(std::string id,std::string value,Vec2 position,Size size,const char* font,float fontSize,TextAlign align,
              Color color={1,1,1,1},unsigned layer=9,Vec2 scale={1,1}){
        if(value.empty())return;
        auto label=prepareText({std::move(id),std::move(value),font,size,fontSize,align,false,0});
        label->position=position;label->scale=scale;
        label->setColor(std::uint8_t(color.r*255),std::uint8_t(color.g*255),std::uint8_t(color.b*255));label->alpha=color.a;
        pushNode(TextNode{std::move(label),layer});
    }
    void number(std::string_view name,int value,Vec2 position,float pitch,float align,unsigned layer=9,bool star=false,std::string tag={},unsigned columns=16){
        auto* t=tile(name,columns,1);if(!t)return;
        NumericAtlas a{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalAtlasBlend(0,t->pixelFormat)};
        auto font=std::make_shared<ImageFont>(std::move(a));font->position=position;if(pitch!=0)font->setPitch(pitch);font->setAlignRate(align);
        if(star)font->setText("*");else font->setNumber(value);font->tick();
        pushNode(NumberNode{std::move(font),t,layer,std::move(tag)});
    }
    void numericText(std::string_view name,std::string value,Vec2 at,float pitch,float align){
        auto* t=tile(name,16,1);if(!t)return;
        auto font=std::make_shared<ImageFont>(NumericAtlas{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalAtlasBlend(0,t->pixelFormat)});
        font->position=at;font->setPitch(pitch);font->setAlignRate(align);font->setText(value);font->tick();
        pushNode(NumberNode{std::move(font),t,9,{}});
    }
    static Frame translatedPreview(const Frame& frame,Vec2 origin){
        // QobBase child world position is local+parent. Translate once for
        // each pose revision, including atlas vertices. Keep the source frame.
        Frame prepared=frame;
        for(auto& sprite:prepared.sprites){
            sprite.center=plus(sprite.center,origin);
            for(auto& vertex:sprite.strip)vertex.position=plus(vertex.position,origin);
            if(sprite.atlasBatch){
                auto translated=std::make_shared<AtlasBatch>(*sprite.atlasBatch);
                for(auto& quad:translated->quads)for(auto& vertex:quad.vertices)
                    vertex.position=plus(vertex.position,origin);
                sprite.atlasBatch=std::move(translated);
            }
        }
        return prepared;
    }
    void icon(const ItemView& item,Vec2 position,unsigned layer=9){
        if(!item.icon.empty())image(item.icon,position,item.iconColumns,item.iconRows,item.iconTile,{1,1},false,0,1,layer);
    }
    void slots(){
    RegionScope scope(*this,Region::content);
        const auto firstNode=nodes.size();
        if(menu::phone(layout.configuration())){RegionScope backdrop(*this,Region::artBackdrop);image(menu::background(layout.configuration(),"SelSlot_BG.png"),{240,160});}else image("SelSlot_BG.png",{384,512});
        addButton(Panel::root,1010,"Common_Btn_DEL.png",{224,80},1,2,0,1,Size{80,40});
        addButton(Panel::root,1004,"Common_Btn_BACK.png",{384,80},1,2,0,1,Size{80,40});
        addButton(Panel::root,1001,"Common_Btn_OK.png",{544,80},1,2,0,1,Size{80,40});
        if(state.recoveryCodeAvailable){
            const Vec2 position{685,80};
            addButton(Panel::root,recoveryButtonId,"",position,1,1,0,0,Size{122,44});
            RegionScope footer(*this,Region::bottom);
            text("GuiSelectSlot.transfer",state.locale=="kr"?"저장 이동":"Transfer",
                position,{122,40},"Arial",20,TextAlign::center,{.65f,1,.99f,1});
        }
        for(unsigned i=0;i<3;++i){
RegionScope card(*this,Region(unsigned(Region::slot0)+i));
            const auto& slot=state.slots[i];const Vec2 origin{384,760-float(i)*240};
            addButton(Panel::root,1009,slot.used?"SelSlot_Sel.png":"SelSlot_Empty.png",origin,1,2,
                state.selectedSlot==int(i)?0:1,0,{},int(i));
            if(!slot.used)continue;
            const std::string source="GuiSelectSlot.slot"+std::to_string(i);
            text(source+".name.shadow",slot.name,plus(origin,{70,64}),{256,32},"Copperplate-Bold",24,TextAlign::left,{0,0,0,1});
            text(source+".name",slot.name,plus(origin,{70,65}),{256,32},"Copperplate-Bold",24,TextAlign::left,{160.f/255,1,1,1});
            if(!slot.country.empty())image(slot.country+".png",plus(origin,{-83,68}));
            image("SelSlot_ExpGauge.png",plus(origin,{slot.experienceRate*123-39,2}),1,1,0,{slot.experienceRate,1});
            number("NumSet_Cr.png",slot.level,plus(origin,{-16,36}),12,0);
            numericText("NumSet01b.png",slot.createdAt,plus(origin,{-80,-44}),6,0);
            numericText("NumSet01b.png",slot.savedAt,plus(origin,{-80,-66}),6,0);
            numericText("NumSet_Cr.png",std::to_string(slot.missionPercent)+"%",plus(origin,{146,36}),12,0);
            text(source+".saved.shadow",slot.location,plus(origin,{143,-41}),{256,16},"Trebuchet-BoldItalic",14,TextAlign::center,{0,0,0,1});
            text(source+".saved",slot.location,plus(origin,{142,-40}),{256,16},"Trebuchet-BoldItalic",14,TextAlign::center,{100.f/255,220.f/255,240.f/255,1});
            number("NumSet_Cr.png",slot.credits,plus(origin,{180,-69}),11,1);
            const Vec2 machOrigin=plus(origin,{-170,34});
            pushNode(PreviewNode{translatedPreview(slot.machPreview,machOrigin),machOrigin,i,slot.machPreviewRevision});
        }
        // GameMain layer3 is inherited through GuiSelectSlot/slotBase.
        // Explicit part child layers (notably shadow0) remain on each Sprite.
        for(std::size_t i=firstNode;i<nodes.size();++i)std::visit([](auto& node){node.layer=3;},nodes[i]);
    }
    void base(){
        const auto name=state.baseId+".tble";
        const auto table=DataTable::parse(resources.read(name));const Vec2 root{384,512};
        const bool compact=menu::phone(layout.configuration());
        const auto artwork=compact?std::optional<DataTable>(DataTable::parse(resources.read(menu::phoneAsset(name)))):std::nullopt;
        const auto visualRow=[&](std::string_view type,int id){if(!artwork)throw std::logic_error("Phone artwork profile absent");for(std::size_t row=0;row<artwork->rowCount();++row)if(artwork->value(row,"Type")==type&&artwork->integer(row,"ID")==id)return row;throw std::runtime_error("Original phone base has no matching visual ID");};
        std::optional<Vec2> commandPosition,descriptionPosition;
        for(std::size_t i=0;i<table.rowCount();++i){
            const auto kind=table.value(i,"Type").value_or("");const int id=table.integer(i,"ID");
            const auto row=compact&&(kind=="Image"||kind=="Button"||kind=="Desc")?visualRow(kind,id):i;
            const auto& source=compact&&(kind=="Image"||kind=="Button"||kind=="Desc")?*artwork:table;
            const auto originalAsset=source.value(row,"ImageFile").value_or("");
            const auto asset=compact?menu::phoneAsset(originalAsset):std::string(originalAsset);
            const Vec2 local{source.real(row,"X"),source.real(row,"Y")};
            const Vec2 at=plus(compact?Vec2{240,160}:root,local);const float scale=source.real(row,"Scale");
            if(kind=="Image"){RegionScope background(*this,compact?Region::artScene:Region::content);image(asset,at,1,1,0,{scale,scale},false,id>10?2:0,id>10?0.f:1.f,7,id>10?"baseSelect"+std::to_string(id-10):"");}
            else if(kind=="Button"){
                std::optional<Size> bounds;if(id<4)bounds=Size{160,100};else if(!compact&&id!=4)bounds=Size{160,60};
                RegionScope control(*this,compact?Region::artScene:Region::content);
                addButton(Panel::root,unsigned(2000+id),asset,at,1,2,state.selectedBaseButton==id?1:0,1,bounds,0,{}, {scale,scale},true,7);
                if(id==1)commandPosition=at;
            }else if(kind=="Desc")descriptionPosition=Vec2{source.real(row,"X"),source.real(row,"Y")};
        }
        if(compact){
            std::vector<std::shared_ptr<Button>> routes;float left=0,right=0,bottom=0,top=0;
            for(const auto& key:keys)if(key.id>=2005&&key.id<=2007)if(auto b=dispatcher.find(key)){const auto box=b->bounds();const auto at=b->position;
                if(routes.empty()){left=at.x+box.left;right=at.x+box.right;bottom=at.y+box.bottom;top=at.y+box.top;}else{left=std::min(left,at.x+box.left);right=std::max(right,at.x+box.right);bottom=std::min(bottom,at.y+box.bottom);top=std::max(top,at.y+box.top);}routes.push_back(b);}
            if(!routes.empty()){const auto delta=layout.keepControlVisible(Region::artScene,{0,0},{left,bottom,right-left,top-bottom});for(auto& b:routes)b->position=plus(b->position,delta);}
        }
        const bool sameBase=previous&&previous->state.screen==ScreenId::base&&previous->state.baseId==state.baseId;
        if(sameBase)baseNoticeVisible=previous->baseNoticeVisible;
        // GuiBase.init0x246d4 adds these markers after the table's controls.
        if(auto* notice=tile(compact?menu::phoneAsset("MissionNotice.png"):"MissionNotice.png",1,2);notice&&commandPosition){
            RegionScope noticeRegion(*this,compact?Region::artScene:Region::content);
            auto addNotice=[&](unsigned index,float offset){
                VisualImage mark;
                mark.tile=tiles.visual(*notice);
                mark.tileNo=index;
                mark.screenPosition=plus(*commandPosition,{0,offset});
                mark.show=baseNoticeVisible;
                pushNode(ImageNode{std::move(mark),7,"baseMissionNotice"});
            };
            if(state.baseHasCompletedMission)addNotice(0,28);
            if(!state.availableMissions.empty())addNotice(1,-28);
        }
        if(!descriptionPosition||state.selectedBaseButton<1||state.selectedBaseButton>7)return;
        if(sameBase&&previous->state.selectedBaseButton==state.selectedBaseButton&&previous->state.baseDescription==state.baseDescription){
            for(const auto& node:previous->nodes)if(const auto* description=std::get_if<BaseDescriptionNode>(&node)){
                RegionScope readable(*this,compact?Region::artScene:Region::baseDescription);pushNode(*description);
                return;
            }
        }
        if(auto* background=tile(compact?menu::phoneAsset("BaseTextBG.png"):"BaseTextBG.png")){
            // GuiBase.setSelect:0x243b8 begins at local x +/-480. The
            // child text only becomes visible when the background arrives.
            VisualImage image;
            image.tile=tiles.visual(*background);
            const float localX=descriptionPosition->x>0?480.f:-480.f;
            const auto descriptionRoot=compact?Vec2{240,160}:root;
            const auto destination=compact?layout.keepControlVisible(Region::artScene,plus(descriptionRoot,*descriptionPosition),{-96,-64,192,128}):plus(root,*descriptionPosition);
            image.screenPosition={descriptionRoot.x+localX,destination.y};
            image.scale.y=.02f;
            std::shared_ptr<Text> label;
            if(!state.baseDescription.empty()){
                label=prepareText({"GuiBase.description",state.baseDescription,"TrebuchetMS-Bold",
                    compact?Size{172,110}:Size{280,184},compact?12.f:18.f,TextAlign::left,false,0});
                label->position=image.screenPosition;
                label->scale.y=image.scale.y;
                label->visible=false;
            }
            {RegionScope readable(*this,compact?Region::artScene:Region::baseDescription);pushNode(BaseDescriptionNode{std::move(image),std::move(label),localX,destination.x-descriptionRoot.x});}
        }
    }
    void tickBaseDescription(BaseDescriptionNode& description){
        auto& background=description.background;
        if(description.localX==description.targetX){
            background.scale.y+=(1.f-background.scale.y)/3.f;
        }else{
            description.localX+=(description.targetX-description.localX)/6.f;
            if(std::abs(description.localX-description.targetX)<=1.f){
                description.localX=description.targetX;
                if(description.text)description.text->visible=true;
            }
            background.screenPosition.x=(menu::phone(layout.configuration())?240.f:384.f)+description.localX;
        }
        if(description.text){
            description.text->position=background.screenPosition;
            description.text->scale.y=background.scale.y;
        }
    }
    void tickBase(float time){
        baseNoticeVisible=std::fmod(std::trunc(time*2.f),2.f)!=0.f;
        for(auto& node:nodes){
            if(auto* description=std::get_if<BaseDescriptionNode>(&node))tickBaseDescription(*description);
            else if(auto* n=std::get_if<ImageNode>(&node)){
                if(n->tag=="baseMissionNotice")n->image.show=baseNoticeVisible;
                else if(n->tag.starts_with("baseSelect")){
                    const int selection=std::stoi(n->tag.substr(10));
                    const float target=state.selectedBaseButton==selection?.5f:0.f;
                    n->image.alpha+=(target-n->image.alpha)/10.f;
                }
            }
        }
    }
    void titleScreen(){
        const bool phone=menu::phone(layout.configuration());const auto spec=menu::title(layout.configuration());
        RegionScope scope(*this,phone?Region::artContent:Region::title);const auto root=spec.root;
        const auto asset=[&](std::string_view name){return phone?menu::phoneAsset(name):std::string(name);};
        image("Main_BG.png",{384,512},1,1,0,{1,1},false,0,1,7,"title.bg");
        if(!phone)image("Main_Copyright.png",plus(root,spec.copyright));
        image(asset("Main_TitleBig.png"),plus(root,{title.bigX,0}),1,1,0,{1,1},false,0,1,7,"title.big");
        image(asset("Main_Title.png"),plus(root,spec.main),1,1,0,{title.titleScale,title.titleScale},false,0,1,7,"title.main");
        image(asset("Main_Episode.png"),plus(root,spec.episode),1,1,0,{title.episodeScale,title.episodeScale},false,0,1,7,"title.episode");
        image(asset("Main_TapToStart.png"),plus(root,spec.tap),1,1,0,{1,1},false,0,1,7,"title.start");
        updateTitleImages();
    }
    void updateTitleImages(){
        const auto spec=menu::title(layout.configuration());
        for(auto& node:nodes)if(auto* n=std::get_if<ImageNode>(&node)){
            auto& v=n->image;
            if(n->tag=="title.bg"){if(menu::phone(layout.configuration())){v.screenPosition={384,512};v.scale={1,1};v.angle=0;}else{v.screenPosition=plus(spec.root,title.bgPosition);v.scale={title.bgScale,title.bgScale};v.angle=title.bgRotate;}}
            else if(n->tag=="title.big"){v.screenPosition.x=spec.root.x+title.bigX;v.show=title.bigVisible;}
            else if(n->tag=="title.main"){v.scale={title.titleScale,title.titleScale};v.show=title.titleVisible;}
            else if(n->tag=="title.episode"){v.scale={title.episodeScale,title.episodeScale};v.show=title.episodeVisible;}
            else if(n->tag=="title.start")v.show=title.startVisible;
        }
    }
    void cropMenuPanel(ClipRect clip){
        auto& node=std::get<ImageNode>(nodes.back());VisualLayer source(1);source.submitImage(node.image);auto frame=source.drain();
        if(frame.nonAtlas.size()!=1)throw std::logic_error("Menu panel requires one original image");
        auto panel=std::make_shared<Sprite>(std::move(frame.nonAtlas.front()));panel->clip=clip;node.menuPanel=std::move(panel);
    }
    void quickSlots(Vec2 origin){
        for(unsigned i=0;i<5;++i)if(!state.quick[i].dataId.empty()){
            const auto at=plus(origin,{57.f*float(i),0});image("ItemBox.png",at,1,1,0,{1,1},false,0,1,9);icon(state.quick[i],at);
        }
        for(unsigned i=0;i<5;++i){const auto& entry=state.quick[i];const Vec2 at=plus(origin,{57.f*float(i),0});
            addButton(Panel::quick,6200+i,"Inven_BtnItem.png",at,1,1,0,0,Size{48,48},0,entry.dataId);
            if(!entry.dataId.empty())number("NumSet01b.png",entry.count,plus(at,{11,-11}),6,1);
            if(state.selectedQuick==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
            if(state.extQuick==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
        }
    }
    void field(){
    RegionScope scope(*this,Region::bottom);
        const auto previousGroup=currentHUDGroup;currentHUDGroup=hud::combat;
        image("WpnBtnBG.png",{405,90},1,1,0,{1,1},false,0,1,9);
        auto left=addButton(Panel::root,1022,state.selectedWeaponLeft,{334,90},1,1,0,0,Size{64,64},0,{}, {.7f,.7f},true,9,false);
        auto right=addButton(Panel::root,1023,state.selectedWeaponRight,{480,90},1,1,0,0,Size{64,64},0,{}, {.7f,.7f},true,9,false);
        // Original creates nil buttons/bounds64/scale.7, then setWeapon: calls
        // setTile: afterward. That setter replaces bounds with unscaled icon
        // tile bounds; do not silently keep the earlier64x64 rectangle.
        auto weaponTile=[&](const std::shared_ptr<Button>& b,std::string_view name){if(!name.empty())if(auto* t=tiles.lookup(name))
            b->setTile(ButtonTile{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalTileBlend(0,t->pixelFormat)});};
        weaponTile(left,state.selectedWeaponLeft);weaponTile(right,state.selectedWeaponRight);
        image("OutofRange.png",{334,90},1,1,0,{1,1},false,0,1,9,"hud.rangeLeft");
        image("OutofRange.png",{480,90},1,1,0,{1,1},false,0,1,9,"hud.rangeRight");
        const Vec2 hud{384,128};currentHUDGroup=-1;image("GameUI_BG.png",hud,1,1,0,{1,1},false,0,1,9,"hud.background");currentHUDGroup=hm2::hud::gauges;
        image("GameUI_GaugeBack.png",plus(hud,{float(state.hpBarWidth)-398,-80}),1,4,0,{1,1},true,0,1,9,"hud.hpBack");
        image("GameUI_GaugeBack.png",plus(hud,{float(state.hpBarFilled)-398,-80}),1,4,1,{1,1},true,0,1,9,"hud.hp");
        image("GameUI_GaugeBack.png",plus(hud,{float(state.spBarWidth)-398,-54}),1,4,0,{1,1},true,0,1,9,"hud.spBack");
        image("GameUI_GaugeBack.png",plus(hud,{state.spBarFilled-398,-54}),1,4,state.boosterReady?3:2,{1,1},true,0,1,9,"hud.sp");
        image("GameUI_GaugeEXP.png",plus(hud,{115*state.experienceRate-343,-113}),1,1,0,{state.experienceRate,1},false,0,1,9,"hud.exp");
        image("EnemyGauge.png",{384,956},1,2,1,{1,1},false,0,1,9,"hud.enemyBack");
        image("EnemyGauge.png",{384-128*(1-state.enemyHpRate),957},1,2,0,{state.enemyHpRate,1},false,0,1,9,"hud.enemy");
        number("NumTile05.png",std::bit_cast<std::int32_t>(std::uint32_t(state.level)+1),plus(hud,{-364,-106}),9,.5f,9,false,"hud.level");
        number("NumSet01b.png",fieldPresentation.displayedCredits,plus(hud,{-122,-37}),6,1,9,false,"hud.displayedCredits");
        currentHUDGroup=hm2::hud::controls;
        addButton(Panel::root,1021,"QuickBtn.png",plus(hud,{237,-36}),2,4,4,5,Size{60,60});
        addButton(Panel::root,1026,"QuickBtn.png",plus(hud,{296,-36}),2,4,0,1,Size{60,60});
        addButton(Panel::root,1025,"QuickBtn.png",plus(hud,{355,-36}),2,4,2,3,Size{60,60});
        currentHUDGroup=hm2::hud::combat;
        if(auto* mark=tile("RaderMark.png",8,1))pushNode(RadarNode{tiles.visual(*mark)});
        currentHUDGroup=hm2::hud::controls;quickSlots({511,29});
        updateHUD(state);
        currentHUDGroup=hm2::hud::combat;if(state.weaponChoiceType!=0)weaponChoices();
        currentHUDGroup=previousGroup;
    }
    void updateHUD(const ScreensState&);
    void build();
    void buildDetail(DetailKind,const ItemView&,Vec2,int=-1);
    void layoutDetail();
    void tickDetail();
    void syncEnemyName();
    void tickRange(WorldRandom&);
    void tickFieldCredits(){
        auto& displayed=fieldPresentation.displayedCredits;
        const auto target=std::int32_t(state.credits);
        if(!state.fieldUIRevision||!state.hasWorkingSlot||displayed==target)return;
        // GuiGame.tick0x29a18 uses signed division on a 32-bit difference.
        const auto delta=std::bit_cast<std::int32_t>(std::uint32_t(target)-std::uint32_t(displayed));
        displayed=std::bit_cast<std::int32_t>(std::uint32_t(displayed)+std::uint32_t(delta/5));
        const auto remaining=std::bit_cast<std::int32_t>(std::uint32_t(displayed)-std::uint32_t(target));
        const auto magnitude=remaining<0?std::bit_cast<std::int32_t>(0u-std::uint32_t(remaining)):remaining;
        if(magnitude<5)displayed=target;
    }
    void inventory(Vec2,bool=false,bool=false);
    void phonePageDots(Vec2,std::size_t,std::size_t);
    void equip(Vec2,unsigned=9,bool=false);
    void bots(Vec2,bool=false);
    void inventoryDialog(Vec2,bool=false);
    void phoneShop(bool);
    void phoneTradeAction();
    Vec2 phoneDropPoint(Vec2)const;
    void maintenance();
    void shop();
    void missionList(bool);
    void phoneCommand();
    void positionCommandList();
    void tickCommandList();
    void completedMission();
    void system();
    void hudEditor();
    void airlift();
    void trade();
    void weaponChoices();
    void tick(float time,WorldRandom& random);
    void append(Frame& out,float time);
    bool splitHUDBackground()const{
        if(hud->editing)return true;
        const auto base=presentation(Region::bottom);
        for(unsigned group=0;group<hud::count;++group){const auto t=presentation(Region::bottom,int(group));if(t.scale!=base.scale||t.offset.x!=base.offset.x||t.offset.y!=base.offset.y)return true;}
        return false;
    }
};

void Screens::Impl::buildDetail(DetailKind kind,const ItemView& entry,Vec2 origin,int absolute){
    const bool phone=phoneMenu();RegionScope scope(*this,phone?Region::phoneMenu:kind==DetailKind::shop?Region::shopStock:kind==DetailKind::maintenance?Region::maintenance:state.screen==ScreenId::shop?Region::shopInventory:Region::panel);
    const bool reuse=previous&&previous->detail&&previous->detail->phoneArtwork==phone&&previous->detail->kind==kind&&previous->detail->revision==state.detailRevision&&previous->detail->absolute==absolute&&previous->detail->entry.dataId==entry.dataId;
    detail=reuse?std::make_shared<DetailPresentation>(*previous->detail):std::make_shared<DetailPresentation>();
    auto& d=*detail;d.phoneArtwork=phone;d.kind=kind;d.revision=state.detailRevision;d.absolute=absolute;d.origin=origin;
    if(!reuse){d.entry=entry;
        if(kind==DetailKind::shop)d.descPos={184,340};
        else if(kind==DetailKind::inventory){const int cell=absolute%25;d.descPos={float((cell%5)*60),float(96-(cell/5)*60)};}
        else{const int cell=absolute%25;d.up=cell>9;d.descPos={108,float(61-(cell/5)*40+(d.up?20:-21))};}
    }
    if(phone){
        const int cell=kind==DetailKind::shop?absolute%int(stockCells()):absolute%25%int(inventoryCells());
        const int row=cell/(kind==DetailKind::shop?4:5);
        d.up=row>=2;
        d.descPos={(kind==DetailKind::shop?-136.f:106.f)*1.5f,(60-40.f*row+(d.up?20.f:-21.f))*1.5f};
        const Vec2 preferred=plus(d.origin,plus(d.descPos,{0,d.up?93.f:-93.f}));
        const auto fitted=menu::shopDetail(layout.configuration(),preferred,kind==DetailKind::shop,phoneEquipment);
        d.phoneScale=fitted.scale;
        if(d.phoneScale==0){detail.reset();return;}
        d.origin={fitted.center.x-d.descPos.x,fitted.center.y-d.descPos.y-(d.up?93.f:-93.f)*d.phoneScale};
    }
    d.layer=phone?13u:kind==DetailKind::inventory?13u:kind==DetailKind::shop?3u:9u;
    if(phone||kind!=DetailKind::shop){
        d.background=addButton(kind==DetailKind::maintenance?Panel::root:Panel::inventory,2112,phone?menu::phoneAsset("ItemDesc.png"):"ItemDesc.png",plus(origin,d.descPos),1,1,0,0,phone?Size{menu::detailHitSize.width*d.phoneScale,menu::detailHitSize.height*d.phoneScale}:Size{206,128},0,{}, {d.x,d.y},true,d.layer,false,reuse);
    }
    if(!reuse){
        const bool maintenance=phone||kind==DetailKind::maintenance;
        const char* font=maintenance?"Arial":"TrebuchetMS-Bold";
        const std::string source=kind==DetailKind::shop?"GuiShop.item":maintenance?"GuiMaintenance.item":"DlgInven.item";
        auto label=[&](std::string id,const std::string& value,Size size,float fontSize,TextAlign align,Color color){
            const auto count=nodes.size();text(std::move(id),value,{},size,font,fontSize,align,color,d.layer);
            if(nodes.size()==count)return std::shared_ptr<Text>{};auto result=std::get<TextNode>(nodes.back()).text;nodes.pop_back();return result;
        };
        d.title=label(source+".title",entry.name,maintenance?Size{128,16}:Size{160,24},maintenance?12.f:16.f,TextAlign::center,{});
        const auto descriptionColor=kind==DetailKind::shop&&!phone?Color{120.f/255,240.f/255,240.f/255,1}:Color{};
        d.description=label(source+".description",entry.description,{160,90},12,TextAlign::left,descriptionColor);
        if(entry.originalType==10||entry.originalType==11||entry.originalType==12)if(auto* t=tile(entry.largeIcon)){
            VisualImage image;image.tile=tiles.visual(*t);image.tileNo=0;image.useAtlas=false;d.large=std::move(image);
        }
    }
    pushNode(DetailNode{detail,d.layer});layoutDetail();
}
void Screens::Impl::layoutDetail(){
    if(!detail)return;auto& d=*detail;const bool shop=d.kind==DetailKind::shop,maintenance=d.kind==DetailKind::maintenance;
    if(d.phoneArtwork){
        const float y=d.vertical?d.y:0;
        const float scale=d.phoneScale;
        const Vec2 parent=plus(d.origin,plus(d.descPos,{0,(d.up?93.f:-93.f)*y*scale}));
        const Vec2 animatedScale{1.5f*d.x*scale,1.5f*d.y*scale};
        if(d.background){
            d.background->position=parent;
            d.background->scale={1,1};
            d.background->setBoundWidthHeight(menu::detailHitSize.width*d.x*scale,menu::detailHitSize.height*d.y*scale);
            d.background->scale=animatedScale;
        }
        if(d.title){d.title->position=plus(parent,{0,70.5f*y*scale});d.title->scale=animatedScale;d.title->visible=d.vertical;}
        const bool weapon=d.entry.originalType==10;
        if(d.description){d.description->position=plus(parent,{d.large?(weapon?48.f:114.f)*scale:0,-18.f*y*scale});d.description->scale=animatedScale;d.description->visible=d.vertical;}
        if(d.large){d.large->screenPosition=plus(parent,{(weapon?-114.f:-78.f)*scale,-15.f*y*scale});d.large->scale={1.5f*scale,1.5f*y*scale};d.large->show=d.vertical;}
        return;
    }
    float parentY=d.descPos.y;if(d.vertical)parentY+=(shop?-62.f:maintenance?(d.up?62.f:-62.f):-96.f)*d.y;
    const Vec2 parent=plus(d.origin,{d.descPos.x,parentY});
    if(d.background){d.background->position=parent;d.background->scale={d.x,d.y};}
    const bool hasLarge=d.large.has_value();const bool weapon=d.entry.originalType==10;
    if(d.title){d.title->position=plus(parent,{0,d.vertical?(shop?40.f:maintenance?47.f:62.f)*d.y:0});d.title->scale={d.x,d.y};d.title->visible=d.vertical;}
    if(d.description){d.description->position=plus(parent,{hasLarge?(weapon?32.f:76.f):0,d.vertical?(shop?-36.f:-12.f)*d.y:0});d.description->scale={d.x,d.y};d.description->visible=d.vertical;}
    if(d.large){d.large->screenPosition=plus(parent,{weapon?-76.f:-52.f,d.vertical?(shop?-36.f:-10.f)*d.y:0});d.large->scale={1,d.vertical?d.y:1};d.large->show=d.vertical;}
}
void Screens::Impl::phoneTradeAction(){
    if(phoneEquipment)return;
    const bool buy=detail&&detail->kind==DetailKind::shop;
    if(!buy){
        if(state.detailInventory<0||state.selectedInventory<0)return;
        const auto position=std::size_t(std::max(0,state.inventoryPage))*25+std::size_t(state.selectedInventory);
        if(!itemAt(state.inventory,position).enabled)return;
    }
    RegionScope scope(*this,Region::phoneMenu);
    const auto button=addButton(Panel::root,buy?2102:2103,
        menu::phoneAsset(buy?"Btn_ShopBuy.png":"Btn_ShopSell.png"),menu::tradeAction,
        2,1,0,1,menu::tradeActionSize,0,{}, {1,1},true,13,false);
    // The receiver and the two frames of the original button share a rectangle.
    button->pushDepth=0;
    button->releaseHeight=0;
}
void Screens::Impl::tickDetail(){
    if(!detail)return;auto& d=*detail;
    if(d.x<1){d.x+=(1-d.x)*.5f;if(d.x>.99f)d.x=1;}
    else if(d.y<1){d.y+=(1-d.y)*.5f;if(d.y>.99f)d.y=1;d.vertical=true;}
    layoutDetail();
}
void Screens::Impl::syncEnemyName(){
    RegionScope scope(*this,Region::top);
    if(state.screen!=ScreenId::field)return;
    if(fieldPresentation.enemyRevision!=state.enemyNameRevision||(!fieldPresentation.enemyName&&!state.enemyName.empty())){
        ++*resourceGeneration;
        const auto old=fieldPresentation.enemyName;std::erase_if(nodes,[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==old;});
        fieldPresentation.enemyName.reset();fieldPresentation.enemyRevision=state.enemyNameRevision;
        if(!state.enemyName.empty()){const auto count=nodes.size();text("GuiGame.enemyName",state.enemyName,{384,956},{256,18},"Arial-BoldMT",12,TextAlign::center,{},9);if(nodes.size()!=count)fieldPresentation.enemyName=std::get<TextNode>(nodes.back()).text;}
    }else if(fieldPresentation.enemyName&&std::none_of(nodes.begin(),nodes.end(),[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==fieldPresentation.enemyName;}))pushNode(TextNode{fieldPresentation.enemyName,9});
    if(fieldPresentation.enemyName)fieldPresentation.enemyName->visible=state.enemyHpRate!=0;
}
void Screens::Impl::tickRange(WorldRandom& random){
    if(state.fieldUIRevision&&state.hasWorkingSlot){const float stamps[]={state.outOfRangeTimeLeft,state.outOfRangeTimeRight};
        for(unsigned i=0;i<2;++i){const float age=float(state.fieldWorldTime-double(stamps[i]));
            if(age>0&&age<1.5f){if(age<.2f){if(random.random()%10<=5)fieldPresentation.range[i]=((random.random()^1)&1)!=0;}
                else if(age<1.2f)fieldPresentation.range[i]=true;else fieldPresentation.range[i]=false;}
        }
    }
    for(auto& node:nodes)if(auto* n=std::get_if<ImageNode>(&node)){if(n->tag=="hud.rangeLeft")n->image.show=fieldPresentation.range[0];else if(n->tag=="hud.rangeRight")n->image.show=fieldPresentation.range[1];}
}

void Screens::Impl::phonePageDots(Vec2 center,std::size_t count,std::size_t selected){
    for(std::size_t i=0;i<count;++i){
        const float x=center.x+(float(i)-(float(count)-1.f)*.5f)*18.f;
        image("InvenPage.png",{x,center.y},2,1,i==selected?1u:0u,{1,1},false,0,1,9);
    }
}
void Screens::Impl::inventory(Vec2 origin,bool shopMode,bool phone){
    if(!phone){
    (void)shopMode;
    for(unsigned i=0;i<25;++i){const auto& entry=itemAt(state.inventory,std::size_t(std::max(0,state.inventoryPage))*25+i);
        if(!entry.dataId.empty()&&entry.count>0){const auto at=plus(origin,{60.f*float(i%5),-60.f*float(i/5)});
            image("ItemBox.png",at,1,1,0,{1,1},false,0,1,9);icon(entry,at);}
    }
    // WndInven.draw0x34218 draws these through Tile2D before its child
    // buttons. pages() is the actual saved inventory capacity, not item count.
    tile("InvenPage.png",2,1);
    for(int page=0;page<state.inventoryPages;++page){
        float x=float(state.inventoryPages)*-.5f;x=float(page)+x;x*=12.f;x=origin.x+x;x+=86.f;
        image("InvenPage.png",{x,origin.y-290.f},2,1,page==state.inventoryPage?1u:0u,{1,1},false,0,1,9);
    }
    for(unsigned i=0;i<25;++i){
        const auto& entry=itemAt(state.inventory,std::size_t(std::max(0,state.inventoryPage))*25+i);
        const auto at=plus(origin,{60.f*float(i%5),-60.f*float(i/5)});
        addButton(Panel::inventory,6000+i,"Inven_BtnItem.png",at,1,1,0,0,Size{60,60},0,entry.dataId);
        if(!entry.dataId.empty()&&(entry.countable||entry.equipped))number("NumSet01b.png",entry.count,plus(at,{11,-11}),6,1,9,entry.equipped);
        if(state.selectedInventory==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extInventory==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }
    addButton(Panel::inventory,6090,"Btn_InvenPage.png",plus(origin,{-2,-290}),2,1,0,0,Size{80,40});
    addButton(Panel::inventory,6091,"Btn_InvenPage.png",plus(origin,{161,-290}),2,1,1,1,Size{80,40});

    return;}
    (void)shopMode;
    const auto [first,last]=phoneInventoryRange();
    {
    RegionScope cells(*this,Region::phoneInventory);
    for(unsigned i=first;i<last;++i){const auto& entry=itemAt(state.inventory,std::size_t(std::max(0,state.inventoryPage))*25+i);const auto at=plus(origin,{60.f*float(i%5),-60.f*float((i-first)/5)});
        if(!entry.dataId.empty()&&entry.count>0){if(!phone)image("ItemBox.png",at,1,1,0,{1,1},false,0,1,9);icon(entry,at);}
    }
    for(unsigned i=first;i<last;++i){const auto& entry=itemAt(state.inventory,std::size_t(std::max(0,state.inventoryPage))*25+i);const auto at=plus(origin,{60.f*float(i%5),-60.f*float((i-first)/5)});
        addButton(Panel::inventory,6000+i,"Inven_BtnItem.png",at,1,1,0,0,Size{60,60},0,entry.dataId);
        if(!entry.dataId.empty()&&(entry.countable||entry.equipped))number("NumSet01b.png",entry.count,plus(at,{11,-11}),6,1,9,entry.equipped);
        if(state.selectedInventory==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extInventory==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }}
    RegionScope pages(*this,Region::phoneInventoryPages);
    const auto windows=menu::pageWindows(25,inventoryCells());
    phonePageDots({origin.x+111.f,menu::pagerY},std::size_t(std::max(0,state.inventoryPages))*windows,
                  std::size_t(std::max(0,state.inventoryPage))*windows+inventoryWindow);
    addButton(Panel::inventory,6090,menu::phoneAsset("Btn_InvenPage.png"),{origin.x-3.f,menu::pagerY},2,1,0,0,Size{120,menu::pagerHitHeight},0,{},Vec2{1,1},true,9,false);
    addButton(Panel::inventory,6091,menu::phoneAsset("Btn_InvenPage.png"),{origin.x+243.f,menu::pagerY},2,1,1,1,Size{120,menu::pagerHitHeight},0,{},Vec2{1,1},true,9,false);
}
void Screens::Impl::equip(Vec2 origin,unsigned inheritedLayer,bool phone){
    constexpr Vec2 hdPositions[]={{0,0},{0,-74},{-56,-9},{56,-9},{56,-74},{-56,-74}};
    const auto positions=phone?std::span<const Vec2>(menu::equipmentOffsets):std::span<const Vec2>(hdPositions);
    constexpr const char* icons[]={"EquipBtn_Body.png","EquipBtn_Btm.png","EquipBtn_Wpn.png","EquipBtn_Wpn.png","EquipBtn_Armor.png","EquipBtn_Sp.png"};
    for(unsigned i=0;i<6;++i)if(!state.equipment[i].dataId.empty()&&state.equipment[i].count>0){
        const auto at=plus(origin,positions[i]);if(!phone)image("ItemBox.png",at,1,1,0,{1,1},false,0,1,inheritedLayer);icon(state.equipment[i],at,inheritedLayer);}
    for(unsigned i=0;i<6;++i){const auto& entry=state.equipment[i];const auto at=plus(origin,positions[i]);
        addButton(Panel::equipment,6100+i,icons[i],at,1,1,0,0,phone?Size{54,54}:Size{36,36},0,entry.dataId,{1,1},true,inheritedLayer);
        if(state.selectedEquipment==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extEquipment==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }
    const auto& stats=state.equipmentStats;
    numericText("NumSet01b.png",std::to_string(stats.attackLeft)+"-"+std::to_string(stats.attackRight),plus(origin,{phone?-30.f:-20.f,phone?-157.5f:-126.f}),6,0);
    const auto defense=std::bit_cast<std::int32_t>(std::uint32_t(stats.bodyDefense)+std::uint32_t(stats.armorDefense));
    numericText("NumSet01b.png",std::to_string(stats.baseHP)+"-"+std::to_string(stats.bonusHP)+"-"+std::to_string(defense),plus(origin,{phone?-30.f:-20.f,phone?-180.f:-146.f}),6,0);
    numericText("NumSet01b.png",std::to_string(stats.stepSize)+"-"+std::to_string(int(stats.boosterMaximum))+"-"+std::to_string(int(stats.boosterRegen*10.f)),plus(origin,{phone?-30.f:-20.f,phone?-202.5f:-166.f}),6,0);
}
void Screens::Impl::bots(Vec2 origin,bool phone){
    for(unsigned i=0;i<3;++i)if(!state.bots[i].dataId.empty()){
        const auto at=plus(origin,{0,-(phone?57.f:60.f)*float(i)});if(!phone)image("ItemBox.png",at,1,1,0,{1,1},false,0,1,9);icon(state.bots[i],at);}
    for(unsigned i=0;i<3;++i){const auto& entry=state.bots[i];const auto at=plus(origin,{0,-(phone?57.f:60.f)*float(i)});
        addButton(Panel::bot,6300+i,"Inven_BtnItem.png",at,1,1,0,0,phone?Size{57,57}:Size{60,60},0,entry.dataId);
        if(state.selectedBot==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extBot==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }
}
void Screens::Impl::inventoryDialog(Vec2 origin,bool shopMode){
    RegionScope scope(*this,shopMode?Region::shopInventory:Region::panel);
    image("InvenBG.png",origin,1,1,0,{1,1},false,0,1,9);
    inventory(plus(origin,{24,120}),shopMode);equip(plus(origin,{-196,76}));bots(plus(origin,{-58,78}));
    if(shopMode){
        // The original stores tile1 even for this one-tile image; draw clamps
        // the sampled index to0. Its wide hit area belongs only to shop mode.
        addButton(Panel::inventory,6093,"Btn_ArrangeItem.png",plus(origin,{240,174}),1,1,1,1,Size{120,42});
    }else{
        addButton(Panel::inventory,6092,"Btn_CloseX.png",plus(origin,{274,174}),2,1,0,1,Size{42,42});
        addButton(Panel::inventory,6093,"Btn_CloseX.png",plus(origin,{226,174}),2,1,1,1,Size{42,42});
    }
    if(shopMode&&state.selectedBot>=0)addButton(Panel::inventory,6310,"Btn_Disarm.png",plus(origin,{-57,-92}),2,1);
    number("NumSet_Cr.png",state.credits,plus(origin,{-76,-151}),12,1,9,false,"hud.credits");
    if(!shopMode&&state.detailInventory>=0&&!state.detailItem.dataId.empty())buildDetail(DetailKind::inventory,state.detailItem,origin,state.detailInventory);
}
void Screens::Impl::phoneShop(bool maintenance){
    const auto canvas=layout.configuration().canvas;
    constexpr auto hangar="PhoneShopHangar.jpg";
    if(const auto* art=tile(hangar)){
        const float scale=std::max(canvas.width/art->pixels.width,canvas.height/art->pixels.height);
        RegionScope backdrop(*this,Region::surface);
        image(hangar,{canvas.width*.5f,canvas.height*.5f},1,1,0,{scale,scale},false,0,1,0);
    }
    {
        RegionScope art(*this,Region::artBanner);
        const auto [first,last]=phoneInventoryRange();
        const auto unusedRows=menu::phoneShopRows-(last-first)/5;
        // The plate has four rows baked in. Clip the unused rows from the
        // final strip of each 25-cell page using the same range as its buttons.
        const auto panel=[&](std::string_view asset,std::string_view tag){
            const auto add=[&]{image(asset,{240,160},1,1,0,{1,1},false,0,1,0,std::string(tag));};
            if(!unusedRows){add();return;}
            const auto grid=menu::inventoryWindow;
            const float left=grid.x/1.5f,right=(grid.x+grid.width)/1.5f;
            const float bottom=grid.y/1.5f,top=bottom+40.f*float(unusedRows);
            const ClipRect slices[]={{0,0,left,320},{right,0,480-right,320},
                                    {left,0,right-left,bottom},{left,top,right-left,320-top}};
            for(const auto& slice:slices){
                const auto before=nodes.size();
                add();
                if(nodes.size()!=before)cropMenuPanel(slice);
            }
        };
        panel("PhoneShopPanel.png","phone.stock.bg");
        panel("PhoneEquipPanel.png","phone.equip.bg");
    }
    inventory(menu::inventoryGrid,!maintenance,true);
    {RegionScope equipment(*this,Region::phoneEquipment);equip(menu::equipmentRoot,9,true);bots(menu::botRoot,true);
        if(state.selectedBot>=0)addButton(Panel::root,6310,menu::phoneAsset("Btn_Disarm.png"),{288,menu::disarmY},2,1,0,1,Size{72,61.5f},0,{}, {1,1},true,9,false);
        addButton(Panel::root,9205,"",{97.5f,34.5f},1,1,0,0,Size{150,60});}
    if(!maintenance){
        {RegionScope header(*this,Region::phoneCategoryHeader);image(menu::phoneAsset("ShopCategory.png"),{172.5f,menu::categoryY},1,8,unsigned(std::clamp(state.shopCategory,0,4)));
            addButton(Panel::root,2104,menu::phoneAsset("Btn_ShopLeft.png"),{75,menu::categoryY},2,1,0,1,Size{120,menu::categoryHitHeight},(state.shopCategory+4)%5,{}, {1,1},true,9,false);
            addButton(Panel::root,2104,menu::phoneAsset("Btn_ShopRight.png"),{270,menu::categoryY},2,1,0,1,Size{120,menu::categoryHitHeight},(state.shopCategory+1)%5,{}, {1,1},true,9,false);
            addButton(Panel::root,9204,"",{250.5f,34.5f},1,1,0,0,Size{150,60});
            RegionScope pagesRegion(*this,Region::phoneStockHeader);
            const auto count=state.shopItems.size();
            const auto windows=menu::pageWindows(24,stockCells());
            const auto pages=count/24*windows+menu::pageWindows(count%24,stockCells());
            const auto selected=std::size_t(std::max(0,state.shopPage))*windows+stockWindow;
            phonePageDots({menu::stockGrid.x+90.f,menu::stockGrid.y-231.f},pages,selected);
            if(pages>1){
                addButton(Panel::root,2105,menu::phoneAsset("Btn_InvenPage.png"),{menu::stockGrid.x-7.5f,menu::pagerY},2,1,0,0,Size{120,menu::pagerHitHeight},0,{}, {1,1},true,9,false);
                addButton(Panel::root,2106,menu::phoneAsset("Btn_InvenPage.png"),{menu::stockGrid.x+186,menu::pagerY},2,1,1,1,Size{120,menu::pagerHitHeight},0,{}, {1,1},true,9,false);
            }
        }
        {RegionScope stock(*this,Region::phoneStock);for(unsigned i=stockWindow*stockCells();i<std::min(24u,(stockWindow+1)*stockCells());++i){const auto index=std::size_t(std::max(0,state.shopPage))*24+i;if(index>=state.shopItems.size())break;const auto& entry=state.shopItems[index];const auto at=plus(menu::stockGrid,{60.f*float(i%4),-60.f*float((i%stockCells())/4)});addButton(Panel::root,2200+i,entry.icon,at,entry.iconColumns,entry.iconRows,entry.iconTile,entry.iconTile,Size{60,60},0,entry.dataId,{1,1},entry.enabled);if(state.selectedShop==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f);}}
    }
    {RegionScope controls(*this,Region::phoneFooter);
        number(menu::phoneAsset("NumTile04.png"),state.credits,{486,48},0,1);
        addButton(Panel::root,6093,menu::phoneAsset("Btn_ArrangeItem.png"),{615,405},1,1,0,0,Size{90,42},0,{}, {1,1},true,9,false);
        addButton(Panel::root,2100,menu::phoneAsset("Btn_ShopExit.png"),{630,34.5f},2,1,0,1,Size{138,60},0,{}, {1,1},true,9,false);
    }
    if(state.shopDetailVisible&&state.selectedShop>=0&&!maintenance){const auto index=std::size_t(std::max(0,state.shopPage))*24+std::size_t(state.selectedShop);if(index<state.shopItems.size())buildDetail(DetailKind::shop,state.shopItems[index],{360,240},state.selectedShop);}
    if(state.tradeVisible&&!state.buyMode)dismissedSaleDetail=state.detailRevision;
    else if(previous&&previous->state.screen==state.screen&&previous->dismissedSaleDetail==state.detailRevision)
        dismissedSaleDetail=state.detailRevision;
    if(!dismissedSaleDetail&&state.detailInventory>=0&&!state.detailItem.dataId.empty())
        buildDetail(maintenance?DetailKind::maintenance:DetailKind::inventory,state.detailItem,{360,240},state.detailInventory);
    if(!maintenance)phoneTradeAction();
    if(state.tradeVisible)trade();setPhoneEquipment(phoneEquipment);
}
Vec2 Screens::Impl::phoneDropPoint(Vec2 canvas)const{
    const auto in=[](Vec2 p,ClipRect b){return p.x>=b.x&&p.x<b.x+b.width&&p.y>=b.y&&p.y<b.y+b.height;};
    if(const auto clip=clipping(Region::phoneInventory);clip&&in(canvas,*clip)){auto point=presentation(Region::phoneInventory).inverse(canvas);point.y-=float(inventoryWindow*menu::phoneShopRows*60);return menu::originalHDInventory(point,state.screen==ScreenId::maintenance);}
    if(phoneEquipment){const auto p=presentation(Region::phoneEquipment).inverse(canvas);const Vec2 root{384,state.screen==ScreenId::maintenance?512.f:312.f};const Vec2 oldEquip{root.x+(state.screen==ScreenId::maintenance?-148.f:-196.f),root.y+(state.screen==ScreenId::maintenance?52.f:76.f)};constexpr Vec2 oldOffsets[]={{0,0},{0,-74},{-56,-9},{56,-9},{56,-74},{-56,-74}};
        for(unsigned i=0;i<6;++i){const auto center=plus(menu::equipmentRoot,menu::equipmentOffsets[i]);if(std::abs(p.x-center.x)<30&&std::abs(p.y-center.y)<30)return plus(plus(oldEquip,oldOffsets[i]),{(p.x-center.x)/1.5f,(p.y-center.y)/1.5f});}
        const float x=p.x-menu::botRoot.x,y=(menu::botRoot.y+28.5f-p.y)/57.f;if(x>=-28.5f&&x<=28.5f&&y>=0&&y<3){const Vec2 oldBot{root.x+(state.screen==ScreenId::maintenance?-47.f:-58.f),root.y+(state.screen==ScreenId::maintenance?52.f:78.f)};return {oldBot.x+x/1.5f,oldBot.y+30-y*60};}}
    return {-10000,-10000};
}
void Screens::Impl::maintenance(){
    if(phoneMenu()){phoneShop(true);return;}
    if(menu::phone(layout.configuration())){RegionScope backdrop(*this,Region::artBackdrop);image(menu::phoneAsset("MaintenanceBG.jpg"),{240,160},1,1,0,{1,1},false,0,1,0,"phone.menu.backdrop");}
    RegionScope scope(*this,Region::maintenance);
    const Vec2 root{384,512};image("MaintenanceBG.png",root,1,1,0,{1,1},false,0,1,3);
    if(menu::phone(layout.configuration()))cropMenuPanel({160,400,448,226});
    inventory(plus(root,{27,63}));equip(plus(root,{-148,52}),3);bots(plus(root,{-47,52}));
    number("NumTile04.png",state.credits,plus(root,{38,-126}),0,0,3);
    addButton(Panel::root,6093,"Btn_ArrangeItem.png",plus(root,{170,94}),1,1,0,0,{},0,{}, {1,1},true,3);
    if(state.selectedBot>=0)addButton(Panel::root,6310,"Btn_Disarm.png",plus(root,{-46,-83}),2,1,0,1,{},0,{}, {1,1},true,3);
    addButton(Panel::root,2100,"Btn_CmdCenterExit.png",plus(root,{186,-140}),2,1,0,1,{},0,{}, {1,1},true,3);
    if(state.detailInventory>=0&&!state.detailItem.dataId.empty())buildDetail(DetailKind::maintenance,state.detailItem,root,state.detailInventory);
}
void Screens::Impl::shop(){
    if(phoneMenu()){phoneShop(false);return;}
    if(menu::phone(layout.configuration())){RegionScope backdrop(*this,Region::artBackdrop);image(menu::phoneAsset("ShopBG.jpg"),{240,160},1,1,0,{1,1},false,0,1,0,"phone.menu.backdrop");}
    RegionScope scope(*this,Region::shopStock);
    const Vec2 root{384,512};image("ShopBG.png",root,1,1,0,{1,1},false,0,1,3);
    if(menu::phone(layout.configuration()))cropMenuPanel({64,584,640,360});
    if(state.inventoryVisible)inventoryDialog(plus(root,{0,-200}),true);
    addButton(Panel::root,2100,"Common_Btn_EXIT.png",plus(root,{256,-460}),1,2,0,1,{},0,{}, {1,1},true,3);
    if(state.selectedShop>=0)addButton(Panel::root,2102,"Common_Btn_BUY.png",plus(root,{182,120}),1,2,0,1,{},0,{}, {1,1},true,3);
    if(state.inventoryVisible&&state.selectedInventory>=0&&itemAt(state.inventory,std::size_t(std::max(0,state.inventoryPage))*25+std::size_t(state.selectedInventory)).enabled)addButton(Panel::root,2103,"Common_Btn_SELL.png",plus(root,{0,40}),1,2,0,1,{},0,{}, {1,1},true,3);
    constexpr const char* categories[]={"ShopBtn_Weapon.png","ShopBtn_Parts.png","ShopBtn_Bot.png","ShopBtn_Attack.png","ShopBtn_Etc.png"};
    int categoryX=-204;
    for(int i=0;i<5;++i){const bool selected=state.shopCategory==i;
        addButton(Panel::root,2104,categories[i],plus(root,{float(categoryX+(selected?15:0)),selected?395.f:390.f}),1,2,selected?1:0,1,
            Size{80,48},i,{},selected?Vec2{1,1}:Vec2{.75f,.75f},true,3,false);
        categoryX+=selected?126:96;
    }
    const Vec2 grid=plus(root,{-256,334});
    for(unsigned i=0;i<24;++i){const auto index=std::size_t(std::max(0,state.shopPage))*24+i;if(index>=state.shopItems.size())break;
        const auto& entry=state.shopItems[index];const Vec2 at=plus(grid,{60.f*float(i%6),-60.f*float(i/6)});
        image("ItemBox.png",at,1,1,0,{1,1},false,0,1,3);
        addButton(Panel::root,2200+i,entry.icon,at,entry.iconColumns,entry.iconRows,entry.iconTile,entry.iconTile,
            Size{60,60},0,entry.dataId,{1,1},entry.enabled,3);
        if(state.selectedShop==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,3);
    }
    // GuiShop.refreshShopItem0x30260 deliberately keeps one extra page at
    // exact multiples of24, including the empty category's first page.
    const auto pages=state.shopItems.size()/24+1;
    for(std::size_t page=0;page<pages;++page){
        float x=float(pages)*-.5f;x=float(page)+x;x*=12.f;x+=66.f;
        image("InvenPage.png",plus(grid,{x,-234.f}),2,1,page==std::size_t(state.shopPage)?1u:0u,{1,1},false,0,1,3);
    }
    addButton(Panel::root,2105,"Btn_InvenPage.png",plus(grid,{-5,-234}),2,1,0,0,Size{80,40},0,{}, {1,1},true,3);
    addButton(Panel::root,2106,"Btn_InvenPage.png",plus(grid,{124,-234}),2,1,1,1,Size{80,40},0,{}, {1,1},true,3);
    if(state.shopDetailVisible&&state.selectedShop>=0){const auto index=std::size_t(std::max(0,state.shopPage))*24+std::size_t(state.selectedShop);if(index<state.shopItems.size())buildDetail(DetailKind::shop,state.shopItems[index],root);}
    // viewItemInfo creates this receiver after the existing shop controls.
    // Keep that dynamic registration order when reconstructing a snapshot:
    // the layer13 popup must precede older root buttons in reverse hit testing.
    if(state.inventoryVisible&&state.detailInventory>=0&&!state.detailItem.dataId.empty())buildDetail(DetailKind::inventory,state.detailItem,plus(root,{0,-200}),state.detailInventory);
    if(state.tradeVisible)trade();
}
void Screens::Impl::system(){
    RegionScope scope(*this,Region::panel);
    const Vec2 root{384,512};image("MissionList_Mid.png",root,1,1,0,{1,3.375f},false,0,1,10);
    image("MissionList_Btm.png",plus(root,{0,-140}),1,1,0,{1,1},false,0,1,10);
    image("SysMenuBG.png",plus(root,{0,140}),1,1,0,{1,1},false,0,1,10);
    constexpr unsigned ids[]={1027,1031,1032,1029};constexpr const char* labels[]={"Airlift to base","Save & Exit game","More games","Help"};
    for(unsigned i=0;i<4;++i){const Vec2 at=plus(root,{0,140-64-44.f*float(i)});
        addButton(Panel::system,ids[i],"Btn_Mission.png",at,1,2,0,1,{},0,{}, {1,1},true,10);
        text("DlgSystemMenu."+std::to_string(ids[i]),labels[i],at,{360,24},"TrebuchetMS-Bold",20,TextAlign::center,{140.f/255,240.f/255,1,1},10);
    }
    addButton(Panel::system,1030,"Common_Btn_CLOSE.png",plus(root,{136,-124}),1,2,0,1,{},0,{}, {1,1},true,10);
    // Restoration feature. Keep its identity separate from original menu IDs.
    const Vec2 edit{384,316};
    addButton(Panel::system,9100,"Btn_Mission.png",edit,1,2,0,1,{},0,{}, {1,1},true,10);
    text("HUDLayout.entry","HUD 배치",edit,{360,24},"TrebuchetMS-Bold",20,TextAlign::center,{140.f/255,240.f/255,1,1},10);
}
void Screens::Impl::hudEditor(){
    RegionScope scope(*this,Region::panel);
    text("HUDLayout.title","HUD 조각을 화면에서 끌어 배치하세요.",{384,720},{640,32},"TrebuchetMS-Bold",20,TextAlign::center,{},13);
    constexpr unsigned ids[]={9101,9102,9103};
    constexpr const char* labels[]={"기본 배치","취소","적용"};
    for(unsigned i=0;i<3;++i){const Vec2 p{160+224.f*float(i),654};
        addButton(Panel::system,ids[i],"Btn_Mission.png",p,1,2,0,1,Size{190,42},0,{}, {.4f,1},true,13,false);
        text("HUDLayout.tool."+std::to_string(i),labels[i],p,{192,24},"TrebuchetMS-Bold",18,TextAlign::center,{140.f/255,240.f/255,1,1},13);
    }
    if(!hud->error.empty())text("HUDLayout.error",hud->error,{384,598},{640,52},"TrebuchetMS-Bold",16,TextAlign::center,{1,.5f,.4f,1},13);
}
void Screens::Impl::trade(){
    const bool phone=menu::phone(layout.configuration());RegionScope scope(*this,phone?Region::phoneTrade:Region::panel);
    const Vec2 root=phone?Vec2{360,240}:Vec2{384,512};
    const auto at=[&](Vec2 legacy,Vec2 original){return plus(root,phone?Vec2{original.x*1.5f,original.y*1.5f}:legacy);};
    const auto asset=[&](std::string_view name){return phone?menu::phoneAsset(name):std::string(name);};
    const auto scale=phone?presentation(Region::phoneTrade).scale:1.f;const auto c=layout.configuration().canvas;
    addButton(Panel::trade,0,asset("TradeItemBG.png"),root,1,1,0,0,phone?Size{c.width/scale,c.height/scale}:Size{768,1024},0,state.tradeItemId,{1,1},true,13);
    const auto found=std::find_if(state.shopItems.begin(),state.shopItems.end(),[&](const auto& i){return i.dataId==state.tradeItemId;});
    const ItemView* entry=found==state.shopItems.end()?nullptr:&*found;
    if(!entry){const auto f=std::find_if(state.inventory.begin(),state.inventory.end(),[&](const auto& i){return i.dataId==state.tradeItemId;});if(f!=state.inventory.end())entry=&*f;}
    if(entry){icon(*entry,at({-98,66},{-78,66}),13);text("GuiShop.trade.name",entry->name,at({10,68},{24,68}),phone?Size{146,24}:Size{160,24},"TrebuchetMS-Bold",phone?12.f:18.f,TextAlign::left,{},13);}
    number(asset("NumTile01.png"),state.tradeCount,at({-58,18},{-52,18}),0,.5f,13,false,{},10);
    addButton(Panel::trade,2110,asset("Common_Btn_DEC.png"),at({8,19},{8,19}),1,2,0,1,phone?Size{76.5f,60}:Size{51,40},0,state.tradeItemId,{1,1},true,13);
    addButton(Panel::trade,2111,asset("Common_Btn_INC.png"),at({59,19},{59,19}),1,2,0,1,phone?Size{76.5f,60}:Size{51,40},0,state.tradeItemId,{1,1},true,13);
    number(phone?menu::phoneAsset("NumTile03.png"):"NumSet_Cr.png",state.tradePrice,at({52,-23},{32,-22}),phone?0:12,1,13);
    addButton(Panel::trade,2107,asset("Common_Btn_CANCEL.png"),at({-63,-62},{-52,-54}),1,2,0,1,phone?Size{150,60}:Size{100,40},0,state.tradeItemId,{1,1},true,13);
    addButton(Panel::trade,state.buyMode?2108:2109,asset(state.buyMode?"Common_Btn_BUY.png":"Common_Btn_SELL.png"),at({63,-62},{52,-54}),1,2,0,1,phone?Size{150,60}:Size{100,40},0,state.tradeItemId,{1,1},true,13);
}
void Screens::Impl::phoneCommand(){
    // Phone201 GuiCommandCenter init0x37024/list0x37d6c/info0x373e8.
    // All points below are Phone original coordinates expressed in HD-skin units.
    {RegionScope background(*this,Region::artBackdrop);image(menu::background(layout.configuration(),"CmdCenterBG.png"),{240,160},1,1,0,{1,1},false,0,1,7,"phone.command.bg");}
    {RegionScope footer(*this,Region::phoneFooter);addButton(Panel::root,2100,menu::phoneAsset("Btn_ShopExit.png"),{630,36},2,1,0,1,Size{138,60},0,{}, {1,1},true,9,false);}
    {RegionScope header(*this,Region::phoneHeader);addButton(Panel::root,5009,"",{292.5f,405},1,1,0,0,Size{192,48},0,{}, {1,1},true,9,false);}
    if(!state.commandListVisible){completedMission();return;}
    struct Row {const ItemView* item;unsigned id,index;bool accepted;};std::vector<Row> rows;
    const auto available=std::min<std::size_t>(5,state.availableMissions.size());
    for(unsigned i=0;i<std::min<std::size_t>(5,state.acceptedMissions.size());++i)rows.push_back({&state.acceptedMissions[i],5200+i,unsigned(available)+i,true});
    for(unsigned i=0;i<available;++i)rows.push_back({&state.availableMissions[i],5100+i,i,false});
    commandWindow=std::min(commandWindow,unsigned(rows.empty()?0:(rows.size()-1)/5));
    RegionScope content(*this,Region::phoneCommandList);
    for(unsigned i=commandWindow*5;i<std::min<unsigned>(unsigned(rows.size()),(commandWindow+1)*5);++i){const auto& row=rows[i];const Vec2 at{222,342-45.f*float(i%5)};
        addButton(Panel::mission,row.id,menu::phoneAsset("Btn_Mission.png"),at,1,2,state.selectedMission==int(row.index)?1u:0u,1,Size{324,45},0,row.item->dataId,{1,1},row.item->enabled,10,false);
        text("PhoneCommand.row."+row.item->dataId,row.item->name,at,{200,14},"TrebuchetMS-Bold",12,TextAlign::left,{100.f/255,200.f/255,1,1},10);
        if(row.accepted)image(menu::phoneAsset("AcceptedMark.png"),plus(at,{114,0}),1,1,0,{1,1},false,0,1,11);
    }
    if(rows.empty())text("PhoneCommand.empty","새로운 임무가 없습니다.",{222,342},{200,24},"TrebuchetMS",12,TextAlign::center,{},10);
    if(rows.size()>5){addButton(Panel::root,9300,menu::phoneAsset("Btn_InvenPage.png"),{96,114},2,1,0,0,Size{72,48},0,{}, {1,1},commandWindow>0,10,false);addButton(Panel::root,9301,menu::phoneAsset("Btn_InvenPage.png"),{324,114},2,1,1,1,Size{72,48},0,{}, {1,1},(commandWindow+1)*5<rows.size(),10,false);}
    if(state.selectedMission>=0){
        RegionScope info(*this,Region::phoneCommandInfo);
        text("PhoneCommand.title",state.missionTitle,{540,348},{200,16},"TrebuchetMS-Bold",12,TextAlign::center,{1,1,0,1},10);
        text("PhoneCommand.body",state.missionBody,{540,228},{176,128},"TrebuchetMS",12,TextAlign::left,{},10);
        if(std::size_t(state.selectedMission)<available){const auto& item=state.availableMissions[std::size_t(state.selectedMission)];addButton(Panel::mission,5004,menu::phoneAsset("Common_Btn_ACCEPT.png"),{540,132},1,2,0,1,{},0,item.dataId,{1,1},item.enabled,10,false);}
    }
}
void Screens::Impl::missionList(bool command){
    RegionScope scope(*this,Region::list);
    const Vec2 root{384,512};
    const auto available=command?std::min<std::size_t>(5,state.availableMissions.size()):0;
    const auto accepted=std::min<std::size_t>(5,state.acceptedMissions.size());
    const auto displayAvailable=available+(command&&available==0&&layout.configuration().enabled&&layout.configuration().compact?1u:0u);
    const float header=command&&accepted?64.f:0.f;
    const float detail=!command&&state.selectedMission>=0?256.f:0.f;
    const float length=float(displayAvailable+accepted)*44+header+(command?0.f:40.f);
    const float half=(length+detail)*.5f;
    listTop=root.y+half+64;listBottom=root.y-half-64;
    int detailSelection=state.selectedMission;
    if(command){
        const bool sameList=previous&&previous->state.screen==state.screen
            &&previous->state.commandListRevision==state.commandListRevision;
        if(sameList)this->command=previous->command;
        if(state.selectedMission>=0){
            this->command.detailSelection=state.selectedMission;
            if(!sameList||previous->state.selectedMission!=state.selectedMission
                ||previous->state.commandSelectionRevision!=state.commandSelectionRevision){
                this->command.size=0;
                this->command.detailY=root.y+half-32-44.f*float(state.selectedMission)
                    -146+this->command.displayedSize;
            }
        }else if(sameList){
            // ACCEPT clears the selection immediately. The old detail and
            // positions survive until Command.tick replaces the list.
            detailSelection=this->command.detailSelection;
            state.missionBody=previous->state.missionBody;
        }
    }
    auto remember=[&](std::size_t first,CommandPart part,float y,int row=-1){
        if(command)for(auto i=first;i<nodes.size();++i)commandNodes.push_back({i,part,y,row});
    };
    auto first=nodes.size();
    image("MissionList_Mid.png",root,1,1,0,{1,half/32},false,0,1,10);
    remember(first,CommandPart::middle,half);
    first=nodes.size();
    image("MissionList_Btm.png",plus(root,{0,-half-32}),1,1,0,{1,1},false,0,1,10);
    remember(first,CommandPart::bottom,root.y-half-32);
    first=nodes.size();
    image(command?"NewMissionList_Top.png":"MissionList_Top.png",plus(root,{0,half+32}),1,1,0,{1,1},false,0,1,10);
    remember(first,CommandPart::top,root.y+half+32);
    const Vec2 list=plus(root,{0,half-32});
    auto row=[&](const ItemView& entry,unsigned id,unsigned index,float y,bool isAccepted){
        if(!command&&state.selectedMission>=0&&int(index)>state.selectedMission)y-=256;
        const auto at=plus(list,{0,y});
        const auto first=nodes.size();
        addButton(Panel::mission,id,"Btn_Mission.png",at,1,2,0,1,isAccepted?std::optional<Size>(Size{420,44}):std::nullopt,0,entry.dataId,{1,1},entry.enabled,10);
        std::string label=entry.name;if(isAccepted)label+=" ["+std::to_string(entry.did)+"/"+std::to_string(entry.total)+"]";
        text("MissionList."+entry.dataId,std::move(label),at,{376,command?16.f:20.f},command?"Copperplate-Bold":"TrebuchetMS-Bold",command?18.f:17.f,TextAlign::left,{240.f/255,240.f/255,1,1},10);
        if(!command&&entry.complete)image("CompleteMark.png",plus(at,{136,0}),1,1,0,{1,1},false,0,1,10);
        if(state.selectedMission==int(index))image("SelMission.png",at,1,1,0,{1,1},false,2,1,10);
        remember(first,CommandPart::row,at.y,int(index));
        if(command&&state.selectedMission==int(index))commandSelectedRowY=at.y;
    };
    for(unsigned i=0;i<available;++i)row(state.availableMissions[i],5100+i,i,-44.f*float(i),false);
    if(command&&available==0){
        first=nodes.size();
        text("Command.noNewMission","There is not a new mission.",list,{384,24},"Copperplate-Bold",20,TextAlign::center,{140.f/255,240.f/255,1,1},10);
        remember(first,CommandPart::row,list.y);
    }
    const float acceptedY=-float(displayAvailable)*44-header;
    if(command&&accepted){
        const auto at=plus(list,{0,-float(displayAvailable)*44-10});
        first=nodes.size();
        image("MyMissions.png",at,1,1,0,{1,1},false,0,1,10);
        remember(first,CommandPart::row,at.y,int(available));
    }
    for(unsigned i=0;i<accepted;++i)row(state.acceptedMissions[i],5200+i,unsigned(available)+i,acceptedY-44.f*float(i),true);
    if(detailSelection>=0&&std::size_t(detailSelection)<available+accepted){
        const Vec2 info=plus(list,{0,-44.f*float(detailSelection)-146});
        first=nodes.size();
        image("MissionInfo.png",info,1,1,0,{1,1},false,0,1,10);
        text("MissionInfo.body",state.missionBody,plus(info,{-6,0}),{378,224},"Helvetica Neue",command?18.f:16.f,TextAlign::left,{100.f/255,240.f/255,240.f/255,1},10);
        remember(first,CommandPart::detail,0);
        if(command&&std::size_t(detailSelection)<available){const auto& selected=state.availableMissions[std::size_t(detailSelection)];
            first=nodes.size();
            const auto button=addButton(Panel::mission,5004,"Common_Btn_ACCEPT.png",plus(info,{122,-84}),1,2,0,1,{},0,selected.dataId,{1,1},selected.enabled,10);
            button->pushDepth=0;
            button->releaseHeight=0;
            button->releaseDelay=0;
            remember(first,CommandPart::accept,0);
        }
    }
    if(command)positionCommandList();
    if(!command)addButton(Panel::mission,5000,"Common_Btn_CLOSE.png",plus(root,{150,-half-16}),1,2,0,1,{},0,{}, {1,1},true,10);
}
void Screens::Impl::positionCommandList(){
    for(const auto& entry:commandNodes){
        auto& node=nodes[entry.index];
        float y=entry.baseY;
        float scale=-1;
        switch(entry.part){
            case CommandPart::middle:
                std::get<ImageNode>(node).image.scale.y=(entry.baseY+command.displayedSize)/32;
                continue;
            case CommandPart::top: y+=command.displayedSize;listTop=y+32;break;
            case CommandPart::bottom: y-=command.displayedSize;listBottom=y-32;break;
            case CommandPart::row:
                y+=command.displayedSize;
                if(entry.row>command.displayedSelection)y-=2*command.displayedSize;
                break;
            case CommandPart::detail: y=command.detailY;scale=command.size/128;break;
            case CommandPart::accept: y=command.detailY-84;break;
        }
        if(auto* image=std::get_if<ImageNode>(&node)){
            image->image.screenPosition.y=y;
            if(scale>=0)image->image.scale.y=scale;
        }else if(auto* text=std::get_if<TextNode>(&node)){
            text->text->position.y=y;
            if(scale>=0)text->text->scale.y=scale;
        }else if(auto* button=std::get_if<ButtonNode>(&node)){
            button->button->position.y=y;
        }
    }
    verticalPan=std::min(verticalPan,verticalLimit());
}
void Screens::Impl::tickCommandList(){
    constexpr float destination=128;
    if(state.selectedMission<0||command.size==destination)return;
    const float remaining=destination-command.size;
    command.size=remaining<=.1f?destination:command.size+remaining*.25f;
    command.displayedSize=command.size;
    command.displayedSelection=state.selectedMission;
    // The list follows the top cap. The selected row rises by wndSize
    // while the detail center sits wndSize + 24 below that row.
    command.detailY=commandSelectedRowY-24;
    positionCommandList();
}
void Screens::Impl::completedMission(){
    RegionScope scope(*this,Region::panel);
    const Vec2 root{384,512};const auto& mission=state.completedMission;
    if(mission.dataId.empty())return;
    image("MissionCompletedBG.png",root,1,1,0,{1,1},false,0,1,9);
    const std::string prefix="Command.completed."+mission.dataId;
    text(prefix+".title",mission.title,plus(root,{0,42}),{256,20},"TrebuchetMS-Bold",16,TextAlign::center,{100.f/255,240.f/255,1,1},9);
    addButton(Panel::mission,5007,"Common_Btn_OK.png",plus(root,{62,-74}),1,2,0,1,{},0,mission.dataId,{1,1},true,9);
    for(std::size_t k=0;k<mission.rewards.size();++k){
        const auto& reward=mission.rewards[k];const Vec2 at=plus(root,{float(k%2)*100-84,float(k/2)*-52-10});
        image(reward.image,reward.type==2?plus(at,{8,8}):at,1,1,0,{1,1},false,0,1,9);
        const auto value=std::to_string(reward.value)+(reward.type==0?"Cr":"");
        text(prefix+".reward"+std::to_string(k),value,plus(at,{reward.type==1?50.f:60.f,0}),{reward.type==0?100.f:60.f,14},"TrebuchetMS-Bold",13,TextAlign::left,{},9);
    }
}
void Screens::Impl::airlift(){
    RegionScope scope(*this,Region::panel);
    const Vec2 root{384,512};unsigned count=0;for(const auto& base:state.airlifts)if(!base.dataId.empty())++count;
    const float half=float(count)*22+20;image("MissionList_Mid.png",root,1,1,0,{1,half/32},false,0,1,10);
    image("MissionList_Btm.png",plus(root,{0,-half-32}),1,1,0,{1,1},false,0,1,10);
    image("AirliftBG.png",plus(root,{0,half+32}),1,1,0,{1,1},false,0,1,10);
    if(state.selectedAirlift>=0)addButton(Panel::airlift,4001,"Common_Btn_OK.png",plus(root,{-10,-half-16}),1,2,0,1,{},0,{}, {1,1},true,10);
    addButton(Panel::airlift,4000,"Common_Btn_CLOSE.png",plus(root,{136,-half-16}),1,2,0,1,{},0,{}, {1,1},true,10);
    for(unsigned i=0;i<6;++i){const auto& base=state.airlifts[i];if(base.dataId.empty())continue;
        const auto at=plus(root,{0,half+32-64-44.f*float(i)});
        addButton(Panel::airlift,4002+i,"Btn_Mission.png",at,1,2,0,1,{},0,base.dataId,{1,1},base.enabled,10);
        text("Airlift.name."+base.dataId,base.name,at,{360,24},"TrebuchetMS-Bold",20,TextAlign::left,{240.f/255,240.f/255,1,1},10);
        text("Airlift.price."+base.dataId,std::to_string(base.price)+"Cr",plus(at,{120,0}),{128,24},"TrebuchetMS-Bold",20,TextAlign::right,{1,1,154.f/255,1},10);
        if(state.selectedAirlift==int(i))image("SelMission.png",at,1,1,0,{1,1},false,2,1,10);
    }
}
void Screens::Impl::weaponChoices(){
    RegionScope scope(*this,Region::chooser);
    if(state.weaponChoiceType!=2&&state.weaponChoiceType!=3)throw std::runtime_error("Original weapon chooser type must be2 or3");
    for(unsigned i=0;i<state.weaponChoices.size();++i){const auto& entry=state.weaponChoices[i];
        if(entry.position<0||entry.dataId.empty())throw std::runtime_error("Weapon chooser requires actual inventory position and ItemCode");
        const auto key=ButtonKey{owner(state.screen,Panel::weaponChoices),1024,entry.position,entry.dataId};
        Vec2 initial{405,90};
        if(previous&&previous->state.weaponChoiceType==state.weaponChoiceType)if(const auto b=previous->dispatcher.find(key))initial=b->position;
        auto b=addButton(Panel::weaponChoices,1024,"Btn_Mission.png",initial,1,2,0,1,{},entry.position,entry.dataId,{1,1},entry.enabled);
        const double sine=std::sin(double(float(i)*.5f));
        const Vec2 target{float((state.weaponChoiceType==2?333.:478.)+sine*(state.weaponChoiceType==2?-50.:50.)),float(i*36+140)};
        chooserMoves.emplace_back(b,target);
        if(!entry.largeIcon.empty()){
            const auto size=nodes.size();image(entry.largeIcon,plus(initial,{-100,0}),1,1,0,{1,1},false,0,1,9);
            if(nodes.size()!=size){auto& icon=std::get<ImageNode>(nodes.back());icon.parent=b;icon.local={-100,0};icon.image.angle=-1.5707964f;}
        }
        auto size=nodes.size();text("WeaponChoice.name."+entry.dataId,entry.name,plus(initial,{20,0}),{128,18},"Arial-BoldMT",14,TextAlign::left,{180.f/255,1,1,1});
        if(nodes.size()!=size){auto& label=std::get<TextNode>(nodes.back());label.parent=b;label.local={20,0};}
        std::array<char,128> value{};std::snprintf(value.data(),value.size(),"Atk %d (%+d)",entry.attackPoint,entry.attackDelta);
        size=nodes.size();text("WeaponChoice.attack."+entry.dataId,value.data(),plus(initial,{80,0}),{128,18},"Arial-BoldMT",14,TextAlign::right);
        if(nodes.size()!=size){auto& label=std::get<TextNode>(nodes.back());label.parent=b;label.local={80,0};}
    }
}
void Screens::Impl::build(){
    switch(state.screen){
        case ScreenId::title:titleScreen();break;
        case ScreenId::slots:slots();break;
        case ScreenId::base:base();break;
        case ScreenId::maintenance:maintenance();break;
        case ScreenId::shop:shop();break;
        case ScreenId::command:
            if(menu::phone(layout.configuration())){phoneCommand();break;}
            if(menu::phone(layout.configuration())){RegionScope backdrop(*this,Region::artBackdrop);image(menu::background(layout.configuration(),"CmdCenterBG.png"),{240,160},1,1,0,{1,1},false,0,1,7,"phone.command.bg");}else image("CmdCenterBG.png",{384,512});
            // GuiCommandCenter creates these tiles before populating its rows.
            // The completion popup is painted over the collapsed background.
            if(!state.commandListVisible){
                image("MissionList_Mid.png",{384,512});
                image("MissionList_Btm.png",{384,480});
                image("NewMissionList_Top.png",{384,544});
            }
            addButton(Panel::root,5009,"Common_Btn_RANKING.png",{484,52});
            addButton(Panel::root,2100,"Common_Btn_EXIT.png",{640,52});
            if(state.commandListVisible)missionList(true);
            completedMission();break;
        case ScreenId::field:{
            const int overlays=int(state.inventoryVisible)+int(state.missionVisible)+int(state.airliftVisible)+int(state.systemVisible);
            if(overlays>1)throw std::runtime_error("Original field dialogs are mutually exclusive");
            field();
            if(state.inventoryVisible)inventoryDialog({384,512});
            if(state.missionVisible)missionList(false);
            if(state.airliftVisible)airlift();
            if(hud->editing)hudEditor();else if(state.systemVisible)system();
            break;
        }
        default:throw std::runtime_error("Unknown original screen ID");
    }
    verticalPan=std::min(verticalPan,verticalLimit());
    syncMessage();
    syncPlaceName();
    dispatcher.subscribe({},[this](const ButtonEvent& event){
        const auto panel=panels.at(event.key.owner);const auto screen=state.screen;const unsigned id=event.key.id;
        // Side effects are requested at their original handler event boundary.
        // GameSession applies the real state transition and may refresh Screens
        // synchronously; this old Impl remains alive through the input call.
        if(menu::phone(layout.configuration())&&state.screen==ScreenId::command&&(id==9300||id==9301)){
            if(event.type==ButtonEventType::pop){if(id==9300&&commandWindow)--commandWindow;else if(id==9301)++commandWindow;pageRebuild=true;dispatcher.discardAll();host.sfx(0);}return;
        }
        if(phoneMenu()&&(id==2105||id==2106||id==6090||id==6091)&&!state.tradeVisible){
            if(event.type!=ButtonEventType::pop)return;
            auto& window=id<6000?stockWindow:inventoryWindow;const bool next=id==2106||id==6091;
            const auto count=id<6000?state.shopItems.size():state.inventory.size();const int page=id<6000?state.shopPage:state.inventoryPage;const unsigned capacity=id<6000?24:25,visible=id<6000?stockCells():inventoryCells();
            const auto pages=id<6000?count/24+1:std::size_t(std::max(1,state.inventoryPages));
            const auto targetPage=std::size_t(std::clamp(page+(next?0:-1),0,int(pages)-1));
            const unsigned targetCount=id<6000?unsigned(count>targetPage*capacity?std::min<std::size_t>(capacity,count-targetPage*capacity):0):capacity;
            if((next&&(window+1)*visible<targetCount)||(!next&&window>0)){window=next?window+1:window-1;dispatcher.discardAll();pageRebuild=true;
                if(state.shopDetailVisible||state.detailInventory>=0){auto close=event;close.type=ButtonEventType::push;close.key={owner(state.screen,state.screen==ScreenId::maintenance?Panel::root:Panel::inventory),2112,0,{}};host.action({state.screen,state.screen==ScreenId::maintenance?Panel::root:Panel::inventory,close});}host.sfx(0);return;}
            // The session clamps boundary presses. Keep the current strip
            // when pressing Next on the last page.
            const bool changesPage=next?std::size_t(page)+1<pages:page>0;
            if(changesPage)window=!next&&targetCount?(targetCount-1)/visible:0;
            auto advance=event;advance.type=ButtonEventType::push;host.action({screen,panel,advance});return;
        }
        if(phoneMenu()&&screen==ScreenId::shop&&(id==9204||id==9205)){
            if(event.type!=ButtonEventType::pop||state.tradeVisible)return;
            dispatcher.discardAll();
            setPhoneEquipment(id==9204);
            pageRebuild=true;
            if(state.shopDetailVisible||state.detailInventory>=0){auto clear=event;clear.type=ButtonEventType::push;clear.key={owner(state.screen,state.screen==ScreenId::maintenance?Panel::root:Panel::inventory),2112,0,{}};host.action({state.screen,state.screen==ScreenId::maintenance?Panel::root:Panel::inventory,clear});}else host.sfx(0);
            return;
        }
        if(id>=9100&&id<=9103){
            if(event.type==ButtonEventType::pop){
                if(id==9100){dispatcher.discardAll();hud->begin();}else if(id==9101)hud->reset();else if(id==9102)hud->cancel();else applyHUDEdit();
                host.sfx(0);
            }return;
        }
        ScreenAction action{screen,panel,event};
        if(event.canvasPosition){
            const auto inventoryRegion=screen==ScreenId::shop?Region::shopInventory:screen==ScreenId::maintenance?Region::maintenance:Region::panel;
            action.dropCoordinates=DropCoordinates{phoneMenu()?phoneDropPoint(*event.canvasPosition):presentation(inventoryRegion).inverse(*event.canvasPosition),presentation(Region::bottom,hm2::hud::controls).inverse(*event.canvasPosition)};
        }
        host.action(action);
        if(screen==ScreenId::slots){
            if(event.type==ButtonEventType::push&&id==1009)host.sfx(0);
            else if(event.type==ButtonEventType::pop&&(id==1001||id==1004||id==1010))host.sfx(1);
        }else if(screen==ScreenId::base){
            if(event.type==ButtonEventType::push&&id>=2001&&id<=2006)host.sfx(0);
        }else if(screen==ScreenId::field&&panel==Panel::root){
            if(event.type==ButtonEventType::push&&id>=1022&&id<=1024)host.sfx(4);
            else if(event.type==ButtonEventType::pop&&(id==1021||id==1025||id==1026||id==1027||id==1029||id==1030||id==1031||id==1032))host.sfx(0);
        }
    });
}
void Screens::Impl::tick(float time,WorldRandom& random){
    if(!std::isfinite(time))throw std::runtime_error("Nonfinite original UI clock");now=time;
    if(state.screen==ScreenId::field)tickMessage();
    // GuiGame keeps ticking its banner while hidden if a slot is loaded.
    tickPlaceName();
    if(state.screen==ScreenId::command&&state.commandListVisible&&!menu::phone(layout.configuration()))tickCommandList();
    if(state.screen==ScreenId::base)tickBase(time);
    tickRange(random);tickFieldCredits();tickDetail();
    if(state.screen==ScreenId::title){
        auto& t=title;const auto spec=menu::title(layout.configuration());const bool phone=menu::phone(layout.configuration());
        t.bgRotate=t.bgRotate<0?t.bgRotate+(.01f-t.bgRotate)/60.f:0;
        t.bgScale=t.bgScale>1?t.bgScale+(.99f-t.bgScale)/100.f:1;
        if(phone&&t.bgRotate<0&&std::abs(.01f-t.bgRotate)<=.01f)t.bgRotate=.01f;
        auto approach=[](float p){return p!=0&&std::abs(p)>.1f?p+(0-p)/50.f:0.f;};
        if(!phone)t.bgPosition={approach(t.bgPosition.x),approach(t.bgPosition.y)};
        if(!t.titleSound&&t.start+spec.titleIn<time){host.sound("Title_TitleIn.wav");t.titleSound=true;}
        if(!t.episodeSound&&t.start+spec.episodeIn<time){host.sound("Title_TitleIn.wav");t.episodeSound=true;}
        if(t.bigX>spec.bigEnd)t.bigX-=spec.bigStep;
        if(t.titleScale!=1&&t.start+spec.titleIn+.3f<time){
            if(t.titleScale<=1){host.sound("Title_Bang.wav");host.quake(10,4,.3f);t.titleScale=1;}
            else t.titleScale-=5;
            t.titleVisible=true;
        }
        if(t.episodeScale!=1&&t.start+spec.episodeIn+.3f<time){
            if(t.episodeScale<=1){host.sound("Title_Bang.wav");host.quake(6,4,.3f);t.episodeScale=1;}
            else t.episodeScale-=5;
            t.episodeVisible=true;
        }
        if(t.start+spec.prompt<time){const float phase=float(std::fmod(double(time-(t.start+spec.prompt)),2.));
            if(phase<.2f){if(!t.startSound){host.sound("Title_TapToStart.wav");t.startSound=true;}
                if(random.random()%10<=5)t.startVisible=((random.random()^1)&1)!=0;
            }else if(phase<1.2f)t.startVisible=true;
            else if(t.startSound){t.startSound=false;t.startVisible=false;}
        }
        updateTitleImages();
    }
    for(auto& node:nodes){
        if(auto* number=std::get_if<NumberNode>(&node)){
            if(number->tag=="hud.displayedCredits"&&number->font->number()!=fieldPresentation.displayedCredits)
                number->font->setNumber(fieldPresentation.displayedCredits);
            number->font->tick();
        }
    }
    for(auto& [button,target]:chooserMoves){
        auto move=[](float p,float dest){return p==dest?p:std::abs(p-dest)>.5f?p+(dest-p)/3.f:dest;};
        button->position={move(button->position.x,target.x),move(button->position.y,target.y)};
    }
}
void Screens::Impl::append(Frame& frame,float time){
    for(const auto& node:nodes)std::visit([&](const auto& n){
        using T=std::decay_t<decltype(n)>;if(!phoneNodeVisible(n.region))return;if constexpr(std::is_same_v<T,ImageNode>){if(n.tag=="phone.stock.bg"&&phoneEquipment)return;if(n.tag=="phone.equip.bg"&&!phoneEquipment)return;}
        auto& layer=layers->at(n.layer);layer.setPresentation(presentation(n.region,n.hudGroup),clipping(n.region,n.hudGroup));
        if constexpr(std::is_same_v<T,ImageNode>){auto image=n.image;if(n.parent)image.screenPosition=plus(n.parent->screenPosition(),n.local);
            if(n.tag=="hud.background"&&splitHUDBackground()){
                if(!n.hudBackgroundParts)throw std::logic_error("HUD source slices were not prepared");
                // Geometry/UV share exact source edges. Scissor retains only
                // the outer original/safe boundary, never partitions adjacent
                // full-size quads with floor/ceil-rounded internal scissors.
                for(unsigned group=0;group<hud::count;++group){layer.setPresentation(presentation(n.region,int(group)),clipping(n.region,int(group)));layer.submitNonAtlas(n.hudBackgroundParts->at(group));}
            }else if(n.menuPanel)layer.submitNonAtlas(*n.menuPanel);
            else if(n.tag=="phone.command.bg"){
                const auto c=layout.configuration().canvas;const float s=std::max(c.width/480,c.height/320),header=menu::menuHeader(layout.configuration()),footer=menu::menuFooter(layout.configuration());
                for(unsigned band=0;band<3;++band){const float offset=band==0?c.height-320*s:band==1?c.height-header-35-220*s:0;const ClipRect clip=band==0?ClipRect{0,c.height-header,c.width,header}:band==1?ClipRect{0,footer,c.width,std::max(0.f,c.height-header-footer)}:ClipRect{0,0,c.width,footer};layer.setPresentation({s,{(c.width-480*s)*.5f,offset}},clip);layer.submitImage(image);}
            }else layer.submitImage(image);}
        else if constexpr(std::is_same_v<T,ButtonNode>){Frame f;n.button->append(f,time);for(auto& s:f.sprites){s.angle+=n.angle;layer.submitNonAtlas(std::move(s));}}
        else if constexpr(std::is_same_v<T,TextNode>){Frame f;if(n.parent)n.text->position=plus(n.parent->screenPosition(),n.local);n.text->append(f);for(auto& s:f.sprites)layer.submitNonAtlas(std::move(s));}
        else if constexpr(std::is_same_v<T,NumberNode>)n.font->append(layer,tiles.visual(*n.tile));
        else if constexpr(std::is_same_v<T,PreviewNode>)for(const auto& sprite:n.frame.sprites)
            {auto& target=layers->at(sprite.visualLayer.value_or(n.layer));target.setPresentation(presentation(n.region),clipping(n.region));target.submitNonAtlas(sprite);}
        else if constexpr(std::is_same_v<T,DetailNode>){const auto& d=*n.detail;
            for(const auto& label:{d.title,d.description})if(label){Frame textFrame;label->append(textFrame);for(auto& sprite:textFrame.sprites)layer.submitNonAtlas(std::move(sprite));}
            if(d.large){auto image=*d.large;layer.submitImage(image);}
        }
        else if constexpr(std::is_same_v<T,BaseDescriptionNode>){
            auto background=n.background;
            layer.submitImage(background);
            if(n.text){Frame f;n.text->append(f);for(auto& s:f.sprites)layer.submitNonAtlas(std::move(s));}
        }
        else if constexpr(std::is_same_v<T,RadarNode>){
            // GuiGame.init0x29c9c / DlgRader.draw0x23d28 use world positions,
            // independent of camera and enemy visibility.
            VisualImage image;
            image.tile=n.tile;
            for(const auto& mark:state.radarMarks){
                image.tileNo=mark.tile;
                image.screenPosition={409.f+mark.worldPosition.x*.0625f,40.f+mark.worldPosition.y*.0625f};
                layer.submitImage(image);
            }
        }
    },node);
    if(state.screen==ScreenId::field&&message.initialized){auto& layer=layers->at(9);layer.setPresentation(presentation(Region::bottom),clipping(Region::bottom));layer.submitImage(message.background);if(message.text){Frame textFrame;message.text->append(textFrame);for(auto& s:textFrame.sprites)layer.submitNonAtlas(std::move(s));}}
    if(state.screen==ScreenId::field&&!placeName.background.tile.image.empty()&&placeName.background.show){
        auto& layer=layers->at(9);layer.setPresentation(presentation(Region::banner),clipping(Region::banner));
        auto background=placeName.background;background.screenPosition=menu::placeNamePosition(layout.configuration(),background.screenPosition);
        layer.submitImage(background);
        if(placeName.label){
            Frame textFrame;
            placeName.shadow->position=plus(background.screenPosition,{2,-2});
            placeName.shadow->append(textFrame);
            placeName.label->position=background.screenPosition;
            placeName.label->append(textFrame);
            for(auto& sprite:textFrame.sprites)layer.submitNonAtlas(std::move(sprite));
        }
    }
    for(auto& layer:*layers){layer.setPresentation();layer.drain().appendBatches(frame);}
    // Each node owns its clip. A final safe-area intersection would cut the
    // full-screen backdrops back into black side bars after assembly.
    if(layout.configuration().enabled){frame.width=layout.configuration().canvas.width;frame.height=layout.configuration().canvas.height;}

}

void Screens::Impl::updateHUD(const ScreensState& next){
    state.messageWorldTime=next.messageWorldTime;state.selectedSlot=next.selectedSlot;
    state.hpBarWidth=next.hpBarWidth;state.hpBarFilled=next.hpBarFilled;
    state.spBarWidth=next.spBarWidth;state.spBarFilled=next.spBarFilled;state.boosterReady=next.boosterReady;
    state.experienceRate=next.experienceRate;state.enemyHpRate=next.enemyHpRate;
    state.level=next.level;state.credits=next.credits;state.hpRate=next.hpRate;state.spRate=next.spRate;
    state.radarMarks=next.radarMarks;
    state.fieldWorldTime=next.fieldWorldTime;state.fieldUIRevision=next.fieldUIRevision;state.hasWorkingSlot=next.hasWorkingSlot;
    state.outOfRangeTimeLeft=next.outOfRangeTimeLeft;state.outOfRangeTimeRight=next.outOfRangeTimeRight;
    state.enemyName=next.enemyName;state.enemyNameRevision=next.enemyNameRevision;
    if(fieldPresentation.identity!=next.fieldUIRevision){++*resourceGeneration;const auto oldName=fieldPresentation.enemyName;std::erase_if(nodes,[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==oldName;});fieldPresentation={};fieldPresentation.identity=next.fieldUIRevision;}
    syncEnemyName();
    for(auto& node:nodes){
        if(auto* n=std::get_if<ImageNode>(&node)){auto& v=n->image;
            if(n->tag=="hud.hpBack")v.screenPosition.x=384+float(next.hpBarWidth)-398;
            else if(n->tag=="hud.hp")v.screenPosition.x=384+float(next.hpBarFilled)-398;
            else if(n->tag=="hud.spBack")v.screenPosition.x=384+float(next.spBarWidth)-398;
            else if(n->tag=="hud.sp"){v.screenPosition.x=384+next.spBarFilled-398;v.tileNo=next.boosterReady?3:2;}
            else if(n->tag=="hud.exp"){v.screenPosition.x=384+115*next.experienceRate-343;v.scale.x=next.experienceRate;}
            else if(n->tag=="hud.enemyBack")v.show=next.enemyHpRate!=0;
            else if(n->tag=="hud.enemy"){v.show=next.enemyHpRate!=0;v.screenPosition.x=384-128*(1-next.enemyHpRate);v.scale.x=next.enemyHpRate;}
            else if(n->tag=="hud.rangeLeft")v.show=fieldPresentation.range[0];
            else if(n->tag=="hud.rangeRight")v.show=fieldPresentation.range[1];
        }else if(auto* n=std::get_if<NumberNode>(&node)){
            if(n->tag=="hud.level")n->font->setNumber(std::bit_cast<std::int32_t>(std::uint32_t(next.level)+1));
            else if(n->tag=="hud.credits")n->font->setNumber(next.credits);
            else if(n->tag=="hud.displayedCredits"&&n->font->number()!=fieldPresentation.displayedCredits)
                n->font->setNumber(fieldPresentation.displayedCredits);
        }else if(auto* n=std::get_if<PreviewNode>(&node)){
            const auto& slot=next.slots[n->slot];
            if(n->revision!=slot.machPreviewRevision){
                n->frame=translatedPreview(slot.machPreview,n->origin);
                n->revision=slot.machPreviewRevision;
            }
        }
    }
}
Screens::Screens(ResourceAccess& r,PreparedTiles& t,ScreensHost& h):hud_(std::make_shared<HUDState>()),resources_(r),tiles_(t),host_(h){}
bool Screens::updateHUD(const ScreensState& state){
    if(!impl_||impl_->state.screen!=state.screen)return false;
    impl_->updateHUD(state);return true;
}
void Screens::refresh(const ScreensState& state,float time){
    const auto priorOwnerRevision=tileOwnerRevision_;
    hud_->selectProfile(display_,host_);
    if(hud_->editing&&(state.screen!=ScreenId::field||!state.systemVisible)){hud_->cancel();hud_->rebuild=false;}
    if(fieldTiles_&&fieldTileRevision_!=state.fieldUIRevision)fieldTiles_.reset();
    if(state.screen==ScreenId::field){
        if(!fieldTiles_){fieldTiles_=std::make_shared<PreparedTiles>("screens.field."+std::to_string(++tileOwnerRevision_));fieldTileRevision_=state.fieldUIRevision;}
        activeTiles_=fieldTiles_;
    }else if(!activeTiles_||tileScreen_!=state.screen||(impl_&&impl_->state.baseId!=state.baseId)){activeTiles_=std::make_shared<PreparedTiles>("screens.owner."+std::to_string(++tileOwnerRevision_));}
    tileScreen_=state.screen;
    auto next=std::make_shared<Impl>(resources_,*activeTiles_,host_,state,time,impl_,display_,hud_,activeTiles_,resourceGeneration_);next->build();
    const bool changed=!impl_||impl_->state.screen!=state.screen;
    next->previous.reset();impl_=std::move(next);
    if(changed){if(state.screen==ScreenId::title)host_.bgm("STAGE1.wav");else if(state.screen==ScreenId::slots)host_.bgm("STAGE2.wav");else if(state.screen==ScreenId::base)host_.bgm("Title.wav");else if(state.screen==ScreenId::field)host_.bgm("STAGE3.mp3");}
    if(state.screen==ScreenId::title&&tileOwnerRevision_!=priorOwnerRevision){
        // GuiTitle.init0x32340: after BGM, request these three cached sounds
        // in the original order. Do not play them or repeat on scalar refresh.
        for(const auto* name:{"Title_TitleIn.wav","Title_Bang.wav","Title_TapToStart.wav"})host_.prepareSound(name);
    }
}
void Screens::tick(float time,WorldRandom& random){if(auto keep=impl_)keep->tick(time,random);}
bool Screens::input(PointerPhase phase,Vec2 point,std::uintptr_t pointer,float time){
    const auto keep=impl_;if(!keep)return false;
    if(hud_->editing){
        const auto rebuild=[&]{if(hud_->rebuild){hud_->rebuild=false;refresh(keep->state,time);}};
        const auto toolMap=[keep](const Button& button,Vec2 canvas){return button.key().id>=9101&&button.key().id<=9103?keep->presentation(keep->buttonRegion(button)).inverse(canvas):Vec2{1.e9f,1.e9f};};
        bool toolCaptured=false;for(const auto& key:keep->keys)if(key.id>=9101&&key.id<=9103)if(auto b=keep->dispatcher.find(key);b&&b->capturedPointer()==pointer)toolCaptured=true;
        if(hud_->dragPointer==pointer&&pointer){
            if(phase==PointerPhase::move){const Vec2 origin{hud_->dragOrigin.x+point.x-hud_->dragStart.x,hud_->dragOrigin.y+point.y-hud_->dragStart.y};hud_->draft.positions[hud_->selected]=hud::draggedOrigin(hud_->selected,keep->layout,origin);}
            if(phase==PointerPhase::end||phase==PointerPhase::cancel)hud_->dragPointer=0;
            return true;
        }
        const bool tool=keep->dispatcher.input(phase,point,pointer,time,toolMap);
        if(tool||toolCaptured){rebuild();return true;}
        if(phase==PointerPhase::begin&&pointer){
            for(unsigned i=hud::count;i>0;--i){const auto b=hud::visualBounds(hud_->draft,i-1,keep->layout);if(point.x>=b.x&&point.x<=b.x+b.width&&point.y>=b.y&&point.y<=b.y+b.height){hud_->selected=i-1;hud_->dragPointer=pointer;hud_->dragStart=point;hud_->dragOrigin={b.x,b.y};break;}}
        }
        rebuild();return true;
    }
    if(keep->state.screen==ScreenId::title){
        if(phase==PointerPhase::end){
            const bool ready=keep->title.start+menu::title(keep->layout.configuration()).prompt<=time;
            if(!ready){keep->title.start=time-menu::title(keep->layout.configuration()).prompt;keep->title.bigVisible=false;keep->updateTitleImages();}
            host_.action({ScreenId::title,Panel::root,{ButtonEventType::pop,{owner(ScreenId::title,Panel::root),0,0},pointer,point},true,ready});
        }
        return true;
    }
    const bool artScene=menu::phone(keep->layout.configuration())&&(keep->state.screen==ScreenId::base||keep->phoneMenu());
    if(phase==PointerPhase::begin&&keep->layout.configuration().enabled){const auto& c=keep->layout.configuration();const bool fullSurface=artScene||(c.compact&&keep->state.screen==ScreenId::field);const auto a=fullSurface?ClipRect{0,0,c.canvas.width,c.canvas.height}:c.content();if(point.x<a.x||point.x>a.x+a.width||point.y<a.y||point.y>a.y+a.height)return false;}
    const bool verticalOwned=keep->verticalPointer==pointer&&pointer;
    if(verticalOwned){
        if(phase==PointerPhase::move){const float delta=point.y-keep->verticalStart.y;if(std::abs(delta)>8)keep->verticalDragging=true;if(keep->verticalDragging){keep->dispatcher.discardAll();const float direction=keep->scrollingRegion()==Region::chooser?-1.f:1.f;keep->verticalPan=std::clamp(keep->verticalOrigin+delta*direction,0.f,keep->verticalLimit());return true;}}
        if(phase==PointerPhase::end||phase==PointerPhase::cancel){keep->verticalPointer=0;if(keep->verticalDragging){keep->verticalDragging=false;return true;}}
    }
    if(phase==PointerPhase::begin&&pointer&&keep->verticalLimit()>0){keep->verticalPointer=pointer;keep->verticalStart=point;keep->verticalOrigin=keep->verticalPan;keep->verticalDragging=false;}
    bool captured=false;for(const auto& key:keep->keys)if(const auto b=keep->dispatcher.find(key);b&&b->capturedPointer()==pointer&&pointer)captured=true;
    if(keep->panPointer==pointer&&pointer){
        if(phase==PointerPhase::move)keep->shopPan=std::clamp(keep->panOrigin-(point.x-keep->panStart.x),0.f,keep->layout.shopPanLimit());
        if(phase==PointerPhase::end||phase==PointerPhase::cancel)keep->panPointer=0;
        return true;
    }
    const auto mapper=[keep](const Button& button,Vec2 canvas){const auto region=keep->buttonRegion(button);const auto group=keep->buttonHUDGroup(button);if(const auto clip=keep->clipping(region,group);clip&&(canvas.x<clip->x||canvas.x>clip->x+clip->width||canvas.y<clip->y||canvas.y>clip->y+clip->height))return Vec2{1.e9f,1.e9f};return keep->presentation(region,group).inverse(canvas);};
    const bool handled=(keep->layout.configuration().enabled||hud_->active().customized())?keep->dispatcher.input(phase,point,pointer,time,mapper):keep->dispatcher.input(phase,point,pointer,time);
    if(keep->pageRebuild){refresh(impl_->state,time);return true;}
    if(hud_->rebuild){hud_->rebuild=false;refresh(keep->state,time);return true;}
    if(handled||captured||verticalOwned||(keep->verticalPointer==pointer&&pointer))return true;
    if(phase==PointerPhase::begin&&pointer&&keep->state.screen==ScreenId::shop&&!menu::phone(keep->layout.configuration())&&keep->layout.shopPanLimit()>0&&!keep->state.tradeVisible){keep->panPointer=pointer;keep->panStart=point;keep->panOrigin=keep->shopPan;return true;}
    return keep->state.tradeVisible||keep->state.systemVisible||keep->state.missionVisible||keep->state.airliftVisible;
}
void Screens::append(Frame& out,float time)const{if(auto keep=impl_)keep->append(out,time);}
void Screens::appendPreparation(Frame& out)const{
    if(activeTiles_)activeTiles_->appendPreparation(out);
    if(fieldTiles_&&fieldTiles_!=activeTiles_)fieldTiles_->appendPreparation(out);
    const auto keep=impl_;if(!keep)return;
    const auto text=[&](const std::shared_ptr<Text>& label){if(label){auto copy=*label;copy.visible=true;copy.append(out);}};
    for(const auto& node:keep->nodes)std::visit([&](const auto& n){using T=std::decay_t<decltype(n)>;
        if constexpr(std::is_same_v<T,TextNode>)text(n.text);
        else if constexpr(std::is_same_v<T,PreviewNode>)out.sprites.insert(out.sprites.end(),n.frame.sprites.begin(),n.frame.sprites.end());
        else if constexpr(std::is_same_v<T,BaseDescriptionNode>)text(n.text);
        else if constexpr(std::is_same_v<T,DetailNode>){text(n.detail->title);text(n.detail->description);}
    },node);
    text(keep->fieldPresentation.enemyName);text(keep->message.text);text(keep->placeName.label);text(keep->placeName.shadow);
    if(!keep->placeName.background.tile.image.empty()){Sprite resource;resource.image=keep->placeName.background.tile.image;resource.textureSource=keep->placeName.background.tile.textureSource;out.sprites.push_back(std::move(resource));}
}
const TileImage* Screens::preparedTile(std::string_view name)const{return activeTiles_?activeTiles_->lookup(name):nullptr;}
void Screens::discardInputs(){if(impl_){impl_->dispatcher.discardAll();impl_->panPointer=0;impl_->verticalPointer=0;impl_->verticalDragging=false;hud_->dragPointer=0;}}
void Screens::setDisplay(DisplayConfiguration display){DisplayLayout check(display);discardInputs();display_=display;if(impl_){const auto state=impl_->state;const auto now=impl_->now;refresh(state,now);}}
Vec2 Screens::buttonScreenPosition(ButtonKey key)const{const auto button=this->button(key);if(!button)throw std::out_of_range("Unknown screen button");return impl_->presentation(impl_->buttonRegion(*button),impl_->buttonHUDGroup(*button)).map(button->screenPosition());}
bool Screens::hudEditing()const{return hud_->editing;}
const hud::Configuration& Screens::hudConfiguration()const{return hud_->active();}
std::string Screens::hudEditorError()const{return hud_->error;}
bool Screens::beginHUDEdit(){if(!impl_||impl_->state.screen!=ScreenId::field||!impl_->state.systemVisible)return false;hud_->begin();hud_->rebuild=false;const auto state=impl_->state;const auto now=impl_->now;discardInputs();refresh(state,now);return true;}
bool Screens::applyHUDEdit(){if(!impl_||!hud_->editing)return false;const bool ok=impl_->applyHUDEdit();hud_->rebuild=false;const auto state=impl_->state;const auto now=impl_->now;refresh(state,now);return ok;}
void Screens::cancelHUDEdit(){if(!impl_||!hud_->editing)return;hud_->cancel();hud_->rebuild=false;const auto state=impl_->state;const auto now=impl_->now;refresh(state,now);}
void Screens::resetHUDEdit(){if(!impl_||!hud_->editing)return;hud_->reset();hud_->rebuild=false;const auto state=impl_->state;const auto now=impl_->now;refresh(state,now);}
ClipRect Screens::buttonScreenBounds(ButtonKey key)const{const auto b=button(key);if(!b)throw std::out_of_range("Unknown screen button");const auto t=impl_->presentation(impl_->buttonRegion(*b),impl_->buttonHUDGroup(*b));const auto p=t.map(b->screenPosition());const auto r=b->bounds();ClipRect result{p.x+r.left*t.scale,p.y+r.bottom*t.scale,(r.right-r.left)*t.scale,(r.top-r.bottom)*t.scale};if(const auto clip=impl_->clipping(impl_->buttonRegion(*b),impl_->buttonHUDGroup(*b))){const float right=std::min(result.x+result.width,clip->x+clip->width),top=std::min(result.y+result.height,clip->y+clip->height);result.x=std::max(result.x,clip->x);result.y=std::max(result.y,clip->y);result.width=std::max(0.f,right-result.x);result.height=std::max(0.f,top-result.y);}return result;}
std::array<float,2> Screens::menuScrollOffsets()const{return impl_?std::array<float,2>{impl_->verticalPan,impl_->shopPan}:std::array<float,2>{};}
void Screens::setHorizontalPan(float value){if(!impl_||!std::isfinite(value))throw std::invalid_argument("Invalid presentation pan");impl_->shopPan=impl_->phoneMenu()?0.f:std::clamp(value,0.f,impl_->layout.shopPanLimit());}
float Screens::horizontalPanLimit()const{return impl_&&!impl_->phoneMenu()?impl_->layout.shopPanLimit():0;}

std::vector<ButtonKey> Screens::buttons()const{return impl_?impl_->keys:std::vector<ButtonKey>{};}
std::shared_ptr<Button> Screens::button(ButtonKey key)const{return impl_?impl_->dispatcher.find(key):nullptr;}
std::optional<std::string> Screens::itemDescription(const ItemDescriptionValues& v,int mode){
    auto integer=[](float f)->std::optional<int>{
        if(!std::isfinite(f)||double(f)<std::numeric_limits<int>::min()||double(f)>std::numeric_limits<int>::max())return {};
        return int(f);
    };
    const auto p1=integer(v.param1),p2=integer(v.param2),p3=integer(v.param3);
    if(!p1||!p2||!p3)return {};
    auto difference=[](int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)-std::uint32_t(b));};
    auto format=[](const char* pattern,auto...args){
        std::array<char,1024> bytes{};const int n=std::snprintf(bytes.data(),bytes.size(),pattern,args...);
        if(n<0||n>=int(bytes.size()))throw std::runtime_error("Original item description exceeds its source buffer");
        return std::string(bytes.data(),std::size_t(n));
    };
    std::string out;
    switch(v.type){
        case 1:
            out="Repair Kit\n\n";
            if(v.param2==0)out+=format("%d point Repair.\n\n",*p1);
            else if(v.param2==1)out+=format("%d%% Repair.\n\n",*p1);
            else return {};
            break;
        case 2:
            out="Special Attack\n\n";
            if(v.weapon){out+=format("Attack Point : %d\n",v.weapon->attackPoint);out+=format("Damage Range : %d\n\n",int(v.weapon->damageRange));}
            break;
        case 3:out="Special Item\n\n";break;
        case 4:
            if(!v.machineResolved)out="BOT\n\n";
            else{
                switch(v.botRefType){case 0:out="Attacker BOT\n\n";break;case 1:out="Defender BOT\n\n";break;
                    case 2:out="Item Picker BOT\n\n";break;case 3:out="Repair BOT\n\n";break;default:return {};}
                out+=format("HP : %d\n\n",v.botHP);
            }break;
        case 6:out="Ticket\n\n";break;
        case 10:
            if(!v.weapon)return {};
            switch(v.weapon->kind){
                case WeaponKind::gun:out="Machine-gun\n\n";break;case WeaponKind::shotgun:out="Shotgun\n\n";break;
                case WeaponKind::laser:out="Laser-gun\n\n";break;case WeaponKind::railgun:out="Railgun\n\n";break;
                case WeaponKind::missile:out="Missile Launcher\n\n";break;default:return {};
            }
            out+=format("Atk %d (L:%+d R:%+d)\n",v.weapon->attackPoint,difference(v.weapon->attackPoint,v.leftAttack),difference(v.weapon->attackPoint,v.rightAttack));
            out+=format("%d / %d / %.2fs / %.1fs\n\n",int(v.weapon->damage),unsigned(v.weapon->shotCount),double(v.weapon->shotDelay),double(v.weapon->reloadDelay));break;
        case 11:{
            auto hp=integer(v.param1-float(v.bodyHP)),def=integer(v.param2-float(v.bodyDefense));if(!hp||!def)return {};
            out="Body Parts\n\n"+format("HP : %d (%+d)\nDef : %d (%+d)\n\n",*p1,*hp,*p2,*def);break;}
        case 12:{
            auto speed=integer(v.param3-float(v.footSpeed)),boost=integer(v.param1-v.boosterMaximum);if(!speed||!boost)return {};
            out="Foot Parts\n\n"+format("Speed : %d (%+d)\nBoost : %d (%+d)\n\n",*p3,*speed,*p1,*boost);break;}
        case 13:{auto delta=integer(v.param1-float(v.armorDefense));if(!delta)return {};
            out="Armor Parts\n\n"+format("Def : %d (%+d)\n\n",*p1,*delta);break;}
        case 14:out="Equip Parts\n\n";break;
        default:break;
    }
    if(mode==1)out+=format("Price : %dcr",v.price);
    else if(mode==2)out+=format("SellPrice : %dcr",v.type==5?v.price:v.price/2);
    return out;
}
std::optional<std::string> Screens::missionDescription(std::string comment,const std::vector<MissionRequirementText>& requirements,bool accepted){
    if(requirements.empty())return comment;
    comment+="\n\nMission Objects\n";
    for(const auto& r:requirements){
        if(r.type!=2&&r.type!=3)return {};
        comment+=r.type==2?"  - destroy ":"  - ";comment+=r.name;
        if(accepted)comment+=" ("+std::to_string(r.did)+"/"+std::to_string(r.total)+")\n";
        else comment+=" x"+std::to_string(r.total)+"\n";
    }
    if(comment.size()>=1024)return {};
    return comment;
}
std::vector<ScreenCoverage> Screens::coverage(){
    return {
        {ScreenId::title,Panel::root,"Title images, intro timing, original two-step tap","GuiTitle0x32340/0x327f4/0x3220c",true},
        {ScreenId::slots,Panel::root,"Three slots1009, OK1001, Back1004, Delete1010, names/font/date/credits","GuiSelectSlot0x2e1a4/0x2e6f8",true},
        {ScreenId::slots,Panel::root,"Selected slot mech movement with equipped parts","GuiSelectSlot0x2e6f8/0x2dfe0/0x2e0a0, GobHvM_Player0xc028",true},
        {ScreenId::base,Panel::root,"Base6 tables and27 table buttons, original bounds and IDs","GuiBase0x246d4/Base001..006.tble",true},
        {ScreenId::base,Panel::root,"Selection description slide and mission notices","GuiBase0x243b8/0x246d4/0x251d0",true},
        {ScreenId::field,Panel::root,"HUD gauges, menu1021, weapon selectors1022/1023, inv1025/mission1026","GuiGame0x29c9c/0x291d4/0x29100/0x29054",true},
        {ScreenId::field,Panel::weaponChoices,"Inventory weapon chooser1024, raw index, originalAffecter targets and labels","GuiGame0x2aad8/QobAffecter0x36698",true},
        {ScreenId::field,Panel::root,"Path move button7000 creation","GuiGame.handleButton; constructor unresolved",false},
        {ScreenId::field,Panel::system,"Airlift1027, SaveExit1031, More1032, Help1029, Resume1030","DlgSystemMenu0x23fb0",true},
        {ScreenId::field,Panel::airlift,"Visited-base rows4002..4007, OK4001, close4000","DlgAirlift0x1fbb0/0x1f5b8",true},
        {ScreenId::field,Panel::mission,"Accepted rows5200..5204, close5000, original text formatter","DlgMission0x22b1c/0x23678/0x23208",true},
        {ScreenId::command,Panel::mission,"Available5100..5104,accepted5200..5204,accept5004,ranking5009,exit2100","GuiCommandCenter0x2614c/0x2708c/0x26af8",true},
        {ScreenId::command,Panel::mission,"Completed result5007/reward overlay","GuiCommandCenter0x279a8/0x269ac",true},
        {ScreenId::command,Panel::mission,"Mission detail expand animation","GuiCommandCenter0x26558/0x26af8",true},
        {ScreenId::maintenance,Panel::inventory,"25 cells6000..6024,pages6090/6091,arrange6093","WndInven0x34918/GuiMaintenance0x2bbb0",true},
        {ScreenId::maintenance,Panel::equipment,"Six equipment targets6100..6105 and actual item identities","WndEquip0x33c5c",true},
        {ScreenId::maintenance,Panel::bot,"Three bot targets6300..6302,disarm6310","WndBotSlot0x33300/GuiMaintenance.handleButton",true},
        {ScreenId::field,Panel::quick,"Five quick targets6200..6204,countable and equipped glyph state","WndQuickSlot0x35554",true},
        {ScreenId::field,Panel::inventory,"Inventory/equipment/BOT window6092,6093,6090/6091","DlgInven0x20b0c",true},
        {ScreenId::shop,Panel::root,"Five categories2104,24 items2200..2223,pages2105/2106,BUY2102/SELL2103/EXIT2100","GuiShop0x30e4c/0x30260",true},
        {ScreenId::shop,Panel::trade,"Trade2110/2111,2107,2108/2109,actual qty/credit rendering","GuiShop0x31b20",true},
        {ScreenId::shop,Panel::root,"Item detail2112, original preview and expansion animation","GuiShop0x31554/0x30888",false},
        {ScreenId::field,Panel::root,"Player, BOT and open gate radar markers","DlgRader0x23c7c/0x23d28",true},
        {ScreenId::field,Panel::root,"Stage-entry place-name banner","GuiGame0x2a820/0x29508, GobWorld0x13778",true},
        {ScreenId::field,Panel::root,"Enemy names,message lifetime,help pages/game-over native alerts","GuiGame/GuiHelp/GameMain",true}
    };
}
}

namespace hm2::ui {
void Screens::revealButton(ButtonKey key){
    if(!impl_||!display_.enabled||impl_->state.tradeVisible)return;
    if(impl_->phoneMenu()){
        const bool stock=key.id>=2200&&key.id<2224,inventory=key.id>=6000&&key.id<6025;
        if(stock||inventory){const unsigned desired=(key.id-(stock?2200:6000))/(stock?impl_->stockCells():impl_->inventoryCells());
            for(unsigned attempt=0;attempt<5;++attempt){const unsigned current=stock?impl_->stockWindow:impl_->inventoryWindow;if(current==desired)break;
                const unsigned page=stock?(current<desired?2106:2105):(current<desired?6091:6090);
                for(const auto target:buttons())if(target.id==page){const auto p=buttonScreenPosition(target);const auto time=impl_->now;input(PointerPhase::begin,p,0x50414745,time);input(PointerPhase::end,p,0x50414745,time);break;}
            }
        }
    }
    auto b=button(key);if(!b)return;auto region=impl_->buttonRegion(*b);
    if(impl_->phoneMenu()&&impl_->state.screen==ScreenId::shop){const bool equipment=region==Region::phoneEquipment,stock=region==Region::phoneStock||region==Region::phoneStockHeader;
        if((equipment&&!impl_->phoneEquipment)||(stock&&impl_->phoneEquipment)){const unsigned toggle=equipment?9204:9205;for(auto target:buttons())if(target.id==toggle){revealButton(target);const auto p=buttonScreenPosition(target);const auto time=impl_->now;input(PointerPhase::begin,p,0x50484d44,time);input(PointerPhase::end,p,0x50484d44,time);break;}b=button(key);if(!b)return;region=impl_->buttonRegion(*b);}
    }
    const auto a=display_.content();auto p=buttonScreenPosition(key);
    if(region==Region::shopStock||region==Region::shopInventory)impl_->shopPan=std::clamp(impl_->shopPan+p.x-std::clamp(p.x,a.x+16,a.x+a.width-16),0.f,impl_->layout.shopPanLimit());
    if(region==Region::list||region==Region::chooser){const auto clip=impl_->clipping(region).value_or(a);const float delta=std::clamp(p.y,clip.y+24,clip.y+clip.height-24)-p.y;impl_->verticalPan=std::clamp(impl_->verticalPan+(region==Region::chooser?-delta:delta),0.f,impl_->verticalLimit());}
}
}
