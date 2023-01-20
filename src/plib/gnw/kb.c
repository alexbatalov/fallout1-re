#include "plib/gnw/kb.h"

#include "plib/gnw/dxinput.h"
#include "plib/gnw/gnw95dx.h"
#include "plib/gnw/input.h"
#include "plib/gnw/vcr.h"

typedef struct key_ansi_t {
    short keys;
    short normal;
    short shift;
    short left_alt;
    short right_alt;
    short ctrl;
} key_ansi_t;

typedef struct key_data_t {
    // NOTE: `mapper2.exe` says it's type is `char`. However when it is too
    // many casts to `unsigned char` is needed to make sure it can be used as
    // offset (otherwise chars above 0x7F will be treated as negative values).
    unsigned char scan_code;
    unsigned short modifiers;
} key_data_t;

typedef int(AsciiConvert)();

static int kb_next_ascii_English_US();
static int kb_next_ascii_French();
static int kb_next_ascii_German();
static int kb_next_ascii_Italian();
static int kb_next_ascii_Spanish();
static int kb_next_ascii();
static void kb_map_ascii_English_US();
static void kb_map_ascii_French();
static void kb_map_ascii_German();
static void kb_map_ascii_Italian();
static void kb_map_ascii_Spanish();
static void kb_init_lock_status();
static void kb_toggle_caps();
static void kb_toggle_num();
static void kb_toggle_scroll();
static int kb_buffer_put(key_data_t* key_data);
static int kb_buffer_get(key_data_t* key_data);
static int kb_buffer_peek(int index, key_data_t** keyboardEventPtr);

// 0x539E00
static unsigned char kb_installed = 0;

// 0x539E04
static bool kb_disabled = false;

// 0x539E08
static bool kb_numpad_disabled = false;

// 0x539E0C
static bool kb_numlock_disabled = false;

// 0x539E10
static int kb_put = 0;

// 0x539E14
static int kb_get = 0;

// 0x539E18
static unsigned short extended_code = 0;

// 0x539E1A
static unsigned char kb_lock_flags = 0;

// 0x539E1C
static AsciiConvert* kb_scan_to_ascii = kb_next_ascii_English_US;

// Ring buffer of keyboard events.
//
// 0x6722A0
static key_data_t kb_buffer[64];

// A map of logical key configurations for physical scan codes [DIK_*].
//
// 0x6723A0
static key_ansi_t ascii_table[256];

// A state of physical keys [DIK_*] currently pressed.
//
// 0 - key is not pressed.
// 1 - key pressed.
//
// 0x672FA0
char keys[256];

// 0x6730A0
static unsigned int kb_idle_start_time;

// 0x6730A4
static key_data_t temp;

// 0x6730A8
kb_layout_t kb_layout;

// The number of keys currently pressed.
//
// 0x6730AC
unsigned char keynumpress;

// 0x4B6430
int GNW_kb_set()
{
    if (kb_installed) {
        return -1;
    }

    kb_installed = 1;

    // NOTE: Uninline.
    kb_clear();

    kb_init_lock_status();
    kb_set_layout(english);

    kb_idle_start_time = get_time();

    return 0;
}

// 0x4B64A0
void GNW_kb_restore()
{
    if (kb_installed) {
        kb_installed = 0;
    }
}

// 0x4B64B4
void kb_wait()
{
    if (kb_installed) {
        // NOTE: Uninline.
        kb_clear();

        do {
            GNW95_process_message();
        } while (keynumpress == 0);

        // NOTE: Uninline.
        kb_clear();
    }
}

// 0x4B6548
void kb_clear()
{
    int i;

    if (kb_installed) {
        keynumpress = 0;

        for (i = 0; i < 256; i++) {
            keys[i] = 0;
        }

        kb_put = 0;
        kb_get = 0;
    }

    dxinput_flush_keyboard_buffer();
    GNW95_clear_time_stamps();
}

// 0x4B6588
int kb_getch()
{
    int rc = -1;

    if (kb_installed != 0) {
        rc = kb_scan_to_ascii();
    }

    return rc;
}

// 0x4B65A0
void kb_disable()
{
    kb_disabled = true;
}

// 0x4B65AC
void kb_enable()
{
    kb_disabled = false;
}

// 0x4B65B8
bool kb_is_disabled()
{
    return kb_disabled;
}

// 0x4B65C0
void kb_disable_numpad()
{
    kb_numpad_disabled = true;
}

// 0x4B65CC
void kb_enable_numpad()
{
    kb_numpad_disabled = false;
}

// 0x4B65D8
bool kb_numpad_is_disabled()
{
    return kb_numpad_disabled;
}

// 0x4B65E0
void kb_disable_numlock()
{
    kb_numlock_disabled = true;
}

// 0x4B65EC
void kb_enable_numlock()
{
    kb_numlock_disabled = false;
}

// 0x4B65F8
bool kb_numlock_is_disabled()
{
    return kb_numlock_disabled;
}

// 0x4B6614
void kb_set_layout(kb_layout_t layout)
{
    int old_layout = kb_layout;
    kb_layout = layout;

    switch (layout) {
    case english:
        kb_scan_to_ascii = kb_next_ascii_English_US;
        kb_map_ascii_English_US();
        break;
    case french:
        kb_scan_to_ascii = kb_next_ascii_French;
        kb_map_ascii_French();
        break;
    case german:
        kb_scan_to_ascii = kb_next_ascii_German;
        kb_map_ascii_German();
        break;
    case italian:
        kb_scan_to_ascii = kb_next_ascii_Italian;
        kb_map_ascii_Italian();
        break;
    case spanish:
        kb_scan_to_ascii = kb_next_ascii_Spanish;
        kb_map_ascii_Spanish();
        break;
    default:
        kb_layout = old_layout;
        break;
    }
}

// 0x4B668C
kb_layout_t kb_get_layout()
{
    return kb_layout;
}

// 0x4B6694
int kb_ascii_to_scan(int ascii)
{
    int k;

    for (k = 0; k < 256; k++) {
        if (ascii_table[k].normal == k
            || ascii_table[k].shift == k
            || ascii_table[k].left_alt == k
            || ascii_table[k].right_alt == k
            || ascii_table[k].ctrl == k) {
            return k;
        }
    }

    return -1;
}

// 0x4B66F0
unsigned int kb_elapsed_time()
{
    return elapsed_time(kb_idle_start_time);
}

// 0x4B66FC
void kb_reset_elapsed_time()
{
    kb_idle_start_time = get_time();
}

