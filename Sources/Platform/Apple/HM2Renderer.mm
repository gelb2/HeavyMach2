#import "HM2Renderer.h"
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/EAGL.h>
#include "Assets/Image.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>

namespace {struct Texture{GLuint id=0;GLsizei width=0,height=0,storageWidth=1,storageHeight=1;};}
@implementation HM2Renderer {
    NSURL* _root;
    EAGLContext* _context;
    GLuint _framebuffer,_colorbuffer;
    std::map<std::string,Texture> _textures;
    NSUInteger _textureUploads,_presentedFrames;
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
        if(_textures.contains(sprite.image))continue;
        NSString* name=[NSString stringWithUTF8String:sprite.image.c_str()];
        if(![name.lastPathComponent isEqualToString:name])throw std::runtime_error("Invalid image resource path");
        NSURL* url=[_root URLByAppendingPathComponent:name];
        NSData* encoded=[NSData dataWithContentsOfURL:url];
        if(!encoded)throw std::runtime_error("Cannot read original image: "+sprite.image);
        const auto image=hm2::decodeImage({static_cast<const std::uint8_t*>(encoded.bytes),encoded.length});
        Texture texture;texture.width=GLsizei(image.width);texture.height=GLsizei(image.height);
        while(texture.storageWidth<texture.width)texture.storageWidth*=2;
        while(texture.storageHeight<texture.height)texture.storageHeight*=2;
        GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if(texture.storageWidth>maximum||texture.storageHeight>maximum)throw std::runtime_error("Original image exceeds GPU texture limit");
        // Same original/common Texture2D path: cleared power-of-two storage,
        // original top-row-first bytes, and content-only UV coordinates.
        std::vector<std::uint8_t> pixels(std::size_t(texture.storageWidth)*texture.storageHeight*4,0);
        for(unsigned y=0;y<image.height;++y)std::copy_n(image.rgba.data()+std::size_t(y)*image.width*4,std::size_t(image.width)*4,pixels.data()+std::size_t(y)*texture.storageWidth*4);
        glGenTextures(1,&texture.id);glBindTexture(GL_TEXTURE_2D,texture.id);
        // HD Texture2D::setAntiAliasTexParameters 0x426a0, called at init.
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texture.storageWidth,texture.storageHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture.id);throw std::runtime_error("Cannot upload original PNG texture");}
        _textures.emplace(sprite.image,texture);++_textureUploads;
    }}catch(...){[EAGLContext setCurrentContext:previous];throw;}
    [EAGLContext setCurrentContext:previous];
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
        for(const auto& s:frame.sprites){
            const auto found=_textures.find(s.image);if(found==_textures.end())throw std::runtime_error("Frame uses unprepared original texture");
            if(s.columns==0||s.tile>=s.columns)throw std::runtime_error("Invalid original tile index");
            const float hx=s.size.width/2,hy=s.size.height/2,c=std::cos(s.angle),sin=std::sin(s.angle);
            const float local[]={-hx,-hy,hx,-hy,-hx,hy,hx,hy};GLfloat vertices[8];
            for(int i=0;i<4;++i){vertices[i*2]=s.center.x+local[i*2]*c-local[i*2+1]*sin;vertices[i*2+1]=s.center.y+local[i*2]*sin+local[i*2+1]*c;}
            const auto& texture=found->second;
            const float contentU=float(texture.width)/texture.storageWidth,contentV=float(texture.height)/texture.storageHeight;
            const float u0=float(s.tile)/s.columns*contentU,u1=float(s.tile+1)/s.columns*contentU;
            const GLfloat uv[]={u0,s.reverseY?0.f:contentV,u1,s.reverseY?0.f:contentV,u0,s.reverseY?contentV:0.f,u1,s.reverseY?contentV:0.f};
            glBindTexture(GL_TEXTURE_2D,found->second.id);glVertexPointer(2,GL_FLOAT,0,vertices);glTexCoordPointer(2,GL_FLOAT,0,uv);glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        }
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
- (NSUInteger)presentedFrames{return _presentedFrames;}
- (NSUInteger)contextIdentity{return reinterpret_cast<NSUInteger>((__bridge void*)_context);}
- (CGSize)drawableSize{return CGSizeMake(_width,_height);}
@end
