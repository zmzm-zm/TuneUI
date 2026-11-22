#include <tuneui/render/Page.h>

#include <tuneui/render/Renderer.h>

#include <algorithm>
#include <utility>

namespace tuneui {

// ---- Floor ----------------------------------------------------------------

Floor::Floor(std::string name, int level)
    : level_(level), name_(std::move(name)) {}

Floor& Floor::setLevel(int level) {
    level_ = level;
    return *this;
}

Floor& Floor::setVisible(bool visible) {
    visible_ = visible;
    return *this;
}

Floor& Floor::setClip(const Rect& rect) {
    clip_ = rect;
    hasClip_ = true;
    return *this;
}

Floor& Floor::clearClip() {
    hasClip_ = false;
    clip_ = Rect{};
    return *this;
}

Floor& Floor::setOffset(Vec2 offset) {
    offset_ = offset;
    return *this;
}

Floor& Floor::addRect(const Rect& dst, const Color& color) {
    QuadCommand command;
    command.kind = QuadCommand::Kind::Solid;
    command.dst = dst;
    command.color = color;
    commands_.push_back(command);
    return *this;
}

Floor& Floor::addTexturedRect(const Rect& dst, const Rect& uv, const Color& tint,
                              std::shared_ptr<const Texture> texture) {
    QuadCommand command;
    command.kind = QuadCommand::Kind::Textured;
    command.dst = dst;
    command.uv = uv;
    command.color = tint;
    command.texture = std::move(texture);
    commands_.push_back(command);
    return *this;
}

void Floor::clearCommands() {
    commands_.clear();
}

// ---- Page -----------------------------------------------------------------

void Page::setBackground(const Color& color) {
    background_ = color;
    hasBackground_ = true;
}

void Page::clearBackground() {
    hasBackground_ = false;
}

void Page::addFloor(Floor floor) {
    floors_.push_back(std::make_unique<Floor>(std::move(floor)));
}

Floor* Page::findFloor(const std::string& name) {
    for (auto& floor : floors_) {
        if (floor->name() == name) {
            return floor.get();
        }
    }
    return nullptr;
}

const Floor* Page::findFloor(const std::string& name) const {
    for (const auto& floor : floors_) {
        if (floor->name() == name) {
            return floor.get();
        }
    }
    return nullptr;
}

void Page::render(Renderer& renderer) {
    if (!renderer.frameActive()) {
        return;
    }

    // The background is an ordered clear: pending caller geometry is emitted
    // first, and an active outer scissor is honored.
    if (hasBackground_) {
        renderer.clear(background_);
    }

    // Ascending level; std::stable_sort keeps insertion order for ties.
    renderOrder_.clear();
    for (const auto& floor : floors_) {
        if (floor->visible()) {
            renderOrder_.push_back(floor.get());
        }
    }
    std::stable_sort(renderOrder_.begin(), renderOrder_.end(),
                     [](const Floor* a, const Floor* b) { return a->level() < b->level(); });

    const Rect page = renderer.logicalBounds();
    bool pageClipActive = false;
    Rect activeClip{};

    for (const Floor* floor : renderOrder_) {
        // An empty floor must not cause a scissor transition (which flushes).
        if (floor->commandCount() == 0) {
            continue;
        }

        Rect wanted{};
        bool wantsClip = false;
        if (floor->hasClip()) {
            if (!intersectRects(floor->clip(), page, wanted)) {
                continue; // clip is entirely off-screen: draw nothing
            }
            // A clip covering the whole page is the same as no clip (an outer
            // scissor still applies); avoiding it keeps neighbours batched.
            wantsClip = !rectContains(wanted, page);
        }

        if (wantsClip != pageClipActive || (wantsClip && !rectsEqual(wanted, activeClip))) {
            if (pageClipActive) {
                renderer.popScissor();
                pageClipActive = false;
            }
            if (wantsClip) {
                // pushScissor intersects with the current clip, including any
                // scissor the caller set before page.render().
                renderer.pushScissor(wanted);
                pageClipActive = true;
                activeClip = wanted;
            }
        }

        const Vec2 offset = floor->offset();
        for (const QuadCommand& command : floor->commands()) {
            Rect dst = command.dst;
            dst.x += offset.x;
            dst.y += offset.y;
            if (command.kind == QuadCommand::Kind::Solid) {
                renderer.drawRect(dst, command.color);
            } else {
                const unsigned int textureId =
                    command.texture ? command.texture->id() : renderer.whiteTextureId();
                renderer.drawTexturedRect(dst, command.uv, command.color, textureId);
            }
        }
    }

    // Pop only the clip this page pushed; the caller's outer clip stays intact.
    if (pageClipActive) {
        renderer.popScissor();
    }
}

} // namespace tuneui