// 0x4B6708
void kb_simulate_key(unsigned short scan_code)
{
    if (vcr_state == 0) {
        if (vcr_buffer_index != VCR_BUFFER_CAPACITY - 1) {
            VcrEntry* vcrEntry = &(vcr_buffer[vcr_buffer_index]);
            vcrEntry->type = VCR_ENTRY_TYPE_KEYBOARD_EVENT;
            vcrEntry->keyboardEvent.key = scan_code & 0xFFFF;
            vcrEntry->time = vcr_time;
            vcrEntry->counter = vcr_counter;

            vcr_buffer_index++;
        }
    }

    kb_idle_start_time = get_bk_time();

    if (scan_code == 224) {
        extended_code = 0x80;
    } else {
        int keyState;
        if (scan_code & 0x80) {
            scan_code &= ~0x80;
            keyState = KEY_STATE_UP;
        } else {
            keyState = KEY_STATE_DOWN;
        }

        int physicalKey = scan_code | extended_code;

        if (keyState != KEY_STATE_UP && keys[physicalKey] != KEY_STATE_UP) {
            keyState = KEY_STATE_REPEAT;
        }

        if (keys[physicalKey] != keyState) {
            keys[physicalKey] = keyState;
            if (keyState == KEY_STATE_DOWN) {
                keynumpress++;
            } else if (keyState == KEY_STATE_UP) {
                keynumpress--;
            }
        }

        if (keyState != KEY_STATE_UP) {
            temp.scan_code = physicalKey & 0xFF;
            temp.modifiers = 0;

            if (physicalKey == DIK_CAPITAL) {
                if (keys[DIK_LCONTROL] == KEY_STATE_UP && keys[DIK_RCONTROL] == KEY_STATE_UP) {
                    // NOTE: Uninline.
                    kb_toggle_caps();
                }
            } else if (physicalKey == DIK_NUMLOCK) {
                if (keys[DIK_LCONTROL] == KEY_STATE_UP && keys[DIK_RCONTROL] == KEY_STATE_UP) {
                    // NOTE: Uninline.
                    kb_toggle_num();
                }
            } else if (physicalKey == DIK_SCROLL) {
                if (keys[DIK_LCONTROL] == KEY_STATE_UP && keys[DIK_RCONTROL] == KEY_STATE_UP) {
                    // NOTE: Uninline.
                    kb_toggle_scroll();
                }
            } else if ((physicalKey == DIK_LSHIFT || physicalKey == DIK_RSHIFT) && (kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0 && kb_layout != 0) {
                if (keys[DIK_LCONTROL] == KEY_STATE_UP && keys[DIK_RCONTROL] == KEY_STATE_UP) {
                    // NOTE: Uninline.
                    kb_toggle_caps();
                }
            }

            if (kb_lock_flags != 0) {
                if ((kb_lock_flags & MODIFIER_KEY_STATE_NUM_LOCK) != 0 && !kb_numlock_disabled) {
                    temp.modifiers |= KEYBOARD_EVENT_MODIFIER_NUM_LOCK;
                }

                if ((kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
                    temp.modifiers |= KEYBOARD_EVENT_MODIFIER_CAPS_LOCK;
                }

                if ((kb_lock_flags & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
                    temp.modifiers |= KEYBOARD_EVENT_MODIFIER_SCROLL_LOCK;
                }
            }

            if (keys[DIK_LSHIFT] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }

            if (keys[DIK_RSHIFT] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_SHIFT;
            }

            if (keys[DIK_LMENU] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_ALT;
            }

            if (keys[DIK_RMENU] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_ALT;
            }

            if (keys[DIK_LCONTROL] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_CONTROL;
            }

            if (keys[DIK_RCONTROL] != KEY_STATE_UP) {
                temp.modifiers |= KEYBOARD_EVENT_MODIFIER_RIGHT_CONTROL;
            }

            // NOTE: Uninline.
            kb_buffer_put(&temp);
        }

        extended_code = 0;
    }

    if (keys[198] != KEY_STATE_UP) {
        // NOTE: Uninline.
        kb_clear();
    }
}

// 0x4B6AC8
static int kb_next_ascii_English_US()
{
    key_data_t* keyboardEvent;
    if (kb_buffer_peek(0, &keyboardEvent) != 0) {
        return -1;
    }

    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_CAPS_LOCK) != 0) {
        unsigned char a = (kb_layout != french ? DIK_A : DIK_Q);
        unsigned char m = (kb_layout != french ? DIK_M : DIK_SEMICOLON);
        unsigned char q = (kb_layout != french ? DIK_Q : DIK_A);
        unsigned char w = (kb_layout != french ? DIK_W : DIK_Z);

        unsigned char y;
        switch (kb_layout) {
        case english:
        case french:
        case italian:
        case spanish:
            y = DIK_Y;
            break;
        default:
            // GERMAN
            y = DIK_Z;
            break;
        }

        unsigned char z;
        switch (kb_layout) {
        case english:
        case italian:
        case spanish:
            z = DIK_Z;
            break;
        case french:
            z = DIK_W;
            break;
        default:
            // GERMAN
            z = DIK_Y;
            break;
        }

        unsigned char scanCode = keyboardEvent->scan_code;
        if (scanCode == a
            || scanCode == DIK_B
            || scanCode == DIK_C
            || scanCode == DIK_D
            || scanCode == DIK_E
            || scanCode == DIK_F
            || scanCode == DIK_G
            || scanCode == DIK_H
            || scanCode == DIK_I
            || scanCode == DIK_J
            || scanCode == DIK_K
            || scanCode == DIK_L
            || scanCode == m
            || scanCode == DIK_N
            || scanCode == DIK_O
            || scanCode == DIK_P
            || scanCode == q
            || scanCode == DIK_R
            || scanCode == DIK_S
            || scanCode == DIK_T
            || scanCode == DIK_U
            || scanCode == DIK_V
            || scanCode == w
            || scanCode == DIK_X
            || scanCode == y
            || scanCode == z) {
            if (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }
    }

    return kb_next_ascii();
}

// 0x4B6D94
static int kb_next_ascii_French()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B7124
static int kb_next_ascii_German()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B75EC
static int kb_next_ascii_Italian()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B78B8
static int kb_next_ascii_Spanish()
{
    // TODO: Incomplete.

    return -1;
}

// 0x4B8224
static int kb_next_ascii()
{
    key_data_t* keyboardEvent;
    if (kb_buffer_peek(0, &keyboardEvent) != 0) {
        return -1;
    }

    switch (keyboardEvent->scan_code) {
    case DIK_DIVIDE:
    case DIK_MULTIPLY:
    case DIK_SUBTRACT:
    case DIK_ADD:
    case DIK_NUMPADENTER:
        if (kb_numpad_disabled) {
            // NOTE: Uninline.
            kb_buffer_get(NULL);
            return -1;
        }
        break;
    case DIK_NUMPAD0:
    case DIK_NUMPAD1:
    case DIK_NUMPAD2:
    case DIK_NUMPAD3:
    case DIK_NUMPAD4:
    case DIK_NUMPAD5:
    case DIK_NUMPAD6:
    case DIK_NUMPAD7:
    case DIK_NUMPAD8:
    case DIK_NUMPAD9:
        if (kb_numpad_disabled) {
            // NOTE: Uninline.
            kb_buffer_get(NULL);
            return -1;
        }

        if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_ALT) == 0 && (keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_NUM_LOCK) != 0) {
            if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
                keyboardEvent->modifiers &= ~KEYBOARD_EVENT_MODIFIER_ANY_SHIFT;
            } else {
                keyboardEvent->modifiers |= KEYBOARD_EVENT_MODIFIER_LEFT_SHIFT;
            }
        }

        break;
    }

    int logicalKey = -1;

    key_ansi_t* logicalKeyDescription = &(ascii_table[keyboardEvent->scan_code]);
    if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_CONTROL) != 0) {
        logicalKey = logicalKeyDescription->ctrl;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_RIGHT_ALT) != 0) {
        logicalKey = logicalKeyDescription->right_alt;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_LEFT_ALT) != 0) {
        logicalKey = logicalKeyDescription->left_alt;
    } else if ((keyboardEvent->modifiers & KEYBOARD_EVENT_MODIFIER_ANY_SHIFT) != 0) {
        logicalKey = logicalKeyDescription->shift;
    } else {
        logicalKey = logicalKeyDescription->normal;
    }

    // NOTE: Uninline.
    kb_buffer_get(NULL);

    return logicalKey;
}

