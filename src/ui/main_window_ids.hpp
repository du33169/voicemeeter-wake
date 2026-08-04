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
    IDC_BTN_TOGGLE = 1103,
    IDC_EDIT_PLAY = 1104,
    IDC_EDIT_SIL = 1105,
    IDC_EDIT_ARM = 1106,
    IDC_EDIT_COOLDOWN = 1107,
    IDC_BTN_SAVE = 1108,
    IDC_STATIC_RESTART_COUNT = 1109,
    IDC_STATIC_LAST_RESTART = 1110,
    IDC_CHK_AUTOSTART = 1111,
    IDC_CHK_MINIMIZE = 1112,
    IDC_SPIN_PLAY = 1113,
    IDC_SPIN_SIL = 1114,
    IDC_SPIN_ARM = 1117,
    IDC_SPIN_COOLDOWN = 1118,
    IDC_CHK_OUTPUT_A1 = 1119,
    IDC_CHK_OUTPUT_A2 = 1120,
    IDC_CHK_OUTPUT_A3 = 1121,
    IDC_CHK_OUTPUT_A4 = 1122,
    IDC_CHK_OUTPUT_A5 = 1123,
    IDC_STATIC_OUTPUTS = 1124,
    IDC_BTN_EXIT = 1125,
    IDC_EDIT_LOG = 1201,
};

enum TrayCmd : unsigned int {
    IDTRAY_SHOW = 2001,
    IDTRAY_TOGGLE = 2002,
    IDTRAY_RESTART = 2003,
    IDTRAY_EXIT = 2004,
};

} // namespace vmwake
