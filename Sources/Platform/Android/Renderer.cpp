#include "Renderer.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>

namespace hm2::android {
namespace {
GLenum factor(BlendFactor value) {
    switch (value) {
    case BlendFactor::zero: return GL_ZERO;
    case BlendFactor::one: return GL_ONE;
    case BlendFactor::sourceAlpha: return GL_SRC_ALPHA;
    case BlendFactor::oneMinusSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::sourceColor: return GL_SRC_COLOR;
    }
    throw std::runtime_error("Invalid sprite blend factor");
}

void checkGL(const std::string& operation) {
    const auto error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error(operation + " failed with GL error " + std::to_string(error));
}

struct Viewport {
    GLint x, y, width, height;
    float scale;
};

void clip(const std::optional<ClipRect>& rect, const Viewport& view) {
    if (!rect) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const auto& r = *rect;
    if (!(std::isfinite(r.x) && std::isfinite(r.y) && r.width >= 0 && r.height >= 0 &&
          std::isfinite(r.width) && std::isfinite(r.height)))
        throw std::runtime_error("Invalid scene clip");
    const GLint left = std::max(view.x, GLint(std::floor(view.x + r.x * view.scale)));
    const GLint bottom = std::max(view.y, GLint(std::floor(view.y + r.y * view.scale)));
    const GLint right = std::min(view.x + view.width, GLint(std::ceil(view.x + (r.x + r.width) * view.scale)));
    const GLint top = std::min(view.y + view.height, GLint(std::ceil(view.y + (r.y + r.height) * view.scale)));
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, bottom, std::max(0, right - left), std::max(0, top - bottom));
}
}

Renderer::~Renderer() {
    for (auto& [name, texture] : textures_) glDeleteTextures(1, &texture.id);
}

void Renderer::contextLost() {
    // Texture names now belong to the replacement context. Never delete old names in it.
    textures_.clear();
}

void Renderer::upload(const std::string& key, Texture texture, GLenum format, const std::uint8_t* data) {
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (maximum <= 0 || texture.storageWidth > static_cast<unsigned>(maximum) ||
        texture.storageHeight > static_cast<unsigned>(maximum))
        throw std::runtime_error("Game texture exceeds GPU limit: " + key);

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, texture.storageWidth, texture.storageHeight,
                 0, format, GL_UNSIGNED_BYTE, data);
    try {
        checkGL("Upload " + key);
        textures_.emplace(key, texture);
    } catch (...) {
        glDeleteTextures(1, &texture.id);
        throw;
    }
}

void Renderer::prepareFrame(const Frame& frame) {
    for (const auto& sprite : frame.sprites) {
        auto source = sprite.textureSource;
        if (source.image.empty()) source.image = sprite.image;
        const auto existing = textures_.find(sprite.image);
        if (existing != textures_.end()) {
            if (existing->second.source != source) throw std::runtime_error("Texture identity changed: " + sprite.image);
            continue;
        }
        const auto image = decodeImage(resources_.read(source.image));
        Texture texture{0, image.width, image.height, std::bit_ceil(image.width), std::bit_ceil(image.height), source};
        if (source.variant == TextureVariant::originalHalfAlpha) {
            if (sprite.image != source.image + "~shd") throw std::runtime_error("Invalid shadow texture identity");
            const auto alpha = prepareOriginalPartAlpha(image);
            upload(sprite.image, texture, GL_ALPHA, alpha.alpha.data());
        } else if (texture.width == texture.storageWidth && texture.height == texture.storageHeight) {
            upload(sprite.image, texture, GL_RGBA, image.rgba.data());
        } else {
            Bytes padded(std::size_t(texture.storageWidth) * texture.storageHeight * 4, 0);
            const auto stride = std::size_t(image.width) * 4;
            for (unsigned y = 0; y < image.height; ++y)
                std::copy_n(image.rgba.data() + stride * y, stride, padded.data() + std::size_t(y) * texture.storageWidth * 4);
            upload(sprite.image, texture, GL_RGBA, padded.data());
        }
    }
}

void Renderer::prepareText(const ui::TextBitmap& bitmap, const std::string& identifier) {
    const auto& layout = bitmap.layout;
    if (!identifier.starts_with("@text/") || !layout.width || !layout.height ||
        bitmap.alpha.size() != std::size_t(layout.width) * layout.height)
        throw std::runtime_error("Invalid text bitmap");
    // Text identities are immutable until the common preparation cache evicts them.
    if (textures_.contains(identifier)) return;
    Texture texture{0, layout.width, layout.height, layout.width, layout.height, {identifier, TextureVariant::rgba}};
    upload(identifier, texture, GL_ALPHA, bitmap.alpha.data());
}

void Renderer::retain(const Frame& owners) {
    std::set<std::string_view> names;
    for (const auto& sprite : owners.sprites) names.insert(sprite.image);
    for (auto i = textures_.begin(); i != textures_.end();) {
        if (names.contains(i->first)) {
            ++i;
        } else {
            glDeleteTextures(1, &i->second.id);
            i = textures_.erase(i);
        }
    }
}

