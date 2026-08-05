#pragma once

#include <cstdint>

namespace vmwake {

enum class MonitorState {
    Disconnected,    // API not reachable; waiting to (re)connect
    WaitingSilence,  // audio present or ambiguous; need a quiet period before arming
    TrackingSilence, // quiet; accumulating silence, not yet armed
    Armed,           // quiet long enough; next sustained playback triggers a restart
    Cooldown,        // a restart was just issued; ignoring triggers for a while
};

struct MonitorConfig {
    int sample_interval_ms = 100;
    float play_threshold_db = -50.0f;    // level >= this counts as "playing"
    float silence_threshold_db = -55.0f; // level <= this counts as "silent"
    int confirm_samples = 3;             // consecutive samples needed to confirm a transition
    int arm_after_ms = 600000;           // total silence needed before arming
    int cooldown_ms = 5000;              // quiet period after a restart before re-arming
};

struct MonitorEvent {
    MonitorState state = MonitorState::Disconnected;
    bool state_changed = false;
    bool restart_requested = false; // one-shot: call Command.Restart now
    bool armed = false;
};

class AudioMonitor {
public:
    explicit AudioMonitor(MonitorConfig cfg = {});

    void reset();
    void set_config(const MonitorConfig& cfg);
    const MonitorConfig& config() const { return cfg_; }

    // Feed one level sample.  now_ms is a monotonic time source (GetTickCount64).
    // api_ok=false reports the Voicemeeter link as lost (resets silence state).
    MonitorEvent update(float peak_db, std::int64_t now_ms, bool api_ok);

    // Complete a restart requested by update(). Cooldown starts only after the
    // Remote API accepted the command; a failure leaves the monitor armed.
    void complete_restart(bool succeeded, std::int64_t now_ms);

    MonitorState state() const { return state_; }
    bool armed() const { return armed_; }
    std::int64_t silence_elapsed_ms() const { return silence_elapsed_ms_; }

private:
    void enter_state(MonitorState s, std::int64_t now_ms);

    MonitorConfig cfg_;
    MonitorState state_ = MonitorState::Disconnected;
    MonitorState last_state_ = MonitorState::Disconnected;
    bool armed_ = false;
    int playing_confirm_ = 0;
    int silence_confirm_ = 0;
    std::int64_t silence_start_ms_ = -1;
    std::int64_t cooldown_until_ms_ = 0;
    std::int64_t retry_not_before_ms_ = 0;
    std::int64_t silence_elapsed_ms_ = 0;
    bool restart_pending_ = false;
};

} // namespace vmwake
