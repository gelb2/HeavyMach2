#include "Renderer.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>

namespace hm2::web {
namespace {
GLuint compile(GLenum type, const char* source) {
    const GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);
    GLint success=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    if(success) return shader;
    char error[1024]{};
    glGetShaderInfoLog(shader,sizeof error,nullptr,error);
    glDeleteShader(shader);
    throw std::runtime_error(std::string("WebGL shader: ")+error);
}
GLenum factor(BlendFactor value) {
    switch(value) {
    case BlendFactor::zero: return GL_ZERO;
    case BlendFactor::one: return GL_ONE;
    case BlendFactor::sourceAlpha: return GL_SRC_ALPHA;
    case BlendFactor::oneMinusSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::sourceColor: return GL_SRC_COLOR;
    }
    throw std::runtime_error("Unsupported sprite blend factor");
}
Bytes rgbaAlpha(std::span<const std::uint8_t> alpha) {
    Bytes rgba(alpha.size()*4,255);
    for(std::size_t i=0;i<alpha.size();++i) rgba[i*4+3]=alpha[i];
    return rgba;
}
bool coversMenu(std::string_view image,bool desktop) {
    if(image=="SelSlot_BG.png") return true;
    return desktop&&image.size()==11&&image.starts_with("Base00")&&image.ends_with(".png");
}
struct PanelArtwork { float sourceTop; ClipRect fittedPanel; };
std::optional<PanelArtwork> fittedMenuPanel(std::string_view image) {
    // Keep the title and built-in frames in their authored 768x1024 positions.
    // Draw the rest of the screen from clean art below those baked details.
    if(image=="ShopBG.png") return PanelArtwork{.45f,{58,579,652,445}};
    if(image=="CmdCenterBG.png") return PanelArtwork{.25f,{65,790,638,234}};
    return {};
}
}

Renderer::~Renderer() {
    for(const auto& [key,texture]:textures_) glDeleteTextures(1,&texture.id);
    glDeleteBuffers(1,&vbo_);
    glDeleteVertexArrays(1,&vao_);
    glDeleteProgram(program_);
}

void Renderer::abandonContext() {
    textures_.clear();
    vbo_=vao_=program_=0;
    extent_=-1;
}

void Renderer::initialize() {
    static constexpr char vertex[]=R"(#version 300 es
layout(location=0) in vec2 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 color;
uniform vec2 extent;
out vec2 texcoord;
out vec4 tint;
void main() {
    gl_Position=vec4(position * 2.0 / extent - 1.0, 0.0, 1.0);
    texcoord=uv;
    tint=color;
})";
    static constexpr char fragment[]=R"(#version 300 es
precision mediump float;
uniform sampler2D sprite;
in vec2 texcoord;
in vec4 tint;
out vec4 pixel;
void main() { pixel=texture(sprite, texcoord) * tint; }
)";
    const GLuint vs=compile(GL_VERTEX_SHADER,vertex);
    GLuint fs=0;
    try { fs=compile(GL_FRAGMENT_SHADER,fragment); }
    catch(...) { glDeleteShader(vs); throw; }
    program_=glCreateProgram();
    glAttachShader(program_,vs);
    glAttachShader(program_,fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint success=0;
    glGetProgramiv(program_,GL_LINK_STATUS,&success);
    if(!success) {
        char error[1024]{};
        glGetProgramInfoLog(program_,sizeof error,nullptr,error);
        throw std::runtime_error(std::string("WebGL program: ")+error);
    }
    glUseProgram(program_);
    extent_=glGetUniformLocation(program_,"extent");
    glUniform1i(glGetUniformLocation(program_,"sprite"),0);
    glGenVertexArrays(1,&vao_);
    glGenBuffers(1,&vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));
    glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
}

