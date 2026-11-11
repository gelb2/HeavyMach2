#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace hm2 {
namespace {
void finite(float f){if(!std::isfinite(f))throw std::runtime_error("Nonfinite visual value");}
void validTile(const VisualTile& t){
    finite(t.content.width);finite(t.content.height);finite(t.storage.width);finite(t.storage.height);
    finite(t.origin.x);finite(t.origin.y);finite(t.color.r);finite(t.color.g);finite(t.color.b);
    if(t.image.empty()||t.content.width<=0||t.content.height<=0||t.storage.width<=0||t.storage.height<=0||
       t.columns==0||t.rows==0||t.columns>std::numeric_limits<unsigned>::max()/t.rows)
        throw std::runtime_error("Invalid original visual tile");
}
struct Geometry {
    float xScale,yScale,halfWidth,halfHeight,u,v;
    unsigned tileNo;
};
Geometry geometry(const VisualImage& image){
    validTile(image.tile);finite(image.screenPosition.x);finite(image.screenPosition.y);
    finite(image.scale.x);finite(image.scale.y);finite(image.angle);finite(image.alpha);
    Geometry g;
    g.u=(image.tile.content.width/image.tile.storage.width)/float(image.tile.columns);
    g.v=(image.tile.content.height/image.tile.storage.height)/float(image.tile.rows);
    g.xScale=image.scale.x*(image.tile.forRetina?.5f:1.f);
    g.yScale=image.scale.y*(image.tile.forRetina?.5f:1.f);
    g.halfWidth=((image.tile.storage.width*g.u)*.5f)*g.xScale;
    g.halfHeight=((image.tile.storage.height*g.v)*.5f)*g.yScale;
    if(image.reverse&1)g.halfWidth=-g.halfWidth;
    if(image.reverse&2)g.halfHeight=-g.halfHeight;
    g.tileNo=std::min(image.tileNo,image.tile.columns*image.tile.rows-1);
    return g;
}
std::array<Vec2,4> tileUV(const VisualImage& image,const Geometry& g){
    const float left=float(g.tileNo%image.tile.columns)*g.u;
    const float top=float(g.tileNo/image.tile.columns)*g.v;
    return {{{left,top},{left,top+g.v},{left+g.u,top},{left+g.u,top+g.v}}};
}
Vec2 rotated(Vec2 local,float c,float s,Vec2 origin){
    return {origin.x+(local.x*c-local.y*s),origin.y+(local.x*s+local.y*c)};
}
Sprite nonAtlasImage(const VisualImage& image){
    const auto g=geometry(image);const auto uv=tileUV(image,g);
    Sprite out;out.image=image.tile.image;out.center=image.screenPosition;out.textureSource=image.tile.textureSource;
    out.color={image.tile.color.r,image.tile.color.g,image.tile.color.b,image.alpha};
    out.blend=originalTileBlend(image.blendType,image.tile.pixelFormat);
    const float ox=image.tile.origin.x*g.xScale,oy=image.tile.origin.y*g.yScale;
    const Vec2 translated={image.screenPosition.x+((image.reverse&1)?ox:-ox),image.screenPosition.y-oy};
    const float c=std::cos(image.angle),s=std::sin(image.angle);
    const Vec2 local[]={{-g.halfWidth,g.halfHeight},{-g.halfWidth,-g.halfHeight},
                        {g.halfWidth,g.halfHeight},{g.halfWidth,-g.halfHeight}};
    const float maxS=image.tile.content.width/image.tile.storage.width;
    const float maxT=image.tile.content.height/image.tile.storage.height;
    for(unsigned i=0;i<4;++i)out.strip.push_back({rotated(local[i],c,s,translated),
        {uv[i].x/maxS,uv[i].y/maxT},out.color});
    return out;
}
bool sameStorage(const VisualTile& a,const VisualTile& b){
    return a.image==b.image&&a.pixelFormat==b.pixelFormat&&a.textureSource==b.textureSource&&
        a.content.width==b.content.width&&a.content.height==b.content.height&&
        a.storage.width==b.storage.width&&a.storage.height==b.storage.height;
}
}
Blend originalTileBlend(unsigned type,unsigned format){
    switch(type){
        case 0:return {format==1?BlendFactor::one:BlendFactor::sourceAlpha,BlendFactor::oneMinusSourceAlpha};
        case 1:return {BlendFactor::sourceColor,BlendFactor::oneMinusSourceAlpha};
        case 2:return {BlendFactor::sourceAlpha,BlendFactor::one};
        case 3:return {BlendFactor::one,BlendFactor::one};
        case 4:return {BlendFactor::sourceAlpha,BlendFactor::oneMinusSourceAlpha};
        default:throw std::runtime_error("Unknown original Tile2D blend type retains GL state");
    }
}
Blend originalAtlasBlend(unsigned type,unsigned format){
    if(type==3)return {BlendFactor::oneMinusSourceAlpha,BlendFactor::oneMinusSourceAlpha};
    return originalTileBlend(type,format);
}
std::uint8_t originalAtlasColorByte(float component){
    finite(component);
    // ARM vcvt.u32.f32 saturates to [0,UINT32_MAX], then strb/uxtb keeps
    // the low byte. Values in [0,1] truncate after the original float multiply.
    const float scaled=component*255.f;
    if(!(scaled>0))return 0;
    if(scaled>=4294967296.f)return 255;
    return static_cast<std::uint8_t>(static_cast<std::uint32_t>(scaled));
}
AtlasQuad originalAtlasQuad(const VisualImage& image){
    const auto g=geometry(image);const auto uv=tileUV(image,g);AtlasQuad q;
    const ColorBytes color{originalAtlasColorByte(image.tile.color.r),originalAtlasColorByte(image.tile.color.g),
                           originalAtlasColorByte(image.tile.color.b),originalAtlasColorByte(image.alpha)};
    const float c=std::cos(image.angle),s=std::sin(image.angle);
    // Preserve the original branch: zero-angle atlas geometry ignores origin;
    // rotated atlas geometry adds origin*scale before rotation. Tile2D's direct
    // draw translates the origin with a different sign and is kept separately.
    const float ox=image.angle==0?0:image.tile.origin.x*g.xScale;
    const float oy=image.angle==0?0:image.tile.origin.y*g.yScale;
    const Vec2 local[]={{ox-g.halfWidth,oy+g.halfHeight},{ox-g.halfWidth,oy-g.halfHeight},
                        {ox+g.halfWidth,oy+g.halfHeight},{ox+g.halfWidth,oy-g.halfHeight}};
    for(unsigned i=0;i<4;++i)q.vertices[i]={rotated(local[i],c,s,image.screenPosition),color,uv[i]};
    return q;
}
void VisualLayerFrame::append(Frame& frame)const{
    frame.sprites.insert(frame.sprites.end(),nonAtlas.begin(),nonAtlas.end());
    for(const auto& batch:atlases){
        const float maxS=batch.content.width/batch.storage.width,maxT=batch.content.height/batch.storage.height;
        for(const auto& quad:batch.quads){
            Sprite sprite;sprite.image=batch.image;sprite.blend=batch.blend;sprite.textureSource=batch.textureSource;
            for(const auto& v:quad.vertices)sprite.strip.push_back({v.position,{v.uv.x/maxS,v.uv.y/maxT},
                {float(v.color.r)/255.f,float(v.color.g)/255.f,float(v.color.b)/255.f,float(v.color.a)/255.f}});
            frame.sprites.push_back(std::move(sprite));
        }
    }
}
void VisualLayerFrame::appendBatches(Frame& frame)const &{
    frame.sprites.insert(frame.sprites.end(),nonAtlas.begin(),nonAtlas.end());
    for(const auto& batch:atlases){
        if(batch.quads.empty())continue;
        Sprite command;command.image=batch.image;command.blend=batch.blend;command.textureSource=batch.textureSource;
        command.atlasBatch=std::make_shared<const AtlasBatch>(batch);
        frame.sprites.push_back(std::move(command));
    }
}
void VisualLayerFrame::appendBatches(Frame& frame)&&{
    frame.sprites.insert(frame.sprites.end(),std::make_move_iterator(nonAtlas.begin()),std::make_move_iterator(nonAtlas.end()));
    for(auto& batch:atlases){
        if(batch.quads.empty())continue;
        Sprite command;command.image=batch.image;command.blend=batch.blend;command.textureSource=batch.textureSource;
        command.atlasBatch=std::make_shared<const AtlasBatch>(std::move(batch));
        frame.sprites.push_back(std::move(command));
    }
    nonAtlas.clear();atlases.clear();
}
VisualLayer::VisualLayer(unsigned unitSize):unitSize_(unitSize),visualCapacity_(unitSize){
    if(!unitSize)throw std::runtime_error("Zero original visual pool unit size");
    nonAtlas_.reserve(unitSize);
}
std::size_t VisualLayer::group(const VisualTile& tile,unsigned blendType){
    validTile(tile);(void)originalAtlasBlend(blendType,tile.pixelFormat);
    if(tile.identity.empty())throw std::runtime_error("Atlas requires stable original Tile2D identity");
    for(std::size_t i=0;i<groups_.size();++i)if(groups_[i].tile.identity==tile.identity&&groups_[i].blendType==blendType){
        if(!sameStorage(groups_[i].tile,tile))throw std::runtime_error("Original Tile2D identity changed texture storage");
        return i;
    }
    Group g;g.tile=tile;g.blendType=blendType;g.quads.reserve(g.capacity);
    groups_.push_back(std::move(g));return groups_.size()-1;
}
void VisualLayer::prepareAtlas(const VisualTile& tile){(void)group(tile,0);}
void VisualLayer::submitNonAtlas(Sprite sprite){
    if(nonAtlas_.size()==visualCapacity_){
        if(visualCapacity_>std::numeric_limits<unsigned>::max()-unitSize_)
            throw std::runtime_error("Original visual pool capacity overflow");
        visualCapacity_+=unitSize_;nonAtlas_.reserve(visualCapacity_);
    }
    nonAtlas_.push_back(std::move(sprite));
}
void VisualLayer::submitImage(VisualImage& image){
    if(!image.show||!image.isInScreen)return;
    if(!image.useAtlas){submitNonAtlas(nonAtlasImage(image));return;}
    const auto quad=originalAtlasQuad(image);
    auto& batch=groups_[group(image.tile,image.blendType)];
    // Original uses uint16 indices. Refuse the original wraparound hazard
    // instead of silently splitting/reordering an unverified oversized group.
    if(batch.quads.size()>=16384)throw std::runtime_error("Original atlas exceeds uint16 quad index range");
    if(batch.quads.size()==batch.capacity){batch.capacity*=2;batch.quads.reserve(batch.capacity);}
    image.tileNo=std::min(image.tileNo,image.tile.columns*image.tile.rows-1);
    batch.quads.push_back(quad);
}
unsigned VisualLayer::atlasCapacity(std::size_t index)const{
    if(index>=groups_.size())throw std::runtime_error("Unknown original atlas group");
    return groups_[index].capacity;
}
VisualLayerFrame VisualLayer::drain(){
    VisualLayerFrame out;out.nonAtlas=nonAtlas_;
    for(auto& g:groups_){
        if(g.quads.empty())continue; // drawNumberOfQuads:0 performs no GL draw.
        AtlasBatch b;b.tileIdentity=g.tile.identity;b.image=g.tile.image;b.blendType=g.blendType;
        b.textureSource=g.tile.textureSource;b.pixelFormat=g.tile.pixelFormat;b.capacity=g.capacity;b.content=g.tile.content;b.storage=g.tile.storage;
        b.blend=originalAtlasBlend(g.blendType,g.tile.pixelFormat);b.quads=g.quads;
        b.indices.reserve(g.quads.size()*6);
        for(std::size_t i=0;i<g.quads.size();++i){const auto n=static_cast<std::uint16_t>(i*4);
            for(const auto index:{0,1,2,3,2,1})b.indices.push_back(static_cast<std::uint16_t>(n+index));}
        out.atlases.push_back(std::move(b));g.quads.clear();
    }
    nonAtlas_.clear();return out;
}
}
