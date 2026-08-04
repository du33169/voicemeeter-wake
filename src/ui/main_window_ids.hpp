#pragma once

namespace vmwake {

// Window / timer identifiers shared across the split implementation files.
constexpr unsigned int kTimerId = 1;
constexpr unsigned int kTimerIntervalMs = 100;

enum ControlId : int {
    IDC_STATIC_STATUS = 1001,
    IDC_PROGRESS_LEVEL = 1002,
    IDC_STATIC_LEVEL = 1003,
    IDC_STATIC_STATE = 1004,
    IDC_STATIC_SILENCE = 1005,
    IDC_BTN_START_VM = 1101,
    IDC_BTN_RESTART = 1102,
    IDC_CHK_ENABLED = 1103,
    IDC_SLIDER_PLAY = 1104,
    IDC_SLIDER_SIL = 1105,
    IDC_SLIDER_ARM = 1106,
    IDC_SLIDER_COOLDOWN = 1107,
    IDC_BTN_SAVE = 1108,
    IDC_STATIC_RESTART_COUNT = 1109,
    IDC_STATIC_LAST_RESTART = 1110,
    IDC_CHK_AUTOSTART = 1111,
    IDC_CHK_MINIMIZE = 1112,
    IDC_STATIC_PLAY_VAL = 1113,
    IDC_STATIC_SIL_VAL = 1114,
    IDC_STATIC_ARM_VAL = 1115,
    IDC_STATIC_COOLDOWN_VAL = 1116,
    IDC_EDIT_LOG = 1201,
};

enum TrayCmd : unsigned int {
    IDTRAY_SHOW = 2001,
    IDTRAY_TOGGLE = 2002,
    IDTRAY_RESTART = 2003,
    IDTRAY_EXIT = 2004,
};

} // namespace vmwake
