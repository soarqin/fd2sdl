/* 炎龙骑士团 2 SDL3 重写 - 单owner普通键缓存
 *
 * input_check @code0 0x620（VA 0x10620）只检查 BIOS BDA 键盘缓冲。
 * 16c57读取一个BIOS键，4e381明确清队。宿主DOWN/UP和唯一repeat源
 * 向同一fd2_input有界缓存交付，不因下一present丢未读确认/取消。
 */
#include "input.h"

#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static volatile LONG host_quit_requested;
#else
static volatile sig_atomic_t host_quit_requested;
#endif

static fd2_input *active_input;

int fd2_input_host_quit_requested(void) {
#ifdef _WIN32
    return InterlockedCompareExchange(&host_quit_requested, 0, 0) != 0;
#else
    return host_quit_requested != 0;
#endif
}

static void request_host_quit(void) {
#ifdef _WIN32
    (void)InterlockedExchange(&host_quit_requested, 1);
#else
    host_quit_requested = 1;
#endif
}

#ifndef _WIN32
static void input_signal_handler(int signal_number) {
    (void)signal_number;
    host_quit_requested = 1;
}
#else
static BOOL WINAPI input_console_handler(DWORD control_type) {
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT ||
        control_type == CTRL_CLOSE_EVENT || control_type == CTRL_LOGOFF_EVENT ||
        control_type == CTRL_SHUTDOWN_EVENT) {
        (void)InterlockedExchange(&host_quit_requested, 1);
        return TRUE;
    }
    return FALSE;
}
#endif

void fd2_input_install_interrupt_handlers(void) {
#ifdef _WIN32
    (void)InterlockedExchange(&host_quit_requested, 0);
#else
    host_quit_requested = 0;
#endif
#ifndef _WIN32
    (void)signal(SIGINT, input_signal_handler);
    (void)signal(SIGTERM, input_signal_handler);
#else
    (void)SetConsoleCtrlHandler(input_console_handler, TRUE);
#endif
}

/* 固定IBM/non-DOSV键盘，独立按协议字段表达；来源是DOSBox-X b6abbd5
 * keyboard.cpp::AddKey1与bios_keyboard.cpp IRQ1/INT16 AH10，不借SDL符号键。
 * 准备页比较AH、标题比较AL，故先保AX，再派生既有游戏键。 */
static fd2_input_key key_from_bios(uint16_t code) {
    switch (code >> 8) {
    case 0x01:return FD2_INPUT_KEY_ESCAPE;
    case 0x1c:return FD2_INPUT_KEY_ENTER;
    case 0xe0:case 0x52:return FD2_INPUT_KEY_KEYPAD_CONFIRM;
    case 0x39:return FD2_INPUT_KEY_SPACE;
    case 0x53:return FD2_INPUT_KEY_CANCEL;
    case 0x48:return FD2_INPUT_KEY_UP;
    case 0x50:return FD2_INPUT_KEY_DOWN;
    case 0x4b:return FD2_INPUT_KEY_LEFT;
    case 0x4d:return FD2_INPUT_KEY_RIGHT;
    case 0x3b:return FD2_INPUT_KEY_F1;
    case 0x3c:return FD2_INPUT_KEY_F2;
    case 0x47:return FD2_INPUT_KEY_HOME;
    case 0x49:return FD2_INPUT_KEY_PAGE_UP;
    case 0x22:return FD2_INPUT_KEY_G;
    case 0x2c:return FD2_INPUT_KEY_Z;
    case 0x4c:return FD2_INPUT_KEY_KEYPAD_5;
    default:return FD2_INPUT_KEY_OTHER;
    }
}

static uint16_t modifier_bit(SDL_Scancode key) {
    switch(key) {
    case SDL_SCANCODE_LSHIFT:return SDL_KMOD_LSHIFT;
    case SDL_SCANCODE_RSHIFT:return SDL_KMOD_RSHIFT;
    case SDL_SCANCODE_LCTRL:return SDL_KMOD_LCTRL;
    case SDL_SCANCODE_RCTRL:return SDL_KMOD_RCTRL;
    case SDL_SCANCODE_LALT:return SDL_KMOD_LALT;
    case SDL_SCANCODE_RALT:return SDL_KMOD_RALT;
    default:return 0;
    }
}

