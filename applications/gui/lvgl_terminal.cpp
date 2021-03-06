#include "Semaphore.h"
#include "lvgl_tools.h"
#include <lvgl.h>
#include <rtdevice.h>
#include <rtthread.h>
extern "C" {
    #include "msh.h"
    #include "shell.h"
}

#define LV_KB_CTRL_BTN_FLAGS (LV_BTNM_CTRL_NO_REPEAT | LV_BTNM_CTRL_CLICK_TRIG)

static const char * kb_map_lc[] = {"1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", LV_SYMBOL_BACKSPACE, "\n",
                                   "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", LV_SYMBOL_NEW_LINE, "\n",
                                   "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
                                   LV_SYMBOL_CLOSE, LV_SYMBOL_LEFT, LV_SYMBOL_UP, " ", LV_SYMBOL_DOWN, LV_SYMBOL_RIGHT, "Tab", ""};

static const lv_btnm_ctrl_t kb_ctrl_lc_map[] = {
    LV_KB_CTRL_BTN_FLAGS | 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 7,
    LV_KB_CTRL_BTN_FLAGS | 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 7,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KB_CTRL_BTN_FLAGS | 2, 2, 2, 6, 2, 2, LV_KB_CTRL_BTN_FLAGS | 2};

static const char * kb_map_uc[] = {"1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", LV_SYMBOL_BACKSPACE, "\n",
                                   "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_NEW_LINE, "\n",
                                   "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
                                   LV_SYMBOL_CLOSE, LV_SYMBOL_LEFT, LV_SYMBOL_UP, " ", LV_SYMBOL_DOWN, LV_SYMBOL_RIGHT, "Tab", ""};

static const lv_btnm_ctrl_t kb_ctrl_uc_map[] = {
    LV_KB_CTRL_BTN_FLAGS | 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 7,
    LV_KB_CTRL_BTN_FLAGS | 6, 3, 3, 3, 3, 3, 3, 3, 3, 3, 7,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KB_CTRL_BTN_FLAGS | 2, 2, 2, 6, 2, 2, LV_KB_CTRL_BTN_FLAGS | 2};

static const char * kb_map_spec[] = {"0", "1", "2", "3", "4" ,"5", "6", "7", "8", "9", LV_SYMBOL_BACKSPACE, "\n",
                                     "abc", "+", "-", "/", "*", "=", "%", "!", "?", "#", "<", ">", "\n",
                                     "\\",  "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'", "\n",
                                     LV_SYMBOL_CLOSE, LV_SYMBOL_LEFT, LV_SYMBOL_UP, " ", LV_SYMBOL_DOWN, LV_SYMBOL_RIGHT, "Tab", ""};

static const lv_btnm_ctrl_t kb_ctrl_spec_map[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, LV_KB_CTRL_BTN_FLAGS | 2,
    LV_KB_CTRL_BTN_FLAGS | 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    LV_KB_CTRL_BTN_FLAGS | 2, 2, 2, 6, 2, 2, LV_KB_CTRL_BTN_FLAGS | 2};

class LvglTerminal
{
  private:
    lv_coord_t width;
    lv_coord_t height;

    lv_style_t style_kb;
    lv_style_t style_kb_rel;
    lv_style_t style_kb_pr;

    lv_anim_t anim_ta_in;
    lv_anim_t anim_ta_out;
    lv_anim_t anim_kb_in;
    lv_anim_t anim_kb_out;

    lv_obj_t *interface;
    lv_obj_t *page;
    lv_obj_t *ta;
    lv_obj_t *kb;

    struct rt_device serial;
    struct rt_ringbuffer rx_buff;
    rtthread::Semaphore rx_count;
    uint8_t buffer[64];

