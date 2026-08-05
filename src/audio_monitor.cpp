#include "audio_monitor.hpp"

#include <algorithm>

namespace vmwake {

namespace {

MonitorConfig sanitize(MonitorConfig cfg) {
    cfg.sample_interval_ms = std::max(cfg.sample_interval_ms, 1);
    cfg.confirm_samples = std::max(cfg.confirm_samples, 1);
    cfg.arm_after_ms = std::max(cfg.arm_after_ms, 0);
    cfg.cooldown_ms = std::max(cfg.cooldown_ms, 0);
    if (cfg.silence_threshold_db >= cfg.play_threshold_db) {
        cfg.silence_threshold_db = cfg.play_threshold_db - 0.1f;
    }
    return cfg;
}

} // namespace

AudioMonitor::AudioMonitor(MonitorConfig cfg) : cfg_(sanitize(cfg)) {
    reset();
}

void AudioMonitor::reset() {
    state_ = MonitorState::Disconnected;
    last_state_ = MonitorState::Disconnected;
    armed_ = false;
    playing_confirm_ = 0;
    silence_confirm_ = 0;
    silence_start_ms_ = -1;
    cooldown_until_ms_ = 0;
    silence_elapsed_ms_ = 0;
    restart_pending_ = false;
}

void AudioMonitor::set_config(const MonitorConfig& cfg) {
    cfg_ = sanitize(cfg);
    reset();
}

void AudioMonitor::enter_state(MonitorState s, std::int64_t now_ms) {
    state_ = s;
    switch (s) {
    case MonitorState::TrackingSilence:
        if (silence_start_ms_ < 0) silence_start_ms_ = now_ms;
        break;
    case MonitorState::Cooldown:
        cooldown_until_ms_ = now_ms + cfg_.cooldown_ms;
        break;
    default:
        break;
    }
}

MonitorEvent AudioMonitor::update(float peak_db, std::int64_t now_ms, bool api_ok) {
    MonitorEvent ev;
    if (!api_ok) {
        if (state_ != MonitorState::Disconnected) {
            reset();
            last_state_ = MonitorState::Disconnected;
        }
        ev.state = state_;
        return ev;
    }

    const bool playing = peak_db >= cfg_.play_threshold_db;
    const bool silent = peak_db <= cfg_.silence_threshold_db;

    if (playing) {
        if (playing_confirm_ < cfg_.confirm_samples) ++playing_confirm_;
        silence_confirm_ = 0;
        silence_start_ms_ = -1;
    } else if (silent) {
        playing_confirm_ = 0;
        if (silence_confirm_ < cfg_.confirm_samples) ++silence_confirm_;
    } else {
        // Ambiguous level (between thresholds): reset both counters.
        playing_confirm_ = 0;
        silence_confirm_ = 0;
        silence_start_ms_ = -1;
    }

    const bool playing_ok = playing_confirm_ >= cfg_.confirm_samples;
    const bool silence_ok = silent && silence_confirm_ >= cfg_.confirm_samples;

    switch (state_) {
    case MonitorState::Disconnected:
        // Reconnected: start fresh. If audio is playing right away we wait for
        // silence; otherwise begin tracking silence immediately.
        if (playing_ok) {
            enter_state(MonitorState::WaitingSilence, now_ms);
        } else {
            // Ambiguous or silent input -> treat as silent so we can arm later.
            enter_state(MonitorState::TrackingSilence, now_ms);
        }
        break;

    case MonitorState::WaitingSilence:
        if (silence_ok) {
            enter_state(MonitorState::TrackingSilence, now_ms);
        }
        break;

    case MonitorState::TrackingSilence:
        if (playing_ok) {
            // Audio returned before we armed: back to waiting.
            enter_state(MonitorState::WaitingSilence, now_ms);
        } else {
            // Any unconfirmed resume resets the silence start so we only arm
            // after a fresh, continuous quiet stretch.
            if (silence_start_ms_ < 0) silence_start_ms_ = now_ms;
            silence_elapsed_ms_ = now_ms - silence_start_ms_;
            if (silence_elapsed_ms_ >= cfg_.arm_after_ms) {
                armed_ = true;
                ev.armed = true;
                enter_state(MonitorState::Armed, now_ms);
            }
        }
        break;

    case MonitorState::Armed:
        if (playing_ok && !restart_pending_) {
            ev.restart_requested = true;
            restart_pending_ = true;
        }
        break;

    case MonitorState::Cooldown:
        if (now_ms >= cooldown_until_ms_) {
            // Cooldown over: re-evaluate against current signal.
            if (playing_ok) {
                enter_state(MonitorState::WaitingSilence, now_ms);
            } else {
                enter_state(MonitorState::TrackingSilence, now_ms);
            }
        }
        break;
    }

    ev.state = state_;
    ev.armed = armed_;
    if (state_ != last_state_) {
        ev.state_changed = true;
        last_state_ = state_;
    }
    return ev;
}

void AudioMonitor::complete_restart(bool succeeded, std::int64_t now_ms) {
    if (!restart_pending_) return;

    restart_pending_ = false;
    if (succeeded) {
        armed_ = false;
        enter_state(MonitorState::Cooldown, now_ms);
    } else {
        // Require a fresh confirmation before retrying a failed command.
        playing_confirm_ = 0;
    }
}

} // namespace vmwake