/* KEYBOARD_AddKey1用物理modifier选择PrtSc字节；BIOS fake Shift不属于它。 */
static uint16_t physical_modifier_bits(const fd2_input *input) {
    static const SDL_Scancode keys[]={SDL_SCANCODE_LSHIFT,SDL_SCANCODE_RSHIFT,
        SDL_SCANCODE_LCTRL,SDL_SCANCODE_RCTRL,SDL_SCANCODE_LALT,SDL_SCANCODE_RALT};
    uint16_t bits=0;
    for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);i++)
        if(input->previous_down[keys[i]])bits|=modifier_bit(keys[i]);
    return bits;
}

static uint16_t lock_bit(SDL_Scancode key) {
    return key==SDL_SCANCODE_CAPSLOCK ? SDL_KMOD_CAPS :
        key==SDL_SCANCODE_NUMLOCKCLEAR ? SDL_KMOD_NUM :
        key==SDL_SCANCODE_SCROLLLOCK ? SDL_KMOD_SCROLL:0;
}

static int keypad_digit(SDL_Scancode key) {
    if(key>=SDL_SCANCODE_KP_1 && key<=SDL_SCANCODE_KP_9)
        return 1+(int)(key-SDL_SCANCODE_KP_1);
    return key==SDL_SCANCODE_KP_0 ? 0:-1;
}