// 0x4B83E0
static void kb_map_ascii_English_US()
{
    int k;

    for (k = 0; k < 256; k++) {
        ascii_table[k].keys = -1;
        ascii_table[k].normal = -1;
        ascii_table[k].shift = -1;
        ascii_table[k].left_alt = -1;
        ascii_table[k].right_alt = -1;
        ascii_table[k].ctrl = -1;
    }

    ascii_table[DIK_ESCAPE].normal = KEY_ESCAPE;
    ascii_table[DIK_ESCAPE].shift = KEY_ESCAPE;
    ascii_table[DIK_ESCAPE].left_alt = KEY_ESCAPE;
    ascii_table[DIK_ESCAPE].right_alt = KEY_ESCAPE;
    ascii_table[DIK_ESCAPE].ctrl = KEY_ESCAPE;

    ascii_table[DIK_F1].normal = KEY_F1;
    ascii_table[DIK_F1].shift = KEY_SHIFT_F1;
    ascii_table[DIK_F1].left_alt = KEY_ALT_F1;
    ascii_table[DIK_F1].right_alt = KEY_ALT_F1;
    ascii_table[DIK_F1].ctrl = KEY_CTRL_F1;

    ascii_table[DIK_F2].normal = KEY_F2;
    ascii_table[DIK_F2].shift = KEY_SHIFT_F2;
    ascii_table[DIK_F2].left_alt = KEY_ALT_F2;
    ascii_table[DIK_F2].right_alt = KEY_ALT_F2;
    ascii_table[DIK_F2].ctrl = KEY_CTRL_F2;

    ascii_table[DIK_F3].normal = KEY_F3;
    ascii_table[DIK_F3].shift = KEY_SHIFT_F3;
    ascii_table[DIK_F3].left_alt = KEY_ALT_F3;
    ascii_table[DIK_F3].right_alt = KEY_ALT_F3;
    ascii_table[DIK_F3].ctrl = KEY_CTRL_F3;

    ascii_table[DIK_F4].normal = KEY_F4;
    ascii_table[DIK_F4].shift = KEY_SHIFT_F4;
    ascii_table[DIK_F4].left_alt = KEY_ALT_F4;
    ascii_table[DIK_F4].right_alt = KEY_ALT_F4;
    ascii_table[DIK_F4].ctrl = KEY_CTRL_F4;

    ascii_table[DIK_F5].normal = KEY_F5;
    ascii_table[DIK_F5].shift = KEY_SHIFT_F5;
    ascii_table[DIK_F5].left_alt = KEY_ALT_F5;
    ascii_table[DIK_F5].right_alt = KEY_ALT_F5;
    ascii_table[DIK_F5].ctrl = KEY_CTRL_F5;

    ascii_table[DIK_F6].normal = KEY_F6;
    ascii_table[DIK_F6].shift = KEY_SHIFT_F6;
    ascii_table[DIK_F6].left_alt = KEY_ALT_F6;
    ascii_table[DIK_F6].right_alt = KEY_ALT_F6;
    ascii_table[DIK_F6].ctrl = KEY_CTRL_F6;

    ascii_table[DIK_F7].normal = KEY_F7;
    ascii_table[DIK_F7].shift = KEY_SHIFT_F7;
    ascii_table[DIK_F7].left_alt = KEY_ALT_F7;
    ascii_table[DIK_F7].right_alt = KEY_ALT_F7;
    ascii_table[DIK_F7].ctrl = KEY_CTRL_F7;

    ascii_table[DIK_F8].normal = KEY_F8;
    ascii_table[DIK_F8].shift = KEY_SHIFT_F8;
    ascii_table[DIK_F8].left_alt = KEY_ALT_F8;
    ascii_table[DIK_F8].right_alt = KEY_ALT_F8;
    ascii_table[DIK_F8].ctrl = KEY_CTRL_F8;

    ascii_table[DIK_F9].normal = KEY_F9;
    ascii_table[DIK_F9].shift = KEY_SHIFT_F9;
    ascii_table[DIK_F9].left_alt = KEY_ALT_F9;
    ascii_table[DIK_F9].right_alt = KEY_ALT_F9;
    ascii_table[DIK_F9].ctrl = KEY_CTRL_F9;

    ascii_table[DIK_F10].normal = KEY_F10;
    ascii_table[DIK_F10].shift = KEY_SHIFT_F10;
    ascii_table[DIK_F10].left_alt = KEY_ALT_F10;
    ascii_table[DIK_F10].right_alt = KEY_ALT_F10;
    ascii_table[DIK_F10].ctrl = KEY_CTRL_F10;

    ascii_table[DIK_F11].normal = KEY_F11;
    ascii_table[DIK_F11].shift = KEY_SHIFT_F11;
    ascii_table[DIK_F11].left_alt = KEY_ALT_F11;
    ascii_table[DIK_F11].right_alt = KEY_ALT_F11;
    ascii_table[DIK_F11].ctrl = KEY_CTRL_F11;

    ascii_table[DIK_F12].normal = KEY_F12;
    ascii_table[DIK_F12].shift = KEY_SHIFT_F12;
    ascii_table[DIK_F12].left_alt = KEY_ALT_F12;
    ascii_table[DIK_F12].right_alt = KEY_ALT_F12;
    ascii_table[DIK_F12].ctrl = KEY_CTRL_F12;

    switch (kb_layout) {
    case english:
        k = DIK_GRAVE;
        break;
    case french:
        k = DIK_2;
        break;
    case italian:
    case spanish:
        k = 0;
        break;
    default:
        k = DIK_RBRACKET;
        break;
    }

    ascii_table[k].normal = KEY_GRAVE;
    ascii_table[k].shift = KEY_TILDE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_1].normal = KEY_1;
    ascii_table[DIK_1].shift = KEY_EXCLAMATION;
    ascii_table[DIK_1].left_alt = -1;
    ascii_table[DIK_1].right_alt = -1;
    ascii_table[DIK_1].ctrl = -1;

    ascii_table[DIK_2].normal = KEY_2;
    ascii_table[DIK_2].shift = KEY_AT;
    ascii_table[DIK_2].left_alt = -1;
    ascii_table[DIK_2].right_alt = -1;
    ascii_table[DIK_2].ctrl = -1;

    ascii_table[DIK_3].normal = KEY_3;
    ascii_table[DIK_3].shift = KEY_NUMBER_SIGN;
    ascii_table[DIK_3].left_alt = -1;
    ascii_table[DIK_3].right_alt = -1;
    ascii_table[DIK_3].ctrl = -1;

    ascii_table[DIK_4].normal = KEY_4;
    ascii_table[DIK_4].shift = KEY_DOLLAR;
    ascii_table[DIK_4].left_alt = -1;
    ascii_table[DIK_4].right_alt = -1;
    ascii_table[DIK_4].ctrl = -1;

    ascii_table[DIK_5].normal = KEY_5;
    ascii_table[DIK_5].shift = KEY_PERCENT;
    ascii_table[DIK_5].left_alt = -1;
    ascii_table[DIK_5].right_alt = -1;
    ascii_table[DIK_5].ctrl = -1;

    ascii_table[DIK_6].normal = KEY_6;
    ascii_table[DIK_6].shift = KEY_CARET;
    ascii_table[DIK_6].left_alt = -1;
    ascii_table[DIK_6].right_alt = -1;
    ascii_table[DIK_6].ctrl = -1;

    ascii_table[DIK_7].normal = KEY_7;
    ascii_table[DIK_7].shift = KEY_AMPERSAND;
    ascii_table[DIK_7].left_alt = -1;
    ascii_table[DIK_7].right_alt = -1;
    ascii_table[DIK_7].ctrl = -1;

    ascii_table[DIK_8].normal = KEY_8;
    ascii_table[DIK_8].shift = KEY_ASTERISK;
    ascii_table[DIK_8].left_alt = -1;
    ascii_table[DIK_8].right_alt = -1;
    ascii_table[DIK_8].ctrl = -1;

    ascii_table[DIK_9].normal = KEY_9;
    ascii_table[DIK_9].shift = KEY_PAREN_LEFT;
    ascii_table[DIK_9].left_alt = -1;
    ascii_table[DIK_9].right_alt = -1;
    ascii_table[DIK_9].ctrl = -1;

    ascii_table[DIK_0].normal = KEY_0;
    ascii_table[DIK_0].shift = KEY_PAREN_RIGHT;
    ascii_table[DIK_0].left_alt = -1;
    ascii_table[DIK_0].right_alt = -1;
    ascii_table[DIK_0].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_MINUS;
        break;
    case french:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
    case french:
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_0;
        break;
    }

    ascii_table[k].normal = KEY_EQUAL;
    ascii_table[k].shift = KEY_PLUS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_BACK].normal = KEY_BACKSPACE;
    ascii_table[DIK_BACK].shift = KEY_BACKSPACE;
    ascii_table[DIK_BACK].left_alt = KEY_BACKSPACE;
    ascii_table[DIK_BACK].right_alt = KEY_BACKSPACE;
    ascii_table[DIK_BACK].ctrl = KEY_DEL;

    ascii_table[DIK_TAB].normal = KEY_TAB;
    ascii_table[DIK_TAB].shift = KEY_TAB;
    ascii_table[DIK_TAB].left_alt = KEY_TAB;
    ascii_table[DIK_TAB].right_alt = KEY_TAB;
    ascii_table[DIK_TAB].ctrl = KEY_TAB;

    switch (kb_layout) {
    case french:
        k = DIK_A;
        break;
    default:
        k = DIK_Q;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Q;
    ascii_table[k].shift = KEY_UPPERCASE_Q;
    ascii_table[k].left_alt = KEY_ALT_Q;
    ascii_table[k].right_alt = KEY_ALT_Q;
    ascii_table[k].ctrl = KEY_CTRL_Q;

    switch (kb_layout) {
    case french:
        k = DIK_Z;
        break;
    default:
        k = DIK_W;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_W;
    ascii_table[k].shift = KEY_UPPERCASE_W;
    ascii_table[k].left_alt = KEY_ALT_W;
    ascii_table[k].right_alt = KEY_ALT_W;
    ascii_table[k].ctrl = KEY_CTRL_W;

    ascii_table[DIK_E].normal = KEY_LOWERCASE_E;
    ascii_table[DIK_E].shift = KEY_UPPERCASE_E;
    ascii_table[DIK_E].left_alt = KEY_ALT_E;
    ascii_table[DIK_E].right_alt = KEY_ALT_E;
    ascii_table[DIK_E].ctrl = KEY_CTRL_E;

    ascii_table[DIK_R].normal = KEY_LOWERCASE_R;
    ascii_table[DIK_R].shift = KEY_UPPERCASE_R;
    ascii_table[DIK_R].left_alt = KEY_ALT_R;
    ascii_table[DIK_R].right_alt = KEY_ALT_R;
    ascii_table[DIK_R].ctrl = KEY_CTRL_R;

    ascii_table[DIK_T].normal = KEY_LOWERCASE_T;
    ascii_table[DIK_T].shift = KEY_UPPERCASE_T;
    ascii_table[DIK_T].left_alt = KEY_ALT_T;
    ascii_table[DIK_T].right_alt = KEY_ALT_T;
    ascii_table[DIK_T].ctrl = KEY_CTRL_T;

    switch (kb_layout) {
    case english:
    case french:
    case italian:
    case spanish:
        k = DIK_Y;
        break;
    default:
        k = DIK_Z;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Y;
    ascii_table[k].shift = KEY_UPPERCASE_Y;
    ascii_table[k].left_alt = KEY_ALT_Y;
    ascii_table[k].right_alt = KEY_ALT_Y;
    ascii_table[k].ctrl = KEY_CTRL_Y;

    ascii_table[DIK_U].normal = KEY_LOWERCASE_U;
    ascii_table[DIK_U].shift = KEY_UPPERCASE_U;
    ascii_table[DIK_U].left_alt = KEY_ALT_U;
    ascii_table[DIK_U].right_alt = KEY_ALT_U;
    ascii_table[DIK_U].ctrl = KEY_CTRL_U;

    ascii_table[DIK_I].normal = KEY_LOWERCASE_I;
    ascii_table[DIK_I].shift = KEY_UPPERCASE_I;
    ascii_table[DIK_I].left_alt = KEY_ALT_I;
    ascii_table[DIK_I].right_alt = KEY_ALT_I;
    ascii_table[DIK_I].ctrl = KEY_CTRL_I;

    ascii_table[DIK_O].normal = KEY_LOWERCASE_O;
    ascii_table[DIK_O].shift = KEY_UPPERCASE_O;
    ascii_table[DIK_O].left_alt = KEY_ALT_O;
    ascii_table[DIK_O].right_alt = KEY_ALT_O;
    ascii_table[DIK_O].ctrl = KEY_CTRL_O;

    ascii_table[DIK_P].normal = KEY_LOWERCASE_P;
    ascii_table[DIK_P].shift = KEY_UPPERCASE_P;
    ascii_table[DIK_P].left_alt = KEY_ALT_P;
    ascii_table[DIK_P].right_alt = KEY_ALT_P;
    ascii_table[DIK_P].ctrl = KEY_CTRL_P;

    switch (kb_layout) {
    case english:
    case italian:
    case spanish:
        k = DIK_LBRACKET;
        break;
    case french:
        k = DIK_5;
        break;
    default:
        k = DIK_8;
        break;
    }

    ascii_table[k].normal = KEY_BRACKET_LEFT;
    ascii_table[k].shift = KEY_BRACE_LEFT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
    case italian:
    case spanish:
        k = DIK_RBRACKET;
        break;
    case french:
        k = DIK_MINUS;
        break;
    default:
        k = DIK_9;
        break;
    }

    ascii_table[k].normal = KEY_BRACKET_RIGHT;
    ascii_table[k].shift = KEY_BRACE_RIGHT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_BACKSLASH;
        break;
    case french:
        k = DIK_8;
        break;
    case italian:
    case spanish:
        k = DIK_GRAVE;
        break;
    default:
        k = DIK_MINUS;
        break;
    }

    ascii_table[k].normal = KEY_BACKSLASH;
    ascii_table[k].shift = KEY_BAR;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = KEY_CTRL_BACKSLASH;

    ascii_table[DIK_CAPITAL].normal = -1;
    ascii_table[DIK_CAPITAL].shift = -1;
    ascii_table[DIK_CAPITAL].left_alt = -1;
    ascii_table[DIK_CAPITAL].right_alt = -1;
    ascii_table[DIK_CAPITAL].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_Q;
        break;
    default:
        k = DIK_A;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_A;
    ascii_table[k].shift = KEY_UPPERCASE_A;
    ascii_table[k].left_alt = KEY_ALT_A;
    ascii_table[k].right_alt = KEY_ALT_A;
    ascii_table[k].ctrl = KEY_CTRL_A;

    ascii_table[DIK_S].normal = KEY_LOWERCASE_S;
    ascii_table[DIK_S].shift = KEY_UPPERCASE_S;
    ascii_table[DIK_S].left_alt = KEY_ALT_S;
    ascii_table[DIK_S].right_alt = KEY_ALT_S;
    ascii_table[DIK_S].ctrl = KEY_CTRL_S;

    ascii_table[DIK_D].normal = KEY_LOWERCASE_D;
    ascii_table[DIK_D].shift = KEY_UPPERCASE_D;
    ascii_table[DIK_D].left_alt = KEY_ALT_D;
    ascii_table[DIK_D].right_alt = KEY_ALT_D;
    ascii_table[DIK_D].ctrl = KEY_CTRL_D;

    ascii_table[DIK_F].normal = KEY_LOWERCASE_F;
    ascii_table[DIK_F].shift = KEY_UPPERCASE_F;
    ascii_table[DIK_F].left_alt = KEY_ALT_F;
    ascii_table[DIK_F].right_alt = KEY_ALT_F;
    ascii_table[DIK_F].ctrl = KEY_CTRL_F;

    ascii_table[DIK_G].normal = KEY_LOWERCASE_G;
    ascii_table[DIK_G].shift = KEY_UPPERCASE_G;
    ascii_table[DIK_G].left_alt = KEY_ALT_G;
    ascii_table[DIK_G].right_alt = KEY_ALT_G;
    ascii_table[DIK_G].ctrl = KEY_CTRL_G;

    ascii_table[DIK_H].normal = KEY_LOWERCASE_H;
    ascii_table[DIK_H].shift = KEY_UPPERCASE_H;
    ascii_table[DIK_H].left_alt = KEY_ALT_H;
    ascii_table[DIK_H].right_alt = KEY_ALT_H;
    ascii_table[DIK_H].ctrl = KEY_CTRL_H;

    ascii_table[DIK_J].normal = KEY_LOWERCASE_J;
    ascii_table[DIK_J].shift = KEY_UPPERCASE_J;
    ascii_table[DIK_J].left_alt = KEY_ALT_J;
    ascii_table[DIK_J].right_alt = KEY_ALT_J;
    ascii_table[DIK_J].ctrl = KEY_CTRL_J;

    ascii_table[DIK_K].normal = KEY_LOWERCASE_K;
    ascii_table[DIK_K].shift = KEY_UPPERCASE_K;
    ascii_table[DIK_K].left_alt = KEY_ALT_K;
    ascii_table[DIK_K].right_alt = KEY_ALT_K;
    ascii_table[DIK_K].ctrl = KEY_CTRL_K;

    ascii_table[DIK_L].normal = KEY_LOWERCASE_L;
    ascii_table[DIK_L].shift = KEY_UPPERCASE_L;
    ascii_table[DIK_L].left_alt = KEY_ALT_L;
    ascii_table[DIK_L].right_alt = KEY_ALT_L;
    ascii_table[DIK_L].ctrl = KEY_CTRL_L;

    switch (kb_layout) {
    case english:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_SEMICOLON;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_APOSTROPHE;
        break;
    case french:
        k = DIK_3;
        break;
    default:
        k = DIK_2;
        break;
    }

    ascii_table[k].normal = KEY_SINGLE_QUOTE;
    ascii_table[k].shift = KEY_QUOTE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_RETURN].normal = KEY_RETURN;
    ascii_table[DIK_RETURN].shift = KEY_RETURN;
    ascii_table[DIK_RETURN].left_alt = KEY_RETURN;
    ascii_table[DIK_RETURN].right_alt = KEY_RETURN;
    ascii_table[DIK_RETURN].ctrl = KEY_CTRL_J;

    ascii_table[DIK_LSHIFT].normal = -1;
    ascii_table[DIK_LSHIFT].shift = -1;
    ascii_table[DIK_LSHIFT].left_alt = -1;
    ascii_table[DIK_LSHIFT].right_alt = -1;
    ascii_table[DIK_LSHIFT].ctrl = -1;

    switch (kb_layout) {
    case english:
    case italian:
    case spanish:
        k = DIK_Z;
        break;
    case french:
        k = DIK_W;
        break;
    default:
        k = DIK_Y;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Z;
    ascii_table[k].shift = KEY_UPPERCASE_Z;
    ascii_table[k].left_alt = KEY_ALT_Z;
    ascii_table[k].right_alt = KEY_ALT_Z;
    ascii_table[k].ctrl = KEY_CTRL_Z;

    ascii_table[DIK_X].normal = KEY_LOWERCASE_X;
    ascii_table[DIK_X].shift = KEY_UPPERCASE_X;
    ascii_table[DIK_X].left_alt = KEY_ALT_X;
    ascii_table[DIK_X].right_alt = KEY_ALT_X;
    ascii_table[DIK_X].ctrl = KEY_CTRL_X;

    ascii_table[DIK_C].normal = KEY_LOWERCASE_C;
    ascii_table[DIK_C].shift = KEY_UPPERCASE_C;
    ascii_table[DIK_C].left_alt = KEY_ALT_C;
    ascii_table[DIK_C].right_alt = KEY_ALT_C;
    ascii_table[DIK_C].ctrl = KEY_CTRL_C;

    ascii_table[DIK_V].normal = KEY_LOWERCASE_V;
    ascii_table[DIK_V].shift = KEY_UPPERCASE_V;
    ascii_table[DIK_V].left_alt = KEY_ALT_V;
    ascii_table[DIK_V].right_alt = KEY_ALT_V;
    ascii_table[DIK_V].ctrl = KEY_CTRL_V;

    ascii_table[DIK_B].normal = KEY_LOWERCASE_B;
    ascii_table[DIK_B].shift = KEY_UPPERCASE_B;
    ascii_table[DIK_B].left_alt = KEY_ALT_B;
    ascii_table[DIK_B].right_alt = KEY_ALT_B;
    ascii_table[DIK_B].ctrl = KEY_CTRL_B;

    ascii_table[DIK_N].normal = KEY_LOWERCASE_N;
    ascii_table[DIK_N].shift = KEY_UPPERCASE_N;
    ascii_table[DIK_N].left_alt = KEY_ALT_N;
    ascii_table[DIK_N].right_alt = KEY_ALT_N;
    ascii_table[DIK_N].ctrl = KEY_CTRL_N;

    switch (kb_layout) {
    case french:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_M;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_M;
    ascii_table[k].shift = KEY_UPPERCASE_M;
    ascii_table[k].left_alt = KEY_ALT_M;
    ascii_table[k].right_alt = KEY_ALT_M;
    ascii_table[k].ctrl = KEY_CTRL_M;

    switch (kb_layout) {
    case french:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_LESS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_GREATER;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_SLASH;
        break;
    case french:
        k = DIK_PERIOD;
        break;
    default:
        k = DIK_7;
        break;
    }

    ascii_table[k].normal = KEY_SLASH;
    ascii_table[k].shift = KEY_QUESTION;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_RSHIFT].normal = -1;
    ascii_table[DIK_RSHIFT].shift = -1;
    ascii_table[DIK_RSHIFT].left_alt = -1;
    ascii_table[DIK_RSHIFT].right_alt = -1;
    ascii_table[DIK_RSHIFT].ctrl = -1;

    ascii_table[DIK_LCONTROL].normal = -1;
    ascii_table[DIK_LCONTROL].shift = -1;
    ascii_table[DIK_LCONTROL].left_alt = -1;
    ascii_table[DIK_LCONTROL].right_alt = -1;
    ascii_table[DIK_LCONTROL].ctrl = -1;

    ascii_table[DIK_LMENU].normal = -1;
    ascii_table[DIK_LMENU].shift = -1;
    ascii_table[DIK_LMENU].left_alt = -1;
    ascii_table[DIK_LMENU].right_alt = -1;
    ascii_table[DIK_LMENU].ctrl = -1;

    ascii_table[DIK_SPACE].normal = KEY_SPACE;
    ascii_table[DIK_SPACE].shift = KEY_SPACE;
    ascii_table[DIK_SPACE].left_alt = KEY_SPACE;
    ascii_table[DIK_SPACE].right_alt = KEY_SPACE;
    ascii_table[DIK_SPACE].ctrl = KEY_SPACE;

    ascii_table[DIK_RMENU].normal = -1;
    ascii_table[DIK_RMENU].shift = -1;
    ascii_table[DIK_RMENU].left_alt = -1;
    ascii_table[DIK_RMENU].right_alt = -1;
    ascii_table[DIK_RMENU].ctrl = -1;

    ascii_table[DIK_RCONTROL].normal = -1;
    ascii_table[DIK_RCONTROL].shift = -1;
    ascii_table[DIK_RCONTROL].left_alt = -1;
    ascii_table[DIK_RCONTROL].right_alt = -1;
    ascii_table[DIK_RCONTROL].ctrl = -1;

    ascii_table[DIK_INSERT].normal = KEY_INSERT;
    ascii_table[DIK_INSERT].shift = KEY_INSERT;
    ascii_table[DIK_INSERT].left_alt = KEY_ALT_INSERT;
    ascii_table[DIK_INSERT].right_alt = KEY_ALT_INSERT;
    ascii_table[DIK_INSERT].ctrl = KEY_CTRL_INSERT;

    ascii_table[DIK_HOME].normal = KEY_HOME;
    ascii_table[DIK_HOME].shift = KEY_HOME;
    ascii_table[DIK_HOME].left_alt = KEY_ALT_HOME;
    ascii_table[DIK_HOME].right_alt = KEY_ALT_HOME;
    ascii_table[DIK_HOME].ctrl = KEY_CTRL_HOME;

    ascii_table[DIK_PRIOR].normal = KEY_PAGE_UP;
    ascii_table[DIK_PRIOR].shift = KEY_PAGE_UP;
    ascii_table[DIK_PRIOR].left_alt = KEY_ALT_PAGE_UP;
    ascii_table[DIK_PRIOR].right_alt = KEY_ALT_PAGE_UP;
    ascii_table[DIK_PRIOR].ctrl = KEY_CTRL_PAGE_UP;

    ascii_table[DIK_DELETE].normal = KEY_DELETE;
    ascii_table[DIK_DELETE].shift = KEY_DELETE;
    ascii_table[DIK_DELETE].left_alt = KEY_ALT_DELETE;
    ascii_table[DIK_DELETE].right_alt = KEY_ALT_DELETE;
    ascii_table[DIK_DELETE].ctrl = KEY_CTRL_DELETE;

    ascii_table[DIK_END].normal = KEY_END;
    ascii_table[DIK_END].shift = KEY_END;
    ascii_table[DIK_END].left_alt = KEY_ALT_END;
    ascii_table[DIK_END].right_alt = KEY_ALT_END;
    ascii_table[DIK_END].ctrl = KEY_CTRL_END;

    ascii_table[DIK_NEXT].normal = KEY_PAGE_DOWN;
    ascii_table[DIK_NEXT].shift = KEY_PAGE_DOWN;
    ascii_table[DIK_NEXT].left_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[DIK_NEXT].right_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[DIK_NEXT].ctrl = KEY_CTRL_PAGE_DOWN;

    ascii_table[DIK_UP].normal = KEY_ARROW_UP;
    ascii_table[DIK_UP].shift = KEY_ARROW_UP;
    ascii_table[DIK_UP].left_alt = KEY_ALT_ARROW_UP;
    ascii_table[DIK_UP].right_alt = KEY_ALT_ARROW_UP;
    ascii_table[DIK_UP].ctrl = KEY_CTRL_ARROW_UP;

    ascii_table[DIK_DOWN].normal = KEY_ARROW_DOWN;
    ascii_table[DIK_DOWN].shift = KEY_ARROW_DOWN;
    ascii_table[DIK_DOWN].left_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[DIK_DOWN].right_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[DIK_DOWN].ctrl = KEY_CTRL_ARROW_DOWN;

    ascii_table[DIK_LEFT].normal = KEY_ARROW_LEFT;
    ascii_table[DIK_LEFT].shift = KEY_ARROW_LEFT;
    ascii_table[DIK_LEFT].left_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[DIK_LEFT].right_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[DIK_LEFT].ctrl = KEY_CTRL_ARROW_LEFT;

    ascii_table[DIK_RIGHT].normal = KEY_ARROW_RIGHT;
    ascii_table[DIK_RIGHT].shift = KEY_ARROW_RIGHT;
    ascii_table[DIK_RIGHT].left_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[DIK_RIGHT].right_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[DIK_RIGHT].ctrl = KEY_CTRL_ARROW_RIGHT;

    ascii_table[DIK_NUMLOCK].normal = -1;
    ascii_table[DIK_NUMLOCK].shift = -1;
    ascii_table[DIK_NUMLOCK].left_alt = -1;
    ascii_table[DIK_NUMLOCK].right_alt = -1;
    ascii_table[DIK_NUMLOCK].ctrl = -1;

    ascii_table[DIK_DIVIDE].normal = KEY_SLASH;
    ascii_table[DIK_DIVIDE].shift = KEY_SLASH;
    ascii_table[DIK_DIVIDE].left_alt = -1;
    ascii_table[DIK_DIVIDE].right_alt = -1;
    ascii_table[DIK_DIVIDE].ctrl = 3;

    ascii_table[DIK_MULTIPLY].normal = KEY_ASTERISK;
    ascii_table[DIK_MULTIPLY].shift = KEY_ASTERISK;
    ascii_table[DIK_MULTIPLY].left_alt = -1;
    ascii_table[DIK_MULTIPLY].right_alt = -1;
    ascii_table[DIK_MULTIPLY].ctrl = -1;

    ascii_table[DIK_SUBTRACT].normal = KEY_MINUS;
    ascii_table[DIK_SUBTRACT].shift = KEY_MINUS;
    ascii_table[DIK_SUBTRACT].left_alt = -1;
    ascii_table[DIK_SUBTRACT].right_alt = -1;
    ascii_table[DIK_SUBTRACT].ctrl = -1;

    ascii_table[DIK_NUMPAD7].normal = KEY_HOME;
    ascii_table[DIK_NUMPAD7].shift = KEY_7;
    ascii_table[DIK_NUMPAD7].left_alt = KEY_ALT_HOME;
    ascii_table[DIK_NUMPAD7].right_alt = KEY_ALT_HOME;
    ascii_table[DIK_NUMPAD7].ctrl = KEY_CTRL_HOME;

    ascii_table[DIK_NUMPAD8].normal = KEY_ARROW_UP;
    ascii_table[DIK_NUMPAD8].shift = KEY_8;
    ascii_table[DIK_NUMPAD8].left_alt = KEY_ALT_ARROW_UP;
    ascii_table[DIK_NUMPAD8].right_alt = KEY_ALT_ARROW_UP;
    ascii_table[DIK_NUMPAD8].ctrl = KEY_CTRL_ARROW_UP;

    ascii_table[DIK_NUMPAD9].normal = KEY_PAGE_UP;
    ascii_table[DIK_NUMPAD9].shift = KEY_9;
    ascii_table[DIK_NUMPAD9].left_alt = KEY_ALT_PAGE_UP;
    ascii_table[DIK_NUMPAD9].right_alt = KEY_ALT_PAGE_UP;
    ascii_table[DIK_NUMPAD9].ctrl = KEY_CTRL_PAGE_UP;

    ascii_table[DIK_ADD].normal = KEY_PLUS;
    ascii_table[DIK_ADD].shift = KEY_PLUS;
    ascii_table[DIK_ADD].left_alt = -1;
    ascii_table[DIK_ADD].right_alt = -1;
    ascii_table[DIK_ADD].ctrl = -1;

    ascii_table[DIK_NUMPAD4].normal = KEY_ARROW_LEFT;
    ascii_table[DIK_NUMPAD4].shift = KEY_4;
    ascii_table[DIK_NUMPAD4].left_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[DIK_NUMPAD4].right_alt = KEY_ALT_ARROW_LEFT;
    ascii_table[DIK_NUMPAD4].ctrl = KEY_CTRL_ARROW_LEFT;

    ascii_table[DIK_NUMPAD5].normal = KEY_NUMBERPAD_5;
    ascii_table[DIK_NUMPAD5].shift = KEY_5;
    ascii_table[DIK_NUMPAD5].left_alt = KEY_ALT_NUMBERPAD_5;
    ascii_table[DIK_NUMPAD5].right_alt = KEY_ALT_NUMBERPAD_5;
    ascii_table[DIK_NUMPAD5].ctrl = KEY_CTRL_NUMBERPAD_5;

    ascii_table[DIK_NUMPAD6].normal = KEY_ARROW_RIGHT;
    ascii_table[DIK_NUMPAD6].shift = KEY_6;
    ascii_table[DIK_NUMPAD6].left_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[DIK_NUMPAD6].right_alt = KEY_ALT_ARROW_RIGHT;
    ascii_table[DIK_NUMPAD6].ctrl = KEY_CTRL_ARROW_RIGHT;

    ascii_table[DIK_NUMPAD1].normal = KEY_END;
    ascii_table[DIK_NUMPAD1].shift = KEY_1;
    ascii_table[DIK_NUMPAD1].left_alt = KEY_ALT_END;
    ascii_table[DIK_NUMPAD1].right_alt = KEY_ALT_END;
    ascii_table[DIK_NUMPAD1].ctrl = KEY_CTRL_END;

    ascii_table[DIK_NUMPAD2].normal = KEY_ARROW_DOWN;
    ascii_table[DIK_NUMPAD2].shift = KEY_2;
    ascii_table[DIK_NUMPAD2].left_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[DIK_NUMPAD2].right_alt = KEY_ALT_ARROW_DOWN;
    ascii_table[DIK_NUMPAD2].ctrl = KEY_CTRL_ARROW_DOWN;

    ascii_table[DIK_NUMPAD3].normal = KEY_PAGE_DOWN;
    ascii_table[DIK_NUMPAD3].shift = KEY_3;
    ascii_table[DIK_NUMPAD3].left_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[DIK_NUMPAD3].right_alt = KEY_ALT_PAGE_DOWN;
    ascii_table[DIK_NUMPAD3].ctrl = KEY_CTRL_PAGE_DOWN;

    ascii_table[DIK_NUMPADENTER].normal = KEY_RETURN;
    ascii_table[DIK_NUMPADENTER].shift = KEY_RETURN;
    ascii_table[DIK_NUMPADENTER].left_alt = -1;
    ascii_table[DIK_NUMPADENTER].right_alt = -1;
    ascii_table[DIK_NUMPADENTER].ctrl = -1;

    ascii_table[DIK_NUMPAD0].normal = KEY_INSERT;
    ascii_table[DIK_NUMPAD0].shift = KEY_0;
    ascii_table[DIK_NUMPAD0].left_alt = KEY_ALT_INSERT;
    ascii_table[DIK_NUMPAD0].right_alt = KEY_ALT_INSERT;
    ascii_table[DIK_NUMPAD0].ctrl = KEY_CTRL_INSERT;

    ascii_table[DIK_DECIMAL].normal = KEY_DELETE;
    ascii_table[DIK_DECIMAL].shift = KEY_DOT;
    ascii_table[DIK_DECIMAL].left_alt = -1;
    ascii_table[DIK_DECIMAL].right_alt = KEY_ALT_DELETE;
    ascii_table[DIK_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4BABD8
static void kb_map_ascii_French()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[DIK_GRAVE].normal = KEY_178;
    ascii_table[DIK_GRAVE].shift = -1;
    ascii_table[DIK_GRAVE].left_alt = -1;
    ascii_table[DIK_GRAVE].right_alt = -1;
    ascii_table[DIK_GRAVE].ctrl = -1;

    ascii_table[DIK_1].normal = KEY_AMPERSAND;
    ascii_table[DIK_1].shift = KEY_1;
    ascii_table[DIK_1].left_alt = -1;
    ascii_table[DIK_1].right_alt = -1;
    ascii_table[DIK_1].ctrl = -1;

    ascii_table[DIK_2].normal = KEY_233;
    ascii_table[DIK_2].shift = KEY_2;
    ascii_table[DIK_2].left_alt = -1;
    ascii_table[DIK_2].right_alt = KEY_152;
    ascii_table[DIK_2].ctrl = -1;

    ascii_table[DIK_3].normal = KEY_QUOTE;
    ascii_table[DIK_3].shift = KEY_3;
    ascii_table[DIK_3].left_alt = -1;
    ascii_table[DIK_3].right_alt = KEY_NUMBER_SIGN;
    ascii_table[DIK_3].ctrl = -1;

    ascii_table[DIK_4].normal = KEY_SINGLE_QUOTE;
    ascii_table[DIK_4].shift = KEY_4;
    ascii_table[DIK_4].left_alt = -1;
    ascii_table[DIK_4].right_alt = KEY_BRACE_LEFT;
    ascii_table[DIK_4].ctrl = -1;

    ascii_table[DIK_5].normal = KEY_PAREN_LEFT;
    ascii_table[DIK_5].shift = KEY_5;
    ascii_table[DIK_5].left_alt = -1;
    ascii_table[DIK_5].right_alt = KEY_BRACKET_LEFT;
    ascii_table[DIK_5].ctrl = -1;

    ascii_table[DIK_6].normal = KEY_150;
    ascii_table[DIK_6].shift = KEY_6;
    ascii_table[DIK_6].left_alt = -1;
    ascii_table[DIK_6].right_alt = KEY_166;
    ascii_table[DIK_6].ctrl = -1;

    ascii_table[DIK_7].normal = KEY_232;
    ascii_table[DIK_7].shift = KEY_7;
    ascii_table[DIK_7].left_alt = -1;
    ascii_table[DIK_7].right_alt = KEY_GRAVE;
    ascii_table[DIK_7].ctrl = -1;

    ascii_table[DIK_8].normal = KEY_UNDERSCORE;
    ascii_table[DIK_8].shift = KEY_8;
    ascii_table[DIK_8].left_alt = -1;
    ascii_table[DIK_8].right_alt = KEY_BACKSLASH;
    ascii_table[DIK_8].ctrl = -1;

    ascii_table[DIK_9].normal = KEY_231;
    ascii_table[DIK_9].shift = KEY_9;
    ascii_table[DIK_9].left_alt = -1;
    ascii_table[DIK_9].right_alt = KEY_136;
    ascii_table[DIK_9].ctrl = -1;

    ascii_table[DIK_0].normal = KEY_224;
    ascii_table[DIK_0].shift = KEY_0;
    ascii_table[DIK_0].left_alt = -1;
    ascii_table[DIK_0].right_alt = KEY_AT;
    ascii_table[DIK_0].ctrl = -1;

    ascii_table[DIK_MINUS].normal = KEY_PAREN_RIGHT;
    ascii_table[DIK_MINUS].shift = KEY_176;
    ascii_table[DIK_MINUS].left_alt = -1;
    ascii_table[DIK_MINUS].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[DIK_MINUS].ctrl = -1;

    switch (kb_layout) {
    case english:
    case french:
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_0;
        break;
    }

    ascii_table[k].normal = KEY_EQUAL;
    ascii_table[k].shift = KEY_PLUS;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = KEY_BRACE_RIGHT;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_LBRACKET].normal = KEY_136;
    ascii_table[DIK_LBRACKET].shift = KEY_168;
    ascii_table[DIK_LBRACKET].left_alt = -1;
    ascii_table[DIK_LBRACKET].right_alt = -1;
    ascii_table[DIK_LBRACKET].ctrl = -1;

    ascii_table[DIK_RBRACKET].normal = KEY_DOLLAR;
    ascii_table[DIK_RBRACKET].shift = KEY_163;
    ascii_table[DIK_RBRACKET].left_alt = -1;
    ascii_table[DIK_RBRACKET].right_alt = KEY_164;
    ascii_table[DIK_RBRACKET].ctrl = -1;

    ascii_table[DIK_APOSTROPHE].normal = KEY_249;
    ascii_table[DIK_APOSTROPHE].shift = KEY_PERCENT;
    ascii_table[DIK_APOSTROPHE].left_alt = -1;
    ascii_table[DIK_APOSTROPHE].right_alt = -1;
    ascii_table[DIK_APOSTROPHE].ctrl = -1;

    ascii_table[DIK_BACKSLASH].normal = KEY_ASTERISK;
    ascii_table[DIK_BACKSLASH].shift = KEY_181;
    ascii_table[DIK_BACKSLASH].left_alt = -1;
    ascii_table[DIK_BACKSLASH].right_alt = -1;
    ascii_table[DIK_BACKSLASH].ctrl = -1;

    ascii_table[DIK_OEM_102].normal = KEY_LESS;
    ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    ascii_table[DIK_OEM_102].left_alt = -1;
    ascii_table[DIK_OEM_102].right_alt = -1;
    ascii_table[DIK_OEM_102].ctrl = -1;

    switch (kb_layout) {
    case english:
    case french:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_QUESTION;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_SEMICOLON;
    ascii_table[k].shift = KEY_DOT;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        // FIXME: Probably error, maps semicolon to colon on QWERTY keyboards.
        // Semicolon is already mapped above, so I bet it should be DIK_COLON.
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_COLON;
    ascii_table[k].shift = KEY_SLASH;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_SLASH].normal = KEY_EXCLAMATION;
    ascii_table[DIK_SLASH].shift = KEY_167;
    ascii_table[DIK_SLASH].left_alt = -1;
    ascii_table[DIK_SLASH].right_alt = -1;
    ascii_table[DIK_SLASH].ctrl = -1;
}

