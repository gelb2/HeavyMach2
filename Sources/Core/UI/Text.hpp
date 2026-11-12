#pragma once
#include "Core/Types.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hm2 {class VisualLayer;struct VisualTile;}
namespace hm2::ui {
// Original NSTextAlignment/UILineBreakMode values, not CSS alignment names.
enum class TextAlign : int {left=0,center=1,right=2};
struct TextRasterRequest {
    // Keep the original table key / UI creation site beside the resolved text.
    std::string sourceId,text,fontName;
    Size dimensions;
    float fontSize=0;
    TextAlign alignment=TextAlign::left;
    bool retina=false; // The original overload without Retina: passes false.
    int originalDeviceType=0;
    bool operator==(const TextRasterRequest&)const;
};
struct TextRasterLayout {
    Size content;
    float fontSize=0;
    unsigned width=0,height=0; // Original next-power-of-two storage dimensions.
    bool forRetina=false;
};
[[nodiscard]] TextRasterLayout textRasterLayout(const TextRasterRequest&);
struct TextInkBand {unsigned firstRow=0,lastRow=0,left=0,right=0;};
struct TextRasterMetrics {
    Size drawnSize;
    float ascender=0,descender=0,leading=0,lineHeight=0;
    // Measured nonempty pixel-row bands; these are not inferred line baselines.
    std::vector<TextInkBand> inkBands;
    std::string resolvedFontName,osVersion;
    // Names selected by the OS for strings unsupported by the requested font.
    // This is font-cascade diagnostic data, not proof of legacy-iOS equality.
    std::vector<std::string> cascadeFontNames;
};
struct TextBitmap {
    TextRasterRequest request;
    TextRasterLayout layout;
    TextRasterMetrics metrics;
    // Tightly packed DeviceGray8 bytes consumed as original GL_ALPHA/A8.
    // Row 0 is bitmap storage row 0, not a claim about screen top/bottom.
    std::vector<std::uint8_t> alpha;
};
void measureTextInk(TextBitmap&);
class TextRasterizer {
public:
    virtual ~TextRasterizer()=default;
    // Preparation only. A missing original font is an error, never substitution.
    virtual TextBitmap rasterize(const TextRasterRequest&)=0;
};
// Explicit preparation cache. append() has no access to a rasterizer and cannot
// trigger text drawing, font lookup, allocation of an OS view, or texture baking.
class TextPreparationCache {
    std::vector<std::shared_ptr<const TextBitmap>> entries_;
public:
    std::shared_ptr<const TextBitmap> prepare(TextRasterizer&,const TextRasterRequest&);
    void clear(){entries_.clear();}
    [[nodiscard]] std::size_t size()const{return entries_.size();}
};
class Text {
    std::string image_;
    std::shared_ptr<const TextBitmap> bitmap_;
    Color color_;
public:
    Vec2 position,scale{1,1};
    float angle=0,alpha=1;
    bool visible=true;
    Text(std::string preparedImage,std::shared_ptr<const TextBitmap>);
    void setColor(std::uint8_t r,std::uint8_t g,std::uint8_t b);
    void append(Frame&)const;
    [[nodiscard]] const TextBitmap& bitmap()const{return *bitmap_;}
};

struct NumericAtlas {
    std::string image;
    Size pixels;
    unsigned columns=16,rows=1;
    bool forRetina=false;
    Blend blend{}; // Original pixelFormat1 RGBA numeric atlas, blendType0.
};
// QobImageFont's digits/punctuation atlas is separate from QobText's OS font.
// UTF-8 bytes unsupported by the atlas stay hidden and retain their spacing.
class ImageFont {
    struct Glyph {unsigned tile=0;bool visible=true;float x=0;Vec2 scale{1,1};};
    NumericAtlas atlas_;
    std::vector<Glyph> glyphs_; // Original child insertion / draw order.
    float pitch_=0,alignRate_=0,containerX_=0;
    Vec2 calculatedScale_;
    std::int32_t number_=0;
    void relayoutContainer();
public:
    Vec2 position,scale{1,1};
    bool visible=true;
    explicit ImageFont(NumericAtlas);
    void setNumber(std::int32_t);
    void setText(std::string_view);
    void setPitch(float); // Original Retina pitch is halved at this setter.
    void setAlignRate(float); // Original setter does not immediately relayout.
    void setBlend(Blend);
    void tick(); // Original scale-change relayout, before frame collection.
    void append(Frame&)const;
    // Original numeric children are QobImage(useAtlas=true). The resource
    // consumer supplies the original Tile2D identity/storage/format explicitly.
    void append(VisualLayer&,const VisualTile&,unsigned originalBlendType=0)const;
    [[nodiscard]] float width()const{return float(glyphs_.size())*pitch_;}
    [[nodiscard]] std::size_t count()const{return glyphs_.size();}
    [[nodiscard]] std::int32_t number()const{return number_;}
};
}
