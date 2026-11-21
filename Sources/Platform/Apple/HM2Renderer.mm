#import "HM2Renderer.h"
#import <OpenGLES/ES1/gl.h>
#include "Core/Data/ResourceName.hpp"
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/EAGL.h>
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
struct Texture{GLuint id=0;GLsizei width=0,height=0,storageWidth=1,storageHeight=1;unsigned bytesPerPixel=4;hm2::TextureSource source;};
GLenum factor(hm2::BlendFactor value){
    switch(value){
        case hm2::BlendFactor::zero:return GL_ZERO;
        case hm2::BlendFactor::one:return GL_ONE;
        case hm2::BlendFactor::sourceAlpha:return GL_SRC_ALPHA;
        case hm2::BlendFactor::oneMinusSourceAlpha:return GL_ONE_MINUS_SRC_ALPHA;
        case hm2::BlendFactor::sourceColor:return GL_SRC_COLOR;
    }throw std::runtime_error("Invalid original blend factor");
}
}
@implementation HM2Renderer {
    NSURL* _root;
    EAGLContext* _context;
    GLuint _framebuffer,_colorbuffer;
    std::map<std::string,Texture> _textures;
    NSUInteger _textureUploads,_imageTextureUploads,_textTextureUploads,_alphaTextureUploads,_imageResourceReads,_imageDecodeCalls,_presentedFrames,_drawCalls,_atlasDrawCalls;
    NSUInteger _textureDeletes;
    GLsizei _width,_height;
    CGFloat _surfaceWidth,_surfaceHeight;
    CGFloat _pointWidth,_pointHeight;
    BOOL _captureNext;
    NSData* _captured;
    CGRect _viewport;
    hm2::Size _logical;
}
- (instancetype)initWithResourceRoot:(NSURL*)root {
    if(!(self=[super init]))return nil;_root=root;
    _context=[[EAGLContext alloc]initWithAPI:kEAGLRenderingAPIOpenGLES1];
    if(!_context)throw std::runtime_error("Cannot create HM2 GLES1 context");
    return self;
}
- (void)dealloc {
    EAGLContext* previous=EAGLContext.currentContext;
    if([EAGLContext setCurrentContext:_context]){
        for(const auto& [name,t]:_textures)glDeleteTextures(1,&t.id);
        if(_framebuffer)glDeleteFramebuffersOES(1,&_framebuffer);
        if(_colorbuffer)glDeleteRenderbuffersOES(1,&_colorbuffer);
        [EAGLContext setCurrentContext:previous==_context?nil:previous];
    }
}
- (void)prepareFrame:(const hm2::Frame&)frame {
    EAGLContext* previous=EAGLContext.currentContext;
    if(![EAGLContext setCurrentContext:_context])throw std::runtime_error("Cannot activate HM2 GLES1 context");
    try{for(const auto& sprite:frame.sprites){
        const auto sourceName=sprite.textureSource.image.empty()?sprite.image:sprite.textureSource.image;
        const hm2::TextureSource source{sourceName,sprite.textureSource.variant};
        const bool alpha=source.variant==hm2::TextureVariant::originalHalfAlpha;
        if(alpha&&sprite.image!=sourceName+"~shd")throw std::runtime_error("Invalid original alpha cache identity");
        if(const auto found=_textures.find(sprite.image);found!=_textures.end()){
            if(found->second.source!=source)throw std::runtime_error("Texture cache identity changed source or format");
            continue;
        }
        NSString* name=[NSString stringWithUTF8String:sourceName.c_str()];
        if(!hm2::validResourceName(sourceName))throw std::runtime_error("Invalid image resource path");
        NSURL* url=[_root URLByAppendingPathComponent:name];
        ++_imageResourceReads;NSData* encoded=[NSData dataWithContentsOfURL:url];
        if(!encoded)throw std::runtime_error("Cannot read original image: "+sprite.image);
        ++_imageDecodeCalls;const auto image=hm2::decodeImage({static_cast<const std::uint8_t*>(encoded.bytes),encoded.length});
        Texture texture;texture.source=source;texture.width=GLsizei(image.width);texture.height=GLsizei(image.height);
        while(texture.storageWidth<texture.width)texture.storageWidth*=2;
        while(texture.storageHeight<texture.height)texture.storageHeight*=2;
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(texture.storageWidth>maximum||texture.storageHeight>maximum)throw std::runtime_error("Original image exceeds GPU texture limit");
        // Same original/common Texture2D path: cleared power-of-two storage,
        // original top-row-first bytes, and content-only UV coordinates.
        std::vector<std::uint8_t> pixels;GLenum format=GL_RGBA;
        if(alpha){
            texture.bytesPerPixel=1;
            auto prepared=hm2::prepareOriginalPartAlpha(image);
            if(prepared.storageWidth!=unsigned(texture.storageWidth)||prepared.storageHeight!=unsigned(texture.storageHeight))
                throw std::runtime_error("Original A8 storage differs from prepared tile");
            pixels=std::move(prepared.alpha);format=GL_ALPHA;
        }else{
            pixels.resize(std::size_t(texture.storageWidth)*texture.storageHeight*4,0);
            for(unsigned y=0;y<image.height;++y)std::copy_n(image.rgba.data()+std::size_t(y)*image.width*4,std::size_t(image.width)*4,pixels.data()+std::size_t(y)*texture.storageWidth*4);
        }
        glGenTextures(1,&texture.id);glBindTexture(GL_TEXTURE_2D,texture.id);
        // HD Texture2D::setAntiAliasTexParameters 0x426a0, called at init.
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,format,texture.storageWidth,texture.storageHeight,0,format,GL_UNSIGNED_BYTE,pixels.data());
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture.id);throw std::runtime_error("Cannot upload original PNG texture");}
        _textures.emplace(sprite.image,texture);++_textureUploads;++_imageTextureUploads;if(alpha)++_alphaTextureUploads;
    }}catch(...){[EAGLContext setCurrentContext:previous];throw;}
    [EAGLContext setCurrentContext:previous];
}
- (void)prepareText:(const hm2::ui::TextBitmap&)bitmap identifier:(const std::string&)identifier {
    const auto& layout=bitmap.layout;
    if(!identifier.starts_with("@text/")||!layout.width||!layout.height||
       bitmap.alpha.size()!=std::size_t(layout.width)*layout.height)
        throw std::runtime_error("Invalid prepared A8 text resource");
    EAGLContext* previous=EAGLContext.currentContext;
    if(![EAGLContext setCurrentContext:_context])throw std::runtime_error("Cannot activate HM2 text preparation");
    try {
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(layout.width>unsigned(maximum)||layout.height>unsigned(maximum))throw std::runtime_error("Text exceeds GPU texture limit");
        Texture texture;texture.bytesPerPixel=1;texture.source={identifier,hm2::TextureVariant::rgba};texture.width=texture.storageWidth=GLsizei(layout.width);texture.height=texture.storageHeight=GLsizei(layout.height);
        const auto old=_textures.find(identifier);
        if(old==_textures.end())glGenTextures(1,&texture.id);else texture.id=old->second.id;
        glBindTexture(GL_TEXTURE_2D,texture.id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        // Original GL_ALPHA returns (1,1,1,A). Preserve that sampling contract;
        // callers use SRC_ALPHA/ONE_MINUS_SRC_ALPHA and prepare on text change.
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,texture.width,texture.height,0,GL_ALPHA,GL_UNSIGNED_BYTE,bitmap.alpha.data());
        if(glGetError()!=GL_NO_ERROR){if(old==_textures.end())glDeleteTextures(1,&texture.id);throw std::runtime_error("Cannot upload original A8 text");}
        _textures.insert_or_assign(identifier,texture);++_textureUploads;++_textTextureUploads;
    }catch(...){[EAGLContext setCurrentContext:previous];throw;}
    [EAGLContext setCurrentContext:previous];
}
- (void)retainPreparedTextures:(const hm2::Frame&)owners {
    // Called only after an original GUI/world owner changes, never per sprite
    // or per draw. The frame lists prepared resources, not just visible ones.
    std::set<std::string,std::less<>> retained;for(const auto& sprite:owners.sprites)retained.insert(sprite.image);
    EAGLContext* previous=EAGLContext.currentContext;
    if(![EAGLContext setCurrentContext:_context])throw std::runtime_error("Cannot activate HM2 resource release");
    for(auto i=_textures.begin();i!=_textures.end();){
        if(retained.contains(i->first)){++i;continue;}
        glDeleteTextures(1,&i->second.id);++_textureDeletes;i=_textures.erase(i);
    }
    const auto error=glGetError();[EAGLContext setCurrentContext:previous];
    if(error!=GL_NO_ERROR)throw std::runtime_error("Cannot release original owner textures");
}
- (void)allocate:(CAEAGLLayer*)layer {
    _pointWidth=layer.bounds.size.width;_pointHeight=layer.bounds.size.height;
    const CGFloat width=layer.bounds.size.width*layer.contentsScale,height=layer.bounds.size.height*layer.contentsScale;
    if(_framebuffer&&_surfaceWidth==width&&_surfaceHeight==height)return;
    if(!_framebuffer){glGenFramebuffersOES(1,&_framebuffer);glGenRenderbuffersOES(1,&_colorbuffer);}
    glBindFramebufferOES(GL_FRAMEBUFFER_OES,_framebuffer);glBindRenderbufferOES(GL_RENDERBUFFER_OES,_colorbuffer);
    if(![_context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:layer])throw std::runtime_error("Cannot allocate HM2 drawable");
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES,GL_COLOR_ATTACHMENT0_OES,GL_RENDERBUFFER_OES,_colorbuffer);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES,GL_RENDERBUFFER_WIDTH_OES,&_width);glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES,GL_RENDERBUFFER_HEIGHT_OES,&_height);
    if(glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES)!=GL_FRAMEBUFFER_COMPLETE_OES)throw std::runtime_error("Incomplete HM2 framebuffer");
    _surfaceWidth=width;_surfaceHeight=height;
}
- (void)drawFrame:(const hm2::Frame&)frame layer:(CAEAGLLayer*)layer {
    if(frame.width<=0||frame.height<=0)throw std::runtime_error("Invalid frame extent");
    EAGLContext* previous=EAGLContext.currentContext;
    if(![EAGLContext setCurrentContext:_context])throw std::runtime_error("Cannot activate HM2 renderer");
    try{[self allocate:layer];glBindFramebufferOES(GL_FRAMEBUFFER_OES,_framebuffer);
        glViewport(0,0,_width,_height);glDisable(GL_SCISSOR_TEST);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        const float scale=std::min(float(_width)/frame.width,float(_height)/frame.height);
        const GLint w=std::lround(frame.width*scale),h=std::lround(frame.height*scale),x=(_width-w)/2,y=(_height-h)/2;
        glViewport(x,y,w,h);_viewport=CGRectMake(x,y,w,h);_logical={frame.width,frame.height};
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,frame.width,0,frame.height,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1,1,1,1);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        std::optional<hm2::ClipRect> currentClip;
        for(const auto& s:frame.sprites){
            if(s.clip!=currentClip){
                currentClip=s.clip;
                if(currentClip){const auto r=*currentClip;
                    if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.width)||!std::isfinite(r.height)||r.width<0||r.height<0)throw std::runtime_error("Invalid validation viewport clip");
                    const GLint left=std::max(x,GLint(std::floor(x+r.x*scale))),bottom=std::max(y,GLint(std::floor(y+r.y*scale)));
                    const GLint right=std::min(x+w,GLint(std::ceil(x+(r.x+r.width)*scale))),top=std::min(y+h,GLint(std::ceil(y+(r.y+r.height)*scale)));
                    glEnable(GL_SCISSOR_TEST);glScissor(left,bottom,std::max(0,right-left),std::max(0,top-bottom));
                }else glDisable(GL_SCISSOR_TEST);
            }
            const auto found=_textures.find(s.image);if(found==_textures.end())throw std::runtime_error("Frame uses unprepared original texture: "+s.image);
            glBlendFunc(factor(s.blend.source),factor(s.blend.destination));
            glColor4f(s.color.r,s.color.g,s.color.b,s.color.a);
            const auto& texture=found->second;
            if(s.atlasBatch){
                const auto& batch=*s.atlasBatch;if(batch.quads.empty())continue;
                if(batch.image!=s.image||batch.textureSource!=s.textureSource||batch.storage.width!=texture.storageWidth||batch.storage.height!=texture.storageHeight||batch.indices.size()!=batch.quads.size()*6)
                    throw std::runtime_error("Invalid prepared atlas batch");
                const auto* vertices=batch.quads.front().vertices.data();
                glBlendFunc(factor(batch.blend.source),factor(batch.blend.destination));
                glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
                glEnableClientState(GL_COLOR_ARRAY);glBindTexture(GL_TEXTURE_2D,texture.id);
                glVertexPointer(2,GL_FLOAT,sizeof(hm2::AtlasVertex),&vertices->position);
                glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(hm2::AtlasVertex),&vertices->color);
                glTexCoordPointer(2,GL_FLOAT,sizeof(hm2::AtlasVertex),&vertices->uv);
                glDrawElements(GL_TRIANGLES,GLsizei(batch.indices.size()),GL_UNSIGNED_SHORT,batch.indices.data());
                ++_drawCalls;++_atlasDrawCalls;
                glDisableClientState(GL_COLOR_ARRAY);continue;
            }
            const float contentU=float(texture.width)/texture.storageWidth,contentV=float(texture.height)/texture.storageHeight;
            if(!s.strip.empty()){
                if(s.strip.size()<3)throw std::runtime_error("Invalid original trail triangle strip");
                static_assert(std::is_standard_layout_v<hm2::StripVertex>);
                const auto* first=s.strip.data();
                glMatrixMode(GL_TEXTURE);glLoadIdentity();glScalef(contentU,contentV,1);glMatrixMode(GL_MODELVIEW);
                glEnableClientState(GL_COLOR_ARRAY);glBindTexture(GL_TEXTURE_2D,texture.id);
                glVertexPointer(2,GL_FLOAT,sizeof(hm2::StripVertex),&first->position);glTexCoordPointer(2,GL_FLOAT,sizeof(hm2::StripVertex),&first->uv);glColorPointer(4,GL_FLOAT,sizeof(hm2::StripVertex),&first->color);glDrawArrays(GL_TRIANGLE_STRIP,0,GLsizei(s.strip.size()));++_drawCalls;glDisableClientState(GL_COLOR_ARRAY);
                glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);continue;
            }
            if(s.columns==0||s.rows==0||s.tile/std::uint64_t(s.columns)>=s.rows)throw std::runtime_error("Invalid original tile index");
            const float hx=s.size.width/2,hy=s.size.height/2,c=std::cos(s.angle),sin=std::sin(s.angle);
            const float local[]={-hx,-hy,hx,-hy,-hx,hy,hx,hy};GLfloat vertices[8];
            for(int i=0;i<4;++i){vertices[i*2]=s.center.x+local[i*2]*c-local[i*2+1]*sin;vertices[i*2+1]=s.center.y+local[i*2]*sin+local[i*2+1]*c;}
            const float u0=float(s.tile%s.columns)/s.columns*contentU,u1=float(s.tile%s.columns+1)/s.columns*contentU;
            const float v0=float(s.tile/s.columns)/s.rows*contentV,v1=float(s.tile/s.columns+1)/s.rows*contentV;
            const GLfloat uv[]={u0,s.reverseY?v0:v1,u1,s.reverseY?v0:v1,u0,s.reverseY?v1:v0,u1,s.reverseY?v1:v0};
            glBindTexture(GL_TEXTURE_2D,found->second.id);glVertexPointer(2,GL_FLOAT,0,vertices);glTexCoordPointer(2,GL_FLOAT,0,uv);glDrawArrays(GL_TRIANGLE_STRIP,0,4);++_drawCalls;
        }
        glDisable(GL_SCISSOR_TEST);
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("HM2 GLES1 submission failed");
        if(_captureNext){
            NSMutableData* bytes=[NSMutableData dataWithLength:NSUInteger(_width)*_height*4];
            glReadPixels(0,0,_width,_height,GL_RGBA,GL_UNSIGNED_BYTE,bytes.mutableBytes);
            if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("HM2 framebuffer capture failed");
            _captured=bytes;_captureNext=NO;
        }
        glBindRenderbufferOES(GL_RENDERBUFFER_OES,_colorbuffer);if(![_context presentRenderbuffer:GL_RENDERBUFFER_OES])throw std::runtime_error("HM2 present failed");++_presentedFrames;
    }catch(...){[EAGLContext setCurrentContext:previous];throw;}
    [EAGLContext setCurrentContext:previous];
}
- (BOOL)logicalPoint:(CGPoint)point result:(hm2::Vec2*)result {
    if(!_width||!_height||CGRectIsEmpty(_viewport)||!result)return NO;
    const float x=point.x*_width/(_pointWidth?:1),y=_height-point.y*_height/(_pointHeight?:1);
    if(!CGRectContainsPoint(_viewport,CGPointMake(x,y)))return NO;
    *result={(x-float(_viewport.origin.x))*_logical.width/float(_viewport.size.width),(y-float(_viewport.origin.y))*_logical.height/float(_viewport.size.height)};return YES;
}
- (NSData*)readRGBA {
    return _captured;
}
- (void)captureNextFrame{_captureNext=YES;}
- (NSUInteger)textureUploads{return _textureUploads;}
- (NSUInteger)residentTextures{return _textures.size();}
- (NSUInteger)textureDeletes{return _textureDeletes;}
- (NSUInteger)residentTextureBytes{NSUInteger total=0;for(const auto& [name,texture]:_textures){(void)name;total+=NSUInteger(texture.storageWidth)*NSUInteger(texture.storageHeight)*texture.bytesPerPixel;}return total;}
- (NSUInteger)imageTextureUploads{return _imageTextureUploads;}
- (NSUInteger)textTextureUploads{return _textTextureUploads;}
- (NSUInteger)alphaTextureUploads{return _alphaTextureUploads;}
- (NSUInteger)imageResourceReads{return _imageResourceReads;}
- (NSUInteger)imageDecodeCalls{return _imageDecodeCalls;}
- (NSUInteger)presentedFrames{return _presentedFrames;}
- (NSUInteger)drawCalls{return _drawCalls;}
- (NSUInteger)atlasDrawCalls{return _atlasDrawCalls;}
- (NSUInteger)contextIdentity{return reinterpret_cast<NSUInteger>((__bridge void*)_context);}
- (CGSize)drawableSize{return CGSizeMake(_width,_height);}
@end