// 0x4BB42C
static void kb_map_ascii_German()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[DIK_GRAVE].normal = KEY_136;
    ascii_table[DIK_GRAVE].shift = KEY_186;
    ascii_table[DIK_GRAVE].left_alt = -1;
    ascii_table[DIK_GRAVE].right_alt = -1;
    ascii_table[DIK_GRAVE].ctrl = -1;

    ascii_table[DIK_2].normal = KEY_2;
    ascii_table[DIK_2].shift = KEY_QUOTE;
    ascii_table[DIK_2].left_alt = -1;
    ascii_table[DIK_2].right_alt = KEY_178;
    ascii_table[DIK_2].ctrl = -1;

    ascii_table[DIK_3].normal = KEY_3;
    ascii_table[DIK_3].shift = KEY_167;
    ascii_table[DIK_3].left_alt = -1;
    ascii_table[DIK_3].right_alt = KEY_179;
    ascii_table[DIK_3].ctrl = -1;

    ascii_table[DIK_6].normal = KEY_6;
    ascii_table[DIK_6].shift = KEY_AMPERSAND;
    ascii_table[DIK_6].left_alt = -1;
    ascii_table[DIK_6].right_alt = -1;
    ascii_table[DIK_6].ctrl = -1;

    ascii_table[DIK_7].normal = KEY_7;
    ascii_table[DIK_7].shift = KEY_166;
    ascii_table[DIK_7].left_alt = -1;
    ascii_table[DIK_7].right_alt = KEY_BRACE_LEFT;
    ascii_table[DIK_7].ctrl = -1;

    ascii_table[DIK_8].normal = KEY_8;
    ascii_table[DIK_8].shift = KEY_PAREN_LEFT;
    ascii_table[DIK_8].left_alt = -1;
    ascii_table[DIK_8].right_alt = KEY_BRACKET_LEFT;
    ascii_table[DIK_8].ctrl = -1;

    ascii_table[DIK_9].normal = KEY_9;
    ascii_table[DIK_9].shift = KEY_PAREN_RIGHT;
    ascii_table[DIK_9].left_alt = -1;
    ascii_table[DIK_9].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[DIK_9].ctrl = -1;

    ascii_table[DIK_0].normal = KEY_0;
    ascii_table[DIK_0].shift = KEY_EQUAL;
    ascii_table[DIK_0].left_alt = -1;
    ascii_table[DIK_0].right_alt = KEY_BRACE_RIGHT;
    ascii_table[DIK_0].ctrl = -1;

    ascii_table[DIK_MINUS].normal = KEY_223;
    ascii_table[DIK_MINUS].shift = KEY_QUESTION;
    ascii_table[DIK_MINUS].left_alt = -1;
    ascii_table[DIK_MINUS].right_alt = KEY_BACKSLASH;
    ascii_table[DIK_MINUS].ctrl = -1;

    ascii_table[DIK_EQUALS].normal = KEY_180;
    ascii_table[DIK_EQUALS].shift = KEY_GRAVE;
    ascii_table[DIK_EQUALS].left_alt = -1;
    ascii_table[DIK_EQUALS].right_alt = -1;
    ascii_table[DIK_EQUALS].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_A;
        break;
    default:
        k = DIK_Q;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_Q;
    ascii_table[k].shift = KEY_UPPERCASE_Q;
    ascii_table[k].left_alt = KEY_ALT_Q;
    ascii_table[k].right_alt = KEY_AT;
    ascii_table[k].ctrl = KEY_CTRL_Q;

    ascii_table[DIK_LBRACKET].normal = KEY_252;
    ascii_table[DIK_LBRACKET].shift = KEY_220;
    ascii_table[DIK_LBRACKET].left_alt = -1;
    ascii_table[DIK_LBRACKET].right_alt = -1;
    ascii_table[DIK_LBRACKET].ctrl = -1;

    switch (kb_layout) {
    case english:
    case french:
        k = DIK_EQUALS;
        break;
    default:
        k = DIK_RBRACKET;
        break;
    }

    ascii_table[k].normal = KEY_PLUS;
    ascii_table[k].shift = KEY_ASTERISK;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = KEY_152;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_SEMICOLON].normal = KEY_246;
    ascii_table[DIK_SEMICOLON].shift = KEY_214;
    ascii_table[DIK_SEMICOLON].left_alt = -1;
    ascii_table[DIK_SEMICOLON].right_alt = -1;
    ascii_table[DIK_SEMICOLON].ctrl = -1;

    ascii_table[DIK_APOSTROPHE].normal = KEY_228;
    ascii_table[DIK_APOSTROPHE].shift = KEY_196;
    ascii_table[DIK_APOSTROPHE].left_alt = -1;
    ascii_table[DIK_APOSTROPHE].right_alt = -1;
    ascii_table[DIK_APOSTROPHE].ctrl = -1;

    ascii_table[DIK_BACKSLASH].normal = KEY_NUMBER_SIGN;
    ascii_table[DIK_BACKSLASH].shift = KEY_SINGLE_QUOTE;
    ascii_table[DIK_BACKSLASH].left_alt = -1;
    ascii_table[DIK_BACKSLASH].right_alt = -1;
    ascii_table[DIK_BACKSLASH].ctrl = -1;

    ascii_table[DIK_OEM_102].normal = KEY_LESS;
    ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    ascii_table[DIK_OEM_102].left_alt = -1;
    ascii_table[DIK_OEM_102].right_alt = KEY_166;
    ascii_table[DIK_OEM_102].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_SEMICOLON;
        break;
    default:
        k = DIK_M;
        break;
    }

    ascii_table[k].normal = KEY_LOWERCASE_M;
    ascii_table[k].shift = KEY_UPPERCASE_M;
    ascii_table[k].left_alt = KEY_ALT_M;
    ascii_table[k].right_alt = KEY_181;
    ascii_table[k].ctrl = KEY_CTRL_M;

    switch (kb_layout) {
    case french:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_MINUS;
        break;
    case french:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_150;
    ascii_table[k].shift = KEY_151;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    ascii_table[DIK_DIVIDE].normal = KEY_247;
    ascii_table[DIK_DIVIDE].shift = KEY_247;
    ascii_table[DIK_DIVIDE].left_alt = -1;
    ascii_table[DIK_DIVIDE].right_alt = -1;
    ascii_table[DIK_DIVIDE].ctrl = -1;

    ascii_table[DIK_MULTIPLY].normal = KEY_215;
    ascii_table[DIK_MULTIPLY].shift = KEY_215;
    ascii_table[DIK_MULTIPLY].left_alt = -1;
    ascii_table[DIK_MULTIPLY].right_alt = -1;
    ascii_table[DIK_MULTIPLY].ctrl = -1;

    ascii_table[DIK_DECIMAL].normal = KEY_DELETE;
    ascii_table[DIK_DECIMAL].shift = KEY_COMMA;
    ascii_table[DIK_DECIMAL].left_alt = -1;
    ascii_table[DIK_DECIMAL].right_alt = KEY_ALT_DELETE;
    ascii_table[DIK_DECIMAL].ctrl = KEY_CTRL_DELETE;
}

