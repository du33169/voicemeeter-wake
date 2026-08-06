#include "main_window.hpp"

#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "win_util.hpp"

namespace vmwake {

namespace {

std::wstring api_error(const std::wstring& operation, long rc,
                       const wchar_t* reason) {
    return L"Error: " + operation + L" failed: rc=" +
           std::to_wstring(rc) + L" (" + reason + L")";
}

} // namespace

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
                last_login_error_ = 0;
                logged_in_ = true;
                type_known_ = false;
                vm_running_ = rc == 0;
                if (vm_running_) {
                    if (!connection_recovery_pending_) {
                        append_log(L"Connected to Voicemeeter");
                    }
                } else {
                    append_log(L"Voicemeeter not running (waiting for launch)");
                }
                refresh_status_texts();
            } else {
                const bool error_changed = rc != last_login_error_;
                if (error_changed) {
                    append_log(api_error(L"VBVMR_Login", rc,
                                         VoicemeeterRemote::login_result_text(rc)));
                    last_login_error_ = rc;
                }
                if (rc == -2) {
                    const long logout_rc = remote_.force_logout();
                    connection_recovery_pending_ = true;
                    if (error_changed) {
                        append_log(L"VBVMR_Login recovery: forced VBVMR_Logout, rc=" +
                                   std::to_wstring(logout_rc));
                    }
                }
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
        const long type_rc = remote_.get_type(t);
        if (type_rc == 0 && t != VoicemeeterRemote::Type::None) {
            last_type_error_ = 0;
            vm_type_ = t;
            type_known_ = true;
            vm_running_ = true;
            const long version_rc = remote_.get_version(vm_version_);
            version_known_ = version_rc == 0;
            if (version_rc == 0) {
                last_version_error_ = 0;
            } else if (version_rc != last_version_error_) {
                append_log(api_error(L"VBVMR_GetVoicemeeterVersion", version_rc,
                                     VoicemeeterRemote::info_result_text(version_rc)));
                last_version_error_ = version_rc;
            }
            update_output_controls();
            if (!connection_recovery_pending_) {
                append_log(L"Voicemeeter detected; monitoring " +
                           selected_outputs_text());
            }
            refresh_status_texts();
        } else if (type_rc != 0) {
            const bool waiting_for_launch = type_rc == -2 && !vm_running_;
            const bool error_changed = type_rc != last_type_error_;
            if (!waiting_for_launch && error_changed) {
                append_log(api_error(L"VBVMR_GetVoicemeeterType", type_rc,
                                     VoicemeeterRemote::info_result_text(type_rc)));
                last_type_error_ = type_rc;
            }
            if (!waiting_for_launch && type_rc == -2) {
                const long logout_rc = remote_.logout();
                connection_recovery_pending_ = true;
                if (error_changed) {
                    append_log(L"Connection reset after type query failure; "
                               L"VBVMR_Logout rc=" + std::to_wstring(logout_rc));
                }
                reset_connection();
                refresh_status_texts();
            }
        }
    }

    // Keep the status label in sync (e.g. if Voicemeeter is quit externally).
    if (tick_ % 50 == 0) {
        refresh_status_texts();
    }

    bool api_ok = type_known_ && output_count() > 0;
    long level_error = 0;
    int failed_channel = -1;
    float peak = -120.0f;
    if (api_ok) {
        const std::uint32_t selected = selected_output_mask();
        for (int output = 0; output < output_count() && api_ok; ++output) {
            if ((selected & (1u << output)) == 0) continue;
            for (int channel = 0; channel < 8; ++channel) {
                float v = 0.0f;
                const int api_channel = output * 8 + channel;
                const long level_rc = remote_.get_level(3, api_channel, v);
                if (level_rc == 0) {
                    const float db =
                        v > 1e-6f ? 20.0f * std::log10(v) : -120.0f;
                    if (db > peak) peak = db;
                } else {
                    api_ok = false;
                    level_error = level_rc;
                    failed_channel = api_channel;
                    break;
                }
            }
        }
    }

    if (level_error == 0) {
        last_level_error_ = 0;
        if (api_ok && connection_recovery_pending_) {
            append_log(L"Voicemeeter connection recovered; monitoring " +
                       selected_outputs_text());
            connection_recovery_pending_ = false;
        }
    } else {
        const bool error_changed = level_error != last_level_error_;
        if (error_changed) {
            append_log(api_error(L"VBVMR_GetLevel(type=3, channel=" +
                                     std::to_wstring(failed_channel) + L")",
                                 level_error,
                                 VoicemeeterRemote::level_result_text(level_error)));
            last_level_error_ = level_error;
        }
        if (level_error == -1 || level_error == -2) {
            const long logout_rc = remote_.logout();
            connection_recovery_pending_ = true;
            if (error_changed) {
                append_log(L"Voicemeeter connection lost; reconnecting "
                           L"(VBVMR_Logout rc=" + std::to_wstring(logout_rc) + L")");
            }
            reset_connection();
            refresh_status_texts();
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
    const long rc = remote_.set_parameter_float("Command.Restart", 1.0f);
    if (rc == 0) {
        last_restart_error_ = 0;
        if (restart_count_ < std::numeric_limits<int>::max()) {
            ++restart_count_;
        }
        last_restart_ = ftime();
        append_log(L"Voicemeeter audio engine restart command accepted (wake-up)");
        wchar_t buf[48];
        swprintf(buf, 48, L"%d", restart_count_);
        SetWindowTextW(hwnd_restart_count_, buf);
        SetWindowTextW(hwnd_last_restart_, last_restart_.c_str());
        update_tray_tooltip();
        return true;
    } else {
        if (rc != last_restart_error_) {
            append_log(api_error(L"VBVMR_SetParameterFloat(Command.Restart)", rc,
                                 VoicemeeterRemote::set_parameter_result_text(rc)));
            last_restart_error_ = rc;
        }
        if (rc == -2) {
            const long logout_rc = remote_.logout();
            connection_recovery_pending_ = true;
            append_log(L"Restart failed because the server is unavailable; "
                       L"reconnecting (VBVMR_Logout rc=" +
                       std::to_wstring(logout_rc) + L")");
            reset_connection();
            refresh_status_texts();
        } else if (rc == -3) {
            append_log(L"Restart parameter is unsupported; waiting for a new "
                       L"silence/playback cycle before another attempt");
            monitor_.reset();
        }
        return false;
    }
}

} // namespace vmwake