static uint16_t bios_key(SDL_Scancode key,uint16_t mods) {
    int alt=(mods&SDL_KMOD_ALT)!=0,ctrl=(mods&SDL_KMOD_CTRL)!=0;
    int shift=(mods&SDL_KMOD_SHIFT)!=0;
    unsigned scan=0,ch=0;
    if(key>=SDL_SCANCODE_A && key<=SDL_SCANCODE_Z) {
        static const uint8_t scans[]={30,48,46,32,18,33,34,35,23,36,37,38,50,
            49,24,25,16,19,31,20,22,47,17,45,21,44};
        unsigned index=(unsigned)(key-SDL_SCANCODE_A);scan=scans[index];
        ch=alt?0:ctrl?index+1u:
            ((shift!=((mods&SDL_KMOD_CAPS)!=0))?'A':'a')+index;
    } else if(key>=SDL_SCANCODE_1 && key<=SDL_SCANCODE_0) {
        unsigned index=(unsigned)(key-SDL_SCANCODE_1);scan=index+2u;
        if(alt)return (uint16_t)((0x78u+index)<<8);
        if(ctrl)return index==1u?0x0300u:index==5u?0x071eu:0;
        ch=(unsigned char)(shift?"!@#$%^&*()":"1234567890")[index];
    } else if((key>=SDL_SCANCODE_F1 && key<=SDL_SCANCODE_F12) ||
              (key>=SDL_SCANCODE_F13 && key<=SDL_SCANCODE_F24)) {
        unsigned index;
        if(key>=SDL_SCANCODE_F13) {index=(unsigned)(key-SDL_SCANCODE_F13);shift=1;}
        else index=(unsigned)(key-SDL_SCANCODE_F1);
        scan=index<10u ? (alt?104u:ctrl?94u:shift?84u:59u)+index:
            (alt?139u:ctrl?137u:shift?135u:133u)+index-10u;
    } else {
        /* 导航与数字小键盘共scan，扩展导航保E0；Alt小键盘由token叶处理。 */
        int digit=keypad_digit(key),pad=digit>=0 || key==SDL_SCANCODE_KP_DECIMAL;
        switch(key) {
        case SDL_SCANCODE_HOME:case SDL_SCANCODE_KP_7:scan=0x47;break;
        case SDL_SCANCODE_UP:case SDL_SCANCODE_KP_8:scan=0x48;break;
        case SDL_SCANCODE_PAGEUP:case SDL_SCANCODE_KP_9:scan=0x49;break;
        case SDL_SCANCODE_LEFT:case SDL_SCANCODE_KP_4:scan=0x4b;break;
        case SDL_SCANCODE_KP_5:scan=0x4c;break;
        case SDL_SCANCODE_RIGHT:case SDL_SCANCODE_KP_6:scan=0x4d;break;
        case SDL_SCANCODE_END:case SDL_SCANCODE_KP_1:scan=0x4f;break;
        case SDL_SCANCODE_DOWN:case SDL_SCANCODE_KP_2:scan=0x50;break;
        case SDL_SCANCODE_PAGEDOWN:case SDL_SCANCODE_KP_3:scan=0x51;break;
        case SDL_SCANCODE_INSERT:case SDL_SCANCODE_KP_0:scan=0x52;break;
        case SDL_SCANCODE_DELETE:case SDL_SCANCODE_KP_DECIMAL:scan=0x53;break;
        default:break;
        }
        if(scan) {
            if(alt)return pad?0:(uint16_t)((scan+0x50u)<<8);
            if(ctrl) {
                switch(scan) {
                case 0x47:scan=0x77;break;case 0x48:scan=0x8d;break;
                case 0x49:scan=0x84;break;case 0x4b:scan=0x73;break;
                case 0x4c:scan=0x8f;break;case 0x4d:scan=0x74;break;
                case 0x4f:scan=0x75;break;case 0x50:scan=0x91;break;
                case 0x51:scan=0x76;break;case 0x52:scan=0x92;break;
                case 0x53:scan=0x93;break;
                }
                ch=pad?0:0xe0;
            } else if(!pad)ch=0xe0;
            else if(shift!=((mods&SDL_KMOD_NUM)!=0))ch=digit<0?'.':(unsigned)('0'+digit);
            return (uint16_t)((scan<<8)|ch);
        }
        /* 非字母US键的普通/Shift字符；Ctrl/Alt特殊值只在各自源表存在时生成。 */
        const char *normal=NULL,*shifted=NULL;
        switch(key) {
        case SDL_SCANCODE_ESCAPE:return alt?0x0100u:0x011bu;
        case SDL_SCANCODE_RETURN:return alt?0:ctrl?0x1c0au:0x1c0du;
        case SDL_SCANCODE_KP_ENTER:return alt?0xa600u:ctrl?0xe00au:0xe00du;
        case SDL_SCANCODE_SPACE:return 0x3920;
        case SDL_SCANCODE_TAB:return alt?0:ctrl?0x9400u:shift?0x0f00u:0x0f09u;
        case SDL_SCANCODE_BACKSPACE:return alt?0x0e00u:ctrl?0x0e7fu:0x0e08u;
        case SDL_SCANCODE_KP_DIVIDE:return alt?0xa400u:ctrl?0x9500u:0xe02fu;
        case SDL_SCANCODE_KP_MULTIPLY:return alt?0x3700u:ctrl?0x9600u:0x372au;
        case SDL_SCANCODE_KP_MINUS:return alt?0x4a00u:ctrl?0x8e00u:0x4a2du;
        case SDL_SCANCODE_KP_PLUS:return alt?0x4e00u:ctrl?0x9000u:0x4e2bu;
        /* AddKey1发E0/5b；固定BIOS的实际0x5b表槽（非注释索引）为AX键。 */
        case SDL_SCANCODE_LGUI:return alt?0xd500u:ctrl?0xd400u:shift?0xd300u:0xd200u;
        case SDL_SCANCODE_MINUS:scan=0x0c;normal="-";shifted="_";break;
        case SDL_SCANCODE_EQUALS:scan=0x0d;normal="=";shifted="+";break;
        case SDL_SCANCODE_LEFTBRACKET:scan=0x1a;normal="[";shifted="{";break;
        case SDL_SCANCODE_RIGHTBRACKET:scan=0x1b;normal="]";shifted="}";break;
        case SDL_SCANCODE_SEMICOLON:scan=0x27;normal=";";shifted=":";break;
        case SDL_SCANCODE_APOSTROPHE:scan=0x28;normal="'";shifted="\"";break;
        case SDL_SCANCODE_GRAVE:scan=0x29;normal="`";shifted="~";break;
        case SDL_SCANCODE_BACKSLASH:scan=0x2b;normal="\\";shifted="|";break;
        case SDL_SCANCODE_COMMA:scan=0x33;normal=",";shifted="<";break;
        case SDL_SCANCODE_PERIOD:scan=0x34;normal=".";shifted=">";break;
        case SDL_SCANCODE_SLASH:scan=0x35;normal="/";shifted="?";break;
        case SDL_SCANCODE_NONUSBACKSLASH:scan=0x56;normal="\\";shifted="|";break;
        default:return 0; /* BIOS无普通返回的modifier/锁键/GUI/媒体键。 */
        }
        if(alt) {
            if(scan==0x0c || scan==0x0d)return (uint16_t)((scan+0x76u)<<8);
            if(scan==0x56)return 0;
        } else if(ctrl) {
            if(scan==0x0c)ch=0x1f;
            else if(scan==0x1a || scan==0x1b || scan==0x2b)ch=scan==0x1a?0x1b:scan==0x1b?0x1d:0x1c;
            else return 0;
        } else ch=(unsigned char)(shift?shifted:normal)[0];
    }
    return (uint16_t)((scan<<8)|ch);
}

