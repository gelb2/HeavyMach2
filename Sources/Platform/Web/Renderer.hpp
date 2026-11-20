#pragma once

#include "Core/Data/Resources.hpp"
#include "Core/Types.hpp"
#include "Core/UI/Text.hpp"

#include <GLES3/gl3.h>
#include <map>
#include <span>

namespace hm2::web {

enum class MenuArtwork { native, desktop, desktopBackdrop };
struct HelpUnderlay {
    std::size_t worldEnd=0;
    std::size_t end=0;
};

// Consumes the same ordered frames as the native renderers. Owns only browser GL state.
class Renderer {
    struct Texture {
        GLuint id = 0;
        unsigned contentWidth = 0;
        unsigned contentHeight = 0;
        unsigned width = 0;
        unsigned height = 0;
        TextureSource source;
    };
    struct Vertex { Vec2 position, uv; Color color; };
    struct Draw {
        GLuint texture;
        std::optional<ClipRect> clip;
        Blend blend;
        GLint first;
        GLsizei count;
    };

    ResourceAccess& resources_;
    std::map<std::string, Texture, std::less<>> textures_;
    GLuint program_ = 0, vbo_ = 0, vao_ = 0;
    GLint extent_ = -1;
    std::vector<Vertex> vertices_;
    std::vector<Draw> draws_;
#if HM2_WEB_VALIDATION
    unsigned assetReads_ = 0, uploads_ = 0;
#endif

    void upload(std::string_view key, Texture texture, const std::uint8_t* rgba);
    static void cover(std::span<Vertex>, const Frame&);
    void collect(const Sprite&, const Texture&, const Frame&, MenuArtwork artwork, float brightness);
public:
    explicit Renderer(ResourceAccess& resources) : resources_(resources) {}
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void initialize();
    void abandonContext();
    void prepare(const Frame& owners);
    void prepareText(std::string_view key,const ui::TextBitmap&);
    void retain(const Frame& owners);
    void draw(const Frame&, int width, int height, MenuArtwork artwork, HelpUnderlay help);
#if HM2_WEB_VALIDATION
    unsigned assetReads() const { return assetReads_; }
    unsigned uploads() const { return uploads_; }
#endif
};
}
