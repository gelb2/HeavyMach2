#include "Core/UI/Screens.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Assets/Image.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <map>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace hm2;
using namespace hm2::ui;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);assert(f);return Bytes(std::istreambuf_iterator<char>(f),{});}
    bool exists(std::string_view n)const override{++probes;return std::filesystem::exists(root/std::string(n));}
};
// Text layout test double: deliberately does not claim original glyph raster QA.
struct Host:ScreensHost {
    unsigned preparations=0;std::vector<ScreenAction> actions;std::vector<std::string> songs,sounds;
    std::vector<TextRasterRequest> requests;
    std::vector<unsigned> ids;std::function<void(const ScreenAction&)> callback;
    std::map<std::string,Bytes> hudSettings;unsigned hudReads=0,hudWrites=0;bool hudWriteSucceeds=true;
    std::optional<Bytes> loadHUDLayout(std::string_view profile)override{++hudReads;const auto i=hudSettings.find(std::string(profile));return i==hudSettings.end()?std::nullopt:std::optional<Bytes>(i->second);}
    bool saveHUDLayout(std::string_view profile,std::span<const std::uint8_t> bytes)override{++hudWrites;if(!hudWriteSucceeds)return false;hudSettings[std::string(profile)]={bytes.begin(),bytes.end()};return true;}

