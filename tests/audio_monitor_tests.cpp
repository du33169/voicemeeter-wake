#include <cstdio>
#include <string>
#include <vector>

#include "audio_monitor.hpp"
#include "voicemeeter_remote.hpp"

using vmwake::AudioMonitor;
using vmwake::MonitorConfig;
using vmwake::MonitorState;
using vmwake::VoicemeeterRemote;

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

// Feed N ticks at sample_interval_ms, each with the given level.
struct Step {
    float db;
    bool ok = true;
    int ticks = 1;
};

MonitorConfig cfg() {
    MonitorConfig c;
    c.sample_interval_ms = 100;
    c.confirm_samples = 3;
    c.arm_after_ms = 1000;  // 10 ticks of silence
    c.cooldown_ms = 500;    // 5 ticks
    return c;
}

// Run a script of steps from a start time; returns the final event stream.
struct ScriptResult {
    bool saw_restart = false;
    int restart_count = 0;
    bool saw_armed = false;
    MonitorState final_state = MonitorState::Disconnected;
};

ScriptResult run_script(const std::vector<Step>& script, long long t0 = 0) {
    AudioMonitor m(cfg());
    long long t = t0;
    ScriptResult r;
    for (const auto& s : script) {
        for (int i = 0; i < s.ticks; ++i) {
            auto ev = m.update(s.db, t, s.ok);
            if (ev.restart_requested) {
                ++r.restart_count;
                m.complete_restart(true, t);
            }
            if (ev.armed) r.saw_armed = true;
            t += 100;
        }
    }
    r.final_state = m.state();
    r.saw_restart = r.restart_count > 0;
    return r;
}

// Silence = -120 dB, playing = -30 dB (well above play threshold -50).
constexpr float kSilence = -120.0f;
constexpr float kPlay = -30.0f;

void test_playing_at_start_does_not_restart() {
    auto r = run_script(
        {{kPlay, true, 30}});  // 3 s continuous playback
    check(!r.saw_restart, "playing at start must not restart");
    check(!r.saw_armed, "playing at start must not arm");
    check(r.final_state == MonitorState::WaitingSilence,
          "playing at start -> WaitingSilence");
}

void test_short_silence_does_not_arm() {
    // 9 ticks of silence = 900 ms < 1000 ms arm threshold.
    auto r = run_script({{kPlay, true, 5}, {kSilence, true, 9}});
    check(!r.saw_armed, "short silence must not arm");
    check(!r.saw_restart, "short silence must not restart");
}

void test_long_silence_arms_then_play_triggers_once() {
    // silence 12 ticks (arm), then 5 ticks of play.
    auto r = run_script(
        {{kPlay, true, 4}, {kSilence, true, 13}, {kPlay, true, 5}});
    check(r.saw_armed, "long silence must arm");
    check(r.restart_count == 1, "one restart after armed+play");
}

void test_continuous_play_after_trigger_no_loop() {
    // After trigger, keep playing forever -> no second restart while cooldown
    // active, and no restart after cooldown either (play never stops).
    auto r = run_script({{kSilence, true, 13}, {kPlay, true, 200}});
    check(r.restart_count == 1, "only one restart during continuous play");
}

void test_cooldown_suppresses_rearm() {
    // After a restart, if playback stops, a *short* gap must not re-trigger
    // because cooldown is still active; after cooldown, need a full silence
    // window again before another trigger.
    auto r = run_script(
        {{kSilence, true, 13},   // arm
         {kPlay, true, 4},       // trigger restart
         {kSilence, true, 3},    // short gap (inside cooldown) -> no re-arm
         {kPlay, true, 4}});     // no trigger expected
    check(r.restart_count == 1, "cooldown suppresses re-trigger");
}

void test_full_rearm_cycle_restarts_twice() {
    // arm -> play(trigger) -> silence long enough again -> play -> second restart.
    auto r = run_script(
        {{kSilence, true, 13},   // arm
         {kPlay, true, 4},       // trigger #1
         {kSilence, true, 5},    // cooldown + some silence
         {kSilence, true, 15},   // re-arm (needs fresh 1000ms after cooldown)
         {kPlay, true, 4}});     // trigger #2
    check(r.restart_count == 2, "re-arm cycle restarts twice");
}

