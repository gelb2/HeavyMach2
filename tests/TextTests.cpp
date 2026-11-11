#include "Core/UI/Text.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace hm2;
using namespace hm2::ui;
namespace {
bool near(float a,float b){return std::abs(a-b)<.0001f;}
template<class F>void fails(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}assert(failed);}
struct CountingRasterizer:TextRasterizer {
    unsigned calls=0;
    TextBitmap rasterize(const TextRasterRequest& r)override{
        ++calls;TextBitmap b;b.request=r;b.layout=textRasterLayout(r);
        b.alpha.resize(std::size_t(b.layout.width)*b.layout.height,0);
        b.alpha[b.layout.width+2]=255;b.alpha[b.layout.width*2+4]=128;
        b.alpha[b.layout.width*4+1]=1;
        return b;
    }
};
}
int main(){
    TextRasterRequest request{"test:original-api-contract","12\n34","Arial",{127.9f,31.5f},18,TextAlign::center,false,3};
    auto l=textRasterLayout(request);
    // Original truncates before next power of two, retaining fractional content.
    assert(l.width==128&&l.height==32&&near(l.content.width,127.9f)&&!l.forRetina);
    request.retina=true;l=textRasterLayout(request);assert(l.forRetina&&l.width==128&&l.fontSize==18);
    request.originalDeviceType=2;l=textRasterLayout(request);
    assert(!l.forRetina&&l.width==64&&l.height==16&&l.fontSize==9);
    request.originalDeviceType=1;l=textRasterLayout(request);assert(!l.forRetina&&l.fontSize==18);
    request.retina=false;request.dimensions.width=128.99f;
    assert(textRasterLayout(request).width==128);
    auto invalid=request;invalid.fontName.clear();fails([&]{(void)textRasterLayout(invalid);});
    invalid=request;invalid.dimensions.height=0;fails([&]{(void)textRasterLayout(invalid);});
    invalid=request;invalid.fontSize=std::numeric_limits<float>::quiet_NaN();fails([&]{(void)textRasterLayout(invalid);});
    invalid=request;invalid.alignment=static_cast<TextAlign>(9);fails([&]{(void)textRasterLayout(invalid);});
    invalid=request;invalid.text=std::string("1\0x",3);fails([&]{(void)textRasterLayout(invalid);});
    CountingRasterizer rasterizer;TextPreparationCache cache;
    auto bitmap=cache.prepare(rasterizer,request);assert(rasterizer.calls==1);
    assert(bitmap==cache.prepare(rasterizer,request)&&rasterizer.calls==1);
    assert(bitmap->metrics.inkBands.size()==2);
    const auto band=bitmap->metrics.inkBands[0];
    assert(band.firstRow==1&&band.lastRow==2&&band.left==2&&band.right==4);
    Text text("prepared:label",bitmap);text.position={10.5f,-10.5f};text.setColor(255,128,0);text.alpha=.4f;
    text.scale={2,.5f};text.angle=.75f;Frame f;text.append(f);text.append(f);
    assert(rasterizer.calls==1&&f.sprites.size()==2); // No draw-time rasterization.
    const auto s=f.sprites[0];assert(s.center.x==11&&s.center.y==-11);
    assert(near(s.size.width,128.99f*2)&&near(s.size.height,31.5f*.5f)&&s.angle==.75f);
    assert(s.color.r==1&&near(s.color.g,128.f/255)&&s.color.b==0&&s.color.a==.4f);
    assert(s.blend.source==BlendFactor::sourceAlpha&&s.blend.destination==BlendFactor::oneMinusSourceAlpha);
    assert(s.strip.size()==4&&near(s.strip[0].uv.y,31.5f/32)&&near(s.strip[1].uv.x,128.99f/128));
    assert(s.strip[0].color.a==.4f&&s.strip[0].color.b==0);
    text.visible=false;text.append(f);assert(f.sprites.size()==2);
    auto changed=request;changed.text="56";cache.prepare(rasterizer,changed);assert(rasterizer.calls==2);
    changed=request;changed.alignment=TextAlign::right;cache.prepare(rasterizer,changed);assert(rasterizer.calls==3);
    changed=request;changed.dimensions.width=120;cache.prepare(rasterizer,changed);assert(rasterizer.calls==4);
    changed=request;changed.fontName="Arial-BoldMT";cache.prepare(rasterizer,changed);assert(rasterizer.calls==5);
    cache.clear();assert(cache.size()==0&&bitmap->alpha.size()==128*32);
    auto retinaRequest=request;retinaRequest.retina=true;retinaRequest.originalDeviceType=3;
    Text retina("prepared:retina",cache.prepare(rasterizer,retinaRequest));Frame rf;retina.append(rf);
    assert(near(rf.sprites[0].size.width,128.99f*.5f)&&near(rf.sprites[0].size.height,31.5f*.5f));

    NumericAtlas atlas{"NumSet01b.png",{256,20},16,1,false};
    ImageFont digits(atlas);digits.setPitch(6);digits.setAlignRate(1);digits.position={100,200};
    digits.setNumber(123);digits.tick();Frame numbers;digits.append(numbers);
    assert(digits.count()==3&&digits.number()==123&&digits.width()==18);
    assert(numbers.sprites.size()==3&&numbers.sprites[0].tile==3&&numbers.sprites[1].tile==2&&numbers.sprites[2].tile==1);
    assert(numbers.sprites[0].center.x==100&&numbers.sprites[1].center.x==94&&numbers.sprites[2].center.x==88);
    VisualLayer uiLayer;
    VisualTile numberTile{"ResMgr:NumSet01b.png","NumSet01b.png",{256,20},{256,32},16,1,1,false};
    digits.append(uiLayer,numberTile);auto numberBatch=uiLayer.drain();
    assert(numberBatch.nonAtlas.empty()&&numberBatch.atlases.size()==1&&numberBatch.atlases[0].quads.size()==3);
    assert(numberBatch.atlases[0].blend.source==BlendFactor::one&&numberBatch.atlases[0].indices.size()==18);
    assert(numberBatch.atlases[0].quads[0].vertices[0].uv.x==3.f/16);
    auto wrongTile=numberTile;wrongTile.columns=8;fails([&]{digits.append(uiLayer,wrongTile);});
    fails([&]{digits.append(uiLayer,numberTile,3);});
    digits.setNumber(-123);numbers.sprites.clear();digits.append(numbers);assert(numbers.sprites[2].tile==1&&digits.count()==3);
    digits.setNumber(0);numbers.sprites.clear();digits.append(numbers);assert(numbers.sprites.size()==1&&numbers.sprites[0].tile==0);
    digits.setNumber(std::numeric_limits<std::int32_t>::min());assert(digits.count()==10);
    // Original text creation uses source-byte positions, not number positions.
    ImageFont punctuation(atlas);punctuation.setPitch(6);punctuation.setAlignRate(1);punctuation.tick();
    punctuation.setText("1./:-%*");Frame symbols;punctuation.append(symbols);
    assert(symbols.sprites.size()==7);
    for(unsigned i=0;i<6;++i)assert(symbols.sprites[i].tile==15-i);
    assert(symbols.sprites[6].tile==1&&symbols.sprites[0].center.x==36&&symbols.sprites[6].center.x==0);
    // A later scale-change tick changes child layout to negative insertion order.
    punctuation.scale={2,3};punctuation.tick();symbols.sprites.clear();punctuation.append(symbols);
    assert(symbols.sprites[0].center.x==0&&symbols.sprites[6].center.x==-72);
    assert(symbols.sprites[0].size.width==32&&symbols.sprites[0].size.height==60);
    punctuation.setText("1?2");symbols.sprites.clear();punctuation.append(symbols);
    assert(punctuation.count()==3&&punctuation.width()==18&&symbols.sprites.size()==2);
    assert(symbols.sprites[0].tile==2&&symbols.sprites[0].center.x==24&&symbols.sprites[1].tile==1);
    punctuation.setText("\xed\x95\x9c");symbols.sprites.clear();punctuation.append(symbols);
    assert(punctuation.count()==3&&symbols.sprites.empty()); // No invented Korean atlas.
    punctuation.setText(std::string_view("1\0x",3));assert(punctuation.count()==1);
    punctuation.setText("");assert(punctuation.count()==0);
    ImageFont align(atlas);align.setPitch(6);align.setAlignRate(.5f);align.setNumber(123);align.tick();
    Frame a;align.append(a);assert(a.sprites[0].center.x==6&&a.sprites[2].center.x==-6);
    align.setAlignRate(1);a.sprites.clear();align.tick();align.append(a);
    assert(a.sprites[0].center.x==6); // Setter alone doesn't trigger original relayout.
    align.setBlend({BlendFactor::one,BlendFactor::one});a.sprites.clear();align.append(a);
    assert(a.sprites[0].blend.source==BlendFactor::one&&a.sprites[0].blend.destination==BlendFactor::one);
    NumericAtlas ra=atlas;ra.forRetina=true;ImageFont rd(ra);rd.setPitch(6);rd.setNumber(12);rd.tick();
    assert(rd.width()==6);Frame r;rd.append(r);assert(r.sprites[0].size.width==8&&r.sprites[0].center.x==3);
    rd.visible=false;rd.append(r);assert(r.sprites.size()==2);
    ImageFont trade({"trade-digits-test",{100,16},10,1,false});trade.setNumber(123);trade.tick();
    Frame tradeFrame;trade.append(tradeFrame);assert(tradeFrame.sprites.size()==3);
    fails([&]{trade.setText("%");});
    invalid=request;invalid.dimensions.width=20000;fails([&]{(void)textRasterLayout(invalid);});
}
