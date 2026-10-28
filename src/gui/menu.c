#include "menu.h"
#include "gfx.h"
#include "wm.h"
#include "string.h"
#include "keyboard.h"

#define ITEM_H     20
#define SEP_H      7
#define TITLE_H    22
#define PAD_X      8
#define PAD_Y      4

static int m_open = 0;
static int m_x, m_y, m_w, m_h;
static char m_title[40];
static char m_labels[MENU_MAX_ITEMS][40];
static menu_action_t m_actions[MENU_MAX_ITEMS];
static int m_count = 0;
static int m_sel = -1;

static int is_sep(int i)
{
    return m_labels[i][0] == '-' && m_labels[i][1] == 0;
}

static int text_w(const char *s)
{
    return (int)strlen(s) * 8;
}

static int max_int(int a, int b) { return a > b ? a : b; }

int menu_height(const char *title, const char *const *labels, int count)
{
    int h = PAD_Y * 2;
    if (title && *title) h += TITLE_H;
    if (count > MENU_MAX_ITEMS) count = MENU_MAX_ITEMS;
    for (int i = 0; i < count; i++) {
        if (labels[i] && labels[i][0] == '-' && labels[i][1] == 0) h += SEP_H;
        else h += ITEM_H;
    }
    return h;
}

void menu_open(int x, int y, const char *title,
               const char *const *labels, const menu_action_t *actions, int count)
{
    if (!labels || !actions || count <= 0) return;
    if (count > MENU_MAX_ITEMS) count = MENU_MAX_ITEMS;

    m_count = count;
    m_title[0] = 0;

    int w = 150;
    if (title && *title) {
        strncpy(m_title, title, sizeof(m_title) - 1);
        m_title[sizeof(m_title) - 1] = 0;
        w = max_int(w, text_w(m_title) + PAD_X * 2 + 16);
    }

    for (int i = 0; i < count; i++) {
        strncpy(m_labels[i], labels[i] ? labels[i] : "", sizeof(m_labels[i]) - 1);
        m_labels[i][sizeof(m_labels[i]) - 1] = 0;
        m_actions[i] = actions[i];
        if (!is_sep(i))
            w = max_int(w, text_w(m_labels[i]) + PAD_X * 2 + 18);
    }

    m_w = w;
    m_h = menu_height(m_title, labels, count);

    int max_x = gfx_width() - m_w - 2;
    int max_y = gfx_height() - WM_TASKBAR_H - m_h - 2;
    if (max_x < 2) max_x = 2;
    if (max_y < 2) max_y = 2;
    if (x > max_x) x = max_x;
    if (y > max_y) y = max_y;
    if (x < 2) x = 2;
    if (y < 2) y = 2;

    m_x = x;
    m_y = y;
    m_sel = -1;
    m_open = 1;
}

void menu_close(void)
{
    m_open = 0;
}

int menu_visible(void) { return m_open; }

int menu_hover(int x, int y)
{
    if (!m_open) return -1;
    if (x < m_x || x >= m_x + m_w || y < m_y || y >= m_y + m_h) return -1;

    int py = m_y + PAD_Y;
    if (m_title[0]) py += TITLE_H;
    for (int i = 0; i < m_count; i++) {
        int ih = is_sep(i) ? SEP_H : ITEM_H;
        if (y >= py && y < py + ih) return is_sep(i) ? -1 : i;
        py += ih;
    }
    return -1;
}

int menu_key(int key)
{
    if (!m_open) return 0;

    if (key == KEY_UP || key == KEY_DOWN) {
        int dir = (key == KEY_DOWN) ? 1 : -1;
        int i = m_sel;
        for (int n = 0; n < m_count; n++) {
            i += dir;
            if (i < 0) i = m_count - 1;
            if (i >= m_count) i = 0;
            if (!is_sep(i)) { m_sel = i; return 1; }
        }
        return 1;
    }

    if (key == '\n' || key == '\r') {
        menu_action_t act = NULL;
        if (m_sel >= 0 && m_sel < m_count && !is_sep(m_sel))
            act = m_actions[m_sel];
        menu_close();
        if (act) act();
        return 1;
    }

    return 0;
}

int menu_click(int x, int y)
{
    if (!m_open) return 0;
    if (x < m_x || x >= m_x + m_w || y < m_y || y >= m_y + m_h) return 0;

    int idx = menu_hover(x, y);
    menu_action_t act = NULL;
    if (idx >= 0) act = m_actions[idx];
    menu_close();
    if (act) act();
    return 1;
}

void menu_draw(int mouse_x, int mouse_y)
{
    if (!m_open) return;

    uint32_t accent = gfx_rgb(0, 150, 136);
    uint32_t body   = gfx_rgb(34, 38, 46);
    uint32_t border = gfx_rgb(80, 86, 98);
    uint32_t text   = gfx_rgb(238, 242, 248);

    gfx_fill_rect(m_x + 3, m_y + 3, m_w, m_h, gfx_rgb(10, 12, 16));
    gfx_fill_rect(m_x, m_y, m_w, m_h, body);
    gfx_draw_rect(m_x, m_y, m_w, m_h, border);

    int py = m_y + PAD_Y;
    if (m_title[0]) {
        gfx_fill_rect(m_x + 1, m_y + 1, m_w - 2, TITLE_H - 2, accent);
        gfx_draw_text(m_x + PAD_X + 4, m_y + 7, m_title,
                      gfx_rgb(255, 255, 255), accent);
        py += TITLE_H;
    }

    int hover = menu_hover(mouse_x, mouse_y);
    if (hover < 0) hover = m_sel;

    for (int i = 0; i < m_count; i++) {
        if (is_sep(i)) {
            gfx_fill_rect(m_x + 6, py + SEP_H / 2, m_w - 12, 1, border);
            py += SEP_H;
            continue;
        }

        uint32_t bg = body;
        if (i == hover) {
            bg = accent;
            gfx_fill_rect(m_x + 2, py, m_w - 4, ITEM_H, bg);
        }
        gfx_draw_text(m_x + PAD_X + 4, py + 6, m_labels[i],
                      i == hover ? gfx_rgb(255, 255, 255) : text, bg);
        py += ITEM_H;
    }
}