// 0x4BBF30
static void kb_map_ascii_Italian()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[DIK_GRAVE].normal = KEY_BACKSLASH;
    ascii_table[DIK_GRAVE].shift = KEY_BAR;
    ascii_table[DIK_GRAVE].left_alt = -1;
    ascii_table[DIK_GRAVE].right_alt = -1;
    ascii_table[DIK_GRAVE].ctrl = -1;

    ascii_table[DIK_OEM_102].normal = KEY_LESS;
    ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    ascii_table[DIK_OEM_102].left_alt = -1;
    ascii_table[DIK_OEM_102].right_alt = -1;
    ascii_table[DIK_OEM_102].ctrl = -1;

    ascii_table[DIK_1].normal = KEY_1;
    ascii_table[DIK_1].shift = KEY_EXCLAMATION;
    ascii_table[DIK_1].left_alt = -1;
    ascii_table[DIK_1].right_alt = -1;
    ascii_table[DIK_1].ctrl = -1;

    ascii_table[DIK_2].normal = KEY_2;
    ascii_table[DIK_2].shift = KEY_QUOTE;
    ascii_table[DIK_2].left_alt = -1;
    ascii_table[DIK_2].right_alt = -1;
    ascii_table[DIK_2].ctrl = -1;

    ascii_table[DIK_3].normal = KEY_3;
    ascii_table[DIK_3].shift = KEY_163;
    ascii_table[DIK_3].left_alt = -1;
    ascii_table[DIK_3].right_alt = -1;
    ascii_table[DIK_3].ctrl = -1;

    ascii_table[DIK_6].normal = KEY_6;
    ascii_table[DIK_6].shift = KEY_AMPERSAND;
    ascii_table[DIK_6].left_alt = -1;
    ascii_table[DIK_6].right_alt = -1;
    ascii_table[DIK_6].ctrl = -1;

    ascii_table[DIK_7].normal = KEY_7;
    ascii_table[DIK_7].shift = KEY_SLASH;
    ascii_table[DIK_7].left_alt = -1;
    ascii_table[DIK_7].right_alt = -1;
    ascii_table[DIK_7].ctrl = -1;

    ascii_table[DIK_8].normal = KEY_8;
    ascii_table[DIK_8].shift = KEY_PAREN_LEFT;
    ascii_table[DIK_8].left_alt = -1;
    ascii_table[DIK_8].right_alt = -1;
    ascii_table[DIK_8].ctrl = -1;

    ascii_table[DIK_9].normal = KEY_9;
    ascii_table[DIK_9].shift = KEY_PAREN_RIGHT;
    ascii_table[DIK_9].left_alt = -1;
    ascii_table[DIK_9].right_alt = -1;
    ascii_table[DIK_9].ctrl = -1;

    ascii_table[DIK_0].normal = KEY_0;
    ascii_table[DIK_0].shift = KEY_EQUAL;
    ascii_table[DIK_0].left_alt = -1;
    ascii_table[DIK_0].right_alt = -1;
    ascii_table[DIK_0].ctrl = -1;

    ascii_table[DIK_MINUS].normal = KEY_SINGLE_QUOTE;
    ascii_table[DIK_MINUS].shift = KEY_QUESTION;
    ascii_table[DIK_MINUS].left_alt = -1;
    ascii_table[DIK_MINUS].right_alt = -1;
    ascii_table[DIK_MINUS].ctrl = -1;

    ascii_table[DIK_LBRACKET].normal = KEY_232;
    ascii_table[DIK_LBRACKET].shift = KEY_233;
    ascii_table[DIK_LBRACKET].left_alt = -1;
    ascii_table[DIK_LBRACKET].right_alt = KEY_BRACKET_LEFT;
    ascii_table[DIK_LBRACKET].ctrl = -1;

    ascii_table[DIK_RBRACKET].normal = KEY_PLUS;
    ascii_table[DIK_RBRACKET].shift = KEY_ASTERISK;
    ascii_table[DIK_RBRACKET].left_alt = -1;
    ascii_table[DIK_RBRACKET].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[DIK_RBRACKET].ctrl = -1;

    ascii_table[DIK_BACKSLASH].normal = KEY_249;
    ascii_table[DIK_BACKSLASH].shift = KEY_167;
    ascii_table[DIK_BACKSLASH].left_alt = -1;
    ascii_table[DIK_BACKSLASH].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[DIK_BACKSLASH].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_MINUS;
        break;
    case french:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;
}

