#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <tuneui/render/Math.h>

namespace tuneui {

class Renderer;
class Texture;

// One retained quad. Maps 1:1 onto a JSON object once serialization lands.
struct QuadCommand {
    enum class Kind {
        Solid,
        Textured,
    };

    Kind kind = Kind::Solid;
    Rect dst{};
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f}; // Textured only
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
    // Shared ownership keeps the texture alive as long as a retained floor
    // references it; the GL id is resolved at replay time, so recreating the
    // texture with the same object is safe. null = built-in white texture.
    // Stage 3 serializes an asset key (path/name), never this pointer.
    std::shared_ptr<const Texture> texture;
};

// One layer of a Page. Floors are drawn in ascending level order; floors that
// share a level keep the order they were added in.
//
// A Floor is also a stand-alone configuration object: build one, chain the
// setters, then hand it to Page::addFloor(). Page owns the layer afterwards;
// the source object is only a template (copied or moved in).
//
// Configuration is axis-aligned quads only: no rotation, no scale, no nested
// clipping, no raw GL callbacks. A floor's clip stays fixed in page space while
// its offset moves the content, which is what a scrolling list needs.
class Floor {
public:
    // Stand-alone configuration: Floor("hud") / Floor("hud", 10).
    Floor(std::string name, int level = 0);

    int level() const { return level_; }
    const std::string& name() const { return name_; }

    bool visible() const { return visible_; }
    bool hasClip() const { return hasClip_; }
    Rect clip() const { return clip_; }
    Vec2 offset() const { return offset_; }

    // Setters return *this so a floor can be configured in one expression:
    //   Floor hud("hud");
    //   hud.setLevel(10).setClip(rect).addRect(...);
    Floor& setLevel(int level);
    Floor& setVisible(bool visible);
    Floor& setClip(const Rect& rect);
    Floor& clearClip();
    Floor& setOffset(Vec2 offset);

    Floor& addRect(const Rect& dst, const Color& color);
    Floor& addTexturedRect(const Rect& dst, const Rect& uv, const Color& tint,
                           std::shared_ptr<const Texture> texture = nullptr);
    void clearCommands();
    std::size_t commandCount() const { return commands_.size(); }
    const std::vector<QuadCommand>& commands() const { return commands_; }

private:
    int level_ = 0;
    std::string name_;
    bool visible_ = true;
    bool hasClip_ = false;
    Rect clip_{};
    Vec2 offset_{0.0f, 0.0f};
    std::vector<QuadCommand> commands_;
};

// A full-screen scene composed of floors.
//
// The caller owns the frame: call Renderer::beginFrame() before render() and
// endFrame()/swapBuffers() after it. Page::render() is a no-op outside an
// active frame. An outer scissor is preserved: floor clips are intersected with
// it using the renderer's scissor stack and the stack depth is restored on
// exit. The background clear honors an active outer scissor.
class Page {
public:
    Page() = default;

    // Optional background. Painted with Renderer::clear(), which covers the
    // whole framebuffer and ignores blending (see README limitations).
    void setBackground(const Color& color);
    void clearBackground();
    bool hasBackground() const { return hasBackground_; }
    Color background() const { return background_; }

    // Hands over a fully configured floor; there is no way to add an empty
    // floor and configure it afterwards. Takes its argument by value, so an
    // lvalue is copied and a temporary is moved; pass std::move(floor) to
    // avoid the copy. The argument is not linked to the stored floor.
    //
    // Returns nothing: the page owns the floor now. Use findFloor()/floorAt()
    // to reach it later; stored floors keep their address as more are added
    // (they live on the heap behind unique_ptr).
    void addFloor(Floor floor);
    std::size_t floorCount() const { return floors_.size(); }
    Floor& floorAt(std::size_t index) { return *floors_.at(index); }
    const Floor& floorAt(std::size_t index) const { return *floors_.at(index); }
    Floor* findFloor(const std::string& name);
    const Floor* findFloor(const std::string& name) const;

    void render(Renderer& renderer);

private:
    std::vector<std::unique_ptr<Floor>> floors_; // insertion order
    std::vector<Floor*> renderOrder_;            // scratch, rebuilt per frame
    bool hasBackground_ = false;
    Color background_{0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace tuneui