static int is_state_only_scancode(SDL_Scancode scancode) {
    /* 当前IBM BIOS普通Caps/Num/ScrollLock只改锁状态，不产生INT16待取键。
     * Pause/Ctrl-Break仍走既有宿主退出映射，不把锁键当详情/对白ACK。 */
    return scancode == SDL_SCANCODE_LCTRL ||
           scancode == SDL_SCANCODE_RCTRL ||
           scancode == SDL_SCANCODE_LSHIFT ||
           scancode == SDL_SCANCODE_RSHIFT ||
           scancode == SDL_SCANCODE_LALT ||
           scancode == SDL_SCANCODE_RALT ||
           scancode == SDL_SCANCODE_CAPSLOCK ||
           scancode == SDL_SCANCODE_NUMLOCKCLEAR ||
           scancode == SDL_SCANCODE_SCROLLLOCK;
}

void fd2_input_init(fd2_input *input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    if(SDL_WasInit(SDL_INIT_EVENTS))input->lock_modifiers=(uint16_t)(SDL_GetModState()&
        (SDL_KMOD_CAPS|SDL_KMOD_NUM|SDL_KMOD_SCROLL));
    active_input = input;
}

static void clear_key_lifecycle(fd2_input *input) {
    if (!input) return;
    memset(input->previous_down, 0, sizeof(input->previous_down));
    input->queued_head=input->queued_count=0;
    memset(input->next_repeat_ms, 0, sizeof(input->next_repeat_ms));
    input->repeat_key=SDL_SCANCODE_UNKNOWN;
    input->held_modifiers=0;
    input->alt_token=0;
}

static void queue_code(fd2_input *input,SDL_Scancode source,int repeat,
        uint16_t modifiers,uint16_t code) {
    if(!code)return; /* IBM add_key拒绝0；AH10将enhanced的低F0规范化为0。 */
    if((code&0xffu)==0xf0u && (code>>8))code&=0xff00u;
    if(input->queued_count==FD2_INPUT_KEY_QUEUE_CAPACITY)return;
    unsigned tail=(input->queued_head+input->queued_count)%FD2_INPUT_KEY_QUEUE_CAPACITY;
    input->queued_keys[tail]=(fd2_input_event){key_from_bios(code),source,
        (uint8_t)(repeat!=0),modifiers,code};
    input->queued_count++;
}

static void emit_make(fd2_input *input,SDL_Scancode source,int repeat,uint16_t modifiers) {
    /* repeat也重发真实make：modifier与F13的fake Shift均可重写BIOS标志。 */
    input->held_modifiers|=modifier_bit(source);
    if(source>=SDL_SCANCODE_F13 && source<=SDL_SCANCODE_F24)
        input->held_modifiers|=SDL_KMOD_LSHIFT;
    modifiers|=input->held_modifiers;
    if(source==SDL_SCANCODE_C && (modifiers&SDL_KMOD_CTRL)) {
        request_host_quit();input->quit_requested=1;return;
    }
    int digit=keypad_digit(source);
    if(source==SDL_SCANCODE_KP_DECIMAL)digit=0; /* IRQ1@53h的Alt列是0，仍参与token。 */
    if((modifiers&SDL_KMOD_ALT) && digit>=0) {
        input->alt_token=(uint8_t)(10u*input->alt_token+(unsigned)digit);return;
    }
    queue_code(input,source,repeat,modifiers,bios_key(source,modifiers));
}

/* 1887..1899硬件每1ms递减wait，和视觉叶的单步不追赶分账。长gap只需
 * 保存FIFO前15项；Alt数字token的仿射递推8次后已忘掉旧BYTE，故同样有界。 */