// 0x4BC5FC
static void kb_map_ascii_Spanish()
{
    int k;

    kb_map_ascii_English_US();

    ascii_table[DIK_1].normal = KEY_1;
    ascii_table[DIK_1].shift = KEY_EXCLAMATION;
    ascii_table[DIK_1].left_alt = -1;
    ascii_table[DIK_1].right_alt = KEY_BAR;
    ascii_table[DIK_1].ctrl = -1;

    ascii_table[DIK_2].normal = KEY_2;
    ascii_table[DIK_2].shift = KEY_QUOTE;
    ascii_table[DIK_2].left_alt = -1;
    ascii_table[DIK_2].right_alt = KEY_AT;
    ascii_table[DIK_2].ctrl = -1;

    ascii_table[DIK_3].normal = KEY_3;
    ascii_table[DIK_3].shift = KEY_149;
    ascii_table[DIK_3].left_alt = -1;
    ascii_table[DIK_3].right_alt = KEY_NUMBER_SIGN;
    ascii_table[DIK_3].ctrl = -1;

    ascii_table[DIK_6].normal = KEY_6;
    ascii_table[DIK_6].shift = KEY_AMPERSAND;
    ascii_table[DIK_6].left_alt = -1;
    ascii_table[DIK_6].right_alt = KEY_172;
    ascii_table[DIK_6].ctrl = -1;

    ascii_table[DIK_7].normal = KEY_7;
    ascii_table[DIK_7].shift = KEY_SLASH;
    ascii_table[DIK_7].left_alt = -1;
    ascii_table[DIK_7].right_alt = -1;
    ascii_table[DIK_7].ctrl = -1;

    ascii_table[DIK_8].normal = KEY_8;
    ascii_table[DIK_8].shift = KEY_PAREN_LEFT;
    ascii_table[DIK_8].left_alt = -1;
    ascii_table[DIK_8].right_alt = -1;
    ascii_table[DIK_8].ctrl = -1;

    ascii_table[DIK_9].normal = KEY_9;
    ascii_table[DIK_9].shift = KEY_PAREN_RIGHT;
    ascii_table[DIK_9].left_alt = -1;
    ascii_table[DIK_9].right_alt = -1;
    ascii_table[DIK_9].ctrl = -1;

    ascii_table[DIK_0].normal = KEY_0;
    ascii_table[DIK_0].shift = KEY_EQUAL;
    ascii_table[DIK_0].left_alt = -1;
    ascii_table[DIK_0].right_alt = -1;
    ascii_table[DIK_0].ctrl = -1;

    ascii_table[DIK_MINUS].normal = KEY_146;
    ascii_table[DIK_MINUS].shift = KEY_QUESTION;
    ascii_table[DIK_MINUS].left_alt = -1;
    ascii_table[DIK_MINUS].right_alt = -1;
    ascii_table[DIK_MINUS].ctrl = -1;

    ascii_table[DIK_EQUALS].normal = KEY_161;
    ascii_table[DIK_EQUALS].shift = KEY_191;
    ascii_table[DIK_EQUALS].left_alt = -1;
    ascii_table[DIK_EQUALS].right_alt = -1;
    ascii_table[DIK_EQUALS].ctrl = -1;

    ascii_table[DIK_GRAVE].normal = KEY_176;
    ascii_table[DIK_GRAVE].shift = KEY_170;
    ascii_table[DIK_GRAVE].left_alt = -1;
    ascii_table[DIK_GRAVE].right_alt = KEY_BACKSLASH;
    ascii_table[DIK_GRAVE].ctrl = -1;

    ascii_table[DIK_LBRACKET].normal = KEY_GRAVE;
    ascii_table[DIK_LBRACKET].shift = KEY_CARET;
    ascii_table[DIK_LBRACKET].left_alt = -1;
    ascii_table[DIK_LBRACKET].right_alt = KEY_BRACKET_LEFT;
    ascii_table[DIK_LBRACKET].ctrl = -1;

    ascii_table[DIK_RBRACKET].normal = KEY_PLUS;
    ascii_table[DIK_RBRACKET].shift = KEY_ASTERISK;
    ascii_table[DIK_RBRACKET].left_alt = -1;
    ascii_table[DIK_RBRACKET].right_alt = KEY_BRACKET_RIGHT;
    ascii_table[DIK_RBRACKET].ctrl = -1;

    ascii_table[DIK_OEM_102].normal = KEY_LESS;
    ascii_table[DIK_OEM_102].shift = KEY_GREATER;
    ascii_table[DIK_OEM_102].left_alt = -1;
    ascii_table[DIK_OEM_102].right_alt = -1;
    ascii_table[DIK_OEM_102].ctrl = -1;

    ascii_table[DIK_SEMICOLON].normal = KEY_241;
    ascii_table[DIK_SEMICOLON].shift = KEY_209;
    ascii_table[DIK_SEMICOLON].left_alt = -1;
    ascii_table[DIK_SEMICOLON].right_alt = -1;
    ascii_table[DIK_SEMICOLON].ctrl = -1;

    ascii_table[DIK_APOSTROPHE].normal = KEY_168;
    ascii_table[DIK_APOSTROPHE].shift = KEY_180;
    ascii_table[DIK_APOSTROPHE].left_alt = -1;
    ascii_table[DIK_APOSTROPHE].right_alt = KEY_BRACE_LEFT;
    ascii_table[DIK_APOSTROPHE].ctrl = -1;

    ascii_table[DIK_BACKSLASH].normal = KEY_231;
    ascii_table[DIK_BACKSLASH].shift = KEY_199;
    ascii_table[DIK_BACKSLASH].left_alt = -1;
    ascii_table[DIK_BACKSLASH].right_alt = KEY_BRACE_RIGHT;
    ascii_table[DIK_BACKSLASH].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_M;
        break;
    default:
        k = DIK_COMMA;
        break;
    }

    ascii_table[k].normal = KEY_COMMA;
    ascii_table[k].shift = KEY_SEMICOLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case french:
        k = DIK_COMMA;
        break;
    default:
        k = DIK_PERIOD;
        break;
    }

    ascii_table[k].normal = KEY_DOT;
    ascii_table[k].shift = KEY_COLON;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;

    switch (kb_layout) {
    case english:
        k = DIK_MINUS;
        break;
    case french:
        k = DIK_6;
        break;
    default:
        k = DIK_SLASH;
        break;
    }

    ascii_table[k].normal = KEY_MINUS;
    ascii_table[k].shift = KEY_UNDERSCORE;
    ascii_table[k].left_alt = -1;
    ascii_table[k].right_alt = -1;
    ascii_table[k].ctrl = -1;
}

