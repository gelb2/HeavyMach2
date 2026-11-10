#include "Core/UI/Screens.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
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
    assert(t->size.width==160&&near(t->size.height,24*.505f)&&near(geometryWidth(large),tiles.lookup("IconB10011.png")->pixels.width)&&large->angle==0);
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
        for(unsigned drawIndex=0;drawIndex<120;++drawIndex)compare(draw(ui),&stable);
        assert(files.reads==reads&&files.probes==probes&&host.preparations==preparations);
        assert(local.sprites[2].atlasBatch.get()==batch.get());for(unsigned i=0;i<4;++i){const auto& v=batch->quads[0].vertices[i];assert(v.position.x==points[i].x&&v.position.y==points[i].y);}
        assert(local.sprites[0].center.x==body.center.x&&local.sprites[1].strip[0].position.x==strip.strip[0].position.x);
    }
    std::cout<<"SLOT_PREVIEW translation3/sharedSourceImmutable/rotatedGeometry/UVcolorIndicesClip/refresh3/append120StableAtlasPayload/warmIO0 PASS\n";
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
    slotPreviewTranslation(argv[1]);
    fieldMessage(argv[1]);
    shopDetails(argv[1]);inventoryMaintenanceDetails(argv[1]);fieldNameRange(argv[1]);pageIndicators(argv[1]);
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
