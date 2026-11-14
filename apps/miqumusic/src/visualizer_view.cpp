#include "visualizer_view.hpp"
#include <miqutoolkit/core/config.hpp>
#include <algorithm>
#include <cstdlib>

namespace miqumusic {

VisualizerView::VisualizerView()
    : m_levels(NUM_BANDS, 0.05f),
      m_peak_levels(NUM_BANDS, 0.05f),
      m_target_levels(NUM_BANDS, 0.05f) {
    set_focusable(false);
}

void VisualizerView::set_playing(bool playing) {
    if (m_playing != playing) {
        m_playing = playing;
        request_redraw();
    }
}

void VisualizerView::set_mode(VisualizerMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        request_redraw();
    }
}

void VisualizerView::cycle_mode() {
    switch (m_mode) {
        case VisualizerMode::SpectrumBars:
            m_mode = VisualizerMode::WaveformScope;
            break;
        case VisualizerMode::WaveformScope:
            m_mode = VisualizerMode::GlowPeaks;
            break;
        case VisualizerMode::GlowPeaks:
            m_mode = VisualizerMode::SpectrumBars;
            break;
    }
    request_redraw();
}

void VisualizerView::tick(double delta_time) {
    m_phase += delta_time * 4.0;
    if (m_phase > 2.0 * M_PI * 1000.0) {
        m_phase = 0.0;
    }

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        if (m_playing) {
            double freq_weight = 1.0 - (static_cast<double>(i) / NUM_BANDS) * 0.45;
            double wave1 = std::sin(m_phase * 1.5 + i * 0.42);
            double wave2 = std::cos(m_phase * 2.8 - i * 0.65);
            double wave3 = std::sin(m_phase * 0.7 + i * 1.1);

            double norm = (wave1 * 0.4 + wave2 * 0.35 + wave3 * 0.25 + 1.0) * 0.5;
            float raw = static_cast<float>(norm * freq_weight);

            // Add dynamic variation
            float variation = static_cast<float>((std::rand() % 25) / 100.0f);
            m_target_levels[i] = std::clamp(raw * 0.85f + variation * 0.25f, 0.08f, 1.0f);
        } else {
            // Resting state: gentle slow wave
            double rest = (std::sin(m_phase * 0.5 + i * 0.2) + 1.0) * 0.06 + 0.03;
            m_target_levels[i] = static_cast<float>(rest);
        }

        // Smooth interpolation
        float smooth_factor = m_target_levels[i] > m_levels[i] ? 0.35f : 0.18f;
        m_levels[i] += (m_target_levels[i] - m_levels[i]) * smooth_factor;

        // Peak decay
        if (m_levels[i] > m_peak_levels[i]) {
            m_peak_levels[i] = m_levels[i];
        } else {
            m_peak_levels[i] = std::max(0.04f, m_peak_levels[i] - 0.015f);
        }
    }

    request_redraw();
}

miqu::Size VisualizerView::measure_size() const {
    return miqu::Size(240, 48);
}

bool VisualizerView::on_mouse_button(int lx, int ly, miqu::MouseButton button, bool pressed, const miqu::Rect& bounds) {
    if (button == miqu::MouseButton::Left && pressed) {
        cycle_mode();
        return true;
    }
    return miqu::View::on_mouse_button(lx, ly, button, pressed, bounds);
}

void VisualizerView::draw(cairo_t* cr, const miqu::Rect& bounds) {
    if (!cr || !is_visible() || bounds.width <= 0 || bounds.height <= 0) return;

    cairo_save(cr);
    cairo_rectangle(cr, bounds.x, bounds.y, bounds.width, bounds.height);
    cairo_clip(cr);

    switch (m_mode) {
        case VisualizerMode::SpectrumBars:
            draw_spectrum_bars(cr, bounds);
            break;
        case VisualizerMode::WaveformScope:
            draw_waveform_scope(cr, bounds);
            break;
        case VisualizerMode::GlowPeaks:
            draw_glow_peaks(cr, bounds);
            break;
    }

    cairo_restore(cr);
}

void VisualizerView::draw_spectrum_bars(cairo_t* cr, const miqu::Rect& bounds) {
    const double bar_gap = 2.5;
    const double total_gaps = (NUM_BANDS - 1) * bar_gap;
    const double bar_width = std::max(2.0, (bounds.width - total_gaps) / NUM_BANDS);
    const double max_h = bounds.height - 4.0;

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        double bx = bounds.x + i * (bar_width + bar_gap);
        double bh = std::max(3.0, m_levels[i] * max_h);
        double by = bounds.y + bounds.height - bh;

        double progress = static_cast<double>(i) / NUM_BANDS;
        // Cyan to Purple neon gradient
        double r = 0.0 + progress * 0.75;
        double g = 0.85 - progress * 0.45;
        double b = 0.95 + progress * 0.05;

        // Draw bar with rounded top
        cairo_set_source_rgba(cr, r, g, b, 0.88);
        double corner_r = std::min(bar_width / 2.0, 3.0);

        cairo_new_sub_path(cr);
        cairo_arc(cr, bx + corner_r, by + corner_r, corner_r, M_PI, 3 * M_PI / 2);
        cairo_arc(cr, bx + bar_width - corner_r, by + corner_r, corner_r, 3 * M_PI / 2, 0);
        cairo_line_to(cr, bx + bar_width, bounds.y + bounds.height);
        cairo_line_to(cr, bx, bounds.y + bounds.height);
        cairo_close_path(cr);
        cairo_fill(cr);

        // Draw peak dot if active
        if (m_playing && m_peak_levels[i] > 0.15f) {
            double py = bounds.y + bounds.height - (m_peak_levels[i] * max_h) - 2.0;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.75);
            cairo_rectangle(cr, bx, py, bar_width, 1.5);
            cairo_fill(cr);
        }
    }
}

void VisualizerView::draw_waveform_scope(cairo_t* cr, const miqu::Rect& bounds) {
    const double mid_y = bounds.y + bounds.height / 2.0;
    const double step_x = static_cast<double>(bounds.width) / (NUM_BANDS - 1);
    const double max_amp = bounds.height * 0.45;

    cairo_set_line_width(cr, 2.2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    // Glowing base line
    cairo_set_source_rgba(cr, 0.1, 0.85, 0.95, 0.9);
    cairo_move_to(cr, bounds.x, mid_y);

    for (size_t i = 0; i < NUM_BANDS; ++i) {
        double x = bounds.x + i * step_x;
        double sign = (i % 2 == 0) ? 1.0 : -1.0;
        double y = mid_y + sign * m_levels[i] * max_amp;
        cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
}

void VisualizerView::draw_glow_peaks(cairo_t* cr, const miqu::Rect& bounds) {
    draw_spectrum_bars(cr, bounds);

    // Add subtle ambient floor glow
    cairo_pattern_t* glow = cairo_pattern_create_linear(bounds.x, bounds.y + bounds.height - 18,
                                                       bounds.x, bounds.y + bounds.height);
    cairo_pattern_add_color_stop_rgba(glow, 0.0, 0.0, 0.8, 1.0, 0.0);
    cairo_pattern_add_color_stop_rgba(glow, 1.0, 0.0, 0.9, 1.0, 0.25);
    cairo_set_source(cr, glow);
    cairo_rectangle(cr, bounds.x, bounds.y + bounds.height - 18, bounds.width, 18);
    cairo_fill(cr);
    cairo_pattern_destroy(glow);
}

} // namespace miqumusic
