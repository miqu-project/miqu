#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace miqu {

/**
 * @brief Velocity tracker and exponential-decay fling animator for touchpad scrolling.
 *
 * Tracks instantaneous scroll velocity during continuous finger motion and coasts
 * smoothly after finger lift (wl_pointer.axis_stop), boosting momentum when the
 * user performs quick repeated swipes in the same direction.
 */
class KineticScroller {
public:
    using Clock = std::chrono::steady_clock;

    void record_scroll(double pixel_delta, uint32_t axis_source) {
        // WL_POINTER_AXIS_SOURCE_FINGER == 1
        if (axis_source != 1) {
            cancel();
            return;
        }

        auto now = Clock::now();
        if (m_flinging) {
            // Preserve residual velocity briefly so repeated fast swipes
            // in the same direction build momentum rather than resetting to zero.
            double elapsed_since_fling = std::chrono::duration<double>(now - m_last_frame_time).count();
            m_residual_velocity = (elapsed_since_fling < 0.25) ? m_velocity : 0.0;
            m_flinging = false;
            m_velocity = 0.0;
        }

        if (m_has_last_sample) {
            double dt = std::chrono::duration<double>(now - m_last_sample_time).count();
            if (dt > 0.001 && dt < 0.15) {
                double inst_v = pixel_delta / dt;
                // Exponential moving average to smooth out libinput event jitter
                m_tracked_velocity = (m_tracked_velocity == 0.0)
                    ? inst_v
                    : (0.65 * inst_v + 0.35 * m_tracked_velocity);
            } else if (dt >= 0.15) {
                m_tracked_velocity = 0.0;
                m_residual_velocity = 0.0;
            }
        }
        m_last_sample_time = now;
        m_has_last_sample = true;
    }

    bool start_fling(uint32_t axis_source) {
        if (axis_source != 1 || !m_has_last_sample) {
            m_tracked_velocity = 0.0;
            m_residual_velocity = 0.0;
            m_has_last_sample = false;
            return false;
        }

        auto now = Clock::now();
        double since_last = std::chrono::duration<double>(now - m_last_sample_time).count();
        m_has_last_sample = false;

        // If fingers paused on the touchpad before lifting, do not fling
        if (since_last > 0.12) {
            m_tracked_velocity = 0.0;
            m_residual_velocity = 0.0;
            return false;
        }

        double v = m_tracked_velocity;
        // Carry over momentum on quick repeated swipes in the same direction
        if ((v > 0.0 && m_residual_velocity > 0.0) || (v < 0.0 && m_residual_velocity < 0.0)) {
            v += m_residual_velocity * 0.75;
        }
        m_tracked_velocity = 0.0;
        m_residual_velocity = 0.0;

        constexpr double kMinFlingVelocity = 60.0;    // px/s
        constexpr double kMaxFlingVelocity = 12000.0; // px/s
        if (std::abs(v) < kMinFlingVelocity) {
            return false;
        }

        m_velocity = std::clamp(v, -kMaxFlingVelocity, kMaxFlingVelocity);
        m_flinging = true;
        m_last_frame_time = now;
        return true;
    }

    // Advance fling physics during draw(). Returns true if another frame should be scheduled.
    bool step(double& scroll_y, double max_scroll) {
        if (!m_flinging) return false;
        if (max_scroll <= 0.0) {
            cancel();
            return false;
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - m_last_frame_time).count();
        m_last_frame_time = now;

        dt = std::clamp(dt, 0.001, 0.05);

        constexpr double kFriction = 2.4; // exponential decay rate (1/s) — light, silky glide
        double decay = std::exp(-kFriction * dt);
        double avg_v = m_velocity * (1.0 - decay) / (kFriction * dt);
        double new_scroll = std::clamp(scroll_y + avg_v * dt, 0.0, max_scroll);
        bool hit_edge = (new_scroll == 0.0 && m_velocity < 0.0) ||
                        (new_scroll == max_scroll && m_velocity > 0.0);

        bool moved = (new_scroll != scroll_y);
        scroll_y = new_scroll;
        m_velocity *= decay;

        constexpr double kStopVelocity = 15.0; // px/s
        if (hit_edge || std::abs(m_velocity) < kStopVelocity) {
            m_flinging = false;
            m_velocity = 0.0;
            return moved;
        }
        return true;
    }

    void cancel() {
        m_flinging = false;
        m_velocity = 0.0;
        m_tracked_velocity = 0.0;
        m_residual_velocity = 0.0;
        m_has_last_sample = false;
    }

    bool is_flinging() const { return m_flinging; }

private:
    bool m_flinging = false;
    bool m_has_last_sample = false;
    double m_velocity = 0.0;
    double m_tracked_velocity = 0.0;
    double m_residual_velocity = 0.0;
    Clock::time_point m_last_sample_time{};
    Clock::time_point m_last_frame_time{};
};

} // namespace miqu
