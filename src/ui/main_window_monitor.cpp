#include "main_window.hpp"

#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "win_util.hpp"

namespace vmwake {

bool MainWindow::start_timer() {
    if (timer_id_) {
        KillTimer(hwnd_, timer_id_);
    }
    timer_id_ = SetTimer(hwnd_, kTimerId, kTimerIntervalMs, nullptr);
    return timer_id_ != 0;
}

void MainWindow::reset_connection() {
    logged_in_ = false;
    type_known_ = false;
    vm_running_ = false;
    version_known_ = false;
    monitor_.reset();
    update_output_controls();
}

void MainWindow::attempt_connect() {
    const auto now = static_cast<std::int64_t>(GetTickCount64());

    // Retry loading the Remote DLL about once per second, but attempt it
    // immediately on the first poll instead of waiting for the tick counter.
    if (!remote_.loaded()) {
        if (now - last_load_try_ms_ >= 1000) {
            last_load_try_ms_ = now;
            if (remote_.load()) {
                append_log(L"Voicemeeter Remote API loaded");
                refresh_status_texts();
            } else {
                set_status_text(L"Voicemeeter not found (not installed?)");
            }
        }
        return;
    }

    if (!logged_in_) {
        if (now - last_login_try_ms_ >= 1000) {
            last_login_try_ms_ = now;
            const long rc = remote_.login();
            if (rc == 0 || rc == 1) {
                logged_in_ = true;
                type_known_ = false;
                vm_running_ = rc == 0;
                if (vm_running_) {
                    append_log(L"Connected to Voicemeeter");
                } else {
                    append_log(L"Voicemeeter not running (waiting for launch)");
                }
                refresh_status_texts();
            }
        }
    }
}

void MainWindow::poll_voicemeeter() {
    const auto now = static_cast<std::int64_t>(GetTickCount64());

    if (!remote_.loaded() || !logged_in_) {
        attempt_connect();
        update_level_display(-120.0f);
        feed_monitor(-120.0f, now, false);
        return;
    }

    if (!type_known_) {
        VoicemeeterRemote::Type t;
        if (remote_.get_type(t) && t != VoicemeeterRemote::Type::None) {
            vm_type_ = t;
            type_known_ = true;
            vm_running_ = true;
            version_known_ = remote_.get_version(vm_version_);
            update_output_controls();
            append_log(L"Voicemeeter detected; monitoring " +
                       selected_outputs_text());
            refresh_status_texts();
        }
    }

    // Keep the status label in sync (e.g. if Voicemeeter is quit externally).
    if (tick_ % 50 == 0) {
        refresh_status_texts();
    }

    bool api_ok = type_known_ && output_count() > 0;
    float peak = -120.0f;
    if (api_ok) {
        const std::uint32_t selected = selected_output_mask();
        for (int output = 0; output < output_count() && api_ok; ++output) {
            if ((selected & (1u << output)) == 0) continue;
            for (int channel = 0; channel < 8; ++channel) {
                float v = 0.0f;
                if (remote_.get_level(3, output * 8 + channel, v)) {
                    const float db =
                        v > 1e-6f ? 20.0f * std::log10(v) : -120.0f;
                    if (db > peak) peak = db;
                } else {
                    api_ok = false;
                    append_log(L"Voicemeeter connection lost; reconnecting");
                    remote_.logout();
                    reset_connection();
                    refresh_status_texts();
                    break;
                }
            }
        }
    }

    update_level_display(peak);
    feed_monitor(peak, now, api_ok);
}

void MainWindow::feed_monitor(float peak_db, std::int64_t now, bool api_ok) {
    if (!settings_.enabled) {
        set_state_text(L"Paused");
        SetWindowTextW(hwnd_silence_, L"0 sec");
        return;
    }

    const MonitorEvent ev = monitor_.update(peak_db, now, api_ok);
    if (ev.state_changed) {
        append_log(L"State -> " + state_text(ev.state));
    }
    if (ev.restart_requested) {
        monitor_.complete_restart(do_restart(), now);
    }

    set_state_text(state_text(monitor_.state()));
    if (monitor_.state() == MonitorState::TrackingSilence ||
        monitor_.state() == MonitorState::Armed) {
        const int secs = static_cast<int>(monitor_.silence_elapsed_ms() / 1000);
        wchar_t buf[32];
        swprintf(buf, 32, L"%d sec", secs);
        SetWindowTextW(hwnd_silence_, buf);
    } else {
        SetWindowTextW(hwnd_silence_, L"0 sec");
    }
}

void MainWindow::update_level_display(float peak_db) {
    wchar_t buf[48];
    if (peak_db <= -120.0f) {
        swprintf(buf, 48, L"-inf dB");
    } else {
        swprintf(buf, 48, L"%+.1f dB", peak_db);
    }
    SetWindowTextW(hwnd_level_text_, buf);

    // Bar maps [-70..0] dB to [0..1000].
    int pos = 0;
    if (peak_db > -70.0f) {
        pos = static_cast<int>((peak_db + 70.0f) / 70.0f * 1000.0f);
    }
    pos = std::clamp(pos, 0, 1000);
    SendMessageW(hwnd_level_bar_, PBM_SETPOS, pos, 0);
}

bool MainWindow::do_restart() {
    if (remote_.set_parameter_float("Command.Restart", 1.0f)) {
        if (settings_.restart_count < std::numeric_limits<int>::max()) {
            ++settings_.restart_count;
        }
        settings_.last_restart = ftime();
        settings::save(settings_);
        append_log(L"Voicemeeter audio engine restarted (wake-up)");
        wchar_t buf[48];
        swprintf(buf, 48, L"%d", settings_.restart_count);
        SetWindowTextW(hwnd_restart_count_, buf);
        SetWindowTextW(hwnd_last_restart_, settings_.last_restart.c_str());
        update_tray_tooltip();
        return true;
    } else {
        append_log(L"Error: restart command failed");
        return false;
    }
}

} // namespace vmwake
