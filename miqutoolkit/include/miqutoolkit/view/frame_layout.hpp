#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include "miqutoolkit/view/view_builder.hpp"

namespace miqu {

class FrameLayout : public ViewGroup {
public:
    FrameLayout() = default;

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
    Size measure_size(int avail_width) const override;
};

class FrameLayoutBuilder : public ViewGroupBuilder<FrameLayoutBuilder, FrameLayout> {
public:
    using ViewGroupBuilder<FrameLayoutBuilder, FrameLayout>::ViewGroupBuilder;

    static std::shared_ptr<FrameLayoutBuilder> create() {
        return std::make_shared<FrameLayoutBuilder>();
    }
};

} // namespace miqu