void Renderer::drawSprite(const Sprite& sprite, const Texture& texture) {
    glBindTexture(GL_TEXTURE_2D, texture.id);
    const auto& blend = sprite.blend;
    glBlendFunc(factor(blend.source), factor(blend.destination));
    glColor4f(sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a);

    if (sprite.atlasBatch) {
        const auto& batch = *sprite.atlasBatch;
        if (batch.quads.empty()) return;
        if (batch.image != sprite.image || batch.textureSource != sprite.textureSource ||
            batch.storage.width != texture.storageWidth || batch.storage.height != texture.storageHeight ||
            batch.indices.size() != batch.quads.size() * 6)
            throw std::runtime_error("Inconsistent atlas batch: " + sprite.image);
        const auto* first = batch.quads.front().vertices.data();
        glBlendFunc(factor(batch.blend.source), factor(batch.blend.destination));
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(2, GL_FLOAT, sizeof(AtlasVertex), &first->position);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(AtlasVertex), &first->color);
        glTexCoordPointer(2, GL_FLOAT, sizeof(AtlasVertex), &first->uv);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.indices.size()), GL_UNSIGNED_SHORT, batch.indices.data());
        glDisableClientState(GL_COLOR_ARRAY);
        return;
    }

    const float contentU = float(texture.width) / texture.storageWidth;
    const float contentV = float(texture.height) / texture.storageHeight;
    if (!sprite.strip.empty()) {
        if (sprite.strip.size() < 3) throw std::runtime_error("Invalid trail triangle strip");
        const auto* first = sprite.strip.data();
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glScalef(contentU, contentV, 1);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(2, GL_FLOAT, sizeof(StripVertex), &first->position);
        glTexCoordPointer(2, GL_FLOAT, sizeof(StripVertex), &first->uv);
        glColorPointer(4, GL_FLOAT, sizeof(StripVertex), &first->color);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(sprite.strip.size()));
        glDisableClientState(GL_COLOR_ARRAY);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        return;
    }

    if (sprite.columns == 0 || sprite.rows == 0 || sprite.tile / sprite.columns >= sprite.rows)
        throw std::runtime_error("Invalid sprite tile: " + sprite.image);
    const float halfWidth = sprite.size.width * .5f, halfHeight = sprite.size.height * .5f;
    const float cosine = std::cos(sprite.angle), sine = std::sin(sprite.angle);
    std::array<Vec2, 4> corners = {{{-halfWidth, -halfHeight}, {halfWidth, -halfHeight},
                                    {-halfWidth, halfHeight}, {halfWidth, halfHeight}}};
    for (auto& corner : corners)
        corner = {sprite.center.x + corner.x * cosine - corner.y * sine,
                  sprite.center.y + corner.x * sine + corner.y * cosine};
    const unsigned column = sprite.tile % sprite.columns, row = sprite.tile / sprite.columns;
    const float u0 = float(column) / sprite.columns * contentU, u1 = float(column + 1) / sprite.columns * contentU;
    const float v0 = float(row) / sprite.rows * contentV, v1 = float(row + 1) / sprite.rows * contentV;
    const float bottom = sprite.reverseY ? v0 : v1, top = sprite.reverseY ? v1 : v0;
    const GLfloat uv[] = {u0, bottom, u1, bottom, u0, top, u1, top};
    glVertexPointer(2, GL_FLOAT, sizeof(Vec2), corners.data());
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::draw(const Frame& frame, int drawableWidth, int drawableHeight) {
    if (drawableWidth <= 0 || drawableHeight <= 0 || !(frame.width > 0 && frame.height > 0))
        throw std::runtime_error("Invalid game surface size");
    const float scale = std::min(drawableWidth / frame.width, drawableHeight / frame.height);
    const GLint width = std::lround(frame.width * scale), height = std::lround(frame.height * scale);
    const Viewport viewport{(drawableWidth - width) / 2, (drawableHeight - height) / 2, width, height, scale};
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(viewport.x, viewport.y, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, frame.width, 0, frame.height, -1, 1);
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    std::optional<ClipRect> previousClip;
    for (const auto& sprite : frame.sprites) {
        const bool slotBackdrop = sprite.image == "SelSlot_BG.png";
        const auto drawClip = slotBackdrop ? std::nullopt : sprite.clip;
        if (drawClip != previousClip) {
            clip(drawClip, viewport);
            previousClip = drawClip;
        }
        const auto texture = textures_.find(sprite.image);
        if (texture == textures_.end()) throw std::runtime_error("Unprepared frame texture: " + sprite.image);
        // The HD slot backdrop is composed on a centered 768x1024 canvas.
        // Cover the Android surface with this artwork; controls and input
        // retain the exact geometry emitted by the shared slot screen.
        if (slotBackdrop) {
            const float fitX = frame.width / 768.f;
            const float fitY = frame.height / 1024.f;
            const float cover = std::max(fitX, fitY) / std::min(fitX, fitY);
            glPushMatrix();
            glTranslatef(frame.width * .5f, frame.height * .5f, 0);
            glScalef(cover, cover, 1);
            glTranslatef(-frame.width * .5f, -frame.height * .5f, 0);
            drawSprite(sprite, texture->second);
            glPopMatrix();
        } else {
            drawSprite(sprite, texture->second);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    checkGL("Draw frame");
}
}
