#include "Core/UI/Text.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hm2::ui {
namespace {
void finite(float n){if(!std::isfinite(n))throw std::runtime_error("Nonfinite text value");}
unsigned powerOfTwo(float n){
    finite(n);
    // Avoid original zero-size context / shift overflow. This limit bounds the
    // preparation allocation; it is not a replacement game font size.
    if(n<1||n>16384)throw std::runtime_error("Invalid text texture dimension");
    const auto integer=static_cast<unsigned>(n);
    unsigned result=1;while(result<integer)result*=2;return result;
}
int glyphTile(unsigned char c){
    if(c>='0'&&c<='9')return c-'0';
    switch(c){case '.':return 10;case '/':return 11;case ':':return 12;
    case '-':return 13;case '%':return 14;case '*':return 15;default:return -1;}
}
}
bool TextRasterRequest::operator==(const TextRasterRequest& r)const{
    return sourceId==r.sourceId&&text==r.text&&fontName==r.fontName&&
           dimensions.width==r.dimensions.width&&dimensions.height==r.dimensions.height&&
           fontSize==r.fontSize&&alignment==r.alignment&&retina==r.retina&&
           originalDeviceType==r.originalDeviceType;
}
TextRasterLayout textRasterLayout(const TextRasterRequest& r){
    finite(r.fontSize);
    if(r.fontName.empty()||r.fontSize<=0||r.text.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid original text request");
    if(r.alignment!=TextAlign::left&&r.alignment!=TextAlign::center&&r.alignment!=TextAlign::right)
        throw std::runtime_error("Unsupported original text alignment");
    TextRasterLayout l;l.content=r.dimensions;l.fontSize=r.fontSize;
    if(r.retina&&r.originalDeviceType==2){l.content.width*=.5f;l.content.height*=.5f;l.fontSize*=.5f;}
    l.forRetina=r.retina&&r.originalDeviceType==3;
    l.width=powerOfTwo(l.content.width);l.height=powerOfTwo(l.content.height);
    return l;
}
void measureTextInk(TextBitmap& b){
    if(!b.layout.width||!b.layout.height||
       b.alpha.size()!=std::size_t(b.layout.width)*b.layout.height)
        throw std::runtime_error("Invalid A8 text bitmap storage");
    b.metrics.inkBands.clear();bool open=false;
    for(unsigned y=0;y<b.layout.height;++y){
        unsigned left=b.layout.width,right=0;
        for(unsigned x=0;x<b.layout.width;++x)if(b.alpha[std::size_t(y)*b.layout.width+x]){
            left=std::min(left,x);right=x;
        }
        if(left==b.layout.width){open=false;continue;}
        if(!open){b.metrics.inkBands.push_back({y,y,left,right});open=true;}
        else{auto& band=b.metrics.inkBands.back();band.lastRow=y;
            band.left=std::min(band.left,left);band.right=std::max(band.right,right);}
    }
}
std::shared_ptr<const TextBitmap> TextPreparationCache::prepare(TextRasterizer& rasterizer,const TextRasterRequest& r){
    const auto layout=textRasterLayout(r);
    const auto found=std::find_if(entries_.begin(),entries_.end(),[&](const auto& b){return b->request==r;});
    if(found!=entries_.end())return *found;
    auto b=std::make_shared<TextBitmap>(rasterizer.rasterize(r));
    if(!(b->request==r)||b->layout.width!=layout.width||b->layout.height!=layout.height||
       b->layout.content.width!=layout.content.width||b->layout.content.height!=layout.content.height||
       b->layout.fontSize!=layout.fontSize||b->layout.forRetina!=layout.forRetina)
        throw std::runtime_error("Rasterizer changed original text layout");
    measureTextInk(*b);entries_.push_back(b);return b;
}
Text::Text(std::string image,std::shared_ptr<const TextBitmap> bitmap):image_(std::move(image)),bitmap_(std::move(bitmap)){
    if(image_.empty()||!bitmap_)throw std::runtime_error("Missing prepared text resource");
    const auto l=textRasterLayout(bitmap_->request);
    if(bitmap_->layout.width!=l.width||bitmap_->layout.height!=l.height||
       bitmap_->alpha.size()!=std::size_t(l.width)*l.height)
        throw std::runtime_error("Invalid prepared text resource");
}
void Text::setColor(std::uint8_t r,std::uint8_t g,std::uint8_t b){color_={float(r)/255,float(g)/255,float(b)/255,1};}
void Text::append(Frame& f)const{
    if(!visible)return;
    finite(position.x);finite(position.y);finite(scale.x);finite(scale.y);finite(angle);finite(alpha);
    const float retina=bitmap_->layout.forRetina?.5f:1.f;
    Sprite s;s.image=image_;s.center={std::round(position.x),std::round(position.y)};
    s.size={bitmap_->layout.content.width*scale.x*retina,bitmap_->layout.content.height*scale.y*retina};
    s.angle=angle;s.color=color_;s.color.a=alpha;
    // Original A8 pixelFormat3 + Tile2D blendType0.
    s.blend={BlendFactor::sourceAlpha,BlendFactor::oneMinusSourceAlpha};
    // Tile2D::tileSplitX:splitY: uses maxS/T = content/POT and content
    // dimensions for its quad. Register the A8 texture at its full POT storage
    // size; the explicit UVs below perform its one and only content crop.
    const float u=bitmap_->layout.content.width/float(bitmap_->layout.width);
    const float v=bitmap_->layout.content.height/float(bitmap_->layout.height);
    const float hx=s.size.width*.5f,hy=s.size.height*.5f;
    const float cosine=std::cos(angle),sine=std::sin(angle);
    const Vec2 local[]={{-hx,-hy},{hx,-hy},{-hx,hy},{hx,hy}};
    const Vec2 uv[]={{0,v},{u,v},{0,0},{u,0}};
    for(unsigned i=0;i<4;++i)s.strip.push_back({
        {s.center.x+local[i].x*cosine-local[i].y*sine,
         s.center.y+local[i].x*sine+local[i].y*cosine},uv[i],s.color});
    f.sprites.push_back(std::move(s));
}
ImageFont::ImageFont(NumericAtlas atlas):atlas_(std::move(atlas)){
    finite(atlas_.pixels.width);finite(atlas_.pixels.height);
    if(atlas_.image.empty()||atlas_.pixels.width<=0||atlas_.pixels.height<=0||
       !atlas_.columns||!atlas_.rows||atlas_.columns>std::numeric_limits<unsigned>::max()/atlas_.rows||
       atlas_.columns*atlas_.rows<10)throw std::runtime_error("Invalid numeric font atlas");
    pitch_=atlas_.pixels.width/float(atlas_.columns)*(atlas_.forRetina?.5f:1.f);
}
void ImageFont::relayoutContainer(){
    containerX_=(float(glyphs_.size())-1)*pitch_*scale.x*(1-alignRate_);
}
void ImageFont::setNumber(std::int32_t value){
    finite(scale.x);finite(scale.y);number_=value;glyphs_.clear();
    // Widen before negation: retain absolute digits without original INT_MIN UB.
    std::uint64_t n=value<0?std::uint64_t(-std::int64_t(value)):std::uint64_t(value);
    do{glyphs_.push_back({unsigned(n%10),true,-float(glyphs_.size())*pitch_*scale.x,{1,1}});n/=10;}while(n);
    relayoutContainer();
}
void ImageFont::setText(std::string_view text){
    finite(scale.x);finite(scale.y);glyphs_.clear();
    // Original consumes a C string; embedded NUL ends it.
    if(const auto end=text.find('\0');end!=std::string_view::npos)text=text.substr(0,end);
    for(std::size_t i=text.size();i>0;--i){const int tile=glyphTile(static_cast<unsigned char>(text[i-1]));
        if(tile>=0&&unsigned(tile)>=atlas_.columns*atlas_.rows)
            throw std::runtime_error("Original text punctuation exceeds numeric atlas grid");
        glyphs_.push_back({tile<0?0u:unsigned(tile),tile>=0,float(i-1)*pitch_*scale.x,{1,1}});}
    relayoutContainer();
}
void ImageFont::setPitch(float pitch){finite(pitch);pitch_=pitch*(atlas_.forRetina?.5f:1.f);}
void ImageFont::setAlignRate(float rate){finite(rate);alignRate_=rate;}
void ImageFont::setBlend(Blend blend){atlas_.blend=blend;}
void ImageFont::tick(){
    finite(scale.x);finite(scale.y);
    if(scale.x==calculatedScale_.x&&scale.y==calculatedScale_.y)return;
    relayoutContainer();
    for(std::size_t i=0;i<glyphs_.size();++i){glyphs_[i].x=-float(i)*pitch_*scale.x;glyphs_[i].scale=scale;}
    calculatedScale_=scale;
}
void ImageFont::append(Frame& f)const{
    if(!visible)return;finite(position.x);finite(position.y);
    const float retina=atlas_.forRetina?.5f:1.f;
    for(const auto& g:glyphs_){if(!g.visible)continue;
        Sprite s;s.image=atlas_.image;s.center={position.x+containerX_+g.x,position.y};
        s.size={atlas_.pixels.width/float(atlas_.columns)*retina*g.scale.x,
                atlas_.pixels.height/float(atlas_.rows)*retina*g.scale.y};
        s.tile=g.tile;s.columns=atlas_.columns;s.rows=atlas_.rows;s.blend=atlas_.blend;
        f.sprites.push_back(std::move(s));
    }
}
void ImageFont::append(VisualLayer& layer,const VisualTile& tile,unsigned blendType)const{
    if(!visible)return;
    if(tile.image!=atlas_.image||tile.content.width!=atlas_.pixels.width||tile.content.height!=atlas_.pixels.height||
       tile.columns!=atlas_.columns||tile.rows!=atlas_.rows||tile.forRetina!=atlas_.forRetina)
        throw std::runtime_error("Numeric font changed original Tile2D resource");
    const auto blend=originalAtlasBlend(blendType,tile.pixelFormat);
    if(blend.source!=atlas_.blend.source||blend.destination!=atlas_.blend.destination)
        throw std::runtime_error("Numeric font original blendType does not match selected blend");
    for(const auto& glyph:glyphs_){if(!glyph.visible)continue;
        VisualImage image;image.tile=tile;image.tileNo=glyph.tile;image.blendType=blendType;
        image.screenPosition={position.x+containerX_+glyph.x,position.y};image.scale=glyph.scale;
        image.useAtlas=true;layer.submitImage(image);
    }
}
}