void Renderer::upload(std::string_view key, Texture texture, const std::uint8_t* data) {
    GLint maxSize=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
    if(!texture.width||!texture.height||maxSize<=0||
       texture.width>unsigned(maxSize)||texture.height>unsigned(maxSize))
        throw std::runtime_error("Texture exceeds browser GPU limits: "+std::string(key));
    glGenTextures(1,&texture.id);
    glBindTexture(GL_TEXTURE_2D,texture.id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,texture.width,texture.height,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    if(glGetError()!=GL_NO_ERROR) {
        glDeleteTextures(1,&texture.id);
        throw std::runtime_error("Texture upload failed: "+std::string(key));
    }
    textures_.emplace(key,std::move(texture));
#if HM2_WEB_VALIDATION
    ++uploads_;
#endif
}

void Renderer::prepare(const Frame& owners) {
    for(const auto& sprite:owners.sprites) {
        auto source=sprite.textureSource;
        if(source.image.empty()) source.image=sprite.image;
        const auto existing=textures_.find(sprite.image);
        if(existing!=textures_.end()) {
            if(existing->second.source!=source) throw std::runtime_error("Texture identity collision: "+sprite.image);
            continue;
        }
        const auto image=decodeImage(resources_.read(source.image));
#if HM2_WEB_VALIDATION
        ++assetReads_;
#endif
        const Texture texture{0,image.width,image.height,std::bit_ceil(image.width),std::bit_ceil(image.height),source};
        if(source.variant==TextureVariant::originalHalfAlpha) {
            const auto rgba=rgbaAlpha(prepareOriginalPartAlpha(image).alpha);
            upload(sprite.image,texture,rgba.data());
        } else if(texture.width==image.width&&texture.height==image.height) {
            upload(sprite.image,texture,image.rgba.data());
        } else {
            Bytes rgba(std::size_t(texture.width)*texture.height*4);
            const auto sourceStride=std::size_t(image.width)*4;
            for(unsigned y=0;y<image.height;++y)
                std::copy_n(image.rgba.data()+sourceStride*y,sourceStride,rgba.data()+std::size_t(texture.width)*4*y);
            upload(sprite.image,texture,rgba.data());
        }
    }
}

void Renderer::prepareText(std::string_view key, const ui::TextBitmap& bitmap) {
    if(textures_.contains(key)) return;
    const auto width=bitmap.layout.width,height=bitmap.layout.height;
    if(!width||!height||bitmap.alpha.size()!=std::size_t(width)*height)
        throw std::runtime_error("Invalid browser text image");
    const auto rgba=rgbaAlpha(bitmap.alpha);
    upload(key,{0,width,height,width,height,{std::string(key),TextureVariant::rgba}},rgba.data());
}

void Renderer::retain(const Frame& owners) {
    std::set<std::string_view> names;
    for(const auto& sprite:owners.sprites) names.insert(sprite.image);
    for(auto it=textures_.begin();it!=textures_.end();) {
        if(names.contains(it->first)) ++it;
        else {
            glDeleteTextures(1,&it->second.id);
            it=textures_.erase(it);
        }
    }
}

void Renderer::collect(const Sprite& sprite,const Texture& texture,const Frame& frame,MenuArtwork artwork,float brightness) {
    const bool desktop=artwork!=MenuArtwork::native;
    auto first=vertices_.size();
    Blend blend=sprite.blend;
    const float uScale=float(texture.contentWidth)/texture.width;
    const float vScale=float(texture.contentHeight)/texture.height;
    if(sprite.atlasBatch) {
        const auto& batch=*sprite.atlasBatch;
        blend=batch.blend;
        if(batch.storage.width!=float(texture.width)||batch.storage.height!=float(texture.height))
            throw std::runtime_error("Browser atlas and texture extents differ");
        for(auto index:batch.indices) {
            const auto& v=batch.quads.at(index/4).vertices[index%4];
            constexpr float scale=1.f/255.f;
            vertices_.push_back({v.position,v.uv,{v.color.r*scale,v.color.g*scale,v.color.b*scale,v.color.a*scale}});
        }
    } else if(!sprite.strip.empty()) {
        for(std::size_t i=2;i<sprite.strip.size();++i) {
            for(const auto index:{i-2,i-1,i}) {
                const auto& v=sprite.strip[index];
                vertices_.push_back({v.position,{v.uv.x*uScale,v.uv.y*vScale},v.color});
            }
        }
    } else {
        if(!sprite.columns||!sprite.rows||sprite.tile/sprite.columns>=sprite.rows)
            throw std::runtime_error("Invalid browser sprite tile");
        const float hw=sprite.size.width*.5f,hh=sprite.size.height*.5f;
        const float cosine=std::cos(sprite.angle),sine=std::sin(sprite.angle);
        const float col=float(sprite.tile%sprite.columns),row=float(sprite.tile/sprite.columns);
        const float u0=col/sprite.columns*uScale,u1=(col+1)/sprite.columns*uScale;
        const float v0=row/sprite.rows*vScale,v1=(row+1)/sprite.rows*vScale;
        const float bottom=sprite.reverseY?v0:v1,top=sprite.reverseY?v1:v0;
        const std::array<Vec2,4> position={{{-hw,-hh},{hw,-hh},{-hw,hh},{hw,hh}}};
        const std::array<Vec2,4> uv={{{u0,bottom},{u1,bottom},{u0,top},{u1,top}}};
        for(int i:{0,1,2,2,1,3}) {
            const auto p=position[i];
            vertices_.push_back({{sprite.center.x+p.x*cosine-p.y*sine,
                                  sprite.center.y+p.x*sine+p.y*cosine},uv[i],sprite.color});
        }
    }
    const auto count=GLsizei(vertices_.size()-first);
    if(!count) return;
    if(brightness!=1.f) {
        for(auto& vertex:std::span(vertices_).subspan(first,count)) {
            vertex.color.r*=brightness;
            vertex.color.g*=brightness;
            vertex.color.b*=brightness;
        }
    }
    auto clip=sprite.clip;
    const auto panel=desktop?fittedMenuPanel(sprite.image):std::nullopt;
    if(panel) {
        const bool backdropOnly=artwork==MenuArtwork::desktopBackdrop;
        const auto end=vertices_.size();
        if(!backdropOnly) {
            vertices_.resize(end+count);
            std::copy_n(vertices_.begin()+first,count,vertices_.begin()+end);
        }
        auto cropped=std::span(vertices_).subspan(first,count);
        const float keep=1.f-panel->sourceTop;
        for(auto& v:cropped) {
            // Apply the same vertical crop to the sampled texture and its mesh.
            v.position.y*=keep;
            v.uv.y=panel->sourceTop*vScale+v.uv.y*keep;
        }
        cover(cropped,frame);
        draws_.push_back({texture.id,{},blend,GLint(first),count});
        if(backdropOnly) return;
        first=end;
        const float fit=std::min(frame.width/768.f,frame.height/1024.f);
        const auto p=panel->fittedPanel;
        clip=ClipRect{frame.width*.5f+(p.x-384)*fit,frame.height*.5f+(p.y-512)*fit,
                      p.width*fit,p.height*fit};
    }
    // Cover standalone portrait art. Menus keep the common hit projection.
    if(coversMenu(sprite.image,desktop)) {
        cover(std::span(vertices_).subspan(first,count),frame);
        clip.reset();
    }
    // Preserve painter order. Only adjacent commands with equal state can combine.
    if(!draws_.empty()) {
        auto& last=draws_.back();
        if(last.texture==texture.id&&last.clip==clip&&last.blend.source==blend.source&&
           last.blend.destination==blend.destination) {
            last.count+=count;
            return;
        }
    }
    draws_.push_back({texture.id,clip,blend,GLint(first),count});
}

void Renderer::cover(std::span<Vertex> vertices,const Frame& frame) {
    Vec2 low=vertices.front().position,high=low;
    for(const auto& v:vertices) {
        low={std::min(low.x,v.position.x),std::min(low.y,v.position.y)};
        high={std::max(high.x,v.position.x),std::max(high.y,v.position.y)};
    }
    if(low.x==high.x||low.y==high.y) return;
    const float scale=std::max(frame.width/(high.x-low.x),frame.height/(high.y-low.y));
    for(auto& v:vertices) {
        v.position.x=(v.position.x-(high.x+low.x)*.5f)*scale+frame.width*.5f;
        v.position.y=(v.position.y-(high.y+low.y)*.5f)*scale+frame.height*.5f;
    }
}

void Renderer::draw(const Frame& frame,int width,int height,MenuArtwork artwork,HelpUnderlay help) {
    if(width<=0||height<=0||!(frame.width>0&&frame.height>0)) return;
    vertices_.clear();
    draws_.clear();
    for(std::size_t index=0;index<frame.sprites.size();++index) {
        const auto& sprite=frame.sprites[index];
        const bool belowHelp=index<help.end;
        if(belowHelp&&index>=help.worldEnd&&!coversMenu(sprite.image,true)&&!fittedMenuPanel(sprite.image))
            continue; // Live controls must not duplicate the examples in the help image.
        const auto texture=textures_.find(sprite.image);
        if(texture==textures_.end()) throw std::runtime_error("Unprepared web texture: "+sprite.image);
        collect(sprite,texture->second,frame,belowHelp?MenuArtwork::desktopBackdrop:artwork,belowHelp?.30f:1.f);
    }
    glViewport(0,0,width,height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glUseProgram(program_);
    glUniform2f(extent_,frame.width,frame.height);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices_.size()*sizeof(Vertex)),vertices_.data(),GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE0);
    const float sx=float(width)/frame.width,sy=float(height)/frame.height;
    for(const auto& command:draws_) {
        if(command.clip) {
            const auto r=*command.clip;
            const int left=std::clamp(int(std::floor(r.x*sx)),0,width);
            const int bottom=std::clamp(int(std::floor(r.y*sy)),0,height);
            const int right=std::clamp(int(std::ceil((r.x+r.width)*sx)),left,width);
            const int top=std::clamp(int(std::ceil((r.y+r.height)*sy)),bottom,height);
            glEnable(GL_SCISSOR_TEST);
            glScissor(left,bottom,right-left,top-bottom);
        } else glDisable(GL_SCISSOR_TEST);
        glBlendFunc(factor(command.blend.source),factor(command.blend.destination));
        glBindTexture(GL_TEXTURE_2D,command.texture);
        glDrawArrays(GL_TRIANGLES,command.first,command.count);
    }
    glDisable(GL_SCISSOR_TEST);
}
}
