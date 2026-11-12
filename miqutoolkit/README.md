# miqutoolkit

> **Modern C++20 UI Framework & Application Development Toolkit for Wayland**

`miqutoolkit` is the shared foundational UI framework powering all desktop applications in the Miqu ecosystem. It abstracts Cairo vector drawing, Pango typography, OpenGL acceleration, and Wayland surface protocols into a clean, declarative C++ widget system.

Website & Documentation: [**miqu-project.github.io/#docs**](https://miqu-project.github.io/#docs)

---

## 🔍 What is What

- **Declarative Layout Engine**: Android-inspired view hierarchy (`LinearLayout`, `GridLayout`, `RelativeLayout`, `CardView`, `TextView`, `ImageView`, `Button`, `ScrollView`).
- **Dual Surface Models**:
  - `AppWindow`: Standard floating or tiled Wayland client application surfaces.
  - `LayerSurface`: Desktop shell panels, bars, drawers, and overlays using `wlr-layer-shell`.
- **Two-Tier Configuration Model**: Runs out of the box from `/usr/share/miqutoolkit/miqutoolkit.conf`, with on-demand user customization in `~/.config/miqutoolkit/miqutoolkit.conf` (`init_user_config()` with safe timestamped archiving).
- **Decoupled Dynamic Theming**: Transparently sources active wallpaper palettes via `source = ~/.config/miqutheme/current/miqutoolkit.conf` with inotify live reload.
- **Strict RAII Safety**: Smart pointers with custom deleters for Cairo contexts, buffers, and Wayland protocols; zero manual memory leaks.

---

## 🛠️ Quick C++ Example

```cpp
#include <miqutoolkit/miqutoolkit.hpp>

int main(int argc, char** argv) {
    auto engine = miqu::AppEngine::create();
    auto window = engine->create_layer_surface("my-panel", miqu::Layer::Top);
    window->set_anchor(miqu::Anchor::Top | miqu::Anchor::Left | miqu::Anchor::Right);
    window->set_exclusive_zone(36);

    auto root = std::make_shared<miqu::LinearLayout>(miqu::Orientation::Horizontal);
    root->set_padding(8, 16);
    root->set_background_color(miqu::Config::get()->colors.surface);

    auto title = std::make_shared<miqu::TextView>("Miqu Desktop");
    title->set_font_size(miqu::Config::get()->metrics.h2_size);
    title->set_text_color(miqu::Config::get()->colors.on_surface);

    root->add_view(title);
    window->set_root_view(root);
    return engine->run();
}
```

---

## 🔨 Build & Run

```bash
# In monorepo root:
cmake --build build --target miqutoolkit

# Standalone build & install:
./make.sh
sudo ./make.sh
```