    /* 键盘主题设定 */
    void set_kb_style(lv_style_t *normal, lv_style_t *pressed, lv_style_t *released)
    {
        if (normal)
        {
            lv_style_copy(normal, &lv_style_plain);
            normal->body.opa            = LV_OPA_70;
            normal->body.main_color     = lv_color_hex3(0x333);
            normal->body.grad_color     = lv_color_hex3(0x333);
            normal->body.padding.left   = 0;
            normal->body.padding.right  = 0;
            normal->body.padding.top    = 0;
            normal->body.padding.bottom = 0;
            normal->body.padding.inner  = 0;
        }
        if (pressed)
        {
            lv_style_copy(pressed, &lv_style_plain);
            pressed->body.radius       = 0;
            pressed->body.opa          = LV_OPA_50;
            pressed->body.main_color   = LV_COLOR_WHITE;
            pressed->body.grad_color   = LV_COLOR_WHITE;
            pressed->body.border.width = 1;
            pressed->body.border.color = LV_COLOR_SILVER;
        }
        if (released)
        {
            lv_style_copy(released, &lv_style_plain);
            released->body.opa          = LV_OPA_TRANSP;
            released->body.radius       = 0;
            released->body.border.width = 1;
            released->body.border.color = LV_COLOR_SILVER;
            released->body.border.opa   = LV_OPA_50;
            released->body.main_color   = lv_color_hex3(0x333); /*Recommended if LV_VDB_SIZE == 0 and bpp > 1 fonts are used*/
            released->body.grad_color   = lv_color_hex3(0x333);
            released->text.color        = LV_COLOR_WHITE;
        }
    }
    /* 开始输入动画 */
    void create_edit_begin_anim(lv_obj_t *ta, lv_obj_t *kb)
    {
        anim_ta_in.var            = ta;
        anim_ta_in.start          = height;
        anim_ta_in.end            = height - lv_obj_get_height(kb);
        anim_ta_in.exec_cb        = (lv_anim_exec_xcb_t)lv_obj_set_height;
        anim_ta_in.path_cb        = lv_anim_path_linear;
        anim_ta_in.ready_cb       = NULL;
        anim_ta_in.act_time       = 0;
        anim_ta_in.time           = 300;
        anim_ta_in.playback       = 0;
        anim_ta_in.playback_pause = 0;
        anim_ta_in.repeat         = 0;
        anim_ta_in.repeat_pause   = 0;
        lv_anim_create(&anim_ta_in);

        anim_kb_in.var            = kb;
        anim_kb_in.start          = height;
        anim_kb_in.end            = height - lv_obj_get_height(kb);
        anim_kb_in.exec_cb        = (lv_anim_exec_xcb_t)lv_obj_set_y;
        anim_kb_in.path_cb        = lv_anim_path_linear;
        anim_kb_in.ready_cb       = NULL;
        anim_kb_in.act_time       = 0;
        anim_kb_in.time           = 300;
        anim_kb_in.playback       = 0;
        anim_kb_in.playback_pause = 0;
        anim_kb_in.repeat         = 0;
        anim_kb_in.repeat_pause   = 0;
        lv_anim_create(&anim_kb_in);
    }
    /* 结束输入动画 */
    void create_edit_end_anim(lv_obj_t *ta, lv_obj_t *kb)
    {
        anim_ta_out.var            = ta;
        anim_ta_out.start          = height - lv_obj_get_height(kb);
        anim_ta_out.end            = height;
        anim_ta_out.exec_cb        = (lv_anim_exec_xcb_t)lv_obj_set_height;
        anim_ta_out.path_cb        = lv_anim_path_linear;
        anim_ta_out.ready_cb       = NULL;
        anim_ta_out.act_time       = 0;
        anim_ta_out.time           = 300;
        anim_ta_out.playback       = 0;
        anim_ta_out.playback_pause = 0;
        anim_ta_out.repeat         = 0;
        anim_ta_out.repeat_pause   = 0;
        lv_anim_create(&anim_ta_out);

        anim_kb_out.var            = kb;
        anim_kb_out.start          = height - lv_obj_get_height(kb);
        anim_kb_out.end            = height;
        anim_kb_out.exec_cb        = (lv_anim_exec_xcb_t)lv_obj_set_y;
        anim_kb_out.path_cb        = lv_anim_path_linear;
        anim_kb_out.ready_cb       = NULL;
        anim_kb_out.act_time       = 0;
        anim_kb_out.time           = 300;
        anim_kb_out.playback       = 0;
        anim_kb_out.playback_pause = 0;
        anim_kb_out.repeat         = 0;
        anim_kb_out.repeat_pause   = 0;
        lv_anim_create(&anim_kb_out);
    }
    /* page 回调函数 */
    static void page_callback(lv_obj_t *page, lv_event_t event)
    {
        LvglTerminal *self = (LvglTerminal *)page->user_data;
        if (event == LV_EVENT_DELETE)
        {
            self->on_page_delete(page);
        }
    }
    /* page 被删除回调函数 */
    void on_page_delete(lv_obj_t *page) { delete this; }
    /* ta 回调函数 */
    static void ta_callback(lv_obj_t *ta, lv_event_t event)
    {
        LvglTerminal *self = (LvglTerminal *)ta->user_data;
        if (event == LV_EVENT_CLICKED)
        {
            self->on_ta_clicked(ta);
        }
    }
    /* ta 被按下回调函数 */
    void on_ta_clicked(lv_obj_t *ta)
    {
        if (lv_obj_get_height(ta) < height * 3 / 4)
        {
            return;
        }
        create_edit_begin_anim(ta, kb);
    }
    /* kb 回调函数 */
    static void kb_callback(lv_obj_t *kb, lv_event_t event)
    {
        LvglTerminal *self = (LvglTerminal *)kb->user_data;
        if (event == LV_EVENT_VALUE_CHANGED)
        {
            self->on_kb_value_changed(kb);
        }
        else if (event == LV_EVENT_CANCEL)
        {
            self->create_edit_end_anim(self->ta, kb);
        }
    }
    /* kb 值变化回调函数 */
    void on_kb_value_changed(lv_obj_t *kb)
    {
        uint16_t btn_id = lv_btnm_get_active_btn(kb);
        if (btn_id == LV_BTNM_BTN_NONE)
            return;
        if (lv_btnm_get_btn_ctrl(kb, btn_id, LV_BTNM_CTRL_HIDDEN | LV_BTNM_CTRL_INACTIVE))
            return;

        const char *txt = lv_btnm_get_active_btn_text(kb);
        if (txt == NULL)
            return;

        /*Do the corresponding action according to the text of the button*/
        if (strcmp(txt, "abc") == 0)
        {
            lv_btnm_set_map(kb, kb_map_lc);
            lv_btnm_set_ctrl_map(kb, kb_ctrl_lc_map);
            return;
        }
        else if (strcmp(txt, "ABC") == 0)
        {
            lv_btnm_set_map(kb, kb_map_uc);
            lv_btnm_set_ctrl_map(kb, kb_ctrl_uc_map);
            return;
        }
        else if (strcmp(txt, "1#") == 0)
        {
            lv_btnm_set_map(kb, kb_map_spec);
            lv_btnm_set_ctrl_map(kb, kb_ctrl_spec_map);
            return;
        }
        else if (strcmp(txt, LV_SYMBOL_CLOSE) == 0)
        {
            lv_event_send(kb, LV_EVENT_CANCEL, NULL);
            return;
        }
        else if (strcmp(txt, "Tab") == 0)
        {
            send('\t');
        }
        else if (strcmp(txt, "Enter") == 0 || strcmp(txt, LV_SYMBOL_NEW_LINE) == 0)
        {
            send('\n');
        }
        else if (strcmp(txt, LV_SYMBOL_LEFT) == 0)
        {
            send(0x1b);
            send(0x5b);
            send(0x44);
            lv_ta_cursor_left(ta);
        }
        else if (strcmp(txt, LV_SYMBOL_RIGHT) == 0)
        {
            send(0x1b);
            send(0x5b);
            send(0x43);
            lv_ta_cursor_right(ta);
        }
        else if (strcmp(txt, LV_SYMBOL_UP) == 0)
        {
            send(0x1b);
            send(0x5b);
            send(0x41);
        }
        else if (strcmp(txt, LV_SYMBOL_DOWN) == 0)
        {
            send(0x1b);
            send(0x5b);
            send(0x42);
        }
        else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0)
        {
            send('\b');
        }
        else
        {
            send(txt[0]);
        }
    }
    /* 输入字符 */
    void put_char(char c)
    {
        if (c == 0x7F || c == 0x08)
        {
            lv_ta_del_char(ta);
            return;
        }
        write(&serial, 0, &c, 1);
    }
    static rt_size_t write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
    {
        LvglTerminal *self = (LvglTerminal *)dev->user_data;

        lv_ta_add_text(self->ta, (const char *)buffer);

        return size;
    }
    /* 读出字符 */
    char get_char(void)
    {
        char c;
        read(&serial, 0, &c, 1);
        return c;
    }
    static rt_size_t read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
    {
        uint8_t *buff      = (uint8_t *)buffer;
        LvglTerminal *self = (LvglTerminal *)dev->user_data;

        for (int i = 0; i < size; i++)
        {
            self->rx_count.wait();
            rt_ringbuffer_getchar(&self->rx_buff, buff + i);
        }

        return size;
    }
    /* 发送字符到缓冲 */
    int send(uint8_t c)
    {
        if (rt_ringbuffer_putchar(&rx_buff, c) == 0)
            return -1;
        rx_count.release();
        serial.rx_indicate(&serial, 1);
        return 0;
    }

  public:
    LvglTerminal(lv_obj_t *obj = NULL) : rx_count("s_v_tty0", 0)
    {
        interface = obj;

        width  = lv_obj_get_width(interface);
        height = lv_obj_get_height(interface);

        /* page 初始化 */
        page = lv_page_create(interface, NULL);
        lv_page_set_style(page, LV_LIST_STYLE_BG, &lv_style_transp_fit);
        lv_page_set_sb_mode(page, LV_SB_MODE_DRAG);
        lv_obj_set_size(page, width, height);
        lv_obj_align(page, interface, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_event_cb(page, this->page_callback);
        page->user_data = this;
        /* 创建 Text area */
        ta = lv_ta_create(page, NULL);
        lv_obj_set_size(ta, width, height / 2);
        lv_obj_align(ta, page, LV_ALIGN_IN_TOP_MID, 0, 0);
        lv_ta_set_text(ta, "");
        lv_ta_set_cursor_click_pos(ta, false);
        lv_ta_set_cursor_type(ta, LV_CURSOR_BLOCK);
        lv_obj_set_event_cb(ta, this->ta_callback);
        ta->user_data = this;
        /* 创建键盘 */
        kb = lv_kb_create(page, NULL);
        lv_obj_set_size(kb, width, height / 2);
        lv_obj_align(kb, ta, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
        lv_obj_set_event_cb(kb, this->kb_callback);
        lv_btnm_set_map(kb, kb_map_lc);
        lv_btnm_set_ctrl_map(kb, kb_ctrl_lc_map);
        kb->user_data = this;
        /* 设定键盘主题 */
        set_kb_style(&style_kb, &style_kb_pr, &style_kb_rel);
        lv_kb_set_style(kb, LV_KB_STYLE_BG, &style_kb);
        lv_kb_set_style(kb, LV_KB_STYLE_BTN_REL, &style_kb_rel);
        lv_kb_set_style(kb, LV_KB_STYLE_BTN_PR, &style_kb_pr);
        /* 字符缓冲区 */
        rt_ringbuffer_init(&rx_buff, buffer, 64);
        /* 注册设备 */
        memset(&serial, 0, sizeof(struct rt_device));
        serial.type      = RT_Device_Class_Char;
        serial.read      = read;
        serial.write     = write;
        serial.user_data = this;
        rt_device_register(&serial, "v_tty0", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
        rt_console_set_device("v_tty0");
        finsh_set_device("v_tty0");
        /* 开始动画 */
        create_edit_begin_anim(ta, kb);
    }
    ~LvglTerminal(void)
    {
        rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
        finsh_set_device(RT_CONSOLE_DEVICE_NAME);
        rt_device_unregister(&serial); 
    }

    lv_obj_t *get_page(void) { return page; }
};

void lvgl_terminal_press_cb(lv_obj_t *btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        lv_obj_t *interface    = lv_find_obj_parent_by_type(btn, (char *)"lv_cont");
        LvglTerminal *terminal = new LvglTerminal(interface);
        /* 添加活动对象 */
        lv_add_activity_obj(terminal->get_page());
    }
}
LVGL_APP_ITEM_EXPORT(Terminal, LV_SYMBOL_WARNING, lvgl_terminal_press_cb);
