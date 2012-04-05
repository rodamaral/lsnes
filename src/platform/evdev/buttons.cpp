extern "C" {
#include <linux/input.h>
}
void evdev_init_buttons(const char** x) {
#ifdef KEY_RESERVED
x[KEY_RESERVED] = "RESERVED";
#endif
#ifdef KEY_ESC
x[KEY_ESC] = "ESC";
#endif
#ifdef KEY_1
x[KEY_1] = "1";
#endif
#ifdef KEY_2
x[KEY_2] = "2";
#endif
#ifdef KEY_3
x[KEY_3] = "3";
#endif
#ifdef KEY_4
x[KEY_4] = "4";
#endif
#ifdef KEY_5
x[KEY_5] = "5";
#endif
#ifdef KEY_6
x[KEY_6] = "6";
#endif
#ifdef KEY_7
x[KEY_7] = "7";
#endif
#ifdef KEY_8
x[KEY_8] = "8";
#endif
#ifdef KEY_9
x[KEY_9] = "9";
#endif
#ifdef KEY_0
x[KEY_0] = "0";
#endif
#ifdef KEY_MINUS
x[KEY_MINUS] = "MINUS";
#endif
#ifdef KEY_EQUAL
x[KEY_EQUAL] = "EQUAL";
#endif
#ifdef KEY_BACKSPACE
x[KEY_BACKSPACE] = "BACKSPACE";
#endif
#ifdef KEY_TAB
x[KEY_TAB] = "TAB";
#endif
#ifdef KEY_Q
x[KEY_Q] = "Q";
#endif
#ifdef KEY_W
x[KEY_W] = "W";
#endif
#ifdef KEY_E
x[KEY_E] = "E";
#endif
#ifdef KEY_R
x[KEY_R] = "R";
#endif
#ifdef KEY_T
x[KEY_T] = "T";
#endif
#ifdef KEY_Y
x[KEY_Y] = "Y";
#endif
#ifdef KEY_U
x[KEY_U] = "U";
#endif
#ifdef KEY_I
x[KEY_I] = "I";
#endif
#ifdef KEY_O
x[KEY_O] = "O";
#endif
#ifdef KEY_P
x[KEY_P] = "P";
#endif
#ifdef KEY_LEFTBRACE
x[KEY_LEFTBRACE] = "LEFTBRACE";
#endif
#ifdef KEY_RIGHTBRACE
x[KEY_RIGHTBRACE] = "RIGHTBRACE";
#endif
#ifdef KEY_ENTER
x[KEY_ENTER] = "ENTER";
#endif
#ifdef KEY_LEFTCTRL
x[KEY_LEFTCTRL] = "LEFTCTRL";
#endif
#ifdef KEY_A
x[KEY_A] = "A";
#endif
#ifdef KEY_S
x[KEY_S] = "S";
#endif
#ifdef KEY_D
x[KEY_D] = "D";
#endif
#ifdef KEY_F
x[KEY_F] = "F";
#endif
#ifdef KEY_G
x[KEY_G] = "G";
#endif
#ifdef KEY_H
x[KEY_H] = "H";
#endif
#ifdef KEY_J
x[KEY_J] = "J";
#endif
#ifdef KEY_K
x[KEY_K] = "K";
#endif
#ifdef KEY_L
x[KEY_L] = "L";
#endif
#ifdef KEY_SEMICOLON
x[KEY_SEMICOLON] = "SEMICOLON";
#endif
#ifdef KEY_APOSTROPHE
x[KEY_APOSTROPHE] = "APOSTROPHE";
#endif
#ifdef KEY_GRAVE
x[KEY_GRAVE] = "GRAVE";
#endif
#ifdef KEY_LEFTSHIFT
x[KEY_LEFTSHIFT] = "LEFTSHIFT";
#endif
#ifdef KEY_BACKSLASH
x[KEY_BACKSLASH] = "BACKSLASH";
#endif
#ifdef KEY_Z
x[KEY_Z] = "Z";
#endif
#ifdef KEY_X
x[KEY_X] = "X";
#endif
#ifdef KEY_C
x[KEY_C] = "C";
#endif
#ifdef KEY_V
x[KEY_V] = "V";
#endif
#ifdef KEY_B
x[KEY_B] = "B";
#endif
#ifdef KEY_N
x[KEY_N] = "N";
#endif
#ifdef KEY_M
x[KEY_M] = "M";
#endif
#ifdef KEY_COMMA
x[KEY_COMMA] = "COMMA";
#endif
#ifdef KEY_DOT
x[KEY_DOT] = "DOT";
#endif
#ifdef KEY_SLASH
x[KEY_SLASH] = "SLASH";
#endif
#ifdef KEY_RIGHTSHIFT
x[KEY_RIGHTSHIFT] = "RIGHTSHIFT";
#endif
#ifdef KEY_KPASTERISK
x[KEY_KPASTERISK] = "KPASTERISK";
#endif
#ifdef KEY_LEFTALT
x[KEY_LEFTALT] = "LEFTALT";
#endif
#ifdef KEY_SPACE
x[KEY_SPACE] = "SPACE";
#endif
#ifdef KEY_CAPSLOCK
x[KEY_CAPSLOCK] = "CAPSLOCK";
#endif
#ifdef KEY_F1
x[KEY_F1] = "F1";
#endif
#ifdef KEY_F2
x[KEY_F2] = "F2";
#endif
#ifdef KEY_F3
x[KEY_F3] = "F3";
#endif
#ifdef KEY_F4
x[KEY_F4] = "F4";
#endif
#ifdef KEY_F5
x[KEY_F5] = "F5";
#endif
#ifdef KEY_F6
x[KEY_F6] = "F6";
#endif
#ifdef KEY_F7
x[KEY_F7] = "F7";
#endif
#ifdef KEY_F8
x[KEY_F8] = "F8";
#endif
#ifdef KEY_F9
x[KEY_F9] = "F9";
#endif
#ifdef KEY_F10
x[KEY_F10] = "F10";
#endif
#ifdef KEY_NUMLOCK
x[KEY_NUMLOCK] = "NUMLOCK";
#endif
#ifdef KEY_SCROLLLOCK
x[KEY_SCROLLLOCK] = "SCROLLLOCK";
#endif
#ifdef KEY_KP7
x[KEY_KP7] = "KP7";
#endif
#ifdef KEY_KP8
x[KEY_KP8] = "KP8";
#endif
#ifdef KEY_KP9
x[KEY_KP9] = "KP9";
#endif
#ifdef KEY_KPMINUS
x[KEY_KPMINUS] = "KPMINUS";
#endif
#ifdef KEY_KP4
x[KEY_KP4] = "KP4";
#endif
#ifdef KEY_KP5
x[KEY_KP5] = "KP5";
#endif
#ifdef KEY_KP6
x[KEY_KP6] = "KP6";
#endif
#ifdef KEY_KPPLUS
x[KEY_KPPLUS] = "KPPLUS";
#endif
#ifdef KEY_KP1
x[KEY_KP1] = "KP1";
#endif
#ifdef KEY_KP2
x[KEY_KP2] = "KP2";
#endif
#ifdef KEY_KP3
x[KEY_KP3] = "KP3";
#endif
#ifdef KEY_KP0
x[KEY_KP0] = "KP0";
#endif
#ifdef KEY_KPDOT
x[KEY_KPDOT] = "KPDOT";
#endif
#ifdef KEY_ZENKAKUHANKAKU
x[KEY_ZENKAKUHANKAKU] = "ZENKAKUHANKAKU";
#endif
#ifdef KEY_102ND
x[KEY_102ND] = "102ND";
#endif
#ifdef KEY_F11
x[KEY_F11] = "F11";
#endif
#ifdef KEY_F12
x[KEY_F12] = "F12";
#endif
#ifdef KEY_RO
x[KEY_RO] = "RO";
#endif
#ifdef KEY_KATAKANA
x[KEY_KATAKANA] = "KATAKANA";
#endif
#ifdef KEY_HIRAGANA
x[KEY_HIRAGANA] = "HIRAGANA";
#endif
#ifdef KEY_HENKAN
x[KEY_HENKAN] = "HENKAN";
#endif
#ifdef KEY_KATAKANAHIRAGANA
x[KEY_KATAKANAHIRAGANA] = "KATAKANAHIRAGANA";
#endif
#ifdef KEY_MUHENKAN
x[KEY_MUHENKAN] = "MUHENKAN";
#endif
#ifdef KEY_KPJPCOMMA
x[KEY_KPJPCOMMA] = "KPJPCOMMA";
#endif
#ifdef KEY_KPENTER
x[KEY_KPENTER] = "KPENTER";
#endif
#ifdef KEY_RIGHTCTRL
x[KEY_RIGHTCTRL] = "RIGHTCTRL";
#endif
#ifdef KEY_KPSLASH
x[KEY_KPSLASH] = "KPSLASH";
#endif
#ifdef KEY_SYSRQ
x[KEY_SYSRQ] = "SYSRQ";
#endif
#ifdef KEY_RIGHTALT
x[KEY_RIGHTALT] = "RIGHTALT";
#endif
#ifdef KEY_LINEFEED
x[KEY_LINEFEED] = "LINEFEED";
#endif
#ifdef KEY_HOME
x[KEY_HOME] = "HOME";
#endif
#ifdef KEY_UP
x[KEY_UP] = "UP";
#endif
#ifdef KEY_PAGEUP
x[KEY_PAGEUP] = "PAGEUP";
#endif
#ifdef KEY_LEFT
x[KEY_LEFT] = "LEFT";
#endif
#ifdef KEY_RIGHT
x[KEY_RIGHT] = "RIGHT";
#endif
#ifdef KEY_END
x[KEY_END] = "END";
#endif
#ifdef KEY_DOWN
x[KEY_DOWN] = "DOWN";
#endif
#ifdef KEY_PAGEDOWN
x[KEY_PAGEDOWN] = "PAGEDOWN";
#endif
#ifdef KEY_INSERT
x[KEY_INSERT] = "INSERT";
#endif
#ifdef KEY_DELETE
x[KEY_DELETE] = "DELETE";
#endif
#ifdef KEY_MACRO
x[KEY_MACRO] = "MACRO";
#endif
#ifdef KEY_MUTE
x[KEY_MUTE] = "MUTE";
#endif
#ifdef KEY_VOLUMEDOWN
x[KEY_VOLUMEDOWN] = "VOLUMEDOWN";
#endif
#ifdef KEY_VOLUMEUP
x[KEY_VOLUMEUP] = "VOLUMEUP";
#endif
#ifdef KEY_POWER
x[KEY_POWER] = "POWER";
#endif
#ifdef KEY_KPEQUAL
x[KEY_KPEQUAL] = "KPEQUAL";
#endif
#ifdef KEY_KPPLUSMINUS
x[KEY_KPPLUSMINUS] = "KPPLUSMINUS";
#endif
#ifdef KEY_PAUSE
x[KEY_PAUSE] = "PAUSE";
#endif
#ifdef KEY_SCALE
x[KEY_SCALE] = "SCALE";
#endif
#ifdef KEY_KPCOMMA
x[KEY_KPCOMMA] = "KPCOMMA";
#endif
#ifdef KEY_HANGEUL
x[KEY_HANGEUL] = "HANGEUL";
#endif
#ifdef KEY_HANGUEL
x[KEY_HANGUEL] = "HANGUEL";
#endif
#ifdef KEY_HANJA
x[KEY_HANJA] = "HANJA";
#endif
#ifdef KEY_YEN
x[KEY_YEN] = "YEN";
#endif
#ifdef KEY_LEFTMETA
x[KEY_LEFTMETA] = "LEFTMETA";
#endif
#ifdef KEY_RIGHTMETA
x[KEY_RIGHTMETA] = "RIGHTMETA";
#endif
#ifdef KEY_COMPOSE
x[KEY_COMPOSE] = "COMPOSE";
#endif
#ifdef KEY_STOP
x[KEY_STOP] = "STOP";
#endif
#ifdef KEY_AGAIN
x[KEY_AGAIN] = "AGAIN";
#endif
#ifdef KEY_PROPS
x[KEY_PROPS] = "PROPS";
#endif
#ifdef KEY_UNDO
x[KEY_UNDO] = "UNDO";
#endif
#ifdef KEY_FRONT
x[KEY_FRONT] = "FRONT";
#endif
#ifdef KEY_COPY
x[KEY_COPY] = "COPY";
#endif
#ifdef KEY_OPEN
x[KEY_OPEN] = "OPEN";
#endif
#ifdef KEY_PASTE
x[KEY_PASTE] = "PASTE";
#endif
#ifdef KEY_FIND
x[KEY_FIND] = "FIND";
#endif
#ifdef KEY_CUT
x[KEY_CUT] = "CUT";
#endif
#ifdef KEY_HELP
x[KEY_HELP] = "HELP";
#endif
#ifdef KEY_MENU
x[KEY_MENU] = "MENU";
#endif
#ifdef KEY_CALC
x[KEY_CALC] = "CALC";
#endif
#ifdef KEY_SETUP
x[KEY_SETUP] = "SETUP";
#endif
#ifdef KEY_SLEEP
x[KEY_SLEEP] = "SLEEP";
#endif
#ifdef KEY_WAKEUP
x[KEY_WAKEUP] = "WAKEUP";
#endif
#ifdef KEY_FILE
x[KEY_FILE] = "FILE";
#endif
#ifdef KEY_SENDFILE
x[KEY_SENDFILE] = "SENDFILE";
#endif
#ifdef KEY_DELETEFILE
x[KEY_DELETEFILE] = "DELETEFILE";
#endif
#ifdef KEY_XFER
x[KEY_XFER] = "XFER";
#endif
#ifdef KEY_PROG1
x[KEY_PROG1] = "PROG1";
#endif
#ifdef KEY_PROG2
x[KEY_PROG2] = "PROG2";
#endif
#ifdef KEY_WWW
x[KEY_WWW] = "WWW";
#endif
#ifdef KEY_MSDOS
x[KEY_MSDOS] = "MSDOS";
#endif
#ifdef KEY_COFFEE
x[KEY_COFFEE] = "COFFEE";
#endif
#ifdef KEY_SCREENLOCK
x[KEY_SCREENLOCK] = "SCREENLOCK";
#endif
#ifdef KEY_DIRECTION
x[KEY_DIRECTION] = "DIRECTION";
#endif
#ifdef KEY_CYCLEWINDOWS
x[KEY_CYCLEWINDOWS] = "CYCLEWINDOWS";
#endif
#ifdef KEY_MAIL
x[KEY_MAIL] = "MAIL";
#endif
#ifdef KEY_BOOKMARKS
x[KEY_BOOKMARKS] = "BOOKMARKS";
#endif
#ifdef KEY_COMPUTER
x[KEY_COMPUTER] = "COMPUTER";
#endif
#ifdef KEY_BACK
x[KEY_BACK] = "BACK";
#endif
#ifdef KEY_FORWARD
x[KEY_FORWARD] = "FORWARD";
#endif
#ifdef KEY_CLOSECD
x[KEY_CLOSECD] = "CLOSECD";
#endif
#ifdef KEY_EJECTCD
x[KEY_EJECTCD] = "EJECTCD";
#endif
#ifdef KEY_EJECTCLOSECD
x[KEY_EJECTCLOSECD] = "EJECTCLOSECD";
#endif
#ifdef KEY_NEXTSONG
x[KEY_NEXTSONG] = "NEXTSONG";
#endif
#ifdef KEY_PLAYPAUSE
x[KEY_PLAYPAUSE] = "PLAYPAUSE";
#endif
#ifdef KEY_PREVIOUSSONG
x[KEY_PREVIOUSSONG] = "PREVIOUSSONG";
#endif
#ifdef KEY_STOPCD
x[KEY_STOPCD] = "STOPCD";
#endif
#ifdef KEY_RECORD
x[KEY_RECORD] = "RECORD";
#endif
#ifdef KEY_REWIND
x[KEY_REWIND] = "REWIND";
#endif
#ifdef KEY_PHONE
x[KEY_PHONE] = "PHONE";
#endif
#ifdef KEY_ISO
x[KEY_ISO] = "ISO";
#endif
#ifdef KEY_CONFIG
x[KEY_CONFIG] = "CONFIG";
#endif
#ifdef KEY_HOMEPAGE
x[KEY_HOMEPAGE] = "HOMEPAGE";
#endif
#ifdef KEY_REFRESH
x[KEY_REFRESH] = "REFRESH";
#endif
#ifdef KEY_EXIT
x[KEY_EXIT] = "EXIT";
#endif
#ifdef KEY_MOVE
x[KEY_MOVE] = "MOVE";
#endif
#ifdef KEY_EDIT
x[KEY_EDIT] = "EDIT";
#endif
#ifdef KEY_SCROLLUP
x[KEY_SCROLLUP] = "SCROLLUP";
#endif
#ifdef KEY_SCROLLDOWN
x[KEY_SCROLLDOWN] = "SCROLLDOWN";
#endif
#ifdef KEY_KPLEFTPAREN
x[KEY_KPLEFTPAREN] = "KPLEFTPAREN";
#endif
#ifdef KEY_KPRIGHTPAREN
x[KEY_KPRIGHTPAREN] = "KPRIGHTPAREN";
#endif
#ifdef KEY_NEW
x[KEY_NEW] = "NEW";
#endif
#ifdef KEY_REDO
x[KEY_REDO] = "REDO";
#endif
#ifdef KEY_F13
x[KEY_F13] = "F13";
#endif
#ifdef KEY_F14
x[KEY_F14] = "F14";
#endif
#ifdef KEY_F15
x[KEY_F15] = "F15";
#endif
#ifdef KEY_F16
x[KEY_F16] = "F16";
#endif
#ifdef KEY_F17
x[KEY_F17] = "F17";
#endif
#ifdef KEY_F18
x[KEY_F18] = "F18";
#endif
#ifdef KEY_F19
x[KEY_F19] = "F19";
#endif
#ifdef KEY_F20
x[KEY_F20] = "F20";
#endif
#ifdef KEY_F21
x[KEY_F21] = "F21";
#endif
#ifdef KEY_F22
x[KEY_F22] = "F22";
#endif
#ifdef KEY_F23
x[KEY_F23] = "F23";
#endif
#ifdef KEY_F24
x[KEY_F24] = "F24";
#endif
#ifdef KEY_PLAYCD
x[KEY_PLAYCD] = "PLAYCD";
#endif
#ifdef KEY_PAUSECD
x[KEY_PAUSECD] = "PAUSECD";
#endif
#ifdef KEY_PROG3
x[KEY_PROG3] = "PROG3";
#endif
#ifdef KEY_PROG4
x[KEY_PROG4] = "PROG4";
#endif
#ifdef KEY_DASHBOARD
x[KEY_DASHBOARD] = "DASHBOARD";
#endif
#ifdef KEY_SUSPEND
x[KEY_SUSPEND] = "SUSPEND";
#endif
#ifdef KEY_CLOSE
x[KEY_CLOSE] = "CLOSE";
#endif
#ifdef KEY_PLAY
x[KEY_PLAY] = "PLAY";
#endif
#ifdef KEY_FASTFORWARD
x[KEY_FASTFORWARD] = "FASTFORWARD";
#endif
#ifdef KEY_BASSBOOST
x[KEY_BASSBOOST] = "BASSBOOST";
#endif
#ifdef KEY_PRINT
x[KEY_PRINT] = "PRINT";
#endif
#ifdef KEY_HP
x[KEY_HP] = "HP";
#endif
#ifdef KEY_CAMERA
x[KEY_CAMERA] = "CAMERA";
#endif
#ifdef KEY_SOUND
x[KEY_SOUND] = "SOUND";
#endif
#ifdef KEY_QUESTION
x[KEY_QUESTION] = "QUESTION";
#endif
#ifdef KEY_EMAIL
x[KEY_EMAIL] = "EMAIL";
#endif
#ifdef KEY_CHAT
x[KEY_CHAT] = "CHAT";
#endif
#ifdef KEY_SEARCH
x[KEY_SEARCH] = "SEARCH";
#endif
#ifdef KEY_CONNECT
x[KEY_CONNECT] = "CONNECT";
#endif
#ifdef KEY_FINANCE
x[KEY_FINANCE] = "FINANCE";
#endif
#ifdef KEY_SPORT
x[KEY_SPORT] = "SPORT";
#endif
#ifdef KEY_SHOP
x[KEY_SHOP] = "SHOP";
#endif
#ifdef KEY_ALTERASE
x[KEY_ALTERASE] = "ALTERASE";
#endif
#ifdef KEY_CANCEL
x[KEY_CANCEL] = "CANCEL";
#endif
#ifdef KEY_BRIGHTNESSDOWN
x[KEY_BRIGHTNESSDOWN] = "BRIGHTNESSDOWN";
#endif
#ifdef KEY_BRIGHTNESSUP
x[KEY_BRIGHTNESSUP] = "BRIGHTNESSUP";
#endif
#ifdef KEY_MEDIA
x[KEY_MEDIA] = "MEDIA";
#endif
#ifdef KEY_SWITCHVIDEOMODE
x[KEY_SWITCHVIDEOMODE] = "SWITCHVIDEOMODE";
#endif
#ifdef KEY_KBDILLUMTOGGLE
x[KEY_KBDILLUMTOGGLE] = "KBDILLUMTOGGLE";
#endif
#ifdef KEY_KBDILLUMDOWN
x[KEY_KBDILLUMDOWN] = "KBDILLUMDOWN";
#endif
#ifdef KEY_KBDILLUMUP
x[KEY_KBDILLUMUP] = "KBDILLUMUP";
#endif
#ifdef KEY_SEND
x[KEY_SEND] = "SEND";
#endif
#ifdef KEY_REPLY
x[KEY_REPLY] = "REPLY";
#endif
#ifdef KEY_FORWARDMAIL
x[KEY_FORWARDMAIL] = "FORWARDMAIL";
#endif
#ifdef KEY_SAVE
x[KEY_SAVE] = "SAVE";
#endif
#ifdef KEY_DOCUMENTS
x[KEY_DOCUMENTS] = "DOCUMENTS";
#endif
#ifdef KEY_BATTERY
x[KEY_BATTERY] = "BATTERY";
#endif
#ifdef KEY_BLUETOOTH
x[KEY_BLUETOOTH] = "BLUETOOTH";
#endif
#ifdef KEY_WLAN
x[KEY_WLAN] = "WLAN";
#endif
#ifdef KEY_UWB
x[KEY_UWB] = "UWB";
#endif
#ifdef KEY_UNKNOWN
x[KEY_UNKNOWN] = "UNKNOWN";
#endif
#ifdef KEY_VIDEO_NEXT
x[KEY_VIDEO_NEXT] = "VIDEO_NEXT";
#endif
#ifdef KEY_VIDEO_PREV
x[KEY_VIDEO_PREV] = "VIDEO_PREV";
#endif
#ifdef KEY_BRIGHTNESS_CYCLE
x[KEY_BRIGHTNESS_CYCLE] = "BRIGHTNESS_CYCLE";
#endif
#ifdef KEY_BRIGHTNESS_ZERO
x[KEY_BRIGHTNESS_ZERO] = "BRIGHTNESS_ZERO";
#endif
#ifdef KEY_DISPLAY_OFF
x[KEY_DISPLAY_OFF] = "DISPLAY_OFF";
#endif
#ifdef KEY_WIMAX
x[KEY_WIMAX] = "WIMAX";
#endif
#ifdef KEY_RFKILL
x[KEY_RFKILL] = "RFKILL";
#endif
#ifdef KEY_MICMUTE
x[KEY_MICMUTE] = "MICMUTE";
#endif
#ifdef BTN_MISC
x[BTN_MISC] = "Button MISC";
#endif
#ifdef BTN_0
x[BTN_0] = "Button 0";
#endif
#ifdef BTN_1
x[BTN_1] = "Button 1";
#endif
#ifdef BTN_2
x[BTN_2] = "Button 2";
#endif
#ifdef BTN_3
x[BTN_3] = "Button 3";
#endif
#ifdef BTN_4
x[BTN_4] = "Button 4";
#endif
#ifdef BTN_5
x[BTN_5] = "Button 5";
#endif
#ifdef BTN_6
x[BTN_6] = "Button 6";
#endif
#ifdef BTN_7
x[BTN_7] = "Button 7";
#endif
#ifdef BTN_8
x[BTN_8] = "Button 8";
#endif
#ifdef BTN_9
x[BTN_9] = "Button 9";
#endif
#ifdef BTN_MOUSE
x[BTN_MOUSE] = "Button MOUSE";
#endif
#ifdef BTN_LEFT
x[BTN_LEFT] = "Button LEFT";
#endif
#ifdef BTN_RIGHT
x[BTN_RIGHT] = "Button RIGHT";
#endif
#ifdef BTN_MIDDLE
x[BTN_MIDDLE] = "Button MIDDLE";
#endif
#ifdef BTN_SIDE
x[BTN_SIDE] = "Button SIDE";
#endif
#ifdef BTN_EXTRA
x[BTN_EXTRA] = "Button EXTRA";
#endif
#ifdef BTN_FORWARD
x[BTN_FORWARD] = "Button FORWARD";
#endif
#ifdef BTN_BACK
x[BTN_BACK] = "Button BACK";
#endif
#ifdef BTN_TASK
x[BTN_TASK] = "Button TASK";
#endif
#ifdef BTN_JOYSTICK
x[BTN_JOYSTICK] = "Button JOYSTICK";
#endif
#ifdef BTN_TRIGGER
x[BTN_TRIGGER] = "Button TRIGGER";
#endif
#ifdef BTN_THUMB
x[BTN_THUMB] = "Button THUMB";
#endif
#ifdef BTN_THUMB2
x[BTN_THUMB2] = "Button THUMB2";
#endif
#ifdef BTN_TOP
x[BTN_TOP] = "Button TOP";
#endif
#ifdef BTN_TOP2
x[BTN_TOP2] = "Button TOP2";
#endif
#ifdef BTN_PINKIE
x[BTN_PINKIE] = "Button PINKIE";
#endif
#ifdef BTN_BASE
x[BTN_BASE] = "Button BASE";
#endif
#ifdef BTN_BASE2
x[BTN_BASE2] = "Button BASE2";
#endif
#ifdef BTN_BASE3
x[BTN_BASE3] = "Button BASE3";
#endif
#ifdef BTN_BASE4
x[BTN_BASE4] = "Button BASE4";
#endif
#ifdef BTN_BASE5
x[BTN_BASE5] = "Button BASE5";
#endif
#ifdef BTN_BASE6
x[BTN_BASE6] = "Button BASE6";
#endif
#ifdef BTN_DEAD
x[BTN_DEAD] = "Button DEAD";
#endif
#ifdef BTN_GAMEPAD
x[BTN_GAMEPAD] = "Button GAMEPAD";
#endif
#ifdef BTN_A
x[BTN_A] = "Button A";
#endif
#ifdef BTN_B
x[BTN_B] = "Button B";
#endif
#ifdef BTN_C
x[BTN_C] = "Button C";
#endif
#ifdef BTN_X
x[BTN_X] = "Button X";
#endif
#ifdef BTN_Y
x[BTN_Y] = "Button Y";
#endif
#ifdef BTN_Z
x[BTN_Z] = "Button Z";
#endif
#ifdef BTN_TL
x[BTN_TL] = "Button TL";
#endif
#ifdef BTN_TR
x[BTN_TR] = "Button TR";
#endif
#ifdef BTN_TL2
x[BTN_TL2] = "Button TL2";
#endif
#ifdef BTN_TR2
x[BTN_TR2] = "Button TR2";
#endif
#ifdef BTN_SELECT
x[BTN_SELECT] = "Button SELECT";
#endif
#ifdef BTN_START
x[BTN_START] = "Button START";
#endif
#ifdef BTN_MODE
x[BTN_MODE] = "Button MODE";
#endif
#ifdef BTN_THUMBL
x[BTN_THUMBL] = "Button THUMBL";
#endif
#ifdef BTN_THUMBR
x[BTN_THUMBR] = "Button THUMBR";
#endif
#ifdef BTN_DIGI
x[BTN_DIGI] = "Button DIGI";
#endif
#ifdef BTN_TOOL_PEN
x[BTN_TOOL_PEN] = "Button TOOL_PEN";
#endif
#ifdef BTN_TOOL_RUBBER
x[BTN_TOOL_RUBBER] = "Button TOOL_RUBBER";
#endif
#ifdef BTN_TOOL_BRUSH
x[BTN_TOOL_BRUSH] = "Button TOOL_BRUSH";
#endif
#ifdef BTN_TOOL_PENCIL
x[BTN_TOOL_PENCIL] = "Button TOOL_PENCIL";
#endif
#ifdef BTN_TOOL_AIRBRUSH
x[BTN_TOOL_AIRBRUSH] = "Button TOOL_AIRBRUSH";
#endif
#ifdef BTN_TOOL_FINGER
x[BTN_TOOL_FINGER] = "Button TOOL_FINGER";
#endif
#ifdef BTN_TOOL_MOUSE
x[BTN_TOOL_MOUSE] = "Button TOOL_MOUSE";
#endif
#ifdef BTN_TOOL_LENS
x[BTN_TOOL_LENS] = "Button TOOL_LENS";
#endif
#ifdef BTN_TOUCH
x[BTN_TOUCH] = "Button TOUCH";
#endif
#ifdef BTN_STYLUS
x[BTN_STYLUS] = "Button STYLUS";
#endif
#ifdef BTN_STYLUS2
x[BTN_STYLUS2] = "Button STYLUS2";
#endif
#ifdef BTN_TOOL_DOUBLETAP
x[BTN_TOOL_DOUBLETAP] = "Button TOOL_DOUBLETAP";
#endif
#ifdef BTN_TOOL_TRIPLETAP
x[BTN_TOOL_TRIPLETAP] = "Button TOOL_TRIPLETAP";
#endif
#ifdef BTN_TOOL_QUADTAP
x[BTN_TOOL_QUADTAP] = "Button TOOL_QUADTAP";
#endif
#ifdef BTN_WHEEL
x[BTN_WHEEL] = "Button WHEEL";
#endif
#ifdef BTN_GEAR_DOWN
x[BTN_GEAR_DOWN] = "Button GEAR_DOWN";
#endif
#ifdef BTN_GEAR_UP
x[BTN_GEAR_UP] = "Button GEAR_UP";
#endif
#ifdef KEY_OK
x[KEY_OK] = "OK";
#endif
#ifdef KEY_SELECT
x[KEY_SELECT] = "SELECT";
#endif
#ifdef KEY_GOTO
x[KEY_GOTO] = "GOTO";
#endif
#ifdef KEY_CLEAR
x[KEY_CLEAR] = "CLEAR";
#endif
#ifdef KEY_POWER2
x[KEY_POWER2] = "POWER2";
#endif
#ifdef KEY_OPTION
x[KEY_OPTION] = "OPTION";
#endif
#ifdef KEY_INFO
x[KEY_INFO] = "INFO";
#endif
#ifdef KEY_TIME
x[KEY_TIME] = "TIME";
#endif
#ifdef KEY_VENDOR
x[KEY_VENDOR] = "VENDOR";
#endif
#ifdef KEY_ARCHIVE
x[KEY_ARCHIVE] = "ARCHIVE";
#endif
#ifdef KEY_PROGRAM
x[KEY_PROGRAM] = "PROGRAM";
#endif
#ifdef KEY_CHANNEL
x[KEY_CHANNEL] = "CHANNEL";
#endif
#ifdef KEY_FAVORITES
x[KEY_FAVORITES] = "FAVORITES";
#endif
#ifdef KEY_EPG
x[KEY_EPG] = "EPG";
#endif
#ifdef KEY_PVR
x[KEY_PVR] = "PVR";
#endif
#ifdef KEY_MHP
x[KEY_MHP] = "MHP";
#endif
#ifdef KEY_LANGUAGE
x[KEY_LANGUAGE] = "LANGUAGE";
#endif
#ifdef KEY_TITLE
x[KEY_TITLE] = "TITLE";
#endif
#ifdef KEY_SUBTITLE
x[KEY_SUBTITLE] = "SUBTITLE";
#endif
#ifdef KEY_ANGLE
x[KEY_ANGLE] = "ANGLE";
#endif
#ifdef KEY_ZOOM
x[KEY_ZOOM] = "ZOOM";
#endif
#ifdef KEY_MODE
x[KEY_MODE] = "MODE";
#endif
#ifdef KEY_KEYBOARD
x[KEY_KEYBOARD] = "KEYBOARD";
#endif
#ifdef KEY_SCREEN
x[KEY_SCREEN] = "SCREEN";
#endif
#ifdef KEY_PC
x[KEY_PC] = "PC";
#endif
#ifdef KEY_TV
x[KEY_TV] = "TV";
#endif
#ifdef KEY_TV2
x[KEY_TV2] = "TV2";
#endif
#ifdef KEY_VCR
x[KEY_VCR] = "VCR";
#endif
#ifdef KEY_VCR2
x[KEY_VCR2] = "VCR2";
#endif
#ifdef KEY_SAT
x[KEY_SAT] = "SAT";
#endif
#ifdef KEY_SAT2
x[KEY_SAT2] = "SAT2";
#endif
#ifdef KEY_CD
x[KEY_CD] = "CD";
#endif
#ifdef KEY_TAPE
x[KEY_TAPE] = "TAPE";
#endif
#ifdef KEY_RADIO
x[KEY_RADIO] = "RADIO";
#endif
#ifdef KEY_TUNER
x[KEY_TUNER] = "TUNER";
#endif
#ifdef KEY_PLAYER
x[KEY_PLAYER] = "PLAYER";
#endif
#ifdef KEY_TEXT
x[KEY_TEXT] = "TEXT";
#endif
#ifdef KEY_DVD
x[KEY_DVD] = "DVD";
#endif
#ifdef KEY_AUX
x[KEY_AUX] = "AUX";
#endif
#ifdef KEY_MP3
x[KEY_MP3] = "MP3";
#endif
#ifdef KEY_AUDIO
x[KEY_AUDIO] = "AUDIO";
#endif
#ifdef KEY_VIDEO
x[KEY_VIDEO] = "VIDEO";
#endif
#ifdef KEY_DIRECTORY
x[KEY_DIRECTORY] = "DIRECTORY";
#endif
#ifdef KEY_LIST
x[KEY_LIST] = "LIST";
#endif
#ifdef KEY_MEMO
x[KEY_MEMO] = "MEMO";
#endif
#ifdef KEY_CALENDAR
x[KEY_CALENDAR] = "CALENDAR";
#endif
#ifdef KEY_RED
x[KEY_RED] = "RED";
#endif
#ifdef KEY_GREEN
x[KEY_GREEN] = "GREEN";
#endif
#ifdef KEY_YELLOW
x[KEY_YELLOW] = "YELLOW";
#endif
#ifdef KEY_BLUE
x[KEY_BLUE] = "BLUE";
#endif
#ifdef KEY_CHANNELUP
x[KEY_CHANNELUP] = "CHANNELUP";
#endif
#ifdef KEY_CHANNELDOWN
x[KEY_CHANNELDOWN] = "CHANNELDOWN";
#endif
#ifdef KEY_FIRST
x[KEY_FIRST] = "FIRST";
#endif
#ifdef KEY_LAST
x[KEY_LAST] = "LAST";
#endif
#ifdef KEY_AB
x[KEY_AB] = "AB";
#endif
#ifdef KEY_NEXT
x[KEY_NEXT] = "NEXT";
#endif
#ifdef KEY_RESTART
x[KEY_RESTART] = "RESTART";
#endif
#ifdef KEY_SLOW
x[KEY_SLOW] = "SLOW";
#endif
#ifdef KEY_SHUFFLE
x[KEY_SHUFFLE] = "SHUFFLE";
#endif
#ifdef KEY_BREAK
x[KEY_BREAK] = "BREAK";
#endif
#ifdef KEY_PREVIOUS
x[KEY_PREVIOUS] = "PREVIOUS";
#endif
#ifdef KEY_DIGITS
x[KEY_DIGITS] = "DIGITS";
#endif
#ifdef KEY_TEEN
x[KEY_TEEN] = "TEEN";
#endif
#ifdef KEY_TWEN
x[KEY_TWEN] = "TWEN";
#endif
#ifdef KEY_VIDEOPHONE
x[KEY_VIDEOPHONE] = "VIDEOPHONE";
#endif
#ifdef KEY_GAMES
x[KEY_GAMES] = "GAMES";
#endif
#ifdef KEY_ZOOMIN
x[KEY_ZOOMIN] = "ZOOMIN";
#endif
#ifdef KEY_ZOOMOUT
x[KEY_ZOOMOUT] = "ZOOMOUT";
#endif
#ifdef KEY_ZOOMRESET
x[KEY_ZOOMRESET] = "ZOOMRESET";
#endif
#ifdef KEY_WORDPROCESSOR
x[KEY_WORDPROCESSOR] = "WORDPROCESSOR";
#endif
#ifdef KEY_EDITOR
x[KEY_EDITOR] = "EDITOR";
#endif
#ifdef KEY_SPREADSHEET
x[KEY_SPREADSHEET] = "SPREADSHEET";
#endif
#ifdef KEY_GRAPHICSEDITOR
x[KEY_GRAPHICSEDITOR] = "GRAPHICSEDITOR";
#endif
#ifdef KEY_PRESENTATION
x[KEY_PRESENTATION] = "PRESENTATION";
#endif
#ifdef KEY_DATABASE
x[KEY_DATABASE] = "DATABASE";
#endif
#ifdef KEY_NEWS
x[KEY_NEWS] = "NEWS";
#endif
#ifdef KEY_VOICEMAIL
x[KEY_VOICEMAIL] = "VOICEMAIL";
#endif
#ifdef KEY_ADDRESSBOOK
x[KEY_ADDRESSBOOK] = "ADDRESSBOOK";
#endif
#ifdef KEY_MESSENGER
x[KEY_MESSENGER] = "MESSENGER";
#endif
#ifdef KEY_DISPLAYTOGGLE
x[KEY_DISPLAYTOGGLE] = "DISPLAYTOGGLE";
#endif
#ifdef KEY_SPELLCHECK
x[KEY_SPELLCHECK] = "SPELLCHECK";
#endif
#ifdef KEY_LOGOFF
x[KEY_LOGOFF] = "LOGOFF";
#endif
#ifdef KEY_DOLLAR
x[KEY_DOLLAR] = "DOLLAR";
#endif
#ifdef KEY_EURO
x[KEY_EURO] = "EURO";
#endif
#ifdef KEY_FRAMEBACK
x[KEY_FRAMEBACK] = "FRAMEBACK";
#endif
#ifdef KEY_FRAMEFORWARD
x[KEY_FRAMEFORWARD] = "FRAMEFORWARD";
#endif
#ifdef KEY_CONTEXT_MENU
x[KEY_CONTEXT_MENU] = "CONTEXT_MENU";
#endif
#ifdef KEY_MEDIA_REPEAT
x[KEY_MEDIA_REPEAT] = "MEDIA_REPEAT";
#endif
#ifdef KEY_10CHANNELSUP
x[KEY_10CHANNELSUP] = "10CHANNELSUP";
#endif
#ifdef KEY_10CHANNELSDOWN
x[KEY_10CHANNELSDOWN] = "10CHANNELSDOWN";
#endif
#ifdef KEY_IMAGES
x[KEY_IMAGES] = "IMAGES";
#endif
#ifdef KEY_DEL_EOL
x[KEY_DEL_EOL] = "DEL_EOL";
#endif
#ifdef KEY_DEL_EOS
x[KEY_DEL_EOS] = "DEL_EOS";
#endif
#ifdef KEY_INS_LINE
x[KEY_INS_LINE] = "INS_LINE";
#endif
#ifdef KEY_DEL_LINE
x[KEY_DEL_LINE] = "DEL_LINE";
#endif
#ifdef KEY_FN
x[KEY_FN] = "FN";
#endif
#ifdef KEY_FN_ESC
x[KEY_FN_ESC] = "FN_ESC";
#endif
#ifdef KEY_FN_F1
x[KEY_FN_F1] = "FN_F1";
#endif
#ifdef KEY_FN_F2
x[KEY_FN_F2] = "FN_F2";
#endif
#ifdef KEY_FN_F3
x[KEY_FN_F3] = "FN_F3";
#endif
#ifdef KEY_FN_F4
x[KEY_FN_F4] = "FN_F4";
#endif
#ifdef KEY_FN_F5
x[KEY_FN_F5] = "FN_F5";
#endif
#ifdef KEY_FN_F6
x[KEY_FN_F6] = "FN_F6";
#endif
#ifdef KEY_FN_F7
x[KEY_FN_F7] = "FN_F7";
#endif
#ifdef KEY_FN_F8
x[KEY_FN_F8] = "FN_F8";
#endif
#ifdef KEY_FN_F9
x[KEY_FN_F9] = "FN_F9";
#endif
#ifdef KEY_FN_F10
x[KEY_FN_F10] = "FN_F10";
#endif
#ifdef KEY_FN_F11
x[KEY_FN_F11] = "FN_F11";
#endif
#ifdef KEY_FN_F12
x[KEY_FN_F12] = "FN_F12";
#endif
#ifdef KEY_FN_1
x[KEY_FN_1] = "FN_1";
#endif
#ifdef KEY_FN_2
x[KEY_FN_2] = "FN_2";
#endif
#ifdef KEY_FN_D
x[KEY_FN_D] = "FN_D";
#endif
#ifdef KEY_FN_E
x[KEY_FN_E] = "FN_E";
#endif
#ifdef KEY_FN_F
x[KEY_FN_F] = "FN_F";
#endif
#ifdef KEY_FN_S
x[KEY_FN_S] = "FN_S";
#endif
#ifdef KEY_FN_B
x[KEY_FN_B] = "FN_B";
#endif
#ifdef KEY_BRL_DOT1
x[KEY_BRL_DOT1] = "BRL_DOT1";
#endif
#ifdef KEY_BRL_DOT2
x[KEY_BRL_DOT2] = "BRL_DOT2";
#endif
#ifdef KEY_BRL_DOT3
x[KEY_BRL_DOT3] = "BRL_DOT3";
#endif
#ifdef KEY_BRL_DOT4
x[KEY_BRL_DOT4] = "BRL_DOT4";
#endif
#ifdef KEY_BRL_DOT5
x[KEY_BRL_DOT5] = "BRL_DOT5";
#endif
#ifdef KEY_BRL_DOT6
x[KEY_BRL_DOT6] = "BRL_DOT6";
#endif
#ifdef KEY_BRL_DOT7
x[KEY_BRL_DOT7] = "BRL_DOT7";
#endif
#ifdef KEY_BRL_DOT8
x[KEY_BRL_DOT8] = "BRL_DOT8";
#endif
#ifdef KEY_BRL_DOT9
x[KEY_BRL_DOT9] = "BRL_DOT9";
#endif
#ifdef KEY_BRL_DOT10
x[KEY_BRL_DOT10] = "BRL_DOT10";
#endif
#ifdef KEY_NUMERIC_0
x[KEY_NUMERIC_0] = "NUMERIC_0";
#endif
#ifdef KEY_NUMERIC_1
x[KEY_NUMERIC_1] = "NUMERIC_1";
#endif
#ifdef KEY_NUMERIC_2
x[KEY_NUMERIC_2] = "NUMERIC_2";
#endif
#ifdef KEY_NUMERIC_3
x[KEY_NUMERIC_3] = "NUMERIC_3";
#endif
#ifdef KEY_NUMERIC_4
x[KEY_NUMERIC_4] = "NUMERIC_4";
#endif
#ifdef KEY_NUMERIC_5
x[KEY_NUMERIC_5] = "NUMERIC_5";
#endif
#ifdef KEY_NUMERIC_6
x[KEY_NUMERIC_6] = "NUMERIC_6";
#endif
#ifdef KEY_NUMERIC_7
x[KEY_NUMERIC_7] = "NUMERIC_7";
#endif
#ifdef KEY_NUMERIC_8
x[KEY_NUMERIC_8] = "NUMERIC_8";
#endif
#ifdef KEY_NUMERIC_9
x[KEY_NUMERIC_9] = "NUMERIC_9";
#endif
#ifdef KEY_NUMERIC_STAR
x[KEY_NUMERIC_STAR] = "NUMERIC_STAR";
#endif
#ifdef KEY_NUMERIC_POUND
x[KEY_NUMERIC_POUND] = "NUMERIC_POUND";
#endif
#ifdef KEY_CAMERA_FOCUS
x[KEY_CAMERA_FOCUS] = "CAMERA_FOCUS";
#endif
#ifdef KEY_WPS_BUTTON
x[KEY_WPS_BUTTON] = "WPS_BUTTON";
#endif
#ifdef KEY_TOUCHPAD_TOGGLE
x[KEY_TOUCHPAD_TOGGLE] = "TOUCHPAD_TOGGLE";
#endif
#ifdef KEY_TOUCHPAD_ON
x[KEY_TOUCHPAD_ON] = "TOUCHPAD_ON";
#endif
#ifdef KEY_TOUCHPAD_OFF
x[KEY_TOUCHPAD_OFF] = "TOUCHPAD_OFF";
#endif
#ifdef KEY_CAMERA_ZOOMIN
x[KEY_CAMERA_ZOOMIN] = "CAMERA_ZOOMIN";
#endif
#ifdef KEY_CAMERA_ZOOMOUT
x[KEY_CAMERA_ZOOMOUT] = "CAMERA_ZOOMOUT";
#endif
#ifdef KEY_CAMERA_UP
x[KEY_CAMERA_UP] = "CAMERA_UP";
#endif
#ifdef KEY_CAMERA_DOWN
x[KEY_CAMERA_DOWN] = "CAMERA_DOWN";
#endif
#ifdef KEY_CAMERA_LEFT
x[KEY_CAMERA_LEFT] = "CAMERA_LEFT";
#endif
#ifdef KEY_CAMERA_RIGHT
x[KEY_CAMERA_RIGHT] = "CAMERA_RIGHT";
#endif
#ifdef BTN_TRIGGER_HAPPY
x[BTN_TRIGGER_HAPPY] = "Button TRIGGER_HAPPY";
#endif
#ifdef BTN_TRIGGER_HAPPY1
x[BTN_TRIGGER_HAPPY1] = "Button TRIGGER_HAPPY1";
#endif
#ifdef BTN_TRIGGER_HAPPY2
x[BTN_TRIGGER_HAPPY2] = "Button TRIGGER_HAPPY2";
#endif
#ifdef BTN_TRIGGER_HAPPY3
x[BTN_TRIGGER_HAPPY3] = "Button TRIGGER_HAPPY3";
#endif
#ifdef BTN_TRIGGER_HAPPY4
x[BTN_TRIGGER_HAPPY4] = "Button TRIGGER_HAPPY4";
#endif
#ifdef BTN_TRIGGER_HAPPY5
x[BTN_TRIGGER_HAPPY5] = "Button TRIGGER_HAPPY5";
#endif
#ifdef BTN_TRIGGER_HAPPY6
x[BTN_TRIGGER_HAPPY6] = "Button TRIGGER_HAPPY6";
#endif
#ifdef BTN_TRIGGER_HAPPY7
x[BTN_TRIGGER_HAPPY7] = "Button TRIGGER_HAPPY7";
#endif
#ifdef BTN_TRIGGER_HAPPY8
x[BTN_TRIGGER_HAPPY8] = "Button TRIGGER_HAPPY8";
#endif
#ifdef BTN_TRIGGER_HAPPY9
x[BTN_TRIGGER_HAPPY9] = "Button TRIGGER_HAPPY9";
#endif
#ifdef BTN_TRIGGER_HAPPY10
x[BTN_TRIGGER_HAPPY10] = "Button TRIGGER_HAPPY10";
#endif
#ifdef BTN_TRIGGER_HAPPY11
x[BTN_TRIGGER_HAPPY11] = "Button TRIGGER_HAPPY11";
#endif
#ifdef BTN_TRIGGER_HAPPY12
x[BTN_TRIGGER_HAPPY12] = "Button TRIGGER_HAPPY12";
#endif
#ifdef BTN_TRIGGER_HAPPY13
x[BTN_TRIGGER_HAPPY13] = "Button TRIGGER_HAPPY13";
#endif
#ifdef BTN_TRIGGER_HAPPY14
x[BTN_TRIGGER_HAPPY14] = "Button TRIGGER_HAPPY14";
#endif
#ifdef BTN_TRIGGER_HAPPY15
x[BTN_TRIGGER_HAPPY15] = "Button TRIGGER_HAPPY15";
#endif
#ifdef BTN_TRIGGER_HAPPY16
x[BTN_TRIGGER_HAPPY16] = "Button TRIGGER_HAPPY16";
#endif
#ifdef BTN_TRIGGER_HAPPY17
x[BTN_TRIGGER_HAPPY17] = "Button TRIGGER_HAPPY17";
#endif
#ifdef BTN_TRIGGER_HAPPY18
x[BTN_TRIGGER_HAPPY18] = "Button TRIGGER_HAPPY18";
#endif
#ifdef BTN_TRIGGER_HAPPY19
x[BTN_TRIGGER_HAPPY19] = "Button TRIGGER_HAPPY19";
#endif
#ifdef BTN_TRIGGER_HAPPY20
x[BTN_TRIGGER_HAPPY20] = "Button TRIGGER_HAPPY20";
#endif
#ifdef BTN_TRIGGER_HAPPY21
x[BTN_TRIGGER_HAPPY21] = "Button TRIGGER_HAPPY21";
#endif
#ifdef BTN_TRIGGER_HAPPY22
x[BTN_TRIGGER_HAPPY22] = "Button TRIGGER_HAPPY22";
#endif
#ifdef BTN_TRIGGER_HAPPY23
x[BTN_TRIGGER_HAPPY23] = "Button TRIGGER_HAPPY23";
#endif
#ifdef BTN_TRIGGER_HAPPY24
x[BTN_TRIGGER_HAPPY24] = "Button TRIGGER_HAPPY24";
#endif
#ifdef BTN_TRIGGER_HAPPY25
x[BTN_TRIGGER_HAPPY25] = "Button TRIGGER_HAPPY25";
#endif
#ifdef BTN_TRIGGER_HAPPY26
x[BTN_TRIGGER_HAPPY26] = "Button TRIGGER_HAPPY26";
#endif
#ifdef BTN_TRIGGER_HAPPY27
x[BTN_TRIGGER_HAPPY27] = "Button TRIGGER_HAPPY27";
#endif
#ifdef BTN_TRIGGER_HAPPY28
x[BTN_TRIGGER_HAPPY28] = "Button TRIGGER_HAPPY28";
#endif
#ifdef BTN_TRIGGER_HAPPY29
x[BTN_TRIGGER_HAPPY29] = "Button TRIGGER_HAPPY29";
#endif
#ifdef BTN_TRIGGER_HAPPY30
x[BTN_TRIGGER_HAPPY30] = "Button TRIGGER_HAPPY30";
#endif
#ifdef BTN_TRIGGER_HAPPY31
x[BTN_TRIGGER_HAPPY31] = "Button TRIGGER_HAPPY31";
#endif
#ifdef BTN_TRIGGER_HAPPY32
x[BTN_TRIGGER_HAPPY32] = "Button TRIGGER_HAPPY32";
#endif
#ifdef BTN_TRIGGER_HAPPY33
x[BTN_TRIGGER_HAPPY33] = "Button TRIGGER_HAPPY33";
#endif
#ifdef BTN_TRIGGER_HAPPY34
x[BTN_TRIGGER_HAPPY34] = "Button TRIGGER_HAPPY34";
#endif
#ifdef BTN_TRIGGER_HAPPY35
x[BTN_TRIGGER_HAPPY35] = "Button TRIGGER_HAPPY35";
#endif
#ifdef BTN_TRIGGER_HAPPY36
x[BTN_TRIGGER_HAPPY36] = "Button TRIGGER_HAPPY36";
#endif
#ifdef BTN_TRIGGER_HAPPY37
x[BTN_TRIGGER_HAPPY37] = "Button TRIGGER_HAPPY37";
#endif
#ifdef BTN_TRIGGER_HAPPY38
x[BTN_TRIGGER_HAPPY38] = "Button TRIGGER_HAPPY38";
#endif
#ifdef BTN_TRIGGER_HAPPY39
x[BTN_TRIGGER_HAPPY39] = "Button TRIGGER_HAPPY39";
#endif
#ifdef BTN_TRIGGER_HAPPY40
x[BTN_TRIGGER_HAPPY40] = "Button TRIGGER_HAPPY40";
#endif
}