static void update_repeat(fd2_input *input,uint64_t now_ms) {
    if(now_ms<input->repeat_sample_ms)now_ms=input->repeat_sample_ms;
    input->repeat_sample_ms=now_ms;
    SDL_Scancode source=input->repeat_key;
    if(source==SDL_SCANCODE_UNKNOWN)return;
    uint64_t deadline=input->next_repeat_ms[source];
    if(now_ms<deadline)return;
    uint64_t due=(now_ms-deadline)/FD2_INPUT_REPEAT_MS+1u;
    uint64_t limit=due<FD2_INPUT_KEY_QUEUE_CAPACITY?due:FD2_INPUT_KEY_QUEUE_CAPACITY;
    uint16_t mods=(uint16_t)(input->held_modifiers|input->lock_modifiers);
    for(uint64_t i=0;i<limit;i++)emit_make(input,source,1,mods);
    uint64_t last=now_ms-(now_ms-deadline)%FD2_INPUT_REPEAT_MS;
    if(UINT64_MAX-last<FD2_INPUT_REPEAT_MS) {
        input->repeat_key=SDL_SCANCODE_UNKNOWN;input->next_repeat_ms[source]=0;
    } else input->next_repeat_ms[source]=last+FD2_INPUT_REPEAT_MS;
}

static void key_transition(fd2_input *input,SDL_Scancode source,int down,
        uint16_t mods,uint64_t now_ms) {
    if(source<=SDL_SCANCODE_UNKNOWN || source>=SDL_SCANCODE_COUNT ||
            input->previous_down[source]==(down!=0))return;
    input->previous_down[source]=down!=0;
    /* SDL媒体键没有原硬件make；不能让它夺走既有repeat owner。 */
    if(!bios_key(source,0) && !is_state_only_scancode(source) &&
        source!=SDL_SCANCODE_PAUSE && source!=SDL_SCANCODE_PRINTSCREEN &&
        source!=SDL_SCANCODE_RGUI && source!=SDL_SCANCODE_APPLICATION)return;
    if(now_ms<input->repeat_sample_ms)now_ms=input->repeat_sample_ms;
    uint16_t bit=modifier_bit(source);
    if(down)input->held_modifiers|=bit;
    else input->held_modifiers&=(uint16_t)~bit;
    /* F13..24的硬件make是LShift+F1..12，break反序；fake Shift也有BIOS写点。 */
    if(source>=SDL_SCANCODE_F13 && source<=SDL_SCANCODE_F24) {
        if(down)input->held_modifiers|=SDL_KMOD_LSHIFT;
        else input->held_modifiers&=UINT16_MAX^SDL_KMOD_LSHIFT;
    }
    if(source==SDL_SCANCODE_PRINTSCREEN) {
        /* b6abbd5 AddKey1：make和break都取消当前last-key（可以已是别的键）。
         * 无物理Alt/Ctrl/Shift才发E0/2a或E0/aa；builtin US437无2a重映射，
         * aa>MAX127，均落到IRQ1的LShift写点。不签foreign/external布局。 */
        uint16_t physical=(uint16_t)(mods|physical_modifier_bits(input));
        if(!(physical&(SDL_KMOD_ALT|SDL_KMOD_CTRL|SDL_KMOD_SHIFT))) {
            if(down)input->held_modifiers|=SDL_KMOD_LSHIFT;
            else input->held_modifiers&=UINT16_MAX^SDL_KMOD_LSHIFT;
        }
        if(input->repeat_key!=SDL_SCANCODE_UNKNOWN)input->next_repeat_ms[input->repeat_key]=0;
        input->repeat_key=SDL_SCANCODE_UNKNOWN;
        return;
    }
    if(!down) {
        input->lock_modifiers^=lock_bit(source); /* 固定SDL2 CAN_USE_LOCK：break切换。 */
        if((bit&SDL_KMOD_ALT) && !(input->held_modifiers&SDL_KMOD_ALT) && input->alt_token) {
            queue_code(input,source,0,(uint16_t)(input->held_modifiers|input->lock_modifiers),
                input->alt_token);input->alt_token=0;
        }
        if(input->repeat_key==source) {
            input->repeat_key=SDL_SCANCODE_UNKNOWN;input->next_repeat_ms[source]=0;
        }
        return;
    }
    if(input->repeat_key!=SDL_SCANCODE_UNKNOWN)input->next_repeat_ms[input->repeat_key]=0;
    if(source==SDL_SCANCODE_PAUSE) {
        input->repeat_key=SDL_SCANCODE_UNKNOWN;
        request_host_quit();input->quit_requested=1;
        return;
    }
    /* 不可表示的500ms期限不是MAX处已到期；仍接受首make，停后续repeat。
     * 恰能在MAX到期则保留该次正常make，update_repeat随后停止owner。 */
    int repeat_fits=UINT64_MAX-now_ms>=FD2_INPUT_INITIAL_REPEAT_MS;
    input->repeat_key=repeat_fits?source:SDL_SCANCODE_UNKNOWN;
    input->next_repeat_ms[source]=repeat_fits?now_ms+FD2_INPUT_INITIAL_REPEAT_MS:0;
    if(source==SDL_SCANCODE_C && (mods&SDL_KMOD_CTRL)) {
        input->repeat_key=SDL_SCANCODE_UNKNOWN;input->next_repeat_ms[source]=0;
        request_host_quit();input->quit_requested=1;return;
    }
    /* F13的fake break可以清掉仍物理held的LShift；不得由后续快照复活。
     * 没有对应raw modifier生命周期的Windows DOWN元数据仍按既有合同保留。 */
    if(input->previous_down[SDL_SCANCODE_LSHIFT] && !(input->held_modifiers&SDL_KMOD_LSHIFT))
        mods&=UINT16_MAX^SDL_KMOD_LSHIFT;
    emit_make(input,source,0,(uint16_t)(mods|input->held_modifiers|input->lock_modifiers));
}

