#pragma once

#include "miqutoolkit/view/view_group.hpp"
#include "miqutoolkit/view/view_builder.hpp"

namespace miqu {

class RelativeLayout : public ViewGroup {
public:
    RelativeLayout() = default;

    void draw(cairo_t* cr, const Rect& bounds) override;
    Size measure_size() const override;
};

class RelativeLayoutBuilder : public ViewGroupBuilder<RelativeLayoutBuilder, RelativeLayout> {
public:
    using ViewGroupBuilder<RelativeLayoutBuilder, RelativeLayout>::ViewGroupBuilder;

    static std::shared_ptr<RelativeLayoutBuilder> create() {
        return std::make_shared<RelativeLayoutBuilder>();
    }
};

} // namespace miqu
