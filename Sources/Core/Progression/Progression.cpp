#include "Core/Progression/Progression.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace hm2::progression {
std::string_view originalSoundName(unsigned id){
    if(id>=22)return {}; // Original sfxID0x14ab8 returns nil for >=22.
    return audio::soundFilename(id);
}
namespace {
void range(std::size_t p,std::size_t n){if(p>SlotView::size||n>SlotView::size-p)throw std::out_of_range("Original slot field outside 0x2360 bytes");}
std::int16_t shortValue(int v){return std::bit_cast<std::int16_t>(std::uint16_t(v));}
int addInt(int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b));}
std::vector<std::string> words(std::string_view s){
    std::vector<std::string> out;std::size_t p=0;
    while(p<s.size()){while(p<s.size()&&s[p]==' ')++p;const auto start=p;while(p<s.size()&&s[p]!=' ')++p;if(start!=p)out.emplace_back(s.substr(start,p-start));}return out;
}
int number(const std::vector<std::string>& w,std::size_t p){return p<w.size()?std::atoi(w[p].c_str()):0;}
std::string word(const std::vector<std::string>& w,std::size_t p){return p<w.size()?w[p]:"";}
Action action(std::string_view text){
    const auto w=words(text);Action a;if(w.empty())return a;
    constexpr std::string_view names[]={"giveMoney","giveExp","giveItem","takeItem","warpTo","setItem","setEnemy","openGate"};
    for(int i=0;i<8;++i)if(w[0]==names[i])a.type=i;
    if(a.type<2)a.value=number(w,1);
    else if(a.type<5){a.item=word(w,1);a.value=number(w,2);}
    else if(a.type==5){a.item=word(w,1);a.place=word(w,2);a.x=number(w,3);a.y=number(w,4);}
    else if(a.type==6){a.enemy=word(w,1);a.place=word(w,2);}
    else a.place=word(w,1);return a;
}
Requirement requirement(std::string_view text){
    const auto w=words(text);Requirement r;if(w.empty())return r;
    if(w[0]=="arrive")r.type=1;else if(w[0]=="kill")r.type=2;else if(w[0]=="item")r.type=3;
    r.object=word(w,1);r.count=number(w,2);r.place=word(w,3);return r;
}
std::optional<std::size_t> index(const Definitions& defs,std::string_view id){
    const auto& all=defs.missions();for(std::size_t i=0;i<all.size();++i)if(all[i].id==id)return i;return {};
}
}
SlotView::SlotView(std::span<std::uint8_t> bytes):bytes_(bytes){if(bytes.size()!=size)throw std::runtime_error("Expected original HD 0x2360-byte slot");}
std::uint8_t SlotView::byte(std::size_t p)const{range(p,1);return bytes_[p];}
void SlotView::byte(std::size_t p,std::uint8_t v){range(p,1);bytes_[p]=v;}
std::uint16_t SlotView::u16(std::size_t p)const{range(p,2);return std::uint16_t(bytes_[p])|(std::uint16_t(bytes_[p+1])<<8);}
void SlotView::u16(std::size_t p,std::uint16_t v){range(p,2);for(int i=0;i<2;++i)bytes_[p+i]=std::uint8_t(v>>(8*i));}
std::uint32_t SlotView::u32(std::size_t p)const{range(p,4);return u16(p)|(std::uint32_t(u16(p+2))<<16);}
void SlotView::u32(std::size_t p,std::uint32_t v){range(p,4);for(int i=0;i<4;++i)bytes_[p+i]=std::uint8_t(v>>(8*i));}
std::string SlotView::string(std::size_t p,std::size_t n)const{range(p,n);std::string out;for(std::size_t i=0;i<n&&bytes_[p+i];++i)out.push_back(char(bytes_[p+i]));return out;}
void SlotView::string(std::size_t p,std::size_t n,std::string_view text){range(p,n);if(text.size()>=n)throw std::runtime_error("Original slot string exceeds field");for(std::size_t i=0;i<text.size();++i)bytes_[p+i]=std::uint8_t(text[i]);bytes_[p+text.size()]=0;}
int SlotView::level()const{return std::bit_cast<std::int32_t>(u32(0x6c));}void SlotView::level(int v){u32(0x6c,std::uint32_t(v));}
std::uint32_t SlotView::experience()const{return u32(0x70);}void SlotView::experience(std::uint32_t v){u32(0x70,v);}
std::uint32_t SlotView::credit()const{return u32(0x78);}void SlotView::credit(std::uint32_t v){u32(0x78,v);}
int SlotView::hp()const{return std::bit_cast<std::int32_t>(u32(0x7c));}void SlotView::hp(int v){u32(0x7c,std::uint32_t(v));}
int SlotView::maxHP()const{return std::bit_cast<std::int32_t>(u32(0x80));}void SlotView::maxHP(int v){u32(0x80,std::uint32_t(v));}
float SlotView::experienceGauge()const{return std::bit_cast<float>(u32(0x84));}void SlotView::experienceGauge(float v){u32(0x84,std::bit_cast<std::uint32_t>(v));}
std::string Item::iconImage()const{char name[64];std::snprintf(name,sizeof(name),"Icon%05d.png",icon);return name;}
Definitions Definitions::parse(DataTable items,DataTable regeneration,DataTable missions,const MissionLoader& load,std::string_view locale){
    Definitions out;
    constexpr std::pair<std::string_view,int> types[]={{"Coin",0},{"AidKit",1},{"SpAtk",2},{"Buff",3},{"Bot",4},{"Etc",5},{"Ticket",6},{"PtWpn",10},{"PtBody",11},{"PtBtm",12},{"PtArmor",13},{"PtEquip",14}};
    for(std::size_t row=0;row<items.rowCount();++row){
        Item i;i.code=items.value(row,"Code").value_or("");i.name=items.value(row,"Name").value_or("");i.stringParameter=items.value(row,"strParam").value_or("");
        i.icon=items.integer(row,"Icon");i.level=items.integer(row,"Lv");i.price=items.integer(row,"Price");i.param1=items.real(row,"Param1");i.param2=items.real(row,"Param2");i.param3=items.real(row,"Param3");
        const auto type=items.value(row,"Type");for(const auto& [name,value]:types)if(type==name)i.type=value;
        const auto attr=items.value(row,"Attr").value_or("");i.buy=attr.find('B')!=attr.npos;i.sell=attr.find('S')!=attr.npos;i.quick=attr.find('Q')!=attr.npos;i.countable=attr.find('C')!=attr.npos;
        if(i.code.empty()||i.code.size()>5)throw std::runtime_error("Original ItemInfo code is invalid");out.items_.insert_or_assign(i.code,std::move(i));
    }
    for(std::size_t row=0;row<regeneration.rowCount();++row){
        SpawnDefinition s;s.minimum=regeneration.integer(row,"Min");s.maximum=regeneration.integer(row,"Max");
        const auto w=words(regeneration.value(row,"Regen").value_or(""));
        for(std::size_t i=0;i+1<w.size();i+=2){const int weight=std::atoi(w[i+1].c_str());if(weight<0||s.totalWeight>std::numeric_limits<int>::max()-weight)throw std::runtime_error("Invalid original ItemRegen weight");s.totalWeight+=weight;s.cumulative.emplace_back(w[i],s.totalWeight);}
        if(s.minimum<0||s.maximum<s.minimum||s.maximum==std::numeric_limits<int>::max())throw std::runtime_error("Invalid original ItemRegen count");
        out.spawns_.insert_or_assign(std::string(regeneration.value(row,"Name").value_or("")),std::move(s));
    }
    for(std::size_t row=0;row<missions.rowCount();++row){
        MissionDefinition d;d.id=missions.value(row,"MissionId").value_or("");d.owner=missions.value(row,"Owner").value_or("");d.completePlace=missions.value(row,"Compl").value_or("");d.previous=missions.value(row,"PrevMission").value_or("");d.level=missions.integer(row,"Lv");
        if(d.id.empty()||d.id.size()>7)throw std::runtime_error("Original mission ID exceeds its8-byte save field");
        const auto table=load(d.id+"_"+std::string(locale)+".tble");
        for(std::size_t n=0;n<table.rowCount();++n){
            const auto key=table.value(n,"Category");const auto v=table.value(n,"Value").value_or("");
            if(key=="Name")d.name=v;else if(key=="AcptComment")d.comment=v;
            else if(key=="Require")d.requirements.push_back(requirement(v));else if(key=="AcceptAct")d.acceptActions.push_back(action(v));else if(key=="CompleteAct")d.completeActions.push_back(action(v));
            // CompleteInf/StorySet are no-op methods0x1a780/0x1a784 in HD.
        }
        if(d.requirements.size()>10)throw std::runtime_error("Original mission has more than10 saved requirement bytes");out.missions_.push_back(std::move(d));
    }
    return out;
}
const Item* Definitions::findItem(std::string_view code)const{const auto i=items_.find(code);return i==items_.end()?nullptr:&i->second;}
const SpawnDefinition* Definitions::findSpawn(std::string_view name)const{const auto i=spawns_.find(name);return i==spawns_.end()?nullptr:&i->second;}
ExperienceBounds Experience::bounds(int level){
    if(level<0||level>99)throw std::runtime_error("Invalid original player level");
    ExperienceBounds b;float factor=1.5f;
    for(int i=0;i<=level;++i){
        b.previous=b.next;const double amount=i<=40?std::pow(double(factor),double(i))*300.0:double(-10300000+300000*i);
        b.next+=std::uint32_t(std::int32_t(amount));if(factor>1.25f)factor-=.01f;
    }
    b.baseMaxHP=level*20+256;b.levelStat=(level+1)*6;return b;
}
ExperienceBounds Experience::recalculate(){cachedLevel_=slot_.level();cached_=bounds(cachedLevel_);slot_.maxHP(cached_.baseMaxHP);return cached_;}
bool Experience::add(std::int32_t amount,int screen,bool hasPlayer){
    slot_.experience(slot_.experience()+std::uint32_t(amount));if(cachedLevel_!=slot_.level()){cachedLevel_=slot_.level();cached_=bounds(cachedLevel_);}auto b=cached_;bool leveled=false;
    if(slot_.level()<99&&b.next<=slot_.experience()){
        slot_.level(slot_.level()+1);b=recalculate();leveled=true;
        if(screen==7&&hasPlayer)events_.levelUp();events_.sound(7);
    }
    float ratio=float(slot_.experience()-b.previous)/float(b.next-b.previous);if(ratio>1)ratio=1;
    slot_.experienceGauge(ratio);events_.experienceGauge(ratio);events_.changed(Change::experience);return leveled;
}
Inventory::Inventory(const Definitions& defs):defs_(defs){equipment_.fill(-1);}
void Inventory::pages(int n){pages_=std::clamp(n,1,8);}
void Inventory::importSlot(SlotView slot){
    entries_={};equipment_.fill(-1);quick_={};pages(shortValue(slot.u16(0x88)));
    for(int i=0;i<pages_*25;++i)if(const auto* item=defs_.findItem(slot.string(0x8c+i*6,6)))entries_[i]={item,std::int16_t(slot.byte(0x53c+i)),0};
    for(unsigned i=0;i<6;++i)setEquip(std::bit_cast<std::int32_t>(slot.u32(0x604+i*4)),i);
    for(int i=0;i<5;++i)quick_[i]=defs_.findItem(slot.string(0x61c+i*6,6));
}
void Inventory::exportSlot(SlotView slot){
    slot.u32(0x88,std::uint32_t(pages_));
    for(int i=0;i<pages_*25;++i){auto& e=entries_[i];if(!e.item){slot.byte(0x8c+i*6,0);slot.byte(0x53c+i,0);}else{
        slot.string(0x8c+i*6,6,e.item->code);if(e.count>255)e.count=255;slot.byte(0x53c+i,std::uint8_t(e.count));}}
    for(int i=0;i<6;++i)slot.u32(0x604+i*4,std::uint32_t(equipment_[i]));
    for(int i=0;i<5;++i){if(quick_[i])slot.string(0x61c+i*6,6,quick_[i]->code);else slot.byte(0x61c+i*6,0);}
    // Original bot fields remain owned by their own consumer; retain.
}
bool Inventory::isQuick(const Item& item)const{return std::find(quick_.begin(),quick_.end(),&item)!=quick_.end();}
void Inventory::quickSlot(unsigned index,const Item* item){if(index>=quick_.size())throw std::out_of_range("Original quick-slot index outside5 slots");quick_[index]=item;}
const InventoryEntry* Inventory::entry(int position)const{return position>=0&&position<pages_*25?&entries_[position]:nullptr;}
bool Inventory::setItem(int position,const Item* item,std::int16_t count){if(!entry(position))return false;entries_[position]={item,count,0};return true;}
bool Inventory::setEquip(int position,unsigned slot,const std::function<void(const Item&,unsigned)>& beforeAssign){
    constexpr int types[]={11,12,10,10,13,14};const auto* e=entry(position);if(slot>=6||!e||!e->item||e->count<1||e->item->type!=types[slot])return false;
    if(beforeAssign)beforeAssign(*e->item,slot);
    const auto old=equipment_[slot];if(entry(old))entries_[old].flags=0;entries_[position].flags=1;equipment_[slot]=position;return true;
}
bool Inventory::subtractFrom(int position,std::int16_t count){
    if(!entry(position))return false;auto& e=entries_[position];e.count=shortValue(int(e.count)-count);if(e.item&&countChanged)countChanged(*e.item);
    if(e.count<1){e.count=0;e.item=nullptr;for(auto& equipped:equipment_)if(equipped==position)equipped=-1;}return true;
}
void Inventory::swap(int a,int b){
    if(!entry(a)||!entry(b))return;std::swap(entries_[a],entries_[b]);for(auto& equipped:equipment_){if(equipped==a)equipped=b;else if(equipped==b)equipped=a;}
}
void Inventory::arrange(){
    constexpr int order[]={11,12,13,14,10,4,1,6,2,3,5};int destination=0;
    for(const auto type:order)for(int i=destination;i<pages_*25;++i)if(entries_[i].item&&entries_[i].item->type==type){if(i!=destination)swap(i,destination);++destination;}
}
int Inventory::count(const Item& item)const{int n=0;for(int i=0;i<pages_*25;++i)if(entries_[i].item==&item)n=addInt(n,entries_[i].count);return n;}
int Inventory::emptySlots()const{int n=0;for(int i=0;i<pages_*25;++i)if(!entries_[i].count)++n;return n;}
bool Inventory::add(const Item& input,std::int16_t count){
    const Item* item=&input;if(item->type==6){count=shortValue(int(count)*int(item->param1));item=defs_.findItem("06001");if(!item)throw std::runtime_error("Unresolved original ticket destination06001");}
    auto first=std::find_if(entries_.begin(),entries_.begin()+pages_*25,[&](const auto& e){return e.item==item;});bool ok=false;
    if(first!=entries_.begin()+pages_*25&&item->countable){first->count=shortValue(int(first->count)+count);ok=true;}
    else{
        InventoryEntry* result=first!=entries_.begin()+pages_*25?&*first:nullptr;
        for(int i=0;i<pages_*25&&count>0;++i)if(!entries_[i].count){result=&entries_[i];*result={item,std::int16_t(item->countable?count:1),0};count=shortValue(int(count)-(item->countable?count:1));}
        ok=count==0&&result;
    }
    if(countChanged)countChanged(*item);return ok;
}
bool Inventory::subtract(const Item& item,std::int16_t requested){
    for(int i=0;i<pages_*25&&requested>0;++i){auto& e=entries_[i];if(e.item!=&item)continue;const auto previous=e.count;subtractFrom(i,requested);requested=shortValue(int(requested)-previous);
    }return requested<1;
}
Missions::Missions(const Definitions& defs,Inventory& inv,Events& events):defs_(defs),inventory_(inv),events_(events),states_(defs.missions().size()){
    for(std::size_t i=0;i<states_.size();++i)states_[i].did.resize(defs.missions()[i].requirements.size());
}
const MissionDefinition* Missions::definition(std::string_view id)const{const auto i=index(defs_,id);return i?&defs_.missions()[*i]:nullptr;}
const MissionState* Missions::state(std::string_view id)const{const auto i=index(defs_,id);return i?&states_[*i]:nullptr;}
bool Missions::check(std::size_t i){auto& s=states_[i];s.requireComplete=true;if(!s.loaded)return true;const auto& d=defs_.missions()[i];for(std::size_t j=0;j<s.did.size();++j)if(s.did[j]<d.requirements[j].count)s.requireComplete=false;return s.requireComplete;}
void Missions::refresh(std::size_t i,int screen){if(screen==7){events_.missionUpdated(defs_.missions()[i],states_[i]);if(states_[i].requireComplete)events_.sound(11);}events_.changed(Change::mission);}
bool Missions::available(std::string_view id,std::string_view base,int level)const{
    const auto i=index(defs_,id);if(!i)return false;const auto& d=defs_.missions()[*i];const auto& s=states_[*i];if(s.accepted||s.complete||d.owner!=base||d.level>level)return false;
    const auto previous=state(d.previous);return !previous||previous->complete;
}
bool Missions::accept(std::string_view id){
    const auto i=index(defs_,id);if(!i||std::find(accepted_.begin(),accepted_.end(),*i)!=accepted_.end())return false;
    auto& s=states_[*i];s.loaded=true;s.accepted=true;s.complete=false;s.requireComplete=false;accepted_.push_back(*i);
    const auto& d=defs_.missions()[*i];for(std::size_t j=0;j<d.requirements.size();++j){const auto& r=d.requirements[j];if(r.type==3&&s.did[j]<r.count){const auto* item=defs_.findItem(r.object);if(item)s.did[j]=std::min(r.count,inventory_.count(*item));}}
    check(*i);return true;
}
bool Missions::finish(std::string_view id){const auto i=index(defs_,id);if(!i)return false;states_[*i].complete=true;accepted_.erase(std::remove(accepted_.begin(),accepted_.end(),*i),accepted_.end());return true;}
void Missions::kill(std::string_view enemy,int count,std::string_view place,int screen){
    (void)place;for(const auto i:accepted_){auto& s=states_[i];if(s.requireComplete)continue;bool changed=false;const auto& d=defs_.missions()[i];
        for(std::size_t j=0;j<d.requirements.size();++j){const auto& r=d.requirements[j];if(r.type==2&&r.object==enemy&&s.did[j]<r.count){s.did[j]=std::min(r.count,addInt(s.did[j],count));changed=true;}}
        if(changed){check(i);refresh(i,screen);}
    }
}
std::vector<const MissionDefinition*> Missions::acceptedDefinitions()const{
    std::vector<const MissionDefinition*> result;result.reserve(accepted_.size());
    for(const auto index:accepted_)result.push_back(&defs_.missions().at(index));return result;
}
void Missions::arrive(std::string_view place,int screen){for(const auto i:accepted_){auto& s=states_[i];if(s.requireComplete)continue;bool changed=false;const auto& d=defs_.missions()[i];for(std::size_t j=0;j<d.requirements.size();++j){const auto& r=d.requirements[j];if(r.type==1&&r.object==place&&s.did[j]<r.count){s.did[j]=std::min(r.count,addInt(s.did[j],1));changed=true;}}if(changed){check(i);refresh(i,screen);}}}
void Missions::checkItemCount(const Item& item,int screen){for(const auto i:accepted_){auto& s=states_[i];if(s.requireComplete)continue;bool changed=false;const auto& d=defs_.missions()[i];for(std::size_t j=0;j<d.requirements.size();++j){const auto& r=d.requirements[j];if(r.type==3&&r.object==item.code&&s.did[j]<r.count){s.did[j]=std::min(r.count,inventory_.count(item));changed=true;}}if(changed){check(i);refresh(i,screen);}}}
void Missions::importSlot(SlotView slot){
    accepted_.clear();for(auto& s:states_){s.loaded=s.accepted=s.complete=s.requireComplete=false;std::fill(s.did.begin(),s.did.end(),0);}
    const auto n=shortValue(slot.u16(0x658));slot.u16(0x65a,0);
    for(int k=0;k<n&&k<256;++k){const auto p=std::size_t(0x65c+k*21);const auto i=index(defs_,slot.string(p,8));if(!i)continue;
        auto& s=states_[*i];s.accepted=slot.byte(p+8)!=0;if(s.accepted)accept(defs_.missions()[*i].id);
        if(s.loaded){const auto& d=defs_.missions()[*i];for(std::size_t j=0;j<s.did.size();++j)if(d.requirements[j].type!=3)s.did[j]=std::bit_cast<std::int8_t>(slot.byte(p+9+j));}
        check(*i);s.complete=slot.byte(p+20)!=0;if(s.complete){finish(defs_.missions()[*i].id);slot.u16(0x65a,std::uint16_t(slot.u16(0x65a)+1));}
    }
}
void Missions::exportSlot(SlotView slot)const{
    const auto& defs=defs_.missions();if(defs.size()>256)throw std::runtime_error("Too many original missions");
    // Reuse existing ID ordering to preserve a loaded slot's dictionary order.
    std::vector<std::size_t> order;const auto oldCount=shortValue(slot.u16(0x658));for(int k=0;k<oldCount&&k<256;++k){const auto i=index(defs_,slot.string(0x65c+k*21,8));if(i&&std::find(order.begin(),order.end(),*i)==order.end())order.push_back(*i);}
    for(std::size_t i=0;i<defs.size();++i)if(std::find(order.begin(),order.end(),i)==order.end())order.push_back(i);
    slot.u16(0x658,std::uint16_t(defs.size()));slot.u16(0x65a,0);
    for(std::size_t k=0;k<order.size();++k){const auto i=order[k],p=0x65c+k*21;const auto& s=states_[i];slot.string(p,8,defs[i].id);slot.byte(p+8,s.accepted);
        if(s.loaded)for(std::size_t j=0;j<s.did.size();++j)slot.byte(p+9+j,std::uint8_t(s.did[j]));slot.byte(p+19,s.requireComplete);slot.byte(p+20,s.complete);if(s.complete)slot.u16(0x65a,std::uint16_t(slot.u16(0x65a)+1));
    }
}
FieldItem FieldItem::create(const Item& item,int value,Vec2 position,WorldRandom& random,bool atlas){
    FieldItem f;f.item=&item;f.value=value;f.position=position;f.rootUsesAtlas=atlas;
    const auto first=random.random(),second=random.random();float angle=float(first)*0x1p-31f*0x1.921fb6p+1f;angle+=angle;const float magnitude=float(second)*0x1p-31f*4+3;
    f.velocity={float(std::cos(double(angle))*double(magnitude)),float(std::sin(double(angle))*double(magnitude))};return f;
}
void FieldItem::tick(double time,Vec2 player,std::span<const PickupBot> bots,WorldRandom& random,const TerrainCollision* terrain,const std::function<void(const Item&,int)>& pickup,const std::function<void(std::size_t)>& picker){
    if(removed||!item)return;
    if(!pickerBot){if(velocity.x!=0||velocity.y!=0){velocity.x+=(0-velocity.x)/7;velocity.y+=(0-velocity.y)/7;position.x+=velocity.x;position.y+=velocity.y;
        if(terrain){const auto selected=terrain->candidates(position);for(const auto i:selected){const auto& edge=terrain->edges()[i];const float depth=edge.overlap(position,16);if(depth!=0){position.x-=edge.normal.x*depth;position.y-=edge.normal.y*depth;}}}
    }}else{velocity.x+=((player.x-position.x)*.05f-velocity.x)/10;velocity.y+=((player.y-position.y)*.05f-velocity.y)/10;speed+=(.1f-speed)/100;position.x+=velocity.x;position.y+=velocity.y;}
    angle=randomUnit(random)*0x1.921fb6p+1f;angle+=angle;scale=std::sin(float(time*10))*.01f+.5f;alpha=randomUnit(random)*.8f+.2f;
    if(time-double(checkTime)>double(.2f)){
        if(std::abs(player.x-position.x)<30&&std::abs(player.y-position.y)<30){pickup(*item,value);removed=true;}
        else if(!pickerBot){for(std::size_t i=0;i<bots.size()&&!pickerBot;++i)if(bots[i].referenceType==2&&std::abs(bots[i].position.x-position.x)<20&&std::abs(bots[i].position.y-position.y)<20){pickerBot=true;if(picker)picker(i);}}
        checkTime=float(time);
    }
}
int ItemSpawner::spawnCount(std::string_view name,WorldRandom& random)const{const auto* s=defs_.findSpawn(name);return s?int(random.random()%std::uint32_t(s->maximum+1-s->minimum))+s->minimum:0;}
const Item* ItemSpawner::spawnItem(std::string_view name,WorldRandom& random)const{const auto* s=defs_.findSpawn(name);if(!s)return nullptr;if(s->totalWeight<1)throw std::runtime_error("Original ItemRegen has zero total weight");const int value=int(random.random()%std::uint32_t(s->totalWeight));for(const auto& [id,end]:s->cumulative)if(value<end)return defs_.findItem(id);return nullptr;}
void ItemSpawner::drop(const MachineInfo& machine,Vec2 position,WorldRandom& random,const std::function<void(FieldItem)>& emit)const{
    if(int(random.random()%100)<machine.coinRate){const float unit=randomUnit(random);const float count=float(machine.coin)+unit*(float(machine.coin)/3.f);const int value=int(count);
        constexpr int limits[]={5,10,50,100,500,1000,5000,10000};int n=0;while(n<8&&value>=limits[n])++n;char code[6];std::snprintf(code,sizeof(code),"%05d",n+1);
        const auto* item=defs_.findItem(code);if(!item)throw std::runtime_error("Unresolved original coin item");emit(FieldItem::create(*item,value,position,random));
    }
    const auto n=spawnCount(machine.itemRegeneration,random);for(int i=0;i<n;++i)if(const auto* item=spawnItem(machine.itemRegeneration,random))emit(FieldItem::create(*item,1,position,random,true));
}
Progression::Progression(SlotView slot,const Definitions& defs,Events& events):slot_(slot),defs_(defs),events_(events),inventory(defs),experience(slot,events),missions(defs,inventory,events),itemSpawner(defs){inventory.countChanged=[this](const Item& item){missions.checkItemCount(item,screen_);events_.changed(Change::inventory);};}
void Progression::restore(){inventory.importSlot(slot_);missions.importSlot(slot_);experience.recalculate();}
void Progression::prepareSave(){inventory.exportSlot(slot_);missions.exportSlot(slot_);}
bool Progression::acceptMission(std::string_view id,std::string_view base){
    if(!missions.available(id,base,slot_.level()))return false;const auto* d=missions.definition(id);if(missions.acceptedCount()>=5){events_.alert("MissionFull");return false;}
    int empty=inventory.emptySlots();for(const auto& a:d->acceptActions)if(a.type==2)empty-=a.value;if(empty<0){events_.alert("TakeItemFail");return false;}
    if(!missions.accept(id))return false;for(const auto& a:d->acceptActions)if(a.type==2)if(const auto* item=defs_.findItem(a.item))inventory.add(*item,shortValue(a.value));events_.changed(Change::mission);return true;
}
bool Progression::claimMission(std::string_view id,std::string_view base){
    const auto* d=missions.definition(id);const auto* s=missions.state(id);if(!d||!s||!s->accepted||s->complete||!s->requireComplete||d->completePlace!=base)return false;
    events_.sound(11);
    for(const auto& a:d->completeActions){if(a.type>=0&&a.type<=2)events_.missionReward(*d,a);
        if(a.type==0){slot_.credit(slot_.credit()+std::uint32_t(a.value));events_.changed(Change::credit);}
        else if(a.type==1)experience.add(a.value,screen_);
        else if(a.type==2){const auto* item=defs_.findItem(a.item);if(!item||!inventory.add(*item,shortValue(a.value)))events_.alert("TakeItemFail");}
        else if(a.type==3){if(const auto* item=defs_.findItem(a.item))inventory.subtract(*item,shortValue(a.value));}
    }
    missions.finish(id);events_.changed(Change::mission);return true;
}
void Progression::killed(std::string_view enemy,int exp,std::string_view place){missions.kill(enemy,1,place,screen_);experience.add(exp,screen_);}
bool Progression::pickup(const Item& item,int value,double time){
    events_.pickupBegin(item);bool ok=true;
    if(item.type==0){slot_.credit(slot_.credit()+std::uint32_t(value));events_.changed(Change::credit);}
    else{ok=inventory.add(item,shortValue(value));if(!ok)events_.alert("TakeItemFail");else if(inventory.isQuick(item))events_.quickItemChanged(item);}
    events_.pickupText(item,value,ok,time);events_.sound(3);return ok;
}
}