static void sample_key_state(fd2_input *input,const bool *state,uint64_t now_ms) {
    update_repeat(input,now_ms);
    /* 无事件先后的物理快照只作fallback；modifier先建立，再按scan稳定处理。
     * 真DOWN/UP始终在poll中按到达序处理，不能用这次快照回写已接受修饰位。 */
    for(int pass=0;pass<2;pass++)for(int i=1;i<SDL_SCANCODE_COUNT;i++) {
        SDL_Scancode source=(SDL_Scancode)i;
        if((is_state_only_scancode(source)!=0)!=(pass==0))continue;
        key_transition(input,source,state[i],
            (uint16_t)(physical_modifier_bits(input)|input->lock_modifiers),now_ms);
    }
}

void fd2_input_poll_host_events(fd2_input *input) {
    if (!input) input = active_input;
    int focus_lost=0;
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        uint64_t now=input && input->use_test_key_state?input->test_now_ms:SDL_GetTicks();
        if(input && !input->use_test_key_state && event.common.timestamp) {
            uint64_t arrived=event.common.timestamp/1000000u;
            if(arrived<now)now=arrived;
            if(now<input->repeat_sample_ms)now=input->repeat_sample_ms;
        }
        if(input)update_repeat(input,now);
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_TERMINATING) {
            request_host_quit();
            if (input) input->quit_requested = 1;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            clear_key_lifecycle(input);
            focus_lost=1;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if ((event.key.mod & SDL_KMOD_CTRL) != 0 &&
                (event.key.scancode == SDL_SCANCODE_C ||
                 event.key.scancode == SDL_SCANCODE_PAUSE)) {
                request_host_quit();
                if (input) input->quit_requested = 1;
                if(input && !event.key.repeat)key_transition(input,event.key.scancode,1,
                    (uint16_t)event.key.mod,now);
            } else if (event.key.scancode == SDL_SCANCODE_PAUSE) {
                request_host_quit();
                if (input) input->quit_requested = 1;
            } else if(input && !event.key.repeat) {
                /* 先按SDL事件到达序接受首按；Windows全局Shift修正不改变
                 * 已接受event的修饰位。UP也不能删除这条BIOS式待取记录。 */
                key_transition(input,event.key.scancode,1,(uint16_t)(event.key.mod &
                    (SDL_KMOD_SHIFT|SDL_KMOD_CTRL|SDL_KMOD_ALT)),now);
            }
        } else if(event.type==SDL_EVENT_KEY_UP && input &&
                event.key.scancode>SDL_SCANCODE_UNKNOWN && event.key.scancode<SDL_SCANCODE_COUNT) {
            key_transition(input,event.key.scancode,0,(uint16_t)event.key.mod,now);
        }
    }
    /* 当前Windows单窗SDL先ResetKeyboard再发LOST；其它/显式注入路径的
     * 本轮快照仍可能DOWN。只禁止本轮fallback复活，后续真实DOWN仍按上方入队。 */
    if(input && !focus_lost)sample_key_state(input,input->use_test_key_state?input->test_key_state:
        SDL_GetKeyboardState(NULL),input->use_test_key_state?input->test_now_ms:SDL_GetTicks());
}

void fd2_input_begin_frame(fd2_input *input) {
    if (!input) return;
    fd2_input_poll_host_events(input);
}