    void action(const ScreenAction& e)override{actions.push_back(e);if(callback)callback(e);}
    PreparedScreenText prepareText(const TextRasterRequest& r)override{
        ++preparations;requests.push_back(r);auto b=std::make_shared<TextBitmap>();b->request=r;b->layout=textRasterLayout(r);
        b->alpha.resize(std::size_t(b->layout.width)*b->layout.height);
        return {"prepared:"+r.sourceId,std::move(b)};
    }
    void bgm(std::string_view s)override{songs.emplace_back(s);}
    void sound(std::string_view s)override{sounds.emplace_back(s);}
    void sfx(unsigned s)override{ids.push_back(s);}
};
struct Random:WorldRandom {unsigned calls=0;std::uint32_t random()override{++calls;return 7;}};
std::vector<ButtonKey> ids(Screens& ui,unsigned id){auto keys=ui.buttons();std::erase_if(keys,[&](const auto& k){return k.id!=id;});return keys;}
const Sprite* hudSelectionLabel(const Frame& f){for(const auto& s:f.sprites)if(s.image.starts_with("prepared:HUDLayout.title."))return &s;return nullptr;}
const Sprite* sprite(const Frame& f,std::string_view name){for(const auto& s:f.sprites)if(s.image==name)return &s;return nullptr;}
struct SequenceRandom:WorldRandom {
    std::vector<std::uint32_t> values;unsigned calls=0;
    std::uint32_t random()override{assert(calls<values.size());return values[calls++];}
};
Frame draw(Screens& ui){Frame f;ui.append(f,0);return f;}
float geometryWidth(const Sprite* s){assert(s);if(s->strip.empty())return s->size.width;float lo=s->strip[0].position.x,hi=lo;for(const auto& p:s->strip){lo=std::min(lo,p.position.x);hi=std::max(hi,p.position.x);}return hi-lo;}
float geometryHeight(const Sprite* s){assert(s);if(s->strip.empty())return s->size.height;float lo=s->strip[0].position.y,hi=lo;for(const auto& p:s->strip){lo=std::min(lo,p.position.y);hi=std::max(hi,p.position.y);}return hi-lo;}
bool near(float a,float b){return std::abs(a-b)<.001f;}
std::size_t positionOf(const Frame& f,std::string_view image){auto i=std::find_if(f.sprites.begin(),f.sprites.end(),[&](const auto& s){return s.image==image;});assert(i!=f.sprites.end());return std::size_t(i-f.sprites.begin());}
const TextRasterRequest& request(const Host& host,std::string_view id){auto i=std::find_if(host.requests.rbegin(),host.requests.rend(),[&](const auto& r){return r.sourceId==id;});assert(i!=host.requests.rend());return *i;}
ItemView detailFixture(){ItemView item;item.dataId="10011";item.name="MG-16d";item.description="Machine-gun\n\nAtk 5\nPrice : 100cr";item.icon="Icon10011.png";item.largeIcon="IconB10011.png";item.originalType=10;item.count=1;return item;}
void shopDetails(const char* root){
    Files files(root);Host host;PreparedTiles tiles("detail-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::shop;state.locale="kr";state.shopItems={detailFixture()};state.selectedShop=0;state.shopDetailVisible=true;state.detailRevision=1;
    ui.refresh(state);auto f=draw(ui);assert(!sprite(f,"ItemDesc.png")&&!sprite(f,"prepared:GuiShop.item.title")&&!sprite(f,"IconB10011.png"));
    const auto& title=request(host,"GuiShop.item.title");assert(title.text=="MG-16d"&&title.fontName=="TrebuchetMS-Bold"&&title.fontSize==16&&title.dimensions.width==160&&title.dimensions.height==24&&title.alignment==TextAlign::center);
    const auto& desc=request(host,"GuiShop.item.description");assert(desc.text==state.shopItems[0].description&&desc.fontName=="TrebuchetMS-Bold"&&desc.fontSize==12&&desc.dimensions.width==160&&desc.dimensions.height==90&&desc.alignment==TextAlign::left);
    const auto count=host.preparations,reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<7;++i){ui.tick(float(i),random);assert(!sprite(draw(ui),"prepared:GuiShop.item.title"));}
    ui.tick(8,random);f=draw(ui);const auto* t=sprite(f,"prepared:GuiShop.item.title");const auto* d=sprite(f,"prepared:GuiShop.item.description");const auto* large=sprite(f,"IconB10011.png");assert(t&&d&&large);
    assert(t->size.width==160&&near(t->size.height,24*.505f)&&near(geometryWidth(large),ui.preparedTile("IconB10011.png")->pixels.width)&&large->angle==0);
    assert(d->color.r==120.f/255&&d->color.g==240.f/255&&d->color.b==240.f/255);
    ui.refresh(state);assert(near(sprite(draw(ui),"prepared:GuiShop.item.title")->size.height,24*.505f)&&host.preparations==count);
    for(unsigned i=0;i<6;++i)ui.tick(10+float(i),random);f=draw(ui);t=sprite(f,"prepared:GuiShop.item.title");d=sprite(f,"prepared:GuiShop.item.description");large=sprite(f,"IconB10011.png");
    assert(t->center.x==568&&t->center.y==830&&t->size.height==24&&d->center.x==600&&d->center.y==754&&large->center.x==492&&large->center.y==754&&large->angle==0);
    for(unsigned i=0;i<100;++i)ui.tick(20+float(i),random);assert(host.preparations==count&&files.reads==reads&&files.probes==probes&&random.calls==0);
    ++state.detailRevision;ui.refresh(state);assert(!sprite(draw(ui),"prepared:GuiShop.item.title")&&host.preparations==count+2);
    state.shopDetailVisible=false;state.tradeVisible=true;state.tradeItemId="10011";state.buyMode=true;state.tradeCount=1;ui.refresh(state);assert(!sprite(draw(ui),"prepared:GuiShop.item.title")&&sprite(draw(ui),"TradeItemBG.png"));
    state.tradeVisible=false;ui.refresh(state);assert(!sprite(draw(ui),"prepared:GuiShop.item.title")&&ids(ui,2102).size()==1); // Selection survives trade; top detail does not reappear.
    state.shopDetailVisible=true;state.shopItems[0].originalType=1;++state.detailRevision;ui.refresh(state);for(unsigned i=0;i<14;++i)ui.tick(float(i),random);assert(!sprite(draw(ui),"IconB10011.png")); // A supplied largeIcon cannot override original type gating.
    std::cout<<"SHOP_DETAIL originalEnglish/typedIconB/font/color/twoPhaseEase/noParentScale/warm100/repeatedSelection/tradeLifetime PASS\n";
}
void inventoryMaintenanceDetails(const char* root){
    Files files(root);Host host;PreparedTiles tiles("inven-detail-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::shop;state.inventoryVisible=true;state.inventoryPage=1;state.selectedInventory=1;state.inventory.resize(27);state.inventory[26]=detailFixture();state.detailItem=state.inventory[26];state.detailItem.description="Machine-gun\n\nSellPrice : 50cr";state.detailInventory=26;state.detailRevision=1;
    ui.refresh(state);auto popup=ui.button(ids(ui,2112).front());assert(popup->position.x==444&&popup->position.y==408&&popup->scale.x==.01f&&popup->bounds().left==-103&&popup->bounds().right==103&&popup->bounds().bottom==-64&&popup->bounds().top==64);
    assert(ui.button(ids(ui,2103).front())->position.y==552&&ids(ui,6092).empty());
    auto arrange=ui.button(ids(ui,6093).front());assert(arrange->visible&&arrange->position.x==624&&arrange->position.y==486&&arrange->bounds().left==-60&&arrange->bounds().right==60);
    auto f=draw(ui);const auto* ar=sprite(f,"Btn_ArrangeItem.png");assert(ar&&ar->size.width==128&&ar->size.height==64&&ar->tile==0); // raw tile1 on 1frame draw-clamps.
    assert(positionOf(f,"Common_Btn_SELL.png")<positionOf(f,"InvenBG.png"));
    for(unsigned i=0;i<14;++i)ui.tick(float(i),random);f=draw(ui);assert(sprite(f,"ItemDesc.png")->center.y==312&&sprite(f,"prepared:DlgInven.item.title")->center.y==374&&sprite(f,"prepared:DlgInven.item.description")->center.x==476&&sprite(f,"IconB10011.png")->center.x==368&&sprite(f,"IconB10011.png")->center.y==302);
    assert(request(host,"DlgInven.item.description").text==state.detailItem.description);
    state.tradeVisible=true;state.tradeItemId="10011";ui.refresh(state);f=draw(ui);assert(positionOf(f,"ItemDesc.png")<positionOf(f,"TradeItemBG.png"));
    state.tradeVisible=false;state.inventory[26].enabled=false;ui.refresh(state);assert(ids(ui,2103).empty());
    // A late-created layer13 popup must receive input before older Shop EXIT,
    // even where their original fixed hit rectangles overlap near the bottom.
    state.inventoryPage=0;state.selectedInventory=24;state.detailInventory=24;++state.detailRevision;ui.refresh(state);for(unsigned i=0;i<14;++i)ui.tick(float(i),random);
    const auto popupCenter=ui.button(ids(ui,2112).front())->position;ui.input(PointerPhase::begin,popupCenter,19,0);assert(host.actions.back().button.key.id==2112);ui.input(PointerPhase::end,popupCenter,19,0);
    state.screen=ScreenId::maintenance;state.inventoryVisible=false;state.detailInventory=0;state.selectedBot=0;++state.detailRevision;
    Sprite forbidden;forbidden.image="unoriginal-maintenance-preview";state.machPreview.sprites.push_back(forbidden);ui.refresh(state);
    f=draw(ui);assert(!sprite(f,"unoriginal-maintenance-preview"));
    const auto* exit=sprite(f,"Btn_CmdCenterExit.png");const auto* disarm=sprite(f,"Btn_Disarm.png");const auto* arr=sprite(f,"Btn_ArrangeItem.png");assert(exit&&disarm&&arr&&exit->size.width==64&&exit->size.height==64&&disarm->size.width==64&&disarm->size.height==64&&arr->size.width==128&&arr->size.height==64);
    assert(positionOf(f,"Btn_CmdCenterExit.png")<positionOf(f,"Inven_BtnItem.png"));
    assert(request(host,"GuiMaintenance.item.title").fontName=="Arial"&&request(host,"GuiMaintenance.item.title").fontSize==12&&request(host,"GuiMaintenance.item.title").dimensions.width==128);
    for(unsigned i=0;i<14;++i)ui.tick(float(i),random);f=draw(ui);assert(sprite(f,"ItemDesc.png")->center.x==492&&sprite(f,"ItemDesc.png")->center.y==490&&sprite(f,"prepared:GuiMaintenance.item.title")->center.y==537);
    state.detailInventory=10;state.detailItem.originalType=11;++state.detailRevision;ui.refresh(state);for(unsigned i=0;i<14;++i)ui.tick(float(i),random);f=draw(ui);assert(sprite(f,"ItemDesc.png")->center.y==575&&sprite(f,"prepared:GuiMaintenance.item.title")->center.y==622&&sprite(f,"prepared:GuiMaintenance.item.description")->center.x==568&&sprite(f,"IconB10011.png")->center.x==440);
    // The original popup handler may synchronously remove itself. One input must not dereference the replaced presentation.
    host.callback=[&](const ScreenAction& event){if(event.button.key.id==2112&&event.button.type==ButtonEventType::push){state.detailInventory=-1;++state.detailRevision;ui.refresh(state);}};
    const auto center=ui.button(ids(ui,2112).front())->position;ui.input(PointerPhase::begin,center,20,0);assert(ids(ui,2112).empty());host.callback={};
    state.screen=ScreenId::field;state.inventoryVisible=true;state.selectedBot=-1;ui.refresh(state);f=draw(ui);assert(ids(ui,6092).size()==1&&ids(ui,6093).size()==1);const auto close=ui.button(ids(ui,6092).front()),sort=ui.button(ids(ui,6093).front());assert(close->position.x==658&&sort->position.x==610&&sort->visible&&sort->bounds().right==21);
    auto closeSprites=std::vector<const Sprite*>{};for(const auto& sp:f.sprites)if(sp.image=="Btn_CloseX.png")closeSprites.push_back(&sp);assert(closeSprites.size()==2&&closeSprites[0]->size.width==64&&closeSprites[1]->tile==1);
    std::cout<<"INVENTORY_MAINT_DETAIL absolute26/cell1/mode2/fixedBounds/layer3_9_13/SELLcondition/sourceSheets/Arial/upperLower/noAddedMach/callbackLifetime PASS\n";
}
void fieldNameRange(const char* root){
    Files files(root);Host host;PreparedTiles tiles("field-name-range-test");Screens ui(files,tiles,host);SequenceRandom random;
    ScreensState state;state.screen=ScreenId::field;state.fieldUIRevision=1;state.hasWorkingSlot=true;state.enemyName="E_Tank_01";state.enemyNameRevision=1;state.enemyHpRate=.5f;state.fieldWorldTime=.1;
    ui.refresh(state);auto f=draw(ui);const auto* name=sprite(f,"prepared:GuiGame.enemyName");assert(name&&name->center.x==384&&name->center.y==956&&name->size.width==256&&name->size.height==18);
    const auto& nr=request(host,"GuiGame.enemyName");assert(nr.fontName=="Arial-BoldMT"&&nr.fontSize==12&&nr.alignment==TextAlign::center&&nr.text=="E_Tank_01");
    assert(!sprite(f,"OutofRange.png"));random.values={0,0,7};ui.tick(1,random);f=draw(ui);const auto* range=sprite(f,"OutofRange.png");assert(range&&range->center.x==334&&range->center.y==90&&geometryWidth(range)==64&&geometryHeight(range)==32&&random.calls==3); // zero stamps have original initial blink; second side retains false.
    const auto preparations=host.preparations,reads=files.reads,probes=files.probes;
    state.fieldWorldTime=.2;assert(ui.updateHUD(state));ui.tick(2,random);f=draw(ui);assert(std::count_if(f.sprites.begin(),f.sprites.end(),[](const auto& s){return s.image=="OutofRange.png";})==2&&random.calls==3);
    state.fieldWorldTime=double(1.2f);ui.updateHUD(state);ui.tick(3,random);assert(!sprite(draw(ui),"OutofRange.png"));
    state.fieldWorldTime=double(1.5f);ui.updateHUD(state);ui.tick(4,random);assert(!sprite(draw(ui),"OutofRange.png"));
    state.fieldWorldTime=double(std::nextafter(1.2f,0.f));ui.updateHUD(state);ui.tick(5,random);assert(sprite(draw(ui),"OutofRange.png"));
    state.fieldWorldTime=double(1.5f);ui.updateHUD(state);ui.tick(6,random);assert(sprite(draw(ui),"OutofRange.png")); // >=1.5 retains existing show.
    state.fieldWorldTime=0;ui.updateHUD(state);ui.tick(7,random);assert(sprite(draw(ui),"OutofRange.png")); // <=0 retains existing show.
    state.enemyHpRate=0;ui.updateHUD(state);assert(!sprite(draw(ui),"prepared:GuiGame.enemyName"));state.enemyHpRate=.5f;ui.updateHUD(state);assert(sprite(draw(ui),"prepared:GuiGame.enemyName"));
    for(unsigned i=0;i<100;++i){ui.updateHUD(state);ui.tick(8+float(i),random);draw(ui);}assert(host.preparations==preparations&&files.reads==reads&&files.probes==probes&&random.calls==3);
    ++state.enemyNameRevision;ui.updateHUD(state);assert(host.preparations==preparations+1); // same name, actual setEnemyName creates replacement.
    state.screen=ScreenId::base;state.fieldWorldTime=.1;ui.refresh(state);random.values.insert(random.values.end(),{0,1,0,0});assert(ui.updateHUD(state));ui.tick(110,random);assert(random.calls==7); // hidden GuiGame still ticks left then right with frozen worldTime.
    state.screen=ScreenId::field;ui.refresh(state);f=draw(ui);assert(sprite(f,"prepared:GuiGame.enemyName")&&sprite(f,"OutofRange.png")->center.x==480);
    state.fieldWorldTime=0;state.fieldUIRevision=2;state.enemyName.clear();state.enemyNameRevision=0;ui.updateHUD(state);f=draw(ui);assert(!sprite(f,"OutofRange.png")&&!sprite(f,"prepared:GuiGame.enemyName"));
    state.fieldWorldTime=.1;state.hasWorkingSlot=false;ui.updateHUD(state);ui.tick(111,random);assert(random.calls==7);
    state.hasWorkingSlot=true;state.fieldUIRevision=0;ui.updateHUD(state);ui.tick(112,random);assert(random.calls==7);
    assert(std::find(host.ids.begin(),host.ids.end(),6)==host.ids.end()); // setOutOfRange request/SFX gate belongs to the actual Session.
    std::cout<<"FIELD_NAME_RANGE nameReplacement/gaugeHide/sharedRNG/f32Boundaries/initialZeroStampBlink/hiddenTick/persistentGeneration/noWarmIO PASS\n";
}
void fieldCredits(const char* root){
    Files files(root);Host host;PreparedTiles tiles("field-credits-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::field;state.hasWorkingSlot=true;state.fieldUIRevision=1;state.credits=500;
    ui.refresh(state);
    auto displayed=[&]{
        auto frame=draw(ui);const auto* glyphs=sprite(frame,"NumSet01b.png");
        assert(glyphs&&glyphs->atlasBatch);
        int value=0,place=1;
        for(const auto& quad:glyphs->atlasBatch->quads){
            value+=place*int(std::lround(quad.vertices[0].uv.x*16));
            place*=10;
        }
        return value;
    };
    assert(displayed()==0);
    ui.tick(1,random);assert(displayed()==100);
    ui.tick(2,random);assert(displayed()==180);
    state.credits=205;assert(ui.updateHUD(state));assert(displayed()==180);
    ui.tick(3,random);assert(displayed()==185);
    ui.refresh(state);assert(displayed()==185);
    state.screen=ScreenId::base;ui.refresh(state);ui.tick(4,random);
    state.screen=ScreenId::field;ui.refresh(state);assert(displayed()==189);
    const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<8;++i)ui.tick(float(5+i),random);
    assert(displayed()==205&&files.reads==reads&&files.probes==probes&&host.preparations==0&&random.calls==0);
    state.credits=1;assert(ui.updateHUD(state));ui.tick(13,random);assert(displayed()==165&&state.credits==1);
    ++state.fieldUIRevision;assert(ui.updateHUD(state));assert(displayed()==0);
    state.hasWorkingSlot=false;assert(ui.updateHUD(state));ui.tick(14,random);assert(displayed()==0);
}
void radarMarks(const char* root){
    Files files(root);Host host;PreparedTiles tiles("radar-test");Screens ui(files,tiles,host);
    ScreensState state;state.screen=ScreenId::field;
    state.radarMarks={{{-512,512},0},{{128,16},1},{{-16,-32},2}};
    ui.refresh(state,0);
    auto marks=[](const Frame& frame){
        std::vector<Sprite> result;
        for(const auto& sprite:frame.sprites)if(sprite.image=="RaderMark.png")result.push_back(sprite);
        return result;
    };
    const auto first=marks(draw(ui));
    assert(first.size()==3);
    const Vec2 centers[]={{377,72},{417,41},{408,38}};
    for(unsigned i=0;i<first.size();++i){
        const auto& mark=first[i];
        assert(!mark.atlasBatch&&mark.strip.size()==4);
        assert(geometryWidth(&mark)==8&&geometryHeight(&mark)==8);
        assert(mark.strip[0].uv.x==i/8.f&&mark.strip[2].uv.x==(i+1)/8.f);
        assert(mark.center.x==centers[i].x&&mark.center.y==centers[i].y);
    }
    const auto reads=files.reads,probes=files.probes;
    state.radarMarks={{{-256,-128},0}};
    assert(ui.updateHUD(state));
    const auto moved=marks(draw(ui));
    assert(moved.size()==1&&moved[0].strip[0].uv.x==0&&moved[0].center.x==393&&moved[0].center.y==32);
    assert(files.reads==reads&&files.probes==probes);
}
void fieldPlaceName(const char* root){
    Files files(root);Host host;PreparedTiles tiles("place-name-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::field;state.hasWorkingSlot=true;state.fieldPlaceRevision=1;state.fieldPlaceName="under beach 01";
    ui.refresh(state);
    auto frame=draw(ui);
    const auto* background=sprite(frame,"PlaceNameBG.png");
    assert(background&&background->center.x==-1024&&background->center.y==512);
    const auto& label=request(host,"GuiGame.placeName");
    assert(label.text==state.fieldPlaceName&&label.fontName=="Arial"&&label.fontSize==32&&label.dimensions.width==256&&label.dimensions.height==40&&label.alignment==TextAlign::center);
    std::vector<const Sprite*> labels;
    for(const auto& s:frame.sprites)if(s.image=="prepared:GuiGame.placeName")labels.push_back(&s);
    assert(labels.size()==2&&labels[0]->center.x==-1022&&labels[0]->center.y==510&&labels[0]->color.r==0);
    assert(labels[1]->center.x==-1024&&labels[1]->center.y==512&&labels[1]->color.r==1);
    ui.tick(.01f,random);frame=draw(ui);
    assert(near(sprite(frame,"PlaceNameBG.png")->center.x,-1024.f+1408.f/12.f));
    const auto preparations=host.preparations;
    ui.refresh(state);
    frame=draw(ui);assert(near(sprite(frame,"PlaceNameBG.png")->center.x,-1024.f+1408.f/12.f));
    state.screen=ScreenId::base;ui.refresh(state);
    ui.tick(.02f,random);assert(!sprite(draw(ui),"PlaceNameBG.png"));
    state.screen=ScreenId::field;ui.refresh(state);
    const float first=-1024.f+1408.f/12.f;
    frame=draw(ui);assert(near(sprite(frame,"PlaceNameBG.png")->center.x,first+(384.f-first)/12.f));
    bool reachedCenter=false;
    const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<300;++i){
        ui.tick(i/60.f,random);frame=draw(ui);
        if(const auto* bg=sprite(frame,"PlaceNameBG.png");bg&&bg->center.x==384)reachedCenter=true;
    }
    assert(reachedCenter&&!sprite(frame,"PlaceNameBG.png")&&!sprite(frame,"prepared:GuiGame.placeName"));
    assert(host.preparations==preparations&&files.reads==reads&&files.probes==probes);
    ui.refresh(state);assert(!sprite(draw(ui),"PlaceNameBG.png"));
    ++state.fieldPlaceRevision;ui.refresh(state);
    frame=draw(ui);assert(sprite(frame,"PlaceNameBG.png")->center.x==-1024);
}
std::vector<const Sprite*> pageSprites(const Frame& frame){std::vector<const Sprite*> out;for(const auto& sprite:frame.sprites)if(sprite.image=="InvenPage.png")out.push_back(&sprite);return out;}
void slotPreviewTranslation(const char* root){
    Files files(root);Host host;PreparedTiles tiles("slot-preview-test");Screens ui(files,tiles,host);
    // Presentation fixture: one shared immutable local Frame is consumed at
    // three actual slot origins. It covers every supported geometry carrier.
    Frame local;Sprite body;body.image="P_Body_01.png";body.center={-3.25f,7.5f};body.size={46,38};
    body.angle=.371f;body.reverseY=true;body.color={.25f,.5f,.75f,.9f};body.visualLayer=3;body.clip=ClipRect{15,25,35,45};
    local.sprites.push_back(body);
    Sprite strip;strip.image="P_Foot_01.png";strip.center={2,6};strip.angle=-.2f;strip.visualLayer=4;strip.clip=body.clip;
    strip.strip={{{-5,3},{.125f,.25f},{.2f,.3f,.4f,.5f}},{{7,-8},{.625f,.75f},{.6f,.7f,.8f,.9f}},{{11,12},{1,0},{1,1,1,1}}};
    local.sprites.push_back(strip);
    auto batch=std::make_shared<AtlasBatch>();batch->tileIdentity="slot-local-shadow";batch->image="P_Body_01.png~shd";
    batch->textureSource={body.image,TextureVariant::originalHalfAlpha};batch->content={46,38};batch->storage={64,64};
    batch->pixelFormat=3;batch->capacity=64;batch->blend={BlendFactor::sourceAlpha,BlendFactor::oneMinusSourceAlpha};
    AtlasQuad quad;const Vec2 points[]={{-8.5f,11.25f},{-1.75f,-6.5f},{19.125f,21.5f},{25.875f,3.75f}};
    for(unsigned i=0;i<4;++i)quad.vertices[i]={points[i],{std::uint8_t(17+i),std::uint8_t(49+i),std::uint8_t(81+i),std::uint8_t(113+i)},{float(i%2)*.5f,float(i/2)*.25f}};
    batch->quads.push_back(quad);batch->indices={0,1,2,3,2,1};
    Sprite shadow;shadow.image=batch->image;shadow.atlasBatch=batch;shadow.textureSource=batch->textureSource;shadow.visualLayer=0;shadow.clip=body.clip;
    local.sprites.push_back(shadow);
    ScreensState state;state.screen=ScreenId::slots;
    for(auto& slot:state.slots){slot.used=true;slot.machPreview=local;}
    const auto compare=[&](const Frame& frame,std::vector<const AtlasBatch*>* previous){
        std::vector<const Sprite*> bodies,strips,shadows;
        for(const auto& s:frame.sprites){if(s.image==body.image)bodies.push_back(&s);else if(s.image==strip.image)strips.push_back(&s);else if(s.image==shadow.image)shadows.push_back(&s);}
        assert(bodies.size()==3&&strips.size()==3&&shadows.size()==3);
        // Original shadow child layer0 precedes the opaque slot background3.
        // Player type0 body parts are direct visuals on inherited layer3;
        // numeric glyph atlases in that layer are drawn after those bodies.
        assert(positionOf(frame,shadow.image)<positionOf(frame,"SelSlot_BG.png"));
        assert(positionOf(frame,"SelSlot_BG.png")<positionOf(frame,body.image));
        assert(positionOf(frame,body.image)<positionOf(frame,"NumSet_Cr.png"));
        for(unsigned slot=0;slot<3;++slot){const Vec2 origin{214,794.f-240.f*float(slot)};
            const auto& b=*bodies[slot];assert(near(b.center.x,body.center.x+origin.x)&&near(b.center.y,body.center.y+origin.y));
            assert(b.angle==body.angle&&b.reverseY==body.reverseY&&b.size.width==body.size.width&&b.size.height==body.size.height&&b.clip==body.clip&&b.visualLayer==body.visualLayer);
            assert(b.color.r==body.color.r&&b.color.g==body.color.g&&b.color.b==body.color.b&&b.color.a==body.color.a);
            const auto& s=*strips[slot];assert(s.clip==strip.clip&&s.angle==strip.angle&&s.strip.size()==strip.strip.size());
            for(unsigned i=0;i<s.strip.size();++i){const auto& a=s.strip[i];const auto& z=strip.strip[i];assert(near(a.position.x,z.position.x+origin.x)&&near(a.position.y,z.position.y+origin.y));assert(a.uv.x==z.uv.x&&a.uv.y==z.uv.y&&a.color.r==z.color.r&&a.color.g==z.color.g&&a.color.b==z.color.b&&a.color.a==z.color.a);}
            const auto& image=*shadows[slot];assert(image.atlasBatch&&image.clip==shadow.clip&&image.textureSource==shadow.textureSource&&image.visualLayer==shadow.visualLayer);
            const auto& a=*image.atlasBatch;assert(a.tileIdentity==batch->tileIdentity&&a.image==batch->image&&a.textureSource==batch->textureSource&&a.capacity==batch->capacity&&a.pixelFormat==batch->pixelFormat&&a.indices==batch->indices);
            assert(a.content.width==46&&a.content.height==38&&a.storage.width==64&&a.storage.height==64&&a.quads.size()==1&&a.blend.source==batch->blend.source&&a.blend.destination==batch->blend.destination);
            for(unsigned i=0;i<4;++i){const auto& v=a.quads[0].vertices[i];const auto& original=quad.vertices[i];assert(near(v.position.x,original.position.x+origin.x)&&near(v.position.y,original.position.y+origin.y));assert(v.uv.x==original.uv.x&&v.uv.y==original.uv.y&&v.color==original.color);}
            assert(image.atlasBatch.get()!=batch.get());
            if(previous){if(previous->size()<3)previous->push_back(image.atlasBatch.get());else assert(previous->at(slot)==image.atlasBatch.get());}
        }
    };
    for(unsigned refresh=0;refresh<3;++refresh){state.selectedSlot=int(refresh);ui.refresh(state);std::vector<const AtlasBatch*> stable;
        const auto reads=files.reads,probes=files.probes,preparations=host.preparations;
        Frame prepared;ui.appendPreparation(prepared);unsigned preparedBodies=0,preparedStrips=0,preparedShadows=0;
        for(const auto& image:prepared.sprites){
            if(image.image==body.image)++preparedBodies;
            if(image.image==strip.image){++preparedStrips;assert(image.strip.size()==3);}
            if(image.image==shadow.image){++preparedShadows;assert(image.textureSource==shadow.textureSource&&image.atlasBatch&&image.atlasBatch->textureSource==batch->textureSource&&image.atlasBatch->image==batch->image);}
        }
        assert(preparedBodies==3&&preparedStrips==3&&preparedShadows==3);
        const auto actual=draw(ui);for(const auto& image:actual.sprites)assert(std::any_of(prepared.sprites.begin(),prepared.sprites.end(),[&](const auto& resource){return resource.image==image.image&&resource.textureSource==image.textureSource;}));
        for(unsigned drawIndex=0;drawIndex<120;++drawIndex)compare(draw(ui),&stable);
        assert(files.reads==reads&&files.probes==probes&&host.preparations==preparations);
        assert(local.sprites[2].atlasBatch.get()==batch.get());for(unsigned i=0;i<4;++i){const auto& v=batch->quads[0].vertices[i];assert(v.position.x==points[i].x&&v.position.y==points[i].y);}
        assert(local.sprites[0].center.x==body.center.x&&local.sprites[1].strip[0].position.x==strip.strip[0].position.x);
    }
    state.screen=ScreenId::base;ui.refresh(state);Frame departed;ui.appendPreparation(departed);assert(!sprite(departed,body.image)&&!sprite(departed,strip.image)&&!sprite(departed,shadow.image));
    std::cout<<"SLOT_PREVIEW translation3/sharedSourceImmutable/rotatedGeometry/UVcolorIndicesClip/refresh3/preparationCarriesAllBodyStripShadowTextures/leaveSlotsRelease/append120StableAtlasPayload/warmIO0 PASS\n";
}
void checkPageSprite(const Sprite& dot,float x,float y,bool current){
    assert(!dot.atlasBatch&&dot.strip.size()==4&&dot.center.x==x&&dot.center.y==y);
    assert(geometryWidth(&dot)==8&&geometryHeight(&dot)==8);
    const float u=current?.5f:0.f;assert(dot.strip[0].uv.x==u&&dot.strip[0].uv.y==0&&dot.strip[2].uv.x==u+.5f&&dot.strip[1].uv.y==1);
    for(const auto& v:dot.strip)assert(v.color.r==1&&v.color.g==1&&v.color.b==1&&v.color.a==1);
    assert(dot.blend.source==BlendFactor::one&&dot.blend.destination==BlendFactor::oneMinusSourceAlpha);
}
void pageIndicators(const char* root){
    Files files(root);Host host;PreparedTiles tiles("page-indicator-test");Screens ui(files,tiles,host);Random random;ScreensState state;state.screen=ScreenId::shop;
    // Synthetic category lengths verify the original quotient+1 boundary;
    // these are presentation fixtures, not claims about duplicate game stock.
    for(unsigned stock:{0u,23u,24u,25u,48u}){
        state.shopItems.resize(stock);const auto count=stock/24+1;
        for(unsigned page=0;page<count;++page){state.shopPage=int(page);ui.refresh(state);const auto frame=draw(ui);const auto dots=pageSprites(frame);assert(dots.size()==count);
            for(unsigned i=0;i<count;++i)checkPageSprite(*dots[i],188.f-6.f*float(count-1)+12.f*float(i),612.f,i==page);
            assert(positionOf(frame,"InvenPage.png")<positionOf(frame,"Btn_InvenPage.png"));
        }
    }
    state.shopItems.clear();state.shopPage=0;state.inventoryVisible=true;state.inventory.resize(200); // Capacity is not pages().
    for(int pages:{1,3,8})for(int current=0;current<pages;++current){
        state.inventoryPages=pages;state.inventoryPage=current;ui.refresh(state);auto frame=draw(ui);auto dots=pageSprites(frame);assert(dots.size()==std::size_t(pages+1));
        checkPageSprite(*dots[0],188,612,true); // Shop's single page remains even with empty stock.
        for(int i=0;i<pages;++i)checkPageSprite(*dots[std::size_t(i+1)],488.f-6.f*float(pages-1)+12.f*float(i),142.f,i==current);
        assert(positionOf(frame,"InvenBG.png")<positionOf(frame,"Inven_BtnItem.png"));
        state.screen=ScreenId::maintenance;ui.refresh(state);frame=draw(ui);dots=pageSprites(frame);assert(dots.size()==std::size_t(pages));
        for(int i=0;i<pages;++i)checkPageSprite(*dots[std::size_t(i)],491.f-6.f*float(pages-1)+12.f*float(i),285.f,i==current);
        assert(positionOf(frame,"InvenPage.png")<positionOf(frame,"Inven_BtnItem.png")); // WndInven's own direct draw precedes child buttons.
        state.screen=ScreenId::field;ui.refresh(state);frame=draw(ui);dots=pageSprites(frame);assert(dots.size()==std::size_t(pages));
        for(int i=0;i<pages;++i)checkPageSprite(*dots[std::size_t(i)],488.f-6.f*float(pages-1)+12.f*float(i),342.f,i==current);
        state.screen=ScreenId::shop;
    }
    state.screen=ScreenId::field;state.inventoryVisible=false;ui.refresh(state);assert(pageSprites(draw(ui)).empty());
    state.screen=ScreenId::maintenance;state.inventoryPages=0;ui.refresh(state);assert(pageSprites(draw(ui)).empty()); // Literal loop condition, no guessed dot.
    state.inventoryPages=3;state.inventoryPage=2;ui.refresh(state);const auto before=ui.buttons();const auto reads=files.reads,probes=files.probes,prepares=host.preparations;
    for(unsigned i=0;i<1000;++i){ui.tick(float(i),random);const auto frame=draw(ui);const auto dots=pageSprites(frame);assert(dots.size()==3);checkPageSprite(*dots[2],503,285,true);}
    assert(ui.buttons()==before&&files.reads==reads&&files.probes==probes&&host.preparations==prepares&&random.calls==0);
    std::cout<<"PAGE_INDICATORS original16x8split2x1/tile01/whiteAlpha1/ShopQuotientPlus1/InventoryActualPages1_3_8/ShopMaintenanceFieldPositions/order/noNewReceiver/warm1000IO0 PASS\n";
}
void fieldMessage(const char* root){
    Files files(root);Host host;PreparedTiles tiles("message-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::field;state.selectedSlot=0;ui.refresh(state,0);
    const auto draw=[&](){Frame frame;ui.append(frame,999);return frame;};
    const auto box=[&](const Frame& frame){return sprite(frame,"MessageBox.png");};
    const auto label=[&](const Frame& frame){return sprite(frame,"prepared:GuiGame.message");};
    const auto width=[](const Sprite* s){assert(s&&s->strip.size()==4);return s->strip[2].position.x-s->strip[0].position.x;};
    const auto height=[](const Sprite* s){assert(s&&s->strip.size()==4);return s->strip[0].position.y-s->strip[1].position.y;};
    assert(!box(draw()));state.message="Tap the Mach. to repair.";state.messageVisible=true;state.messageRevision=1;state.messageWorldTime=1.25;ui.refresh(state,100);
    auto frame=draw();assert(box(frame)&&label(frame));assert(box(frame)->center.x==405&&box(frame)->center.y==158&&width(box(frame))==512&&height(box(frame))==64&&box(frame)->strip[0].uv.y==0&&box(frame)->strip[1].uv.y==.5f);
    assert(label(frame)->size.width==512&&label(frame)->size.height==24&&label(frame)->center.x==405);
    const auto& request=host.requests.back();assert(request.fontName=="TrebuchetMS-Bold"&&request.fontSize==16&&request.alignment==TextAlign::center&&!request.retina);
    assert(std::count(host.ids.begin(),host.ids.end(),10)==1);const auto reads=files.reads,probes=files.probes,preparations=host.preparations;
    state.messageWorldTime=1.35;ui.updateHUD(state);ui.tick(1000,random);frame=draw();assert(box(frame)->center.y==166&&label(frame)->center.y==166&&random.calls==0);
    ui.refresh(state,1001);frame=draw();assert(box(frame)->center.y==166&&std::count(host.ids.begin(),host.ids.end(),10)==1); // unrelated HUD refresh keeps lifetime.
    state.messageRevision=2;state.messageWorldTime=1.4;ui.refresh(state,1002);assert(box(draw())->center.y==166&&std::count(host.ids.begin(),host.ids.end(),10)==2); // set does not reset Y.
    state.messageVisible=false;state.messageRevision=3;ui.refresh(state,1003);frame=draw();assert(box(frame)&&!label(frame)&&height(box(frame))==64);
    const double frozen=state.messageWorldTime;ui.tick(1004,random);frame=draw();assert(std::abs(height(box(frame))-64*.7725f)<.0001f); // world pause does not stop GUI folding.
    for(unsigned i=0;i<64;++i){state.messageWorldTime=frozen;ui.updateHUD(state);ui.tick(1005+float(i),random);}assert(!box(draw())&&random.calls==0);
    assert(files.reads==reads&&files.probes==probes); // Same message may prepare a wrapper, never reload PNG.
    state.messageVisible=true;state.messageAutoHide=true;state.messageRevision=4;state.messageWorldTime=20;ui.refresh(state,2000);
    state.messageWorldTime=30;ui.updateHUD(state);ui.tick(2001,random);assert(label(draw()));
    state.messageWorldTime=20+double(std::nextafter(10.f,11.f));ui.updateHUD(state);ui.tick(2002,random);assert(!label(draw())&&box(draw()));
    state.messageAutoHide=false;state.messageRevision=5;state.messageWorldTime=16777216.;ui.refresh(state,3000);
    state.messageWorldTime=16777216.25;ui.updateHUD(state);ui.tick(3001,random);frame=draw();assert(std::abs(box(frame)->center.y-161.9598f)<.001f&&label(frame)->center.y==162); // double current time survives.
    state.messageRevision=6;state.messageWorldTime=0;ui.refresh(state,4000);const auto beforeSentinel=draw();const float sentinelY=box(beforeSentinel)->center.y;
    state.messageWorldTime=1;ui.updateHUD(state);ui.tick(4001,random);frame=draw();assert(box(frame)->center.y==sentinelY&&label(frame)); // original msgTime0 sentinel.
    state.messageVisible=false;state.messageRevision=7;ui.refresh(state,4002);for(unsigned i=0;i<100;++i)ui.tick(4003+float(i),random);assert(box(draw()));
    assert(host.preparations>=preparations&&random.calls==0);
    std::cout<<"FIELD_MESSAGE original512x64/512x24/font/SFX10/floatDoubleClock/bounce/hide/pause/refresh/sentinel PASS\n";
}
}
namespace {
void hudMeasuredGeometry(const char* root){
    Files files(root);const auto bg=decodeImage(files.read("GameUI_BG.png"));assert(bg.width==768&&bg.height==256);
    const std::array<unsigned,hud::count> heights{124,138,133};
    for(unsigned group=0;group<hud::count;++group){const auto source=hud::backgroundSlices[group];unsigned highest=0;bool found=false;
        for(unsigned y=0;y<bg.height;++y)for(unsigned x=unsigned(source.x);x<unsigned(source.x+source.width);++x)if(bg.rgba[((bg.height-1-y)*bg.width+x)*4+3]){highest=std::max(highest,y);found=true;}
        assert(found&&highest+1==heights[group]&&hud::bounds[group].height==heights[group]);
    }
    // Compare actual decoded source pixels, not just three emitted rectangles.
    // Identity custom placement must partition every visible background sample
    // once and retain the original texture's source mapping. Include fractional
    // output boundaries: full-size sprites under rounded scissors overblend.
    for(const auto display:std::array<DisplayConfiguration,5>{DisplayConfiguration{},DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({857,393},{59,17,60,0}),DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({1024,1366},{0,25,0,0})}){
        const DisplayLayout layout(display);const auto base=layout.ui(DisplayRegion::bottom);hud::Configuration identity;
        for(unsigned group=0;group<hud::count;++group){const auto b=hud::bounds[group];identity.positions[group]=hud::draggedOrigin(group,layout,base.map({b.x,b.y}));const auto t=hud::transform(identity,group,layout);assert(near(t.scale,base.scale)&&near(t.offset.x,base.offset.x)&&near(t.offset.y,base.offset.y));}
        const auto content=display.enabled?display.content():ClipRect{0,0,768,1024};
        for(unsigned y=0;y<bg.height;++y)for(unsigned x=0;x<bg.width;++x){const auto p=base.map({float(x)+.5f,float(y)+.5f});if(p.x<content.x||p.x>=content.x+content.width||p.y<content.y||p.y>=content.y+content.height)continue;
            unsigned coverage=0;for(unsigned group=0;group<hud::count;++group){const auto c=hud::backgroundClip(identity,group,layout);coverage+=p.x>=c.x&&p.x<c.x+c.width&&p.y>=c.y&&p.y<c.y+c.height;}assert(coverage==1);
        }
        Host defaultHost,customHost;PreparedTiles defaultTiles("HUD-original"),customTiles("HUD-cropped");Screens original(files,defaultTiles,defaultHost),custom(files,customTiles,customHost);
        customHost.hudSettings[display.compact?"compact":"regular"]=hud::encode(identity);original.setDisplay(display);custom.setDisplay(display);
        ScreensState state;state.screen=ScreenId::field;state.fieldUIRevision=1;state.hasWorkingSlot=true;original.refresh(state);custom.refresh(state);
        const auto originalFrame=draw(original),identityFrame=draw(custom);const auto* identitySprite=sprite(identityFrame,"GameUI_BG.png");
        assert(identitySprite&&std::count_if(identityFrame.sprites.begin(),identityFrame.sprites.end(),[](const auto& s){return s.image=="GameUI_BG.png";})==1);
        const auto& full=*sprite(originalFrame,"GameUI_BG.png");assert(full.strip.size()==4&&identitySprite->strip.size()==4);
        for(unsigned i=0;i<4;++i){assert(identitySprite->strip[i].position.x==full.strip[i].position.x&&identitySprite->strip[i].position.y==full.strip[i].position.y&&identitySprite->strip[i].uv.x==full.strip[i].uv.x&&identitySprite->strip[i].uv.y==full.strip[i].uv.y);}
        // Editing still uses the movable exact-UV pieces. A saved unchanged
        // placement uses the original single primitive, avoiding the distinct
        // linear-filter rounding produced by unnecessary subdivisions.
        state.systemVisible=true;custom.refresh(state);assert(custom.beginHUDEdit());const auto customFrame=draw(custom);std::vector<const Sprite*> pieces;
        for(const auto& s:customFrame.sprites)if(s.image=="GameUI_BG.png")pieces.push_back(&s);
        assert(pieces.size()==hud::count);
        for(unsigned group=0;group<hud::count;++group){const auto& piece=*pieces[group];const auto slice=hud::backgroundSlices[group];assert(piece.strip.size()==4&&!piece.atlasBatch&&piece.textureSource==full.textureSource&&piece.image==full.image);
            const auto left=base.map({slice.x,0}),right=base.map({slice.x+slice.width,256});
            assert(piece.strip[0].position.x==left.x&&piece.strip[1].position.x==left.x&&piece.strip[2].position.x==right.x&&piece.strip[3].position.x==right.x);
            assert(piece.strip[0].position.y==right.y&&piece.strip[1].position.y==left.y);
            assert(piece.strip[0].uv.x==slice.x/768&&piece.strip[2].uv.x==(slice.x+slice.width)/768&&piece.strip[0].uv.y==full.strip[0].uv.y&&piece.strip[1].uv.y==full.strip[1].uv.y);
            if(group){assert(pieces[group-1]->strip[2].position.x==piece.strip[0].position.x&&pieces[group-1]->strip[2].uv.x==piece.strip[0].uv.x);}
            if(display.enabled){assert(piece.clip&&full.clip);const auto shared=hud::intersect(*piece.clip,*full.clip);assert(near(shared.x,full.clip->x)&&near(shared.y,full.clip->y)&&near(shared.width,full.clip->width)&&near(shared.height,full.clip->height));} // Expanded canvas clip retains the original visible pixels.
        }
    }
    const DisplayLayout phone(DisplayConfiguration::surface({912,420},{68,20,68,0}));hud::Configuration low,high;low.positions[hud::controls]=hud::Placement{.5f,0};high.positions[hud::controls]=hud::Placement{.5f,1};
    const auto a=hud::visualBounds(low,hud::controls,phone),b=hud::visualBounds(high,hud::controls,phone);assert(b.y-a.y>100); // Previously transparent padding allowed only8px.
    const auto t=hud::transform(high,hud::controls,phone);const auto c=hud::originalClip(high,hud::controls,phone);assert(c.x>=t.map({0,0}).x); // Hidden sliding gauge pixels stay clipped.
    std::cout<<"HUD_GEOMETRY decodedAlpha124_138_133/widgetsAndHits/sourcePartitionIdentityPixels/phoneVerticalTravel100 PASS\n";
}
void screenResourceOwners(const char* root){
    Files files(root);Host host;PreparedTiles legacy("external-compat-only");Screens ui(files,legacy,host);ScreensState state;
    ui.refresh(state);Frame prepared;ui.appendPreparation(prepared);assert(sprite(prepared,"Main_BG.png"));
    const auto titleReads=files.reads,titleSongs=unsigned(host.songs.size());ui.refresh(state);assert(files.reads==titleReads&&host.songs.size()==titleSongs); // Same original owner reuses its tiles.
    state.screen=ScreenId::base;ui.refresh(state);prepared={};ui.appendPreparation(prepared);assert(!sprite(prepared,"Main_BG.png"));
    state.screen=ScreenId::field;state.fieldUIRevision=1;state.hasWorkingSlot=true;state.enemyName="E_Tank_01";state.enemyNameRevision=1;ui.refresh(state);
    const auto* fieldTile=ui.preparedTile("GameUI_BG.png");assert(fieldTile);const auto fieldReads=files.reads;ui.refresh(state);assert(ui.preparedTile("GameUI_BG.png")==fieldTile&&files.reads==fieldReads);
    state.screen=ScreenId::base;ui.refresh(state);prepared={};ui.appendPreparation(prepared);assert(sprite(prepared,"GameUI_BG.png")&&sprite(prepared,"prepared:GuiGame.enemyName"));
    state.screen=ScreenId::field;ui.refresh(state);assert(ui.preparedTile("GameUI_BG.png")==fieldTile); // Hidden GuiGame retains its original owner.
    state.screen=ScreenId::title;ui.refresh(state);prepared={};ui.appendPreparation(prepared);assert(sprite(prepared,"Main_BG.png")&&sprite(prepared,"GameUI_BG.png"));
    state.fieldUIRevision=2;ui.refresh(state);prepared={};ui.appendPreparation(prepared);assert(!sprite(prepared,"GameUI_BG.png"));
    assert(legacy.all().empty()); // Former shared map no longer retains departed Title/Base resources.
    std::cout<<"SCREEN_RESOURCE_OWNERS sameScreenReuse/titleBaseRelease/hiddenGuiGameRetained/revisionRelease/preparedTextReachability PASS\n";
}
void phoneHUDBottomInput(const char* root){
    Files files(root);Host host;PreparedTiles tiles("phone-bottom-input");Screens ui(files,tiles,host);
    const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});const DisplayLayout layout(config);ui.setDisplay(config);
    ScreensState state;state.screen=ScreenId::field;state.fieldUIRevision=1;state.hasWorkingSlot=true;ui.refresh(state);
    const auto frame=draw(ui);const auto* background=sprite(frame,"GameUI_BG.png");assert(background&&background->strip.size()==4);
    float bottom=1.e9f;for(const auto& v:background->strip)bottom=std::min(bottom,v.position.y);assert(bottom==0);
    const auto key=ids(ui,6200).front();auto point=ui.buttonScreenPosition(key);point.y-=10;
    assert(point.y>0&&point.y<config.insets.bottom);
    assert(ui.input(PointerPhase::begin,point,9120,1));assert(ui.button(key)->capturedPointer()==9120);
    assert(ui.input(PointerPhase::end,point,9120,1.1f));assert(!ui.button(key)->capturedPointer());
    assert(!host.actions.empty()&&host.actions.back().button.key.id==6200);
    // A world drag crossing this button must not press it without a begin.
    const auto actions=host.actions.size();assert(!ui.input(PointerPhase::move,point,9121,2));assert(!ui.input(PointerPhase::end,point,9121,2.1f));assert(host.actions.size()==actions);
    // Full-canvas input is specific to compact fields, not every menu.
    state.screen=ScreenId::base;ui.refresh(state);assert(!ui.input(PointerPhase::begin,{-1,10},9122,3));
    hud::Configuration legacy;legacy.legacySafeArea=true;legacy.positions[hud::controls]=hud::Placement{.5f,0};
    const auto upgraded=hud::upgrade(legacy,layout);assert(hud::visualBounds(upgraded,hud::controls,layout).y==0);
    const DisplayLayout ipad(DisplayConfiguration::surface({1024,1366},{0,20,0,24}));
    const auto oldPad=hud::visualBounds(legacy,hud::controls,ipad),newPad=hud::visualBounds(hud::upgrade(legacy,ipad),hud::controls,ipad);assert(near(oldPad.x,newPad.x)&&near(oldPad.y,newPad.y));
    std::cout<<"PHONE_HUD_BOTTOM originalQuadY0/safeAreaButtonCapture/noDragButtonLeak/v1BottomEdge/ipadPreserved PASS\n";
}
void experimentalHUD(const char* root){
 for(const auto display:{DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){
    Files files(root);Host host;PreparedTiles tiles("experimental-test");Screens ui(files,tiles,host);ui.setDisplay(display);Random random;
    ScreensState state;state.screen=ScreenId::field;state.systemVisible=true;state.fieldUIRevision=1;state.hasWorkingSlot=true;state.hpBarWidth=270;state.hpBarFilled=120;state.spBarWidth=270;state.spBarFilled=90;state.experienceRate=.2f;state.enemyHpRate=.7f;state.enemyName="E_Tank_01";state.enemyNameRevision=1;state.radarMarks={{{0,0},0},{{160,80},1}};
    ui.refresh(state);assert(ui.beginHUDEdit());auto switcher=ids(ui,9104).front();auto point=ui.buttonScreenPosition(switcher);ui.input(PointerPhase::begin,point,201,1);ui.input(PointerPhase::end,point,201,1.1f);assert(ui.hudConfiguration().style==hud::Style::experimental);assert(ui.applyHUDEdit());assert(host.hudWrites==1);
    state.systemVisible=false;ui.refresh(state);auto frame=draw(ui);
    unsigned backgrounds=0;for(const auto& sp:frame.sprites)if(sp.image=="GameUI_BG.png"){
        ++backgrounds;assert(sp.strip.size()==4);const auto& tl=sp.strip[0];const auto& bl=sp.strip[1];assert(tl.position.y>bl.position.y&&tl.uv.y<bl.uv.y);
    }assert(backgrounds==7);assert(sprite(frame,"RaderMark.png")&&sprite(frame,"GameUI_GaugeEXP.png"));assert(ids(ui,1021).size()==1&&ids(ui,1022).size()==1&&ids(ui,1023).size()==1&&ids(ui,1025).size()==1&&ids(ui,1026).size()==1);
    const auto before=frame;const auto reads=files.reads,probes=files.probes,preparations=host.preparations;
    for(unsigned i=1;i<=100;++i){state.hpBarFilled=int(120+i);state.spBarFilled=90+i;state.boosterReady=i>50;state.experienceRate=.2f+.005f*i;state.radarMarks[1].worldPosition={160.f+i,80.f+i};state.level=int(i/20);state.credits=int(i*10);ui.updateHUD(state);ui.tick(float(i+2),random);frame=draw(ui);assert(sprite(frame,"RaderMark.png")&&sprite(frame,"GameUI_GaugeEXP.png"));}
    assert(files.reads==reads&&files.probes==probes&&host.preparations==preparations&&host.hudWrites==1);
    assert(sprite(frame,"GameUI_GaugeEXP.png")->strip[2].position.x-sprite(frame,"GameUI_GaugeEXP.png")->strip[0].position.x>sprite(before,"GameUI_GaugeEXP.png")->strip[2].position.x-sprite(before,"GameUI_GaugeEXP.png")->strip[0].position.x);
    auto lastMark=[](const Frame& f){const Sprite* result=nullptr;unsigned n=0;for(const auto& s:f.sprites)if(s.image=="RaderMark.png"){result=&s;++n;}assert(n==2);return result;};assert(lastMark(frame)->center.x>lastMark(before)->center.x&&lastMark(frame)->center.y>lastMark(before)->center.y);
    auto gauge=[](const Frame& f,float y){const AtlasQuad* result=nullptr;for(const auto& s:f.sprites)if(s.image=="GameUI_GaugeBack.png"&&s.atlasBatch)for(const auto& q:s.atlasBatch->quads)if(std::abs(q.vertices[0].position.y-y)<.1f)result=&q;assert(result);return result;};const auto gaugeTransform=DisplayLayout(display).ui(DisplayRegion::bottom);assert(gauge(frame,gaugeTransform.map({0,56}).y)->vertices[0].position.x>gauge(before,gaugeTransform.map({0,56}).y)->vertices[0].position.x);assert(gauge(frame,gaugeTransform.map({0,82}).y)->vertices[0].uv.y==.75f);
    // Functional source pixels have one owner; dragging a value must not leave another value's frame behind.
    auto bg=decodeImage(files.read("GameUI_BG.png"));std::vector<unsigned> owners(bg.width*bg.height);for(const auto& b:hud::functionalSlices)for(int y=int(b.y);y<int(b.y+b.height);++y)for(int x=int(b.x);x<int(b.x+b.width);++x)if(bg.rgba[(std::size_t(bg.height-1-y)*bg.width+x)*4+3])assert(++owners[std::size_t(y)*bg.width+x]==1);
    const auto hpClip=hud::originalClip(ui.hudConfiguration(),hud::hp,DisplayLayout(display));const auto maxPixel=gaugeTransform.map({278.5f,48});assert(maxPixel.x>=hpClip.x&&maxPixel.x<hpClip.x+hpClip.width);
    // Both editor handles select their own panel and preserve image aspect.
    state.systemVisible=true;ui.refresh(state);assert(ui.beginHUDEdit());auto editor=draw(ui);const auto* leftHandle=sprite(editor,"prepared:HUDLayout.panel.11");assert(leftHandle);const auto start=leftHandle->center;ui.input(PointerPhase::begin,start,271,120);ui.input(PointerPhase::move,{start.x+12,start.y+12},271,120.1f);ui.input(PointerPhase::end,{start.x+12,start.y+12},271,120.2f);assert(ui.hudConfiguration().experimentalPositions[hud::leftPanel]&&!ui.hudConfiguration().experimentalPositions[hud::rightPanel]);ui.cancelHUDEdit();assert(ui.beginHUDEdit());editor=draw(ui);const auto* rightHandle=sprite(editor,"prepared:HUDLayout.panel.12");assert(rightHandle);const auto rightStart=rightHandle->center;ui.input(PointerPhase::begin,rightStart,272,121);ui.input(PointerPhase::move,{rightStart.x-12,rightStart.y+12},272,121.1f);ui.input(PointerPhase::end,{rightStart.x-12,rightStart.y+12},272,121.2f);assert(ui.hudConfiguration().experimentalPositions[hud::rightPanel]&&!ui.hudConfiguration().experimentalPositions[hud::leftPanel]);ui.cancelHUDEdit();state.systemVisible=false;ui.refresh(state);
    auto numberUV=[](const Frame& f,std::string_view name){std::vector<float> result;for(const auto& sp:f.sprites)if(sp.image==name&&sp.atlasBatch)for(const auto& q:sp.atlasBatch->quads)result.push_back(q.vertices[0].uv.x);assert(!result.empty());return result;};assert(numberUV(before,"NumTile05.png")!=numberUV(frame,"NumTile05.png")&&numberUV(before,"NumSet01b.png")!=numberUV(frame,"NumSet01b.png"));
    // Each value gets its own transform; radar and all changing values keep updating after restart.
    auto config=ui.hudConfiguration();config.experimentalPositions[hud::hp]=hud::Placement{0,.4f};config.experimentalPositions[hud::radar]=hud::Placement{1,1};config.experimentalPositions[hud::targetInfo]=hud::Placement{.5f,.7f};host.hudSettings[display.compact?"compact":"regular"]=hud::encode(config);
    PreparedTiles movedTiles("experimental-moved");Screens moved(files,movedTiles,host);moved.setDisplay(display);moved.refresh(state);auto mf=draw(moved);assert(moved.hudConfiguration()==config);assert(sprite(mf,"RaderMark.png")&&sprite(mf,"prepared:GuiGame.enemyName"));
    for(const auto& sp:mf.sprites)if(sp.image=="GameUI_GaugeBack.png"&&sp.atlasBatch)for(const auto& q:sp.atlasBatch->quads)if(q.vertices[0].color.a){assert(sp.clip&&sp.clip->width>0&&sp.clip->height>0);}
    // Both drawers retain actual IDs through parent movement, relative offsets and all pages.
    for(int side=2;side<=3;++side){
        state.weaponChoiceType=side;state.weaponChoices.clear();for(int i=0;i<19;++i){ItemView entry;entry.position=30+i;entry.dataId="weapon"+std::to_string(i);entry.name=entry.dataId;state.weaponChoices.push_back(entry);}
        config.experimentalPositions[side==2?hud::leftWeapon:hud::rightWeapon]=hud::Placement{side==2?0.f:1.f,.5f};config.experimentalPositions[side==2?hud::leftPanel:hud::rightPanel]=hud::Placement{side==2?-1.f:1.f,side==2?1.f:-1.f};host.hudSettings[display.compact?"compact":"regular"]=hud::encode(config);
        PreparedTiles drawerTiles("drawer-"+std::to_string(side));Screens drawer(files,drawerTiles,host);drawer.setDisplay(display);drawer.refresh(state);assert(ids(drawer,1024).size()<=6&&ids(drawer,9301).size()==1);unsigned visited=0;
        for(unsigned page=0;page<4;++page){for(unsigned tick=0;tick<35;++tick)drawer.tick(float(200+tick),random);for(const auto& k:ids(drawer,1024)){assert(k.intData==30+int(visited));const auto point=drawer.buttonScreenPosition(k);assert(point.x>=0&&point.x<=display.canvas.width&&point.y>=0&&point.y<=display.canvas.height);const auto beforeEvents=host.actions.size();drawer.input(PointerPhase::begin,point,550+visited,400+float(visited));drawer.input(PointerPhase::end,point,550+visited,400.1f+float(visited));assert(host.actions.size()>beforeEvents&&host.actions.back().button.key==k);++visited;}
            if(visited<19){const auto next=drawer.buttonScreenPosition(ids(drawer,9301).front());drawer.input(PointerPhase::begin,next,650+page,450+float(page));drawer.input(PointerPhase::end,next,650+page,450.1f+float(page));}}
        assert(visited==19);
    }
    // Choosing classic retains its original positions, and cancel cannot change either saved style.
    const auto savedMoved=moved.hudConfiguration();state.weaponChoiceType=0;state.systemVisible=true;moved.refresh(state);assert(moved.beginHUDEdit());point=moved.buttonScreenPosition(ids(moved,9104).front());moved.input(PointerPhase::begin,point,299,300);moved.input(PointerPhase::end,point,299,300.1f);assert(moved.hudConfiguration().style==hud::Style::classic);moved.cancelHUDEdit();assert(moved.hudConfiguration()==savedMoved);
    assert(hud::decode(hud::encode(config))==config);
 }
 std::cout<<"EXPERIMENTAL_HUD HP_SP_EXP_level_credits/radarWorldMarks/warm100/noIO/noTextBake/11independentPieces/real19WeaponsPaged/profileSave/cancel/classicPreserved PASS\n";
}
ClipRect commandBounds(const Sprite& s){
    if(s.strip.empty())return {s.center.x-s.size.width*.5f,s.center.y-s.size.height*.5f,s.size.width,s.size.height};
    float x=s.strip.front().position.x,y=s.strip.front().position.y,r=x,t=y;
    for(const auto& v:s.strip){x=std::min(x,v.position.x);y=std::min(y,v.position.y);r=std::max(r,v.position.x);t=std::max(t,v.position.y);}return {x,y,r-x,t-y};
}
void sameGPUCommands(const Frame& a,const Frame& b){
    assert(a.width==b.width&&a.height==b.height&&a.sprites.size()==b.sprites.size());
    const auto point=[](Vec2 a,Vec2 b){assert(a.x==b.x&&a.y==b.y);};
    const auto color=[](Color a,Color b){assert(a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a);};
    for(unsigned i=0;i<a.sprites.size();++i){const auto& x=a.sprites[i];const auto& y=b.sprites[i];
        assert(x.image==y.image&&x.textureSource==y.textureSource&&x.clip==y.clip&&x.visualLayer==y.visualLayer&&x.tile==y.tile&&x.columns==y.columns&&x.rows==y.rows&&x.reverseY==y.reverseY&&x.angle==y.angle&&x.blend.source==y.blend.source&&x.blend.destination==y.blend.destination);
        point(x.center,y.center);assert(x.size.width==y.size.width&&x.size.height==y.size.height);color(x.color,y.color);assert(x.strip.size()==y.strip.size());
        for(unsigned j=0;j<x.strip.size();++j){point(x.strip[j].position,y.strip[j].position);point(x.strip[j].uv,y.strip[j].uv);color(x.strip[j].color,y.strip[j].color);}
        assert(bool(x.atlasBatch)==bool(y.atlasBatch));if(x.atlasBatch){const auto& p=*x.atlasBatch;const auto& q=*y.atlasBatch;assert(p.tileIdentity==q.tileIdentity&&p.image==q.image&&p.textureSource==q.textureSource&&p.blendType==q.blendType&&p.pixelFormat==q.pixelFormat&&p.capacity==q.capacity&&p.content.width==q.content.width&&p.content.height==q.content.height&&p.storage.width==q.storage.width&&p.storage.height==q.storage.height&&p.blend.source==q.blend.source&&p.blend.destination==q.blend.destination&&p.indices==q.indices&&p.clip==q.clip&&p.quads.size()==q.quads.size());
            for(unsigned j=0;j<p.quads.size();++j)for(unsigned k=0;k<4;++k){const auto& u=p.quads[j].vertices[k];const auto& v=q.quads[j].vertices[k];point(u.position,v.position);point(u.uv,v.uv);assert(u.color==v.color);}
        }
    }
}
void experimentalHUDExtents(const char* root){
    Files files(root);const auto quick=decodeImage(files.read("QuickBtn.png"));assert(quick.width==128&&quick.height==128);
    // Tile3 is the pressed last menu button. Its opaque right edge reaches
    //739+32=771; normal2 stops at768 and the60-point hit stops at769.
    unsigned normalRight=0,pressedRight=0;for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x){if(quick.rgba[((32+y)*128+x)*4+3])normalRight=std::max(normalRight,x+1);if(quick.rgba[((32+y)*128+64+x)*4+3])pressedRight=std::max(pressedRight,x+1);}
    assert(normalRight==61&&pressedRight==64&&hud::experimentalBounds[hud::menuThree].x+hud::experimentalBounds[hud::menuThree].width==771);
    for(const auto display:{DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({857,393},{59,17,60,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){
        const DisplayLayout layout(display);const auto area=hud::placementArea(layout);const auto profile=display.compact?"compact":"regular";
        for(const Vec2 corner:{Vec2{0,0},Vec2{1,0},Vec2{0,1},Vec2{1,1}}){
            Host host;hud::Configuration config;config.style=hud::Style::experimental;config.experimentalPositions[hud::menuThree]=hud::Placement{corner.x,corner.y};config.experimentalPositions[hud::radar]=hud::Placement{corner.x,corner.y};host.hudSettings[profile]=hud::encode(config);
            PreparedTiles tiles("dynamic-hud-extent");Screens ui(files,tiles,host);ui.setDisplay(display);ScreensState state;state.screen=ScreenId::field;state.fieldUIRevision=1;state.hasWorkingSlot=true;state.radarMarks={{{-660,-660},0},{{660,-660},1},{{-660,660},2},{{660,660},3}};ui.refresh(state);
            const auto key=ids(ui,1025).front();const auto button=ui.button(key);const auto p=ui.buttonScreenPosition(key);const auto t=hud::transform(config,hud::menuThree,layout);const auto hit=button->bounds();
            assert(p.x+hit.right*t.scale<=area.x+area.width+.001f&&p.y+hit.top*t.scale<=area.y+area.height+.001f);
            const Vec2 hitPoint{p.x+hit.right*t.scale-.05f,p.y};assert(ui.input(PointerPhase::begin,hitPoint,12345,1));assert(button->capturedPointer()==12345);
            Frame pressed;ui.append(pressed,1.1f);const Sprite* last=nullptr;for(const auto& image:pressed.sprites)if(image.image=="QuickBtn.png"&&image.tile==3)last=&image;assert(last&&near(last->size.width,64*t.scale)&&near(last->size.height,32*t.scale));
            float right=0;unsigned opaque=0;for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)if(quick.rgba[((32+y)*128+64+x)*4+3]){const Vec2 sample{last->center.x+(float(x)+.5f-32)*t.scale,last->center.y+(16-float(y)-.5f)*t.scale};assert(sample.x>=area.x&&sample.x<area.x+area.width&&sample.y>=area.y&&sample.y<area.y+area.height);assert(last->clip&&sample.x>=last->clip->x&&sample.x<last->clip->x+last->clip->width&&sample.y>=last->clip->y&&sample.y<last->clip->y+last->clip->height);right=std::max(right,sample.x);++opaque;}
            assert(opaque&&(!corner.x||right>area.x+area.width-t.scale));ui.input(PointerPhase::end,hitPoint,12345,1.2f);assert(host.actions.back().button.key==key&&host.actions.back().button.type==ButtonEventType::pop);
            const auto reads=files.reads,probes=files.probes,preparations=host.preparations;unsigned expectedHUDWrites=0;
            const auto verify=[&](float extent){state.radarMarks={{{-extent,-extent},0},{{extent,-extent},1},{{-extent,extent},2},{{extent,extent},3}};assert(ui.updateHUD(state));const auto frame=draw(ui);const float scale=layout.ui(DisplayRegion::bottom).scale;const Sprite* grid=nullptr;std::vector<const Sprite*> marks;
                for(const auto& image:frame.sprites){if(image.image=="GameUI_BG.png"&&near(geometryWidth(&image),80*scale)&&near(geometryHeight(&image),80*scale))grid=&image;if(image.image=="RaderMark.png")marks.push_back(&image);}assert(grid&&marks.size()==state.radarMarks.size());
                const auto raw=hud::transform(config,hud::radar,layout);const auto gridBox=commandBounds(*grid);const auto originalGrid=raw.map({369,0});const Vec2 delta{gridBox.x-originalGrid.x,gridBox.y-originalGrid.y};
                float lowX=originalGrid.x,lowY=originalGrid.y,highX=lowX+80*scale,highY=lowY+80*scale;
                for(unsigned i=0;i<marks.size();++i){const auto source=state.radarMarks[i];const auto expected=raw.map({409+source.worldPosition.x*.0625f,40+source.worldPosition.y*.0625f});const auto& mark=*marks[i];assert(near(mark.center.x,expected.x+delta.x)&&near(mark.center.y,expected.y+delta.y));assert(near(geometryWidth(&mark),8*scale)&&near(geometryHeight(&mark),8*scale));assert(mark.strip[0].uv.x==source.tile/8.f&&mark.strip[2].uv.x==(source.tile+1)/8.f);
                    lowX=std::min(lowX,expected.x-4*scale);highX=std::max(highX,expected.x+4*scale);lowY=std::min(lowY,expected.y-4*scale);highY=std::max(highY,expected.y+4*scale);const auto bounds=commandBounds(mark);assert(bounds.x>=area.x-.001f&&bounds.y>=area.y-.001f&&bounds.x+bounds.width<=area.x+area.width+.001f&&bounds.y+bounds.height<=area.y+area.height+.001f);
                }
                const Vec2 minimal{lowX<area.x?area.x-lowX:highX>area.x+area.width?area.x+area.width-highX:0,lowY<area.y?area.y-lowY:highY>area.y+area.height?area.y+area.height-highY:0};assert(near(delta.x,minimal.x)&&near(delta.y,minimal.y));assert(gridBox.x>=area.x-.001f&&gridBox.y>=area.y-.001f&&gridBox.x+gridBox.width<=area.x+area.width+.001f&&gridBox.y+gridBox.height<=area.y+area.height+.001f);assert(ui.hudConfiguration()==config&&host.hudWrites==expectedHUDWrites);
            };
            for(float extent:{660.f,580.f,512.f,640.f,660.f})verify(extent);assert(files.reads==reads&&files.probes==probes&&host.preparations==preparations);
            // This menu-edge fixture overlaps the original QuickFive on iPad.
            // Preserve its rejection separately from Radar-only persistence.
            if(!display.compact&&corner.x==1&&corner.y==0){
                state.systemVisible=true;ui.refresh(state);assert(ui.beginHUDEdit());const auto key=ids(ui,9103).front();const auto point=ui.buttonScreenPosition(key);ui.input(PointerPhase::begin,point,12348,2);ui.input(PointerPhase::end,point,12348,2.1f);assert(ui.hudEditing()&&ui.hudConfiguration()==config&&host.hudWrites==0&&host.hudSettings.at(profile)==hud::encode(config)&&ui.hudEditorError()=="버튼이 겹칩니다. HUD 조각을 옮겨 주세요.");ui.cancelHUDEdit();state.systemVisible=false;ui.refresh(state);verify(660);
            }
            {
            Host saveHost;auto saveConfig=config;saveConfig.experimentalPositions[hud::menuThree].reset();saveHost.hudSettings[profile]=hud::encode(saveConfig);PreparedTiles saveTiles("radar-apply-with-original-menu");Screens saveUI(files,saveTiles,saveHost);saveUI.setDisplay(display);state.systemVisible=true;saveUI.refresh(state);unsigned savedWrites=0;
            // A zero-distance drag must not feed the current inward visual
            // correction back into the requested corner, including on Apply.

            const auto gridOf=[&](const Frame& frame){const Sprite* grid=nullptr;for(const auto& image:frame.sprites)if(image.image=="GameUI_BG.png"&&near(geometryWidth(&image),80*t.scale)&&near(geometryHeight(&image),80*t.scale))grid=&image;assert(grid);return commandBounds(*grid);};
            const auto apply=[&]{const auto key=ids(saveUI,9103).front();const auto point=saveUI.buttonScreenPosition(key);const bool down=saveUI.input(PointerPhase::begin,point,12349,3),up=saveUI.input(PointerPhase::end,point,12349,3.1f);const unsigned expectedWrites=savedWrites+1;const bool sameConfig=saveUI.hudConfiguration()==saveConfig,sameStored=saveHost.hudSettings.at(profile)==hud::encode(saveConfig);if(saveUI.hudEditing()||!sameConfig||saveHost.hudWrites!=expectedWrites||!sameStored)std::cerr<<"RADAR_APPLY_FAILURE canvas="<<display.canvas.width<<","<<display.canvas.height<<" corner="<<corner.x<<","<<corner.y<<" down="<<down<<" up="<<up<<" editing="<<saveUI.hudEditing()<<" sameConfig="<<sameConfig<<" writes="<<saveHost.hudWrites<<" expectedWrites="<<expectedWrites<<" sameStored="<<sameStored<<" error="<<saveUI.hudEditorError()<<std::endl;assert(!saveUI.hudEditing());assert(sameConfig);assert(saveHost.hudWrites==expectedWrites);assert(sameStored);savedWrites=expectedWrites;};
            for(unsigned path=0;path<3;++path){assert(saveUI.beginHUDEdit());const auto edit=draw(saveUI);const auto grid=gridOf(edit);Vec2 grab{grid.x+grid.width*.5f,grid.y+grid.height*.5f};
                if(path==1){const auto* handle=hudSelectionLabel(edit);assert(handle);grab=handle->center;}else if(path==2)grab={display.canvas.width*.5f,display.canvas.height*.8f};
                saveUI.input(PointerPhase::begin,grab,12346,2);saveUI.input(PointerPhase::move,grab,12346,2.1f);saveUI.input(PointerPhase::end,grab,12346,2.2f);assert(saveUI.hudConfiguration()==saveConfig);apply();
            }
            // Grab the actually rendered grid, then move the requested anchor
            // by precisely10 points. Painting can still retain a boundary
            // correction; that correction must never accumulate in settings.
            assert(saveUI.beginHUDEdit());const auto editFrame=draw(saveUI);const auto gb=gridOf(editFrame);const Vec2 grab{gb.x+gb.width*.5f,gb.y+gb.height*.5f},delta{corner.x?-10.f:10.f,corner.y?-10.f:10.f},end{grab.x+delta.x,grab.y+delta.y};const auto rawBefore=hud::transform(saveConfig,hud::radar,layout).map({369,0});saveUI.input(PointerPhase::begin,grab,12347,4);saveUI.input(PointerPhase::move,end,12347,4.1f);saveUI.input(PointerPhase::end,end,12347,4.2f);const auto rawAfter=hud::transform(saveUI.hudConfiguration(),hud::radar,layout).map({369,0});assert(near(rawAfter.x-rawBefore.x,delta.x)&&near(rawAfter.y-rawBefore.y,delta.y));assert(saveUI.hudConfiguration().experimentalPositions[hud::radar]!=saveConfig.experimentalPositions[hud::radar]&&saveUI.hudConfiguration().experimentalPositions[hud::menuThree]==saveConfig.experimentalPositions[hud::menuThree]&&saveHost.hudWrites==savedWrites);saveUI.cancelHUDEdit();assert(saveUI.hudConfiguration()==saveConfig);state.systemVisible=false;saveUI.refresh(state);state.radarMarks={{{-512,-512},0},{{512,-512},1},{{-512,512},2},{{512,512},3}};assert(saveUI.updateHUD(state));const auto restored=gridOf(draw(saveUI));assert(near(restored.x,rawBefore.x)&&near(restored.y,rawBefore.y)&&saveUI.hudConfiguration()==saveConfig&&saveHost.hudSettings.at(profile)==hud::encode(saveConfig));
            }
            // Stored Experimental placements do not alter any Classic GPU
            // primitive, texture UV, blend, clip, atlas vertex, or state value.
            Host classicHost,storedHost;hud::Configuration classic,stored=config;stored.style=hud::Style::classic;storedHost.hudSettings[profile]=hud::encode(stored);PreparedTiles a("classic-control"),b("classic-stored-experimental");Screens classicUI(files,a,classicHost),storedUI(files,b,storedHost);classicUI.setDisplay(display);storedUI.setDisplay(display);classicUI.refresh(state);storedUI.refresh(state);sameGPUCommands(draw(classicUI),draw(storedUI));assert(storedUI.hudConfiguration()==stored&&classicUI.hudConfiguration()==classic&&storedHost.hudWrites==0);
        }
        // A default grid whose actual markers already fit gets exactly its
        // original transform, including after dynamic marker refreshes.
        Host host;hud::Configuration config;config.style=hud::Style::experimental;host.hudSettings[profile]=hud::encode(config);PreparedTiles tiles("radar-unchanged-default");Screens ui(files,tiles,host);ui.setDisplay(display);ScreensState state;state.screen=ScreenId::field;state.radarMarks={{{-512,-512},0},{{512,512},1}};ui.refresh(state);const auto frame=draw(ui);const auto* mark=sprite(frame,"RaderMark.png");const auto t=layout.ui(DisplayRegion::bottom);const auto expected=t.map({377,8});assert(mark&&mark->center.x==expected.x&&mark->center.y==expected.y&&ui.hudConfiguration()==config);
        const DisplayTransform raw{1,{12,34}};const auto oversized=hud::containBounds(raw,{0,0,area.width+1,area.height+1},area);assert(oversized.scale==raw.scale&&oversized.offset.x==raw.offset.x&&oversized.offset.y==raw.offset.y);
    }
    std::cout<<"EXPERIMENTAL_HUD_EXTENTS originalPressed3Alpha771/hit769/radarActual4Markers/gridUnion/minimalTranslation/fourCorners/menuOverlapRejected/radarOnlyZeroDragApply3Paths/exact10PointAnchor/markersReturnToAnchor/warmReuse/settingsUnchanged/classicCommandsExact PASS\n";
}
void experimentalHUDPlacementInputs(const char* root){
 for(const auto display:{DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){
    Files files(root);Host host;hud::Configuration settings;settings.style=hud::Style::experimental;
    settings.positions[hud::gauges]=hud::Placement{.1f,.9f};settings.positions[hud::combat]=hud::Placement{.4f,.4f};settings.positions[hud::controls]=hud::Placement{1,0};
    const auto profile=display.compact?"compact":"regular";host.hudSettings[profile]=hud::encode(settings);PreparedTiles tiles("experimental-inputs");Screens ui(files,tiles,host);ui.setDisplay(display);ScreensState state;state.screen=ScreenId::field;state.systemVisible=true;ui.refresh(state);const DisplayLayout layout(display);
    auto tap=[&](Screens& screen,unsigned id,unsigned pointer){const auto keys=ids(screen,id);assert(!keys.empty());const auto p=screen.buttonScreenPosition(keys.front());screen.input(PointerPhase::begin,p,pointer,1);screen.input(PointerPhase::end,p,pointer,1.1f);};
    // Selecting through the toolbar and dragging empty world space recovers every widget, including one under the toolbar.
    for(unsigned group=0;group<hud::experimentalCount;++group){assert(ui.beginHUDEdit());if(group)tap(ui,9105,300+group);
        for(unsigned corner=0;corner<2;++corner){const auto handle=draw(ui);const auto* label=hudSelectionLabel(handle);assert(label);const Vec2 start=label->center;Vec2 delta;
            if(group<hud::leftPanel){const auto b=hud::visualBounds(ui.hudConfiguration(),group,layout);delta={float(corner)*(display.canvas.width-b.width)-b.x,float(corner)*(display.canvas.height-b.height)-b.y};}
            else delta={display.canvas.width*(corner?2.f:-1.f),display.canvas.height*(corner?2.f:-1.f)};
            const Vec2 end{start.x+delta.x,start.y+delta.y};ui.input(PointerPhase::begin,start,400+group,2);ui.input(PointerPhase::move,end,400+group,2.1f);ui.input(PointerPhase::end,end,400+group,2.2f);
            const auto actual=ui.hudConfiguration().experimentalPositions[group];assert(actual);const float want=group>=hud::leftPanel?(corner?1.f:-1.f):float(corner);if(!near(actual->x,want)||!near(actual->y,want))std::cerr<<"HUD_EDGE_FAIL profile="<<profile<<" group="<<group<<" corner="<<corner<<" actual="<<actual->x<<","<<actual->y<<" want="<<want<<std::endl;assert(near(actual->x,want)&&near(actual->y,want));
        }
        ui.cancelHUDEdit();assert(ui.hudConfiguration()==settings);
    }
    // Canceling Experimental after selecting13, then entering a Classic profile, must never index the Classic names out of range.
    assert(ui.beginHUDEdit());tap(ui,9104,500);assert(ui.hudConfiguration().style==hud::Style::classic);assert(ui.applyHUDEdit());ui.setDisplay(display.compact?DisplayConfiguration::surface({1024,1366},{0,20,0,24}):DisplayConfiguration::surface({912,420},{68,20,68,0}));assert(ui.beginHUDEdit());auto safeFrame=draw(ui);assert(hudSelectionLabel(safeFrame));ui.cancelHUDEdit();ui.setDisplay(display);
    // Save both complete layouts; Classic Apply and restart retain every experimental coordinate.
    settings.style=hud::Style::experimental;for(unsigned g=0;g<hud::leftPanel;++g){const auto b=hud::groupBounds(settings,g);settings.experimentalPositions[g]=hud::draggedOrigin(g,layout,hud::baseTransform(settings,g,layout).map({b.x,b.y}),settings);}settings.experimentalPositions[hud::leftPanel]=hud::Placement{0,0};settings.experimentalPositions[hud::rightPanel]=hud::Placement{0,0};host.hudSettings[profile]=hud::encode(settings);
    PreparedTiles bothTiles("both-layouts");Screens both(files,bothTiles,host);both.setDisplay(display);both.refresh(state);assert(both.beginHUDEdit());tap(both,9104,502);assert(both.applyHUDEdit());auto classic=both.hudConfiguration();assert(classic.style==hud::Style::classic&&classic.positions==settings.positions&&classic.experimentalPositions==settings.experimentalPositions);
    PreparedTiles restartedTiles("both-restarted");Screens restarted(files,restartedTiles,host);restarted.setDisplay(display);restarted.refresh(state);assert(restarted.hudConfiguration()==classic);assert(restarted.beginHUDEdit());tap(restarted,9104,503);assert(restarted.applyHUDEdit());assert(restarted.hudConfiguration()==settings);
 }
 std::cout<<"EXPERIMENTAL_INPUT all13Selected/edgeDrags/coveredWidgetRecovery/profileCrossNoOOB/classicCustomAnd13CoordinatesAppliedRestarted PASS\n";
}
void movableHUD(const char* root){
    Files files(root);Host host;PreparedTiles tiles("hud-layout-test");Screens ui(files,tiles,host);
    const auto display=DisplayConfiguration::surface({912,420},{68,20,68,0});ui.setDisplay(display);
    ScreensState state;state.screen=ScreenId::field;state.systemVisible=true;state.fieldUIRevision=1;state.hasWorkingSlot=true;
    ui.refresh(state);const auto menu=ids(ui,1021).front();const auto original=ui.buttonScreenPosition(menu);const auto defaultFrame=draw(ui);
    assert(host.hudReads==1&&host.hudWrites==0&&ids(ui,9100).size()==1);
    const auto entry=ui.buttonScreenPosition(ids(ui,9100).front());ui.input(PointerPhase::begin,entry,179,.5f);ui.input(PointerPhase::end,entry,179,.6f);assert(ui.hudEditing());const auto controls=hud::visualBounds(ui.hudConfiguration(),hud::controls,DisplayLayout(display));
    const Vec2 start{controls.x+controls.width*.5f,controls.y+controls.height*.5f};
    const auto actions=host.actions.size();const auto reads=files.reads,probes=files.probes,preparations=host.preparations;
    assert(ui.input(PointerPhase::begin,start,180,1));assert(ui.input(PointerPhase::move,{start.x+45,start.y+25},180,1.1f));assert(ui.input(PointerPhase::end,{start.x+45,start.y+25},180,1.2f));
    const auto moved=ui.buttonScreenPosition(menu);assert(moved.x!=original.x&&moved.y!=original.y&&host.hudWrites==0&&host.actions.size()==actions);
    const auto movedFrame=draw(ui);bool aligned=false;unsigned backgroundPieces=0;for(const auto& image:movedFrame.sprites){if(image.image=="QuickBtn.png"&&image.tile==4)aligned=near(image.center.x,moved.x)&&near(image.center.y,moved.y);if(image.image=="GameUI_BG.png"){++backgroundPieces;assert(image.clip&&image.textureSource.image.empty());}}assert(aligned&&backgroundPieces==hud::count);
    for(unsigned i=0;i<60;++i){auto f=draw(ui);assert(!f.sprites.empty());}assert(files.reads==reads&&files.probes==probes&&host.preparations==preparations);
    ui.cancelHUDEdit();assert(!ui.hudEditing()&&ui.buttonScreenPosition(menu).x==original.x&&host.hudWrites==0);
    assert(ui.beginHUDEdit());assert(ui.input(PointerPhase::begin,start,181,2));ui.input(PointerPhase::move,{start.x+45,start.y+25},181,2.1f);ui.input(PointerPhase::end,{start.x+45,start.y+25},181,2.2f);
    host.hudWriteSucceeds=false;assert(!ui.applyHUDEdit()&&ui.hudEditing()&&!ui.hudEditorError().empty()&&host.hudWrites==1);
    host.hudWriteSucceeds=true;assert(ui.applyHUDEdit()&&!ui.hudEditing()&&host.hudWrites==2);const auto saved=ui.hudConfiguration();
    assert(hud::decode(host.hudSettings.at("compact"))==saved&&saved.positions[hud::controls]);
    PreparedTiles restoredTiles("hud-restored");Screens restored(files,restoredTiles,host);restored.setDisplay(display);restored.refresh(state);
    assert(restored.hudConfiguration()==saved&&restored.buttonScreenPosition(ids(restored,1021).front()).x==moved.x);
    state.systemVisible=false;restored.refresh(state);const auto beforeActions=host.actions.size();const auto point=restored.buttonScreenPosition(ids(restored,1021).front());
    restored.input(PointerPhase::begin,point,182,3);restored.input(PointerPhase::end,point,182,3.1f);assert(host.actions.size()>beforeActions&&host.actions.back().button.key.id==1021);
    assert(!restored.beginHUDEdit());state.systemVisible=true;restored.refresh(state);assert(restored.beginHUDEdit());restored.resetHUDEdit();assert(!restored.hudConfiguration().customized());restored.cancelHUDEdit();assert(restored.hudConfiguration()==saved);
    const auto writes=host.hudWrites;restored.setDisplay(DisplayConfiguration::surface({1024,1366},{0,25,0,0}));assert(!restored.hudConfiguration().customized()&&host.hudWrites==writes);
    restored.setDisplay(display);assert(restored.hudConfiguration()==saved);
    hud::Configuration invalid;invalid.positions[0]=hud::Placement{std::numeric_limits<float>::quiet_NaN(),0};bool rejected=false;try{(void)hud::encode(invalid);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    for(std::string bad:{"{\"version\":3,\"positions\":[null,null,null]}","{\"version\":1,\"positions\":[[2,0],null,null]}","{\"version\":1,\"positions\":[null,null,null]}extra"}){rejected=false;try{(void)hud::decode(Bytes(bad.begin(),bad.end()));}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
    // Existing v1 sidecars keep their actual visual origin after migration.
    hud::Configuration old;old.legacySafeArea=true;old.positions[hud::controls]=hud::Placement{.7f,.8f};const DisplayLayout layout(display);
    const auto legacy=hud::visualBounds(old,hud::controls,layout);const auto upgraded=hud::upgrade(hud::decode(hud::encode(old)),layout);const auto migrated=hud::visualBounds(upgraded,hud::controls,layout);assert(!upgraded.legacySafeArea&&near(legacy.x,migrated.x)&&near(legacy.y,migrated.y));
    for(const Vec2 edge:{Vec2{0,0},Vec2{1,0},Vec2{0,1},Vec2{1,1}}){hud::Configuration corner;corner.positions[hud::controls]=hud::Placement{edge.x,edge.y};Host edgeHost;edgeHost.hudSettings["compact"]=hud::encode(corner);PreparedTiles edgeTiles("edge");Screens edgeUI(files,edgeTiles,edgeHost);edgeUI.setDisplay(display);edgeUI.refresh(state);const auto b=hud::visualBounds(edgeUI.hudConfiguration(),hud::controls,layout);assert(near(b.x,edge.x*(display.canvas.width-b.width))&&near(b.y,edge.y*(display.canvas.height-b.height)));const auto clip=hud::originalClip(corner,hud::controls,layout);assert(b.x>=clip.x-.01f&&b.y>=clip.y-.01f&&b.x+b.width<=clip.x+clip.width+.01f&&b.y+b.height<=clip.y+clip.height+.01f);}
    std::cout<<"HUD_LAYOUT originalGPUgroups/drawHitTransform/dragNoWrites/applyFailureRetry/cancel/default/profile/restart/warm60 PASS\n";
}
}
int main(int argc,char** argv){
    ItemDescriptionValues desc;desc.type=1;desc.param1=30;desc.price=100;
    assert(Screens::itemDescription(desc,1)=="Repair Kit\n\n30 point Repair.\n\nPrice : 100cr");
    desc.param2=1;assert(Screens::itemDescription(desc,2)=="Repair Kit\n\n30% Repair.\n\nSellPrice : 50cr");
    desc.param2=2;assert(!Screens::itemDescription(desc,0));
    desc={};desc.type=11;desc.param1=101;desc.param2=13;desc.bodyHP=99;desc.bodyDefense=16;
    assert(Screens::itemDescription(desc,0)=="Body Parts\n\nHP : 101 (+2)\nDef : 13 (-3)\n\n");
    WeaponInfo weapon;weapon.attackPoint=9;weapon.damage=10;weapon.shotCount=2;weapon.shotDelay=.25f;weapon.reloadDelay=1.5f;
    desc={};desc.type=10;desc.weapon=&weapon;desc.leftAttack=4;desc.rightAttack=12;
    assert(Screens::itemDescription(desc,0)=="Machine-gun\n\nAtk 9 (L:+5 R:-3)\n10 / 2 / 0.25s / 1.5s\n\n");
    weapon.kind=WeaponKind::sniper;assert(!Screens::itemDescription(desc,0));
    assert(Screens::missionDescription("Original comment",{{2,"E_Tank_01",3,5},{3,"Repair",1,2}},false)=="Original comment\n\nMission Objects\n  - destroy E_Tank_01 x5\n  - Repair x2\n");
    assert(Screens::missionDescription("Original comment",{{2,"E_Tank_01",3,5}},true)=="Original comment\n\nMission Objects\n  - destroy E_Tank_01 (3/5)\n");
    assert(!Screens::missionDescription("Original",{{1,"unresolved",0,1}},false));
    if(argc<2)return 77;
    experimentalHUD(argv[1]);experimentalHUDExtents(argv[1]);experimentalHUDPlacementInputs(argv[1]);phoneHUDBottomInput(argv[1]);hudMeasuredGeometry(argv[1]);screenResourceOwners(argv[1]);movableHUD(argv[1]);
    slotPreviewTranslation(argv[1]);
    fieldMessage(argv[1]);
    shopDetails(argv[1]);inventoryMaintenanceDetails(argv[1]);fieldNameRange(argv[1]);fieldCredits(argv[1]);radarMarks(argv[1]);fieldPlaceName(argv[1]);pageIndicators(argv[1]);
    Files files(argv[1]);Host host;PreparedTiles tiles("screens-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;ui.refresh(state,0);assert(ui.buttons().empty()&&host.songs.back()=="STAGE1.wav");
    ui.input(PointerPhase::end,{100,100},1,1);assert(host.actions.back().titleTap&&!host.actions.back().titleReady);
    ui.input(PointerPhase::end,{100,100},2,2);assert(host.actions.back().titleReady);
    state.screen=ScreenId::slots;ui.refresh(state,2);assert(ui.buttons().size()==6&&ids(ui,1009).size()==3);
    const auto slot=ids(ui,1009).front();const auto before=ui.button(slot);
    assert(before->position.x==384&&before->position.y==760);
    host.callback=[&](const ScreenAction& a){if(a.button.type==ButtonEventType::push){state.selectedSlot=a.button.key.intData;ui.refresh(state,3);}};
    ui.input(PointerPhase::begin,before->position,7,3);assert(ui.button(slot)==before&&before->capturedPointer()==7);
    ui.input(PointerPhase::end,before->position,7,4);assert(!before->capturedPointer());host.callback={};
    unsigned baseButtons=0;state.screen=ScreenId::base;
    for(unsigned i=1;i<=6;++i){state.baseId="Base00"+std::to_string(i);ui.refresh(state,5);baseButtons+=unsigned(ui.buttons().size());Frame f;ui.append(f,5);assert(!f.sprites.empty());}
    assert(baseButtons==27);
    state={};state.screen=ScreenId::field;state.hpBarWidth=100;state.hpBarFilled=80;state.spBarWidth=120;state.spBarFilled=60;state.fieldUIRevision=1;state.hasWorkingSlot=true;state.fieldWorldTime=.25;
    ui.refresh(state,6);assert(ui.buttons().size()==10);const auto stable=ui.button(ids(ui,1021).front());
    const auto reads=files.reads,probes=files.probes,prepares=host.preparations;
    for(unsigned i=0;i<100;++i){state.hpBarFilled=int(i);state.experienceRate=float(i)/100;state.enemyHpRate=.5f;state.credits=int(i);state.outOfRangeLeft=true;
        assert(ui.updateHUD(state));ui.tick(float(i+7),random);Frame f;ui.append(f,float(i+7));assert(sprite(f,"EnemyGauge.png")&&sprite(f,"OutofRange.png"));}
    assert(files.reads==reads&&files.probes==probes&&host.preparations==prepares&&ui.button(ids(ui,1021).front())==stable);
    state.weaponChoiceType=2;ItemView a;a.dataId="original-item-A";a.position=3;state.weaponChoices={a};ui.refresh(state,108);
    const auto keyA=ids(ui,1024).front();assert(keyA.intData==3&&keyA.dataId==a.dataId);auto first=ui.button(keyA);assert(first->position.x==405);
    ui.tick(109,random);assert(first->position.x==381&&first->position.y>90);
    state.weaponChoices[0].dataId="original-item-B";ui.refresh(state,110);assert(!ui.button(keyA));
    state.weaponChoiceType=0;state.inventoryVisible=true;ui.refresh(state,111);assert(ids(ui,6000).size()==1&&ids(ui,6100).size()==1&&ids(ui,6092).size()==1);
    state.inventoryVisible=false;state.airliftVisible=true;state.airlifts[0].dataId="Base001";state.airlifts[0].name="Original base";
    ui.refresh(state,112);assert(ids(ui,4001).empty()&&ids(ui,4002).size()==1);state.selectedAirlift=0;ui.refresh(state,113);assert(ids(ui,4001).size()==1);
    state={};state.screen=ScreenId::shop;state.shopItems={a};ui.refresh(state,114);assert(ids(ui,2104).size()==5&&ids(ui,2200).size()==1);
    state.screen=ScreenId::maintenance;state.equipmentStats={7,8,100,20,2,3,30,80,1};
    ui.refresh(state,115);assert(ids(ui,6000).size()==1&&ids(ui,6302).size()==1);
    Frame maintenanceFrame;ui.append(maintenanceFrame,115);const auto* statGlyphs=sprite(maintenanceFrame,"NumSet01b.png");
    assert(statGlyphs&&statGlyphs->atlasBatch&&statGlyphs->atlasBatch->quads.size()==19); // 7-8 / 100-20-5 / 30-80-10
    unsigned separators=0;for(const auto& q:statGlyphs->atlasBatch->quads)if(q.vertices[0].uv.x==13.f/16)++separators;
    assert(separators==5);
    state.screen=ScreenId::command;ItemView m;m.dataId="M001010";m.name="Original mission";state.availableMissions={m};state.selectedMission=0;ui.refresh(state,116);
    assert(ids(ui,5100).front().dataId==m.dataId&&ids(ui,5004).front().dataId==m.dataId);
    const auto coverage=Screens::coverage();assert(std::any_of(coverage.begin(),coverage.end(),[](const auto& c){return !c.implemented;}));
    std::cout<<"SCREENS original27BaseButtons, slot callback refresh/capture, raw item identities, original formats, HUD100ticks warmIO0/noTextBake PASS\n";
}
