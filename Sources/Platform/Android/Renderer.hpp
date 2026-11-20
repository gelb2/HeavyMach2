#pragma once

#include "Core/Data/Resources.hpp"
#include "Core/Types.hpp"
#include "Core/UI/Text.hpp"
#include <GLES/gl.h>
#include <map>
#include <string>

namespace hm2::android {

// GLSurfaceView owns the GLES1 context and default framebuffer.
// All operations run on the thread that owns that context.
class Renderer final {
public:
    explicit Renderer(ResourceAccess& resources) : resources_(resources) {}
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void contextLost();
    void prepareFrame(const Frame& frame);
    void prepareText(const ui::TextBitmap& bitmap, const std::string& identifier);
    void retain(const Frame& owners);
    void draw(const Frame& frame, int drawableWidth, int drawableHeight);

private:
    struct Texture {
        GLuint id = 0;
        unsigned width = 0, height = 0, storageWidth = 0, storageHeight = 0;
        TextureSource source;
    };
    ResourceAccess& resources_;
    std::map<std::string, Texture, std::less<>> textures_;
    void upload(const std::string& key, Texture texture, GLenum format, const std::uint8_t* data);
    void drawSprite(const Sprite& sprite, const Texture& texture);
};
}
