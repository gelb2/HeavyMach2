#include "Core/UI/Screens.hpp"
#include "Core/Data/DataTable.hpp"
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
struct ImageNode {VisualImage image;unsigned layer=7;std::string tag;std::shared_ptr<Button> parent{};Vec2 local{};};
struct ButtonNode {std::shared_ptr<Button> button;unsigned layer=7;};
struct TextNode {std::shared_ptr<Text> text;unsigned layer=7;std::shared_ptr<Button> parent{};Vec2 local{};};
struct NumberNode {std::shared_ptr<ImageFont> font;TileImage* tile;unsigned layer=7;std::string tag;};
struct PreviewNode {Frame frame;unsigned layer=7;};
enum class DetailKind {shop,inventory,maintenance};
struct DetailPresentation {
    DetailKind kind=DetailKind::shop;std::uint64_t revision=0;int absolute=-1;
    ItemView entry;Vec2 origin,descPos;float x=.01f,y=.01f;bool vertical=false,up=false;
    unsigned layer=9;std::shared_ptr<Button> background;
    std::shared_ptr<Text> title,description;std::optional<VisualImage> large;
};
struct DetailNode {std::shared_ptr<DetailPresentation> detail;unsigned layer=9;};
struct FieldPresentation {
    std::uint64_t identity=0,enemyRevision=0;bool range[2]={false,false};
    std::shared_ptr<Text> enemyName;
};
using Node=std::variant<ImageNode,ButtonNode,TextNode,NumberNode,PreviewNode,DetailNode>;
struct MessagePresentation {
    VisualImage background;
    std::shared_ptr<Text> text;
    std::uint64_t revision=0;
    float start=0;
    bool initialized=false,hiding=false,autoHide=false;
};
struct TitlePresentation {
    float start=0,bgScale=1.8f,bgRotate=-.05f,titleScale=50,episodeScale=40,bigX=1024;
    Vec2 bgPosition{100,-10};
    bool bigVisible=true,titleVisible=false,episodeVisible=false,startVisible=false;
    bool titleSound=false,episodeSound=false,startSound=false;
};
}
struct Screens::Impl {
    ResourceAccess& resources;PreparedTiles& tiles;ScreensHost& host;ScreensState state;
    std::vector<Node> nodes;
    ButtonDispatcher dispatcher;
    std::vector<ButtonKey> keys;
    std::map<Owner,Panel> panels;
    std::shared_ptr<std::array<VisualLayer,14>> layers;
    std::shared_ptr<Impl> previous;
    TitlePresentation title;
    MessagePresentation message;
    std::vector<std::pair<std::shared_ptr<Button>,Vec2>> chooserMoves;
    float now=0;
    std::shared_ptr<DetailPresentation> detail;FieldPresentation fieldPresentation;
    Impl(ResourceAccess& r,PreparedTiles& t,ScreensHost& h,const ScreensState& s,float time,std::shared_ptr<Impl> old)
        :resources(r),tiles(t),host(h),state(s),previous(std::move(old)),now(time){
        if(previous&&previous->state.screen==s.screen){title=previous->title;message=previous->message;layers=previous->layers;}
        else{title.start=time;layers=std::make_shared<std::array<VisualLayer,14>>();}
        if(previous)fieldPresentation=previous->fieldPresentation;
        // accept5004 only invalidates Command.curState. Keep its existing rows
        // until the following Command tick actually refreshes the list.
        if(previous&&s.screen==ScreenId::command&&previous->state.screen==s.screen&&s.commandListRevision&&s.commandListRevision==previous->state.commandListRevision){
            state.availableMissions=previous->state.availableMissions;state.acceptedMissions=previous->state.acceptedMissions;
            state.selectedMission=-1;
            const auto select=[&](const std::vector<ItemView>& list,int first){for(std::size_t i=0;i<list.size();++i)if(!s.selectedMissionDataId.empty()&&list[i].dataId==s.selectedMissionDataId&&state.selectedMission<0)state.selectedMission=first+int(i);};
            select(state.availableMissions,0);select(state.acceptedMissions,int(state.availableMissions.size()));
        }
        if(fieldPresentation.identity!=s.fieldUIRevision){fieldPresentation={};fieldPresentation.identity=s.fieldUIRevision;}
    }
    void hideMessage(){message.hiding=true;message.text.reset();}
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
        m.text.reset();TextRasterRequest request{"GuiGame.message",state.message,"TrebuchetMS-Bold",{512,24},16,TextAlign::center,false,0};
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
        auto* t=tiles.prepare(resources,name);if(t){t->columns=columns;t->rows=rows;}return t;
    }
    void image(std::string_view name,Vec2 position,unsigned columns=1,unsigned rows=1,unsigned index=0,
               Vec2 scale={1,1},bool atlas=false,unsigned blend=0,float alpha=1,unsigned layer=7,std::string tag={}){
        auto* t=tile(name,columns,rows);if(!t)return;
        VisualImage visual;visual.tile=tiles.visual(*t);visual.tileNo=index;visual.screenPosition=position;visual.scale=scale;
        visual.useAtlas=atlas;visual.blendType=blend;visual.alpha=alpha;
        nodes.push_back(ImageNode{std::move(visual),layer,std::move(tag)});
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
        button->setDefaultTile(normal);button->setReleaseTile(pressed);button->setDeactiveTile(normal);
        button->position=position;button->scale=scaleBeforeBounds?scale:Vec2{1,1};button->visible=true;button->active=enabled;
        if(bounds)button->setBoundWidthHeight(bounds->width,bounds->height);
        button->scale=scale;dispatcher.add(button);keys.push_back(key);panels[key.owner]=panel;
        nodes.push_back(ButtonNode{button,layer});return button;
    }
    void text(std::string id,std::string value,Vec2 position,Size size,const char* font,float fontSize,TextAlign align,
              Color color={1,1,1,1},unsigned layer=9,Vec2 scale={1,1}){
        if(value.empty())return;
        TextRasterRequest request{std::move(id),std::move(value),font,size,fontSize,align,false,0};
        auto prepared=host.prepareText(request);
        if(!prepared.bitmap||!(prepared.bitmap->request==request))throw std::runtime_error("UI text preparation changed the original request");
        auto label=std::make_shared<Text>(std::move(prepared.image),std::move(prepared.bitmap));label->position=position;label->scale=scale;
        label->setColor(std::uint8_t(color.r*255),std::uint8_t(color.g*255),std::uint8_t(color.b*255));label->alpha=color.a;
        nodes.push_back(TextNode{std::move(label),layer});
    }
    void number(std::string_view name,int value,Vec2 position,float pitch,float align,unsigned layer=9,bool star=false,std::string tag={},unsigned columns=16){
        auto* t=tile(name,columns,1);if(!t)return;
        NumericAtlas a{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalAtlasBlend(0,t->pixelFormat)};
        auto font=std::make_shared<ImageFont>(std::move(a));font->position=position;if(pitch!=0)font->setPitch(pitch);font->setAlignRate(align);
        if(star)font->setText("*");else font->setNumber(value);font->tick();
        nodes.push_back(NumberNode{std::move(font),t,layer,std::move(tag)});
    }
    void numericText(std::string_view name,std::string value,Vec2 at,float pitch,float align){
        auto* t=tile(name,16,1);if(!t)return;
        auto font=std::make_shared<ImageFont>(NumericAtlas{t->name,t->pixels,t->columns,t->rows,t->forRetina,originalAtlasBlend(0,t->pixelFormat)});
        font->position=at;font->setPitch(pitch);font->setAlignRate(align);font->setText(value);font->tick();
        nodes.push_back(NumberNode{std::move(font),t,9,{}});
    }
    void preview(const Frame& frame,Vec2 origin,unsigned layer=7){
        if(frame.sprites.empty())return;
        // QobBase child world position is local+parent. Freeze that translation
        // once at preparation, including absolute atlas vertices. The source
        // frame may be shared by several slots and remains immutable.
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
        nodes.push_back(PreviewNode{std::move(prepared),layer});
    }
    void icon(const ItemView& item,Vec2 position,unsigned layer=9){
        if(!item.icon.empty())image(item.icon,position,item.iconColumns,item.iconRows,item.iconTile,{1,1},false,0,1,layer);
    }
    void slots(){
        const auto firstNode=nodes.size();
        image("SelSlot_BG.png",{384,512});
        addButton(Panel::root,1010,"Common_Btn_DEL.png",{224,80},1,2,0,1,Size{80,40});
        addButton(Panel::root,1004,"Common_Btn_BACK.png",{384,80},1,2,0,1,Size{80,40});
        addButton(Panel::root,1001,"Common_Btn_OK.png",{544,80},1,2,0,1,Size{80,40});
        for(unsigned i=0;i<3;++i){
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
            preview(slot.machPreview,plus(origin,{-170,34}));
        }
        // GameMain layer3 is inherited through GuiSelectSlot/slotBase.
        // Explicit part child layers (notably shadow0) remain on each Sprite.
        for(std::size_t i=firstNode;i<nodes.size();++i)std::visit([](auto& node){node.layer=3;},nodes[i]);
    }
    void base(){
        const auto name=state.baseId+".tble";
        const auto table=DataTable::parse(resources.read(name));const Vec2 root{384,512};
        for(std::size_t i=0;i<table.rowCount();++i){
            const auto kind=table.value(i,"Type").value_or("");const auto asset=table.value(i,"ImageFile").value_or("");
            const Vec2 at=plus(root,{table.real(i,"X"),table.real(i,"Y")});const float scale=table.real(i,"Scale");const int id=table.integer(i,"ID");
            if(kind=="Image")image(asset,at,1,1,0,{scale,scale},false,id>10?2:0,id>10?0.f:1.f,7,id>10?"baseSelect"+std::to_string(id-10):"");
            else if(kind=="Button"){
                std::optional<Size> bounds;if(id<4)bounds=Size{160,100};else if(id!=4)bounds=Size{160,60};
                addButton(Panel::root,unsigned(2000+id),asset,at,1,2,state.selectedBaseButton==id?1:0,1,bounds,0,{}, {scale,scale});
            }
            // Desc supplies the selection animation anchor. Ads are excluded
            // by the product contract and never create a network/UI consumer.
        }
    }
    void titleScreen(){
        const Vec2 root{384,512};
        image("Main_BG.png",plus(root,title.bgPosition),1,1,0,{title.bgScale,title.bgScale},false,0,1,7,"title.bg");
        image("Main_Copyright.png",plus(root,{0,-500}));
        image("Main_TitleBig.png",plus(root,{title.bigX,0}),1,1,0,{1,1},false,0,1,7,"title.big");
        image("Main_Title.png",plus(root,{0,-380}),1,1,0,{title.titleScale,title.titleScale},false,0,1,7,"title.main");
        image("Main_Episode.png",plus(root,{114,-310}),1,1,0,{title.episodeScale,title.episodeScale},false,0,1,7,"title.episode");
        image("Main_TapToStart.png",plus(root,{0,-230}),1,1,0,{1,1},false,0,1,7,"title.start");
        updateTitleImages();
    }
    void updateTitleImages(){
        for(auto& node:nodes)if(auto* n=std::get_if<ImageNode>(&node)){
            auto& v=n->image;
            if(n->tag=="title.bg"){v.screenPosition=plus({384,512},title.bgPosition);v.scale={title.bgScale,title.bgScale};v.angle=title.bgRotate;}
            else if(n->tag=="title.big"){v.screenPosition.x=384+title.bigX;v.show=title.bigVisible;}
            else if(n->tag=="title.main"){v.scale={title.titleScale,title.titleScale};v.show=title.titleVisible;}
            else if(n->tag=="title.episode"){v.scale={title.episodeScale,title.episodeScale};v.show=title.episodeVisible;}
            else if(n->tag=="title.start")v.show=title.startVisible;
        }
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
        const Vec2 hud{384,128};image("GameUI_BG.png",hud,1,1,0,{1,1},false,0,1,9);
        image("GameUI_GaugeBack.png",plus(hud,{float(state.hpBarWidth)-398,-80}),1,4,0,{1,1},true,0,1,9,"hud.hpBack");
        image("GameUI_GaugeBack.png",plus(hud,{float(state.hpBarFilled)-398,-80}),1,4,1,{1,1},true,0,1,9,"hud.hp");
        image("GameUI_GaugeBack.png",plus(hud,{float(state.spBarWidth)-398,-54}),1,4,0,{1,1},true,0,1,9,"hud.spBack");
        image("GameUI_GaugeBack.png",plus(hud,{state.spBarFilled-398,-54}),1,4,state.boosterReady?3:2,{1,1},true,0,1,9,"hud.sp");
        image("GameUI_GaugeEXP.png",plus(hud,{115*state.experienceRate-343,-113}),1,1,0,{state.experienceRate,1},false,0,1,9,"hud.exp");
        image("EnemyGauge.png",{384,956},1,2,1,{1,1},false,0,1,9,"hud.enemyBack");
        image("EnemyGauge.png",{384-128*(1-state.enemyHpRate),957},1,2,0,{state.enemyHpRate,1},false,0,1,9,"hud.enemy");
        number("NumTile05.png",std::bit_cast<std::int32_t>(std::uint32_t(state.level)+1),plus(hud,{-364,-106}),9,.5f,9,false,"hud.level");
        number("NumSet01b.png",state.credits,plus(hud,{-122,-37}),6,1,9,false,"hud.credits");
        addButton(Panel::root,1021,"QuickBtn.png",plus(hud,{237,-36}),2,4,4,5,Size{60,60});
        addButton(Panel::root,1026,"QuickBtn.png",plus(hud,{296,-36}),2,4,0,1,Size{60,60});
        addButton(Panel::root,1025,"QuickBtn.png",plus(hud,{355,-36}),2,4,2,3,Size{60,60});
        quickSlots({511,29});
        updateHUD(state);
        if(state.weaponChoiceType!=0)weaponChoices();
    }
    void updateHUD(const ScreensState&);
    void build();
    void buildDetail(DetailKind,const ItemView&,Vec2,int=-1);
    void layoutDetail();
    void tickDetail();
    void syncEnemyName();
    void tickRange(WorldRandom&);
    void inventory(Vec2,bool=false);
    void equip(Vec2,unsigned=9);
    void bots(Vec2);
    void inventoryDialog(Vec2,bool=false);
    void maintenance();
    void shop();
    void missionList(bool);
    void completedMission();
    void system();
    void airlift();
    void trade();
    void weaponChoices();
    void tick(float time,WorldRandom& random);
    void append(Frame& out,float time);
};