void test_api_loss_resets_silence() {
    // 5 ticks of silence, then API loss -> silence state must reset so a
    // short subsequent silence cannot arm.
    auto r = run_script(
        {{kSilence, true, 5}, {kSilence, false, 4}, {kSilence, true, 9}});
    check(!r.saw_armed, "API loss must reset silence accumulation");
}

void test_threshold_jitter_no_false_trigger() {
    // Alternating silent/playing around the threshold must never arm or trigger.
    std::vector<Step> s;
    for (int i = 0; i < 60; ++i) {
        s.push_back({kSilence, true, 1});
        s.push_back({kPlay, true, 1});
    }
    auto r = run_script(s);
    check(!r.saw_armed, "jitter must not arm");
    check(!r.saw_restart, "jitter must not trigger");
}

void test_reset_clears_state() {
    AudioMonitor m(cfg());
    long long t = 0;
    for (int i = 0; i < 12; ++i) {
        m.update(kSilence, t, true);
        t += 100;
    }
    check(m.armed(), "armed after long silence");
    m.reset();
    check(!m.armed(), "reset clears armed");
    check(m.state() == MonitorState::Disconnected, "reset -> Disconnected");
}

void test_failed_restart_stays_armed_and_retries() {
    AudioMonitor m(cfg());
    long long t = 0;
    for (int i = 0; i < 13; ++i) {
        m.update(kSilence, t, true);
        t += 100;
    }

    bool first_request = false;
    for (int i = 0; i < 3; ++i) {
        auto ev = m.update(kPlay, t, true);
        t += 100;
        if (ev.restart_requested) {
            first_request = true;
            m.complete_restart(false, t);
        }
    }
    check(first_request, "armed playback requests a restart");
    check(m.armed(), "failed restart must leave monitor armed");
    check(m.state() == MonitorState::Armed,
          "failed restart must not enter cooldown");

    bool retried_too_soon = false;
    for (int i = 0; i < 3; ++i) {
        auto ev = m.update(kPlay, t, true);
        t += 100;
        if (ev.restart_requested) retried_too_soon = true;
    }
    check(!retried_too_soon, "failed restart must wait before retrying");

    bool retried = false;
    for (int i = 0; i < 8; ++i) {
        auto ev = m.update(kPlay, t, true);
        t += 100;
        if (ev.restart_requested) retried = true;
    }
    check(retried, "failed restart retries after the backoff window");
}

void test_remote_result_descriptions() {
    check(std::wstring(VoicemeeterRemote::login_result_text(-2)) ==
              L"unexpected login; logout required",
          "login -2 describes required logout recovery");
    check(std::wstring(VoicemeeterRemote::level_result_text(-3)) ==
              L"level temporarily unavailable",
          "level -3 is distinguished from a lost server");
    check(std::wstring(VoicemeeterRemote::set_parameter_result_text(-2)) ==
              L"Voicemeeter server unavailable",
          "set parameter -2 describes unavailable server");
    check(std::wstring(VoicemeeterRemote::set_parameter_result_text(-3)) ==
              L"unknown parameter",
          "set parameter -3 describes unknown parameter");
}

} // namespace

int main() {
    test_playing_at_start_does_not_restart();
    test_short_silence_does_not_arm();
    test_long_silence_arms_then_play_triggers_once();
    test_continuous_play_after_trigger_no_loop();
    test_cooldown_suppresses_rearm();
    test_full_rearm_cycle_restarts_twice();
    test_api_loss_resets_silence();
    test_threshold_jitter_no_false_trigger();
    test_reset_clears_state();
    test_failed_restart_stays_armed_and_retries();
    test_remote_result_descriptions();

    if (g_failures == 0) {
        std::printf("All audio_monitor tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) failed.\n", g_failures);
    return 1;
}
