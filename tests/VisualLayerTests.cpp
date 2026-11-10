#include "Core/Presentation/VisualLayer.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
using namespace hm2;
namespace {
bool near(float a,float b){return std::abs(a-b)<.0002f;}
template<class F>void fails(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}assert(failed);}
VisualImage image(std::string id="tile-A"){
    VisualImage i;i.tile={id,"original.png",{120,60},{128,64},4,2,1,false,{0,0},{1,.5f,0,1}};
    i.screenPosition={100,200};i.alpha=.5f;i.useAtlas=true;i.tileNo=5;return i;
}
}
int main(){
    assert(originalAtlasColorByte(1)==255&&originalAtlasColorByte(.5f)==127&&originalAtlasColorByte(0)==0);
    assert(originalAtlasColorByte(-1)==0&&originalAtlasColorByte(2)==254);
    assert(originalAtlasColorByte(std::numeric_limits<float>::max())==255);
    fails([]{(void)originalAtlasColorByte(std::numeric_limits<float>::quiet_NaN());});
    assert(originalTileBlend(0,1).source==BlendFactor::one&&originalAtlasBlend(0,3).source==BlendFactor::sourceAlpha);
    assert(originalTileBlend(3,1).source==BlendFactor::one&&originalTileBlend(3,1).destination==BlendFactor::one);
    assert(originalAtlasBlend(3,1).source==BlendFactor::oneMinusSourceAlpha&&originalAtlasBlend(3,1).destination==BlendFactor::oneMinusSourceAlpha);
    assert(originalAtlasBlend(1,1).source==BlendFactor::sourceColor&&originalAtlasBlend(2,1).destination==BlendFactor::one);
    fails([]{(void)originalAtlasBlend(5,1);});

    auto a=image();const auto q=originalAtlasQuad(a);
    assert(near(q.vertices[0].position.x,85)&&near(q.vertices[0].position.y,215)); // TL
    assert(near(q.vertices[1].position.x,85)&&near(q.vertices[1].position.y,185)); // BL
    assert(near(q.vertices[2].position.x,115)&&near(q.vertices[2].position.y,215)); // TR
    assert(near(q.vertices[3].position.x,115)&&near(q.vertices[3].position.y,185)); // BR
    assert(near(q.vertices[0].uv.x,30.f/128)&&near(q.vertices[0].uv.y,30.f/64));
    assert(near(q.vertices[3].uv.x,60.f/128)&&near(q.vertices[3].uv.y,60.f/64));
    assert((q.vertices[0].color==ColorBytes{255,127,0,127}));
    a.reverse=3;const auto flipped=originalAtlasQuad(a);
    assert(flipped.vertices[0].position.x==115&&flipped.vertices[0].position.y==185);
    assert(flipped.vertices[0].uv.x==q.vertices[0].uv.x&&flipped.vertices[0].uv.y==q.vertices[0].uv.y);
    a=image();a.scale={2,3};a.tile.forRetina=true;const auto retina=originalAtlasQuad(a);
    assert(retina.vertices[0].position.x==85&&retina.vertices[0].position.y==222.5f);
    a=image();a.tile.origin={2,3};const auto ignoredOrigin=originalAtlasQuad(a);
    assert(ignoredOrigin.vertices[0].position.x==85&&ignoredOrigin.vertices[0].position.y==215);
    a.angle=std::numbers::pi_v<float>*.5f;const auto rotated=originalAtlasQuad(a);
    assert(near(rotated.vertices[0].position.x,82)&&near(rotated.vertices[0].position.y,187));
    assert(near(rotated.vertices[1].position.x,112)&&near(rotated.vertices[1].position.y,187));

    VisualLayer layer(2);auto prepared=image("prepared-first");layer.prepareAtlas(prepared.tile);
    a=image();layer.submitImage(a);auto b=image("tile-B");layer.submitImage(b);
    prepared.tileNo=2;layer.submitImage(prepared);
    auto direct=image("direct");direct.useAtlas=false;direct.tile.origin={2,3};layer.submitImage(direct);
    Sprite custom;custom.image="manual-original.png";layer.submitNonAtlas(custom);layer.submitNonAtlas(custom);
    assert(layer.visualCount()==3&&layer.visualCapacity()==4&&layer.atlasCount()==3);
    auto frame=layer.drain();assert(layer.visualCount()==0&&layer.atlasCount()==3);
    assert(frame.nonAtlas.size()==3&&frame.atlases.size()==3);
    assert(frame.nonAtlas[0].image=="original.png"&&frame.nonAtlas[1].image=="manual-original.png");
    // Direct Tile2D subtracts origin even at zero angle; atlas doesn't.
    assert(near(frame.nonAtlas[0].strip[0].position.x,83)&&near(frame.nonAtlas[0].strip[0].position.y,212));
    assert(frame.nonAtlas[0].strip[0].color.g==.5f&&frame.nonAtlas[0].strip[0].color.a==.5f);
    assert(frame.atlases[0].tileIdentity=="prepared-first"&&frame.atlases[1].tileIdentity=="tile-A"&&frame.atlases[2].tileIdentity=="tile-B");
    assert(frame.atlases[0].indices==std::vector<std::uint16_t>({0,1,2,3,2,1}));
    Frame flattened;frame.append(flattened);assert(flattened.sprites.size()==6);
    assert(flattened.sprites[0].image=="original.png"&&flattened.sprites[2].image=="manual-original.png");
    assert(near(flattened.sprites[4].strip[0].uv.x,.25f)&&near(flattened.sprites[4].strip[0].uv.y,.5f));
    assert(near(flattened.sprites[4].strip[0].color.g,127.f/255));
    Frame native;frame.appendBatches(native);assert(native.sprites.size()==6);
    assert(!native.sprites[0].atlasBatch&&native.sprites[3].atlasBatch);
    const auto retained=native.sprites[4].atlasBatch;
    assert(retained->image==native.sprites[4].image&&retained->quads[0].vertices[0].uv.x==30.f/128);
    assert(retained->quads[0].vertices[0].color.g==127&&retained->indices.size()==6);
    Frame moved;std::move(frame).appendBatches(moved);assert(moved.sprites.size()==6&&frame.atlases.empty());
    assert(moved.sprites[4].atlasBatch->indices.size()==6);
    assert(layer.drain().atlases.empty());
    assert(retained->quads.size()==1); // Frame payload survives layer reuse.
    // Group order persists when next-frame insertion order changes.
    layer.submitImage(b);layer.submitImage(a);auto next=layer.drain();
    assert(next.atlases[0].tileIdentity=="tile-A"&&next.atlases[1].tileIdentity=="tile-B");
    a.blendType=3;layer.submitImage(a);auto blend=layer.drain();assert(layer.atlasCount()==4);
    assert(blend.atlases[0].blend.source==BlendFactor::oneMinusSourceAlpha);
    // Equal resource names with distinct Tile2D identities remain distinct.
    auto alias=image("alias");layer.submitImage(alias);assert(layer.atlasCount()==5);
    auto changed=alias;changed.tile.storage.width=256;fails([&]{layer.submitImage(changed);});
    auto hidden=image();hidden.show=false;hidden.tileNo=100;layer.submitImage(hidden);assert(hidden.tileNo==100);
    hidden.show=true;hidden.isInScreen=false;layer.submitImage(hidden);assert(hidden.tileNo==100);
    auto clamp=image();clamp.tileNo=999;layer.submitImage(clamp);assert(clamp.tileNo==7);
    clamp.tileNo=999;clamp.useAtlas=false;layer.submitImage(clamp);assert(clamp.tileNo==999);

    VisualLayer growth;auto unit=image();
    for(unsigned n=0;n<33;++n){unit.screenPosition.x=float(n);growth.submitImage(unit);}
    assert(growth.atlasCapacity(0)==64);growth.prepareAtlas(unit.tile); // Existing atlas is not cleared.
    auto grown=growth.drain();assert(grown.atlases[0].quads.size()==33&&grown.atlases[0].capacity==64);
    const auto& indices=grown.atlases[0].indices;
    assert(indices.size()==198&&indices[192]==128&&indices[197]==129);
    assert(grown.atlases[0].quads[0].vertices[0].position.x==-15&&grown.atlases[0].quads[32].vertices[0].position.x==17);
    growth.submitImage(unit);assert(growth.atlasCapacity(0)==64);growth.clearVisuals();
    assert(growth.drain().atlases[0].quads.size()==1);
    growth.removeAllAtlases();assert(growth.atlasCount()==0);growth.submitImage(unit);assert(growth.atlasCapacity(0)==32);
    VisualLayer indexLimit;
    for(unsigned n=0;n<16384;++n)indexLimit.submitImage(unit);
    assert(indexLimit.atlasCapacity(0)==16384);fails([&]{indexLimit.submitImage(unit);});
    auto maximum=indexLimit.drain();assert(maximum.atlases[0].indices.back()==65533);
    fails([]{VisualLayer invalid(0);});
    auto invalid=image();invalid.tile.identity.clear();fails([&]{growth.submitImage(invalid);});
    invalid=image();invalid.tile.columns=0;fails([&]{(void)originalAtlasQuad(invalid);});
}