void Screens::Impl::buildDetail(DetailKind kind,const ItemView& entry,Vec2 origin,int absolute){
    const bool reuse=previous&&previous->detail&&previous->detail->kind==kind&&previous->detail->revision==state.detailRevision&&previous->detail->absolute==absolute&&previous->detail->entry.dataId==entry.dataId;
    detail=reuse?std::make_shared<DetailPresentation>(*previous->detail):std::make_shared<DetailPresentation>();
    auto& d=*detail;d.kind=kind;d.revision=state.detailRevision;d.absolute=absolute;d.origin=origin;
    if(!reuse){d.entry=entry;
        if(kind==DetailKind::shop)d.descPos={184,340};
        else if(kind==DetailKind::inventory){const int cell=absolute%25;d.descPos={float((cell%5)*60),float(96-(cell/5)*60)};}
        else{const int cell=absolute%25;d.up=cell>9;d.descPos={108,float(61-(cell/5)*40+(d.up?20:-21))};}
    }
    d.layer=kind==DetailKind::inventory?13u:kind==DetailKind::shop?3u:9u;
    if(kind!=DetailKind::shop){
        d.background=addButton(kind==DetailKind::maintenance?Panel::root:Panel::inventory,2112,"ItemDesc.png",plus(origin,d.descPos),1,1,0,0,Size{206,128},0,{}, {d.x,d.y},true,d.layer,false,reuse);
    }
    if(!reuse){
        const bool maintenance=kind==DetailKind::maintenance;
        const char* font=maintenance?"Arial":"TrebuchetMS-Bold";
        const std::string source=kind==DetailKind::shop?"GuiShop.item":maintenance?"GuiMaintenance.item":"DlgInven.item";
        auto label=[&](std::string id,const std::string& value,Size size,float fontSize,TextAlign align,Color color){
            const auto count=nodes.size();text(std::move(id),value,{},size,font,fontSize,align,color,d.layer);
            if(nodes.size()==count)return std::shared_ptr<Text>{};auto result=std::get<TextNode>(nodes.back()).text;nodes.pop_back();return result;
        };
        d.title=label(source+".title",entry.name,maintenance?Size{128,16}:Size{160,24},maintenance?12.f:16.f,TextAlign::center,{});
        d.description=label(source+".description",entry.description,{160,90},12,TextAlign::left,kind==DetailKind::shop?Color{120.f/255,240.f/255,240.f/255,1}:Color{});
        if(entry.originalType==10||entry.originalType==11||entry.originalType==12)if(auto* t=tile(entry.largeIcon)){
            VisualImage image;image.tile=tiles.visual(*t);image.tileNo=0;image.useAtlas=false;d.large=std::move(image);
        }
    }
    nodes.push_back(DetailNode{detail,d.layer});layoutDetail();
}
void Screens::Impl::layoutDetail(){
    if(!detail)return;auto& d=*detail;const bool shop=d.kind==DetailKind::shop,maintenance=d.kind==DetailKind::maintenance;
    float parentY=d.descPos.y;if(d.vertical)parentY+=(shop?-62.f:maintenance?(d.up?62.f:-62.f):-96.f)*d.y;
    const Vec2 parent=plus(d.origin,{d.descPos.x,parentY});
    if(d.background){d.background->position=parent;d.background->scale={d.x,d.y};}
    const bool hasLarge=d.large.has_value();const bool weapon=d.entry.originalType==10;
    if(d.title){d.title->position=plus(parent,{0,d.vertical?(shop?40.f:maintenance?47.f:62.f)*d.y:0});d.title->scale={d.x,d.y};d.title->visible=d.vertical;}
    if(d.description){d.description->position=plus(parent,{hasLarge?(weapon?32.f:76.f):0,d.vertical?(shop?-36.f:-12.f)*d.y:0});d.description->scale={d.x,d.y};d.description->visible=d.vertical;}
    if(d.large){d.large->screenPosition=plus(parent,{weapon?-76.f:-52.f,d.vertical?(shop?-36.f:-10.f)*d.y:0});d.large->scale={1,d.vertical?d.y:1};d.large->show=d.vertical;}
}
void Screens::Impl::tickDetail(){
    if(!detail)return;auto& d=*detail;
    if(d.x<1){d.x+=(1-d.x)*.5f;if(d.x>.99f)d.x=1;}
    else if(d.y<1){d.y+=(1-d.y)*.5f;if(d.y>.99f)d.y=1;d.vertical=true;}
    layoutDetail();
}
void Screens::Impl::syncEnemyName(){
    if(state.screen!=ScreenId::field)return;
    if(fieldPresentation.enemyRevision!=state.enemyNameRevision||(!fieldPresentation.enemyName&&!state.enemyName.empty())){
        const auto old=fieldPresentation.enemyName;std::erase_if(nodes,[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==old;});
        fieldPresentation.enemyName.reset();fieldPresentation.enemyRevision=state.enemyNameRevision;
        if(!state.enemyName.empty()){const auto count=nodes.size();text("GuiGame.enemyName",state.enemyName,{384,956},{256,18},"Arial-BoldMT",12,TextAlign::center,{},9);if(nodes.size()!=count)fieldPresentation.enemyName=std::get<TextNode>(nodes.back()).text;}
    }else if(fieldPresentation.enemyName&&std::none_of(nodes.begin(),nodes.end(),[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==fieldPresentation.enemyName;}))nodes.push_back(TextNode{fieldPresentation.enemyName,9});
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

void Screens::Impl::inventory(Vec2 origin,bool shopMode){
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
}
void Screens::Impl::equip(Vec2 origin,unsigned inheritedLayer){
    constexpr Vec2 positions[]={{0,0},{0,-74},{-56,-9},{56,-9},{56,-74},{-56,-74}};
    constexpr const char* icons[]={"EquipBtn_Body.png","EquipBtn_Btm.png","EquipBtn_Wpn.png","EquipBtn_Wpn.png","EquipBtn_Armor.png","EquipBtn_Sp.png"};
    for(unsigned i=0;i<6;++i)if(!state.equipment[i].dataId.empty()&&state.equipment[i].count>0){
        const auto at=plus(origin,positions[i]);image("ItemBox.png",at,1,1,0,{1,1},false,0,1,inheritedLayer);icon(state.equipment[i],at,inheritedLayer);}
    for(unsigned i=0;i<6;++i){const auto& entry=state.equipment[i];const auto at=plus(origin,positions[i]);
        addButton(Panel::equipment,6100+i,icons[i],at,1,1,0,0,Size{36,36},0,entry.dataId,{1,1},true,inheritedLayer);
        if(state.selectedEquipment==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extEquipment==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }
    const auto& stats=state.equipmentStats;
    numericText("NumSet01b.png",std::to_string(stats.attackLeft)+"-"+std::to_string(stats.attackRight),plus(origin,{-20,-126}),6,0);
    const auto defense=std::bit_cast<std::int32_t>(std::uint32_t(stats.bodyDefense)+std::uint32_t(stats.armorDefense));
    numericText("NumSet01b.png",std::to_string(stats.baseHP)+"-"+std::to_string(stats.bonusHP)+"-"+std::to_string(defense),plus(origin,{-20,-146}),6,0);
    numericText("NumSet01b.png",std::to_string(stats.stepSize)+"-"+std::to_string(int(stats.boosterMaximum))+"-"+std::to_string(int(stats.boosterRegen*10.f)),plus(origin,{-20,-166}),6,0);
}
void Screens::Impl::bots(Vec2 origin){
    for(unsigned i=0;i<3;++i)if(!state.bots[i].dataId.empty()){
        const auto at=plus(origin,{0,-60.f*float(i)});image("ItemBox.png",at,1,1,0,{1,1},false,0,1,9);icon(state.bots[i],at);}
    for(unsigned i=0;i<3;++i){const auto& entry=state.bots[i];const auto at=plus(origin,{0,-60.f*float(i)});
        addButton(Panel::bot,6300+i,"Inven_BtnItem.png",at,1,1,0,0,Size{60,60},0,entry.dataId);
        if(state.selectedBot==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.8f,9);
        if(state.extBot==int(i))image("Inven_Sel.png",at,1,1,0,{1,1},false,2,.5f,9);
    }
}
void Screens::Impl::inventoryDialog(Vec2 origin,bool shopMode){
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
void Screens::Impl::maintenance(){
    const Vec2 root{384,512};image("MaintenanceBG.png",root,1,1,0,{1,1},false,0,1,3);
    inventory(plus(root,{27,63}));equip(plus(root,{-148,52}),3);bots(plus(root,{-47,52}));
    number("NumTile04.png",state.credits,plus(root,{38,-126}),0,0,3);
    addButton(Panel::root,6093,"Btn_ArrangeItem.png",plus(root,{170,94}),1,1,0,0,{},0,{}, {1,1},true,3);
    if(state.selectedBot>=0)addButton(Panel::root,6310,"Btn_Disarm.png",plus(root,{-46,-83}),2,1,0,1,{},0,{}, {1,1},true,3);
    addButton(Panel::root,2100,"Btn_CmdCenterExit.png",plus(root,{186,-140}),2,1,0,1,{},0,{}, {1,1},true,3);
    if(state.detailInventory>=0&&!state.detailItem.dataId.empty())buildDetail(DetailKind::maintenance,state.detailItem,root,state.detailInventory);
}
void Screens::Impl::shop(){
    const Vec2 root{384,512};image("ShopBG.png",root,1,1,0,{1,1},false,0,1,3);
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
    const Vec2 root{384,512};image("MissionList_Mid.png",root,1,1,0,{1,3.375f},false,0,1,10);
    image("MissionList_Btm.png",plus(root,{0,-140}),1,1,0,{1,1},false,0,1,10);
    image("SysMenuBG.png",plus(root,{0,140}),1,1,0,{1,1},false,0,1,10);
    constexpr unsigned ids[]={1027,1031,1032,1029};constexpr const char* labels[]={"Airlift to base","Save & Exit game","More games","Help"};
    for(unsigned i=0;i<4;++i){const Vec2 at=plus(root,{0,140-64-44.f*float(i)});
        addButton(Panel::system,ids[i],"Btn_Mission.png",at,1,2,0,1,{},0,{}, {1,1},true,10);
        text("DlgSystemMenu."+std::to_string(ids[i]),labels[i],at,{360,24},"TrebuchetMS-Bold",20,TextAlign::center,{140.f/255,240.f/255,1,1},10);
    }
    addButton(Panel::system,1030,"Common_Btn_CLOSE.png",plus(root,{136,-124}),1,2,0,1,{},0,{}, {1,1},true,10);
}
void Screens::Impl::trade(){
    const Vec2 root{384,512};
    addButton(Panel::trade,0,"TradeItemBG.png",root,1,1,0,0,Size{768,1024},0,state.tradeItemId,{1,1},true,13);
    const auto found=std::find_if(state.shopItems.begin(),state.shopItems.end(),[&](const auto& i){return i.dataId==state.tradeItemId;});
    const ItemView* entry=found==state.shopItems.end()?nullptr:&*found;
    if(!entry){const auto f=std::find_if(state.inventory.begin(),state.inventory.end(),[&](const auto& i){return i.dataId==state.tradeItemId;});if(f!=state.inventory.end())entry=&*f;}
    if(entry){icon(*entry,plus(root,{-98,66}),13);text("GuiShop.trade.name",entry->name,plus(root,{10,68}),{160,24},"TrebuchetMS-Bold",18,TextAlign::left,{},13);}
    number("NumTile01.png",state.tradeCount,plus(root,{-58,18}),0,.5f,13,false,{},10);
    addButton(Panel::trade,2110,"Common_Btn_DEC.png",plus(root,{8,19}),1,2,0,1,Size{51,40},0,state.tradeItemId,{1,1},true,13);
    addButton(Panel::trade,2111,"Common_Btn_INC.png",plus(root,{59,19}),1,2,0,1,Size{51,40},0,state.tradeItemId,{1,1},true,13);
    number("NumSet_Cr.png",state.tradePrice,plus(root,{52,-23}),12,1,13);
    addButton(Panel::trade,2107,"Common_Btn_CANCEL.png",plus(root,{-63,-62}),1,2,0,1,Size{100,40},0,state.tradeItemId,{1,1},true,13);
    addButton(Panel::trade,state.buyMode?2108:2109,state.buyMode?"Common_Btn_BUY.png":"Common_Btn_SELL.png",plus(root,{63,-62}),1,2,0,1,Size{100,40},0,state.tradeItemId,{1,1},true,13);
}
void Screens::Impl::missionList(bool command){
    const Vec2 root{384,512};
    const auto available=command?std::min<std::size_t>(5,state.availableMissions.size()):0;
    const auto accepted=std::min<std::size_t>(5,state.acceptedMissions.size());
    const float header=command&&accepted?64.f:0.f;
    const float detail=state.selectedMission>=0?256.f:0.f;
    const float length=float(available+accepted)*44+header+(command?0.f:40.f);
    const float half=(length+detail)*.5f;
    image("MissionList_Mid.png",root,1,1,0,{1,half/32},false,0,1,10);
    image("MissionList_Btm.png",plus(root,{0,-half-32}),1,1,0,{1,1},false,0,1,10);
    image(command?"NewMissionList_Top.png":"MissionList_Top.png",plus(root,{0,half+32}),1,1,0,{1,1},false,0,1,10);
    const Vec2 list=plus(root,{0,half-32});
    auto row=[&](const ItemView& entry,unsigned id,unsigned index,float y,bool isAccepted){
        if(state.selectedMission>=0&&int(index)>state.selectedMission)y-=256;
        const auto at=plus(list,{0,y});
        addButton(Panel::mission,id,"Btn_Mission.png",at,1,2,0,1,isAccepted?std::optional<Size>(Size{420,44}):std::nullopt,0,entry.dataId,{1,1},entry.enabled,10);
        std::string label=entry.name;if(isAccepted)label+=" ["+std::to_string(entry.did)+"/"+std::to_string(entry.total)+"]";
        text("MissionList."+entry.dataId,std::move(label),at,{376,command?16.f:20.f},command?"Copperplate-Bold":"TrebuchetMS-Bold",command?18.f:17.f,TextAlign::left,{240.f/255,240.f/255,1,1},10);
        if(entry.complete)image("CompleteMark.png",plus(at,{136,0}),1,1,0,{1,1},false,0,1,10);
        if(state.selectedMission==int(index))image("SelMission.png",at,1,1,0,{1,1},false,2,1,10);
    };
    for(unsigned i=0;i<available;++i)row(state.availableMissions[i],5100+i,i,-44.f*float(i),false);
    if(command&&available==0)text("Command.noNewMission","There is not a new mission.",list,{384,24},"Copperplate-Bold",20,TextAlign::center,{140.f/255,240.f/255,1,1},10);
    const float acceptedY=-float(available)*44-header;
    if(command&&accepted)image("MyMissions.png",plus(list,{0,-float(available)*44-10}),1,1,0,{1,1},false,0,1,10);
    for(unsigned i=0;i<accepted;++i)row(state.acceptedMissions[i],5200+i,unsigned(available)+i,acceptedY-44.f*float(i),true);
    if(state.selectedMission>=0&&std::size_t(state.selectedMission)<available+accepted){
        const Vec2 info=plus(list,{0,-44.f*float(state.selectedMission)-146});
        image("MissionInfo.png",info,1,1,0,{1,1},false,0,1,10);
        text("MissionInfo.body",state.missionBody,plus(info,{-6,0}),{378,224},"Helvetica Neue",command?18.f:16.f,TextAlign::left,{100.f/255,240.f/255,240.f/255,1},10);
        if(command&&std::size_t(state.selectedMission)<available){const auto& selected=state.availableMissions[std::size_t(state.selectedMission)];
            addButton(Panel::mission,5004,"Common_Btn_ACCEPT.png",plus(info,{122,-84}),1,2,0,1,{},0,selected.dataId,{1,1},selected.enabled,10);}
    }
    if(!command)addButton(Panel::mission,5000,"Common_Btn_CLOSE.png",plus(root,{150,-half-16}),1,2,0,1,{},0,{}, {1,1},true,10);
}
void Screens::Impl::completedMission(){
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
            image("CmdCenterBG.png",{384,512});
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
            if(state.systemVisible)system();
            break;
        }
        default:throw std::runtime_error("Unknown original screen ID");
    }
    syncMessage();
    dispatcher.subscribe({},[this](const ButtonEvent& event){
        const auto panel=panels.at(event.key.owner);const auto screen=state.screen;const unsigned id=event.key.id;
        // Side effects are requested at their original handler event boundary.
        // GameSession applies the real state transition and may refresh Screens
        // synchronously; this old Impl remains alive through the input call.
        host.action({screen,panel,event});
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
    tickRange(random);tickDetail();
    if(state.screen==ScreenId::title){
        auto& t=title;
        t.bgRotate=t.bgRotate<0?t.bgRotate+(.01f-t.bgRotate)/60.f:0;
        t.bgScale=t.bgScale>1?t.bgScale+(.99f-t.bgScale)/100.f:1;
        auto approach=[](float p){return p!=0&&std::abs(p)>.1f?p+(0-p)/50.f:0.f;};
        t.bgPosition={approach(t.bgPosition.x),approach(t.bgPosition.y)};
        if(!t.titleSound&&t.start+6.2f<time){host.sound("Title_TitleIn.wav");t.titleSound=true;}
        if(!t.episodeSound&&t.start+7.7f<time){host.sound("Title_TitleIn.wav");t.episodeSound=true;}
        if(t.bigX>-1024)t.bigX-=6;
        if(t.titleScale!=1&&t.start+6.2f+.3f<time){
            if(t.titleScale<=1){host.sound("Title_Bang.wav");host.quake(10,4,.3f);t.titleScale=1;}
            else t.titleScale-=5;
            t.titleVisible=true;
        }
        if(t.episodeScale!=1&&t.start+7.7f+.3f<time){
            if(t.episodeScale<=1){host.sound("Title_Bang.wav");host.quake(6,4,.3f);t.episodeScale=1;}
            else t.episodeScale-=5;
            t.episodeVisible=true;
        }
        if(t.start+10<time){const float phase=float(std::fmod(double(time-(t.start+10)),2.));
            if(phase<.2f){if(!t.startSound){host.sound("Title_TapToStart.wav");t.startSound=true;}
                if(random.random()%10<=5)t.startVisible=((random.random()^1)&1)!=0;
            }else if(phase<1.2f)t.startVisible=true;
            else if(t.startSound){t.startSound=false;t.startVisible=false;}
        }
        updateTitleImages();
    }
    for(auto& node:nodes){
        if(auto* number=std::get_if<NumberNode>(&node))number->font->tick();
        if(state.screen==ScreenId::base)if(auto* n=std::get_if<ImageNode>(&node);n&&n->tag.starts_with("baseSelect")){
            const int selection=std::stoi(n->tag.substr(10));const float target=state.selectedBaseButton==selection?.5f:0.f;
            n->image.alpha+=(target-n->image.alpha)/10.f;
        }
    }
    for(auto& [button,target]:chooserMoves){
        auto move=[](float p,float dest){return p==dest?p:std::abs(p-dest)>.5f?p+(dest-p)/3.f:dest;};
        button->position={move(button->position.x,target.x),move(button->position.y,target.y)};
    }
}
void Screens::Impl::append(Frame& frame,float time){
    for(const auto& node:nodes)std::visit([&](const auto& n){
        using T=std::decay_t<decltype(n)>;auto& layer=layers->at(n.layer);
        if constexpr(std::is_same_v<T,ImageNode>){auto image=n.image;if(n.parent)image.screenPosition=plus(n.parent->screenPosition(),n.local);layer.submitImage(image);}
        else if constexpr(std::is_same_v<T,ButtonNode>){Frame f;n.button->append(f,time);for(auto& s:f.sprites)layer.submitNonAtlas(std::move(s));}
        else if constexpr(std::is_same_v<T,TextNode>){Frame f;if(n.parent)n.text->position=plus(n.parent->screenPosition(),n.local);n.text->append(f);for(auto& s:f.sprites)layer.submitNonAtlas(std::move(s));}
        else if constexpr(std::is_same_v<T,NumberNode>)n.font->append(layer,tiles.visual(*n.tile));
        else if constexpr(std::is_same_v<T,PreviewNode>)for(const auto& sprite:n.frame.sprites)
            layers->at(sprite.visualLayer.value_or(n.layer)).submitNonAtlas(sprite);
        else if constexpr(std::is_same_v<T,DetailNode>){const auto& d=*n.detail;
            for(const auto& label:{d.title,d.description})if(label){Frame textFrame;label->append(textFrame);for(auto& sprite:textFrame.sprites)layer.submitNonAtlas(std::move(sprite));}
            if(d.large){auto image=*d.large;layer.submitImage(image);}
        }
    },node);
    if(state.screen==ScreenId::field&&message.initialized){auto& layer=layers->at(9);layer.submitImage(message.background);if(message.text){Frame textFrame;message.text->append(textFrame);for(auto& s:textFrame.sprites)layer.submitNonAtlas(std::move(s));}}
    for(auto& layer:*layers)layer.drain().appendBatches(frame);
}

void Screens::Impl::updateHUD(const ScreensState& next){
    state.messageWorldTime=next.messageWorldTime;state.selectedSlot=next.selectedSlot;
    state.hpBarWidth=next.hpBarWidth;state.hpBarFilled=next.hpBarFilled;
    state.spBarWidth=next.spBarWidth;state.spBarFilled=next.spBarFilled;state.boosterReady=next.boosterReady;
    state.experienceRate=next.experienceRate;state.enemyHpRate=next.enemyHpRate;
    state.level=next.level;state.credits=next.credits;state.hpRate=next.hpRate;state.spRate=next.spRate;
    state.fieldWorldTime=next.fieldWorldTime;state.fieldUIRevision=next.fieldUIRevision;state.hasWorkingSlot=next.hasWorkingSlot;
    state.outOfRangeTimeLeft=next.outOfRangeTimeLeft;state.outOfRangeTimeRight=next.outOfRangeTimeRight;
    state.enemyName=next.enemyName;state.enemyNameRevision=next.enemyNameRevision;
    if(fieldPresentation.identity!=next.fieldUIRevision){const auto oldName=fieldPresentation.enemyName;std::erase_if(nodes,[&](const Node& node){const auto* n=std::get_if<TextNode>(&node);return n&&n->text==oldName;});fieldPresentation={};fieldPresentation.identity=next.fieldUIRevision;}
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
        }
    }
}
Screens::Screens(ResourceAccess& r,PreparedTiles& t,ScreensHost& h):resources_(r),tiles_(t),host_(h){}
bool Screens::updateHUD(const ScreensState& state){
    if(!impl_||impl_->state.screen!=state.screen)return false;
    impl_->updateHUD(state);return true;
}
void Screens::refresh(const ScreensState& state,float time){
    auto next=std::make_shared<Impl>(resources_,tiles_,host_,state,time,impl_);next->build();
    const bool changed=!impl_||impl_->state.screen!=state.screen;
    next->previous.reset();impl_=std::move(next);
    if(changed){if(state.screen==ScreenId::title)host_.bgm("STAGE1.wav");else if(state.screen==ScreenId::slots)host_.bgm("STAGE2.wav");else if(state.screen==ScreenId::base)host_.bgm("Title.wav");else if(state.screen==ScreenId::field)host_.bgm("STAGE3.mp3");}
}
void Screens::tick(float time,WorldRandom& random){if(auto keep=impl_)keep->tick(time,random);}
bool Screens::input(PointerPhase phase,Vec2 point,std::uintptr_t pointer,float time){
    const auto keep=impl_;if(!keep)return false;
    if(keep->state.screen==ScreenId::title){
        if(phase==PointerPhase::end){
            const bool ready=keep->title.start+10<=time;
            if(!ready){keep->title.start=time-10;keep->title.bigVisible=false;keep->updateTitleImages();}
            host_.action({ScreenId::title,Panel::root,{ButtonEventType::pop,{owner(ScreenId::title,Panel::root),0,0},pointer,point},true,ready});
        }
        return true;
    }
    bool captured=false;for(const auto& key:keep->keys)if(const auto b=keep->dispatcher.find(key);b&&b->capturedPointer()==pointer&&pointer)captured=true;
    return keep->dispatcher.input(phase,point,pointer,time)||captured;
}
void Screens::append(Frame& out,float time)const{if(auto keep=impl_)keep->append(out,time);}
void Screens::discardInputs(){if(impl_)impl_->dispatcher.discardAll();}
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
        {ScreenId::slots,Panel::root,"Slot complete stats/date and current mech animation","GuiSelectSlot.refreshSlot; host assembly",false},
        {ScreenId::base,Panel::root,"Base6 tables and27 table buttons, original bounds and IDs","GuiBase0x246d4/Base001..006.tble",true},
        {ScreenId::base,Panel::root,"Selection description slide and mission notices","GuiBase0x243b8/0x251d0",false},
        {ScreenId::field,Panel::root,"HUD gauges, menu1021, weapon selectors1022/1023, inv1025/mission1026","GuiGame0x29c9c/0x291d4/0x29100/0x29054",true},
        {ScreenId::field,Panel::weaponChoices,"Inventory weapon chooser1024, raw index, originalAffecter targets and labels","GuiGame0x2aad8/QobAffecter0x36698",true},
        {ScreenId::field,Panel::root,"Path move button7000 creation","GuiGame.handleButton; constructor unresolved",false},
        {ScreenId::field,Panel::system,"Airlift1027, SaveExit1031, More1032, Help1029, Resume1030","DlgSystemMenu0x23fb0",true},
        {ScreenId::field,Panel::airlift,"Visited-base rows4002..4007, OK4001, close4000","DlgAirlift0x1fbb0/0x1f5b8",true},
        {ScreenId::field,Panel::mission,"Accepted rows5200..5204, close5000, original text formatter","DlgMission0x22b1c/0x23678/0x23208",true},
        {ScreenId::command,Panel::mission,"Available5100..5104,accepted5200..5204,accept5004,ranking5009,exit2100","GuiCommandCenter0x2614c/0x2708c/0x26af8",true},
        {ScreenId::command,Panel::mission,"Completed result5007/reward overlay","GuiCommandCenter0x279a8/0x269ac",true},
        {ScreenId::command,Panel::mission,"Mission list expand animation","GuiCommandCenter0x26558",false},
        {ScreenId::maintenance,Panel::inventory,"25 cells6000..6024,pages6090/6091,arrange6093","WndInven0x34918/GuiMaintenance0x2bbb0",true},
        {ScreenId::maintenance,Panel::equipment,"Six equipment targets6100..6105 and actual item identities","WndEquip0x33c5c",true},
        {ScreenId::maintenance,Panel::bot,"Three bot targets6300..6302,disarm6310","WndBotSlot0x33300/GuiMaintenance.handleButton",true},
        {ScreenId::field,Panel::quick,"Five quick targets6200..6204,countable and equipped glyph state","WndQuickSlot0x35554",true},
        {ScreenId::field,Panel::inventory,"Inventory/equipment/BOT window6092,6093,6090/6091","DlgInven0x20b0c",true},
        {ScreenId::shop,Panel::root,"Five categories2104,24 items2200..2223,pages2105/2106,BUY2102/SELL2103/EXIT2100","GuiShop0x30e4c/0x30260",true},
        {ScreenId::shop,Panel::trade,"Trade2110/2111,2107,2108/2109,actual qty/credit rendering","GuiShop0x31b20",true},
        {ScreenId::shop,Panel::root,"Item detail2112, original preview and expansion animation","GuiShop0x31554/0x30888",false},
        {ScreenId::field,Panel::root,"Radar, place/enemy names,message lifetime,help pages/game-over native alerts","GuiGame/GuiHelp/GameMain; host consumers pending",false}
    };
}
}