void fd2_input_begin_test_frame(fd2_input *input, uint64_t now_ms) {
    if (!input) return;
    if(!input->use_test_key_state)input->lock_modifiers=0;
    input->use_test_key_state = 1;
    input->test_now_ms=now_ms;
    sample_key_state(input, input->test_key_state, now_ms);
}

void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed) {
    if (!input || source <= SDL_SCANCODE_UNKNOWN ||
        source >= SDL_SCANCODE_COUNT)
        return;
    if(!input->use_test_key_state)input->lock_modifiers=0;
    input->use_test_key_state = 1;
    input->test_key_state[source] = pressed != 0;
}

int fd2_input_has_any_key(const fd2_input *input) {
    return input && input->queued_count!=0;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    if(!input || !input->queued_count)return 0;
    fd2_input_event value=input->queued_keys[input->queued_head];
    input->queued_head=(uint8_t)((input->queued_head+1u)%FD2_INPUT_KEY_QUEUE_CAPACITY);
    input->queued_count--;
    if(event)*event=value;
    return 1;
}

void fd2_input_clear_keys(fd2_input *input) {
    if(!input)return;
    fd2_input_poll_host_events(input);
    input->queued_head=input->queued_count=0;
}

uint8_t fd2_input_function_scan(const fd2_input_event *event) {
    if(!event || !((event->source>=SDL_SCANCODE_F1 && event->source<=SDL_SCANCODE_F12) ||
        (event->source>=SDL_SCANCODE_F13 && event->source<=SDL_SCANCODE_F24)))return 0;
    return (uint8_t)((event->bios_code?event->bios_code:bios_key(event->source,event->modifiers))>>8);
}

fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key) {
    switch (context) {
        case FD2_INPUT_CONTEXT_FIELD_DETAIL:
            /* 17aed的16c57(0)接受任意普通键，不走browse Esc轮询。 */
            return FD2_INPUT_ACTION_CONFIRM;
        case FD2_INPUT_CONTEXT_TITLE:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_CHOICE:
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL ||
                key == FD2_INPUT_KEY_Z || key == FD2_INPUT_KEY_KEYPAD_5)
                return FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE;
            if (key == FD2_INPUT_KEY_F1 || key == FD2_INPUT_KEY_PAGE_UP)
                return FD2_INPUT_ACTION_FIELD_AUXILIARY;
            if (key == FD2_INPUT_KEY_F2 || key == FD2_INPUT_KEY_HOME)
                return FD2_INPUT_ACTION_FIELD_DETAIL;
            if (key == FD2_INPUT_KEY_G)
                return FD2_INPUT_ACTION_FIELD_SPECIAL_G;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_COMMAND:
        case FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU:
        case FD2_INPUT_CONTEXT_FIELD_TARGETING:
        case FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING:
        case FD2_INPUT_CONTEXT_FIELD_MOVEMENT:
            if ((context == FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING ||
                 context == FD2_INPUT_CONTEXT_FIELD_TARGETING) &&
                (key == FD2_INPUT_KEY_Z || key == FD2_INPUT_KEY_KEYPAD_5))
                return FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE;
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_AUXILIARY:
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_EXIT;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_PREVIEW:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_EXIT;
            return FD2_INPUT_ACTION_NONE;
    }
    return FD2_INPUT_ACTION_NONE;
}

int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action) {
    fd2_input_event event;
    if (!fd2_input_take_key(input, &event)) return 0;
    if (action) {
        if(context==FD2_INPUT_CONTEXT_TITLE && event.bios_code) {
            /* 1f894@1feb2..1fece：AL=CR/Space或AH=E0/52才确认；
             * Ctrl+Enter为1c0a，不得只看物理Enter或游戏规范scan。 */
            unsigned ascii=event.bios_code&0xffu,scan=event.bios_code>>8;
            *action=ascii==0x0d || ascii==0x20 || scan==0xe0 || scan==0x52 ?
                FD2_INPUT_ACTION_CONFIRM:scan==0x48 ? FD2_INPUT_ACTION_UP:
                scan==0x50 ? FD2_INPUT_ACTION_DOWN:FD2_INPUT_ACTION_NONE;
        } else *action = fd2_input_action_for_key(context, event.key);
    }
    return 1;
}

int fd2_input_take_quit(fd2_input *input) {
    if (!input) return fd2_input_host_quit_requested();
    if (fd2_input_host_quit_requested()) input->quit_requested = 1;
    return input->quit_requested != 0;
}