// 0x4BCCD0
static void kb_init_lock_status()
{
    if (GetKeyState(VK_CAPITAL) & 1) {
        kb_lock_flags |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }

    if (GetKeyState(VK_NUMLOCK) & 1) {
        kb_lock_flags |= MODIFIER_KEY_STATE_NUM_LOCK;
    }

    if (GetKeyState(VK_SCROLL) & 1) {
        kb_lock_flags |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
}

// 0x4BCD18
static void kb_toggle_caps()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_CAPS_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_CAPS_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_CAPS_LOCK;
    }
}

// 0x4BCD34
static void kb_toggle_num()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_NUM_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_NUM_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_NUM_LOCK;
    }
}

// 0x4BCD50
static void kb_toggle_scroll()
{
    if ((kb_lock_flags & MODIFIER_KEY_STATE_SCROLL_LOCK) != 0) {
        kb_lock_flags &= ~MODIFIER_KEY_STATE_SCROLL_LOCK;
    } else {
        kb_lock_flags |= MODIFIER_KEY_STATE_SCROLL_LOCK;
    }
}

// 0x4BCD6C
static int kb_buffer_put(key_data_t* key_data)
{
    int rc = -1;

    if (((kb_put + 1) & (KEY_QUEUE_SIZE - 1)) != kb_get) {
        kb_buffer[kb_put] = *key_data;

        kb_put++;
        kb_put &= KEY_QUEUE_SIZE - 1;

        rc = 0;
    }

    return rc;
}

// 0x4BCDAC
static int kb_buffer_get(key_data_t* key_data)
{
    int rc = -1;

    if (kb_get != kb_put) {
        if (key_data != NULL) {
            *key_data = kb_buffer[kb_get];
        }

        kb_get++;
        kb_get &= KEY_QUEUE_SIZE - 1;

        rc = 0;
    }

    return rc;
}

// Get pointer to pending key event from the queue but do not consume it.
//
// 0x4BCDEC
static int kb_buffer_peek(int index, key_data_t** keyboardEventPtr)
{
    int rc = -1;

    if (kb_get != kb_put) {
        int end;
        if (kb_put <= kb_get) {
            end = kb_put + KEY_QUEUE_SIZE - kb_get - 1;
        } else {
            end = kb_put - kb_get - 1;
        }

        if (index <= end) {
            int eventIndex = (kb_get + index) & (KEY_QUEUE_SIZE - 1);
            *keyboardEventPtr = &(kb_buffer[eventIndex]);
            rc = 0;
        }
    }

    return rc;
}
