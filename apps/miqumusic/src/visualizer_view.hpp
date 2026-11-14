#pragma once

#include <miqutoolkit/view/view.hpp>
#include <vector>
#include <cmath>

namespace miqumusic {

enum class VisualizerMode {
    SpectrumBars,
    WaveformScope,
    GlowPeaks
};

class VisualizerView : public miqu::View {
public:
    VisualizerView();
    ~VisualizerView() override = default;

    void set_playing(bool playing);
    bool is_playing() const { return m_playing; }

    void set_mode(VisualizerMode mode);
    VisualizerMode get_mode() const { return m_mode; }
    void cycle_mode();

    void tick(double delta_time);

    void draw(cairo_t* cr, const miqu::Rect& bounds) override;
    miqu::Size measure_size() const override;

    bool on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) override;

private:
    void draw_spectrum_bars(cairo_t* cr, const miqu::Rect& bounds);
    void draw_waveform_scope(cairo_t* cr, const miqu::Rect& bounds);
    void draw_glow_peaks(cairo_t* cr, const miqu::Rect& bounds);

    bool m_playing = false;
    VisualizerMode m_mode = VisualizerMode::SpectrumBars;

    static constexpr size_t NUM_BANDS = 36;
    std::vector<float> m_levels;
    std::vector<float> m_peak_levels;
    std::vector<float> m_target_levels;

    double m_phase = 0.0;
};

} // namespace miqumusic
