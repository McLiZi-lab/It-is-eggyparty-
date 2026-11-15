// =====================================================================
//  蛋仔派对 · 蛋仔岛（ASCII 版）
//  网易《蛋仔派对》蛋仔岛玩法的控制台重现：自由漫游、NPC 社交、
//  盲盒机、商店、任务板、蹦床/秋千/滑梯、表情动作、昼夜循环。
//  无贴图 —— 纯字符渲染，120x40 控制台双缓冲整帧输出。
//
//  编译（源码须为 UTF-8）：
//    g++ -O2 -std=c++17 -o eggy.exe eggy.cpp    (MinGW-w64)
//    cl /utf-8 /EHsc /O2 eggy.cpp               (MSVC)
// =====================================================================
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <cwchar>
#include <conio.h>
using namespace std;

// ------------------------- 屏幕 --------------------------------------
static const int SW = 120;        // 120 列
static const int SH = 40;         // 40 行
static const int SCENE_TOP = 2;   // 场景区（2~33 行）
static const int SCENE_BOT = 33;  // 场景区底
static const int SCENE_H = SCENE_BOT - SCENE_TOP + 1;  // 32 行

// ------------------------- 颜色 --------------------------------------
static const WORD A_DARK  = 0x08, A_GRAY = 0x07, A_WHITE = 0x0F;
static const WORD A_RED   = 0x0C, A_DRED = 0x04, A_YEL  = 0x0E;
static const WORD A_CYAN  = 0x0B, A_GREEN= 0x0A, A_MAG  = 0x0D;
static const WORD A_DCYAN = 0x03, A_DGREEN=0x02, A_DMAG = 0x05, A_DYEL = 0x06;

// ------------------------- 画布 --------------------------------------
struct Cell { wchar_t ch; WORD attr; bool cont; };
static Cell g_canvas[SH][SW];

static void clearBuffer() {
    for (int y = 0; y < SH; y++)
        for (int x = 0; x < SW; x++) {
            g_canvas[y][x].ch = L' ';
            g_canvas[y][x].attr = A_GRAY;
            g_canvas[y][x].cont = false;
        }
}
static bool isWideChar(wchar_t c) {
    return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xFF00 && c <= 0xFFEF) || c == 0x3000;
}
static void put(int x, int y, wchar_t c, WORD attr = A_GRAY) {
    if (y < 0 || y >= SH || x < 0 || x >= SW) return;
    if (isWideChar(c)) {
        if (x + 1 >= SW) return;
        g_canvas[y][x].ch = c;      g_canvas[y][x].attr = attr;  g_canvas[y][x].cont = false;
        g_canvas[y][x+1].ch = L' '; g_canvas[y][x+1].attr = attr; g_canvas[y][x+1].cont = true;
    } else {
        g_canvas[y][x].ch = c; g_canvas[y][x].attr = attr; g_canvas[y][x].cont = false;
    }
}
static int textWidth(const wstring& s) {
    int w = 0; for (wchar_t c : s) w += isWideChar(c) ? 2 : 1; return w;
}
static void drawText(int x, int y, const wstring& s, WORD attr = A_GRAY) {
    int cx = x;
    for (wchar_t c : s) { put(cx, y, c, attr); cx += isWideChar(c) ? 2 : 1; }
}
static void centerText(int y, const wstring& s, WORD attr = A_GRAY) {
    drawText((SW - textWidth(s)) / 2, y, s, attr);
}
static void rightText(int y, const wstring& s, WORD attr = A_GRAY) {
    drawText(SW - textWidth(s), y, s, attr);
}
static void fillRect(int x, int y, int w, int h, wchar_t c, WORD attr) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put(x + i, y + j, c, attr);
}
static void drawBox(int x, int y, int w, int h, WORD attr) {
    for (int i = 0; i < w; i++) {
        put(x + i, y, L'-', attr); put(x + i, y + h - 1, L'-', attr);
    }
    for (int j = 0; j < h; j++) {
        put(x, y + j, L'|', attr); put(x + w - 1, y + j, L'|', attr);
    }
    put(x, y, L'+', attr); put(x + w - 1, y, L'+', attr);
    put(x, y + h - 1, L'+', attr); put(x + w - 1, y + h - 1, L'+', attr);
}

// 整帧输出：逐行按同色段写 WriteConsoleW，全角字符天然占 2 列，
// 与 put() 的双格占位对齐，不残留旧帧、无闪烁。
static void present() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD wr = 0;
    for (int y = 0; y < SH; y++) {
        int x = 0, col = 0;
        while (x < SW) {
            if (g_canvas[y][x].cont) { x++; continue; }
            WORD a = g_canvas[y][x].attr;
            wstring s;
            int colStart = col;
            while (x < SW && !g_canvas[y][x].cont && g_canvas[y][x].attr == a) {
                wchar_t c = g_canvas[y][x].ch;
                s += c; x++;
                col += isWideChar(c) ? 2 : 1;
                if (x < SW && g_canvas[y][x].cont) x++;
            }
            SetConsoleTextAttribute(h, a);
            COORD pos = { (SHORT)colStart, (SHORT)y };
            SetConsoleCursorPosition(h, pos);
            WriteConsoleW(h, s.data(), (DWORD)s.size(), &wr, NULL);
        }
    }
}

// ------------------------- 控制台初始化 -------------------------------
static CONSOLE_CURSOR_INFO g_savedCursor;
static void initConsole() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(65001);
    SetConsoleTitleW(L"蛋仔岛 · EGGY ISLAND");
    GetConsoleCursorInfo(hOut, &g_savedCursor);
    CONSOLE_CURSOR_INFO ci = g_savedCursor;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(hOut, &ci);
    SMALL_RECT tmp = { 0, 0, (SHORT)(SW - 1), (SHORT)(SH - 1) };
    SetConsoleWindowInfo(hOut, TRUE, &tmp);
    COORD sz = { (SHORT)SW, (SHORT)SH };
    SetConsoleScreenBufferSize(hOut, sz);
    SetConsoleWindowInfo(hOut, TRUE, &tmp);
    system("cls");
}

// ------------------------- 实时键盘（事件轮询，不阻塞渲染） -----------
// 按住持续移动：KEY_EVENT 的 down/up 维护 held 表；单次按键走 onKey。
static bool g_held[256];
static DWORD g_time = 0;
static void onKey(WORD vk);   // 前向声明：定义在交互层

static void pollInput() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD n = 0;
    GetNumberOfConsoleInputEvents(hIn, &n);
    INPUT_RECORD rec;
    while (n-- > 0) {
        DWORD got = 0;
        ReadConsoleInputW(hIn, &rec, 1, &got);
        if (rec.EventType != KEY_EVENT) continue;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode;
        if (rec.Event.KeyEvent.bKeyDown) {
            if (!g_held[vk]) onKey(vk);   // 去抖：按下沿触发一次
            g_held[vk] = true;
        } else g_held[vk] = false;
    }
}
// =====================================================================
//  Part 2：蛋仔岛地图（64x48 格）与渲染
// =====================================================================
static const int MW = 64, MH = 48;          // 地图尺寸（格）
static char g_map[MH][MW];                  // 地形格

// 格子含义：~水 ,草 ,沙 :路 f花 T树 #建筑墙
enum Tile { T_WATER='~', T_GRASS='.', T_SAND=',', T_PATH=':', T_FLOWER='f', T_TREE='T', T_WALL='#' };

// ------------------------- 场景对象 ----------------------------------
enum ObjType { OB_GASHA, OB_SHOP, OB_BOARD, OB_PORTAL, OB_TRAMP, OB_SWING, OB_SLIDE, OB_FOUNTAIN };
struct Obj {
    float x, y;            // 中心坐标（格）
    int type;
};
static vector<Obj> g_objs;

// ------------------------- 昼夜 --------------------------------------
static int g_clock = 8 * 60;    // 游戏内时间（分钟），从 08:00 开始
static int dayPhase() {         // 0 白天 1 黄昏 2 夜晚
    int h = g_clock / 60;
    if (h >= 5 && h < 17) return 0;
    if (h >= 17 && h < 19) return 1;
    return 2;
}
static wstring clockText() {
    int h = g_clock / 60 % 24, m = g_clock % 60;
    wchar_t b[16]; swprintf(b, 16, L"%02d:%02d", h, m);
    return wstring(b);
}

// ------------------------- 地图生成 ----------------------------------
static void setRect(int x0, int y0, int x1, int y1, char c) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && x < MW && y >= 0 && y < MH) g_map[y][x] = c;
}
static void genMap() {
    int cx = MW / 2, cy = MH / 2;
    // 椭圆岛：草心 + 沙滩环，其余是海
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            float dx = (x - cx) / 27.0f, dy = (y - cy) / 19.0f;
            float d = dx * dx + dy * dy;
            g_map[y][x] = (d < 1.0f) ? T_GRASS : T_WATER;
            if (d >= 0.8f && d < 1.0f) g_map[y][x] = T_SAND;
        }
    // 中央广场（石板路圆）
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) {
            float dx = (x - cx) / 6.5f, dy = (y - cy) / 5.0f;
            if (dx * dx + dy * dy < 1.0f) g_map[y][x] = T_PATH;
        }
    // 四向主路
    setRect(cx - 1, 8,  cx + 1, cy, T_PATH);     // 北路 → 盲盒店
    setRect(cx - 1, cy, cx + 1, 40, T_PATH);     // 南路 → 任务板
    setRect(14, cy - 1, cx, cy + 1, T_PATH);     // 西路 → 派对之门
    setRect(cx, cy - 1, 50, cy + 1, T_PATH);     // 东路 → 商店
    // 建筑地基（墙）
    setRect(cx - 5, 5,  cx + 5, 11, T_WALL);     // 盲盒店
    setRect(48, 18, 56, 26, T_WALL);             // 商店
    // 游乐场（东南）：几条小路
    setRect(42, 30, 58, 30, T_PATH);
    setRect(46, 26, 46, 34, T_PATH);
    // 花丛撒点
    for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
            if (g_map[y][x] == T_GRASS && (x * 7 + y * 13) % 29 == 0)
                g_map[y][x] = T_FLOWER;
    // 树（固定点位，避开路和建筑）
    int trees[][2] = {{18,14},{24,9},{40,8},{45,13},{13,29},{18,35},{15,20},
                      {38,36},{42,40},{52,14},{27,40},{44,44},{10,25},{22,26}};
    for (auto& t : trees)
        if (g_map[t[1]][t[0]] == T_GRASS || g_map[t[1]][t[0]] == T_FLOWER)
            g_map[t[1]][t[0]] = T_TREE;

    // 交互对象
    g_objs.push_back({(float)cx,     (float)cy,     OB_FOUNTAIN});   // 广场喷泉
    g_objs.push_back({(float)cx,     12.5f,         OB_GASHA});      // 盲盒机（盲盒店门口）
    g_objs.push_back({50.0f,         17.5f,         OB_SHOP});       // 商店（门口）
    g_objs.push_back({(float)cx,     41.5f,         OB_BOARD});      // 任务板（南路尽头）
    g_objs.push_back({16.5f,         (float)cy,     OB_PORTAL});     // 派对之门（西路）
    g_objs.push_back({44.0f, 31.5f, OB_TRAMP});                      // 蹦床 x3
    g_objs.push_back({48.0f, 31.5f, OB_TRAMP});
    g_objs.push_back({52.0f, 33.5f, OB_TRAMP});
    g_objs.push_back({56.0f, 28.5f, OB_SWING});                      // 秋千
    g_objs.push_back({57.5f, 35.0f, OB_SLIDE});                      // 滑梯
}

// ------------------------- 碰撞 --------------------------------------
static char tileAt(float wx, float wy) {
    int x = (int)floor(wx), y = (int)floor(wy);
    if (x < 0 || x >= MW || y < 0 || y >= MH) return T_WATER;
    return g_map[y][x];
}
static bool solidTile(char c) {
    return c == T_WATER || c == T_WALL || c == T_TREE;
}
// 对象占位（喷泉/秋千/滑梯/门有实体；蹦床可踩上去）
static bool blockedByObj(float wx, float wy) {
    for (auto& o : g_objs) {
        if (o.type == OB_TRAMP) continue;          // 蹦床是触发器，可走
        float dx = wx - o.x, dy = wy - o.y;
        if (dx * dx + dy * dy < 0.8f) return true;
    }
    return false;
}
static bool isSolid(float wx, float wy) {
    return solidTile(tileAt(wx, wy)) || blockedByObj(wx, wy);
}

// ------------------------- 相机 --------------------------------------
static float g_camX = 0, g_camY = 0;    // 相机左上角（格）
// 每格 2 列宽（补偿字符 1:2 纵横比），1 行高
static int screenX(float wx) { return (int)((wx - g_camX) * 2.0f); }
static int screenY(float wy) { return (int)(wy - g_camY) + SCENE_TOP; }

// ------------------------- 地图渲染 ----------------------------------
// 白天/黄昏/夜晚三种调色；夜晚路面点灯笼、海面有波光
static WORD tileAttr(char c) {
    int p = dayPhase();
    switch (c) {
    case T_WATER:  return p == 2 ? A_DCYAN : (p == 1 ? A_CYAN : A_DCYAN);
    case T_GRASS:  return p == 2 ? A_DGREEN : A_GREEN;
    case T_FLOWER: return p == 2 ? A_DMAG   : A_MAG;
    case T_SAND:   return p == 2 ? A_DYEL   : A_YEL;
    case T_PATH:   return p == 2 ? A_DARK   : A_DYEL;
    case T_TREE:   return p == 2 ? A_DGREEN : A_GREEN;
    case T_WALL:   return p == 2 ? A_DARK   : A_WHITE;
    }
    return A_GRAY;
}
static wchar_t tileChar(char c, float wx, float wy) {
    switch (c) {
    case T_WATER:  return ((int)(wx + wy) + g_time / 500) % 3 == 0 ? L'≈' : L'~';  // 海浪
    case T_GRASS:  return L'"';
    case T_FLOWER: return L'*';
    case T_SAND:   return L'.';
    case T_PATH:   return L':';
    case T_TREE:   return L'\x2663';   // ♣ 树冠
    case T_WALL:   return L'#';
    }
    return L' ';
}

static void drawMap() {
    int tw = SW / 2, th = SCENE_H;          // 可视格数
    for (int ty = 0; ty < th; ty++) {
        for (int tx = 0; tx < tw; tx++) {
            float wx = g_camX + tx, wy = g_camY + ty;
            char c = tileAt(wx, wy);
            wchar_t ch = tileChar(c, wx, wy);
            WORD a = tileAttr(c);
            // 夜晚路面的灯笼
            if (dayPhase() == 2 && c == T_PATH && ((int)(wx * 3 + wy * 5) % 11) == 0
                && (g_time / 600) % 2 == 0) {
                put(tx * 2, ty + SCENE_TOP, L'*', A_YEL);
                put(tx * 2 + 1, ty + SCENE_TOP, L' ', A_DARK);
            } else {
                put(tx * 2,     ty + SCENE_TOP, ch, a);
                put(tx * 2 + 1, ty + SCENE_TOP, ch, a);
            }
        }
    }
}

// ------------------------- 对象精灵（画在地图之上） -------------------
static void drawObj(const Obj& o) {
    int sx = screenX(o.x), sy = screenY(o.y);
    switch (o.type) {
    case OB_FOUNTAIN:   // 喷泉：三层圈 + 中心水柱（动画）
        drawText(sx - 2, sy - 1, L"(≈≈≈)", A_CYAN);
        drawText(sx - 2, sy,     L"(|__|)", A_CYAN);
        put(sx, sy - 2, (g_time / 300) % 2 ? L'\x18' : L'\x2191', A_WHITE);  // 水柱
        break;
    case OB_GASHA:      // 盲盒机：礼盒造型
        drawText(sx - 3, sy - 3, L" .-''-.", A_MAG);
        drawText(sx - 3, sy - 2, L"/______\\", A_MAG);
        drawText(sx - 3, sy - 1, L"| \x25CF\x25CF |", A_MAG);
        drawText(sx - 3, sy,     L"+------+ ", A_MAG);
        drawText(sx - 3, sy + 1, L" 盲盒机", A_YEL);
        break;
    case OB_SHOP:       // 商店摊位
        drawText(sx - 4, sy - 2, L"___________", A_YEL);
        drawText(sx - 4, sy - 1, L"/  商店 \\", A_YEL);
        drawText(sx - 4, sy,     L"| \x25CB\x25CB\x25CB\x25CB |", A_YEL);
        break;
    case OB_BOARD:      // 任务板
        drawText(sx - 3, sy - 2, L"+---------+", A_WHITE);
        drawText(sx - 3, sy - 1, L"| 任务板  |", A_WHITE);
        drawText(sx - 3, sy,     L"+----+----+", A_WHITE);
        break;
    case OB_PORTAL:    // 派对之门：闪烁的星门
        drawText(sx - 2, sy - 3, L"  __  ", A_MAG);
        drawText(sx - 2, sy - 2, L" /  \\ ", A_MAG);
        drawText(sx - 2, sy - 1, L"|  *  |", (g_time / 250) % 2 ? A_WHITE : A_MAG);
        drawText(sx - 2, sy,     L" \\  / ", A_MAG);
        drawText(sx - 2, sy + 1, L" 派对门", A_MAG);
        break;
    case OB_TRAMP:     // 蹦床：弹面（踩上去自动弹跳）
        drawText(sx - 2, sy - 1, L" _____ ", A_CYAN);
        drawText(sx - 2, sy,     L"(WWWWW)", A_CYAN);
        break;
    case OB_SWING:     // 秋千：架子 + 摆动坐板
        drawText(sx - 3, sy - 3, L"\\_______/", A_YEL);
        drawText(sx - 3, sy - 2, L" |     | ", A_YEL);
        {   // 坐板随时间左右摆
            int sw = (int)(sinf(g_time / 300.0f) * 2.0f);
            drawText(sx + sw, sy - 1, L"o", A_DARK);
            drawText(sx - 1 + sw, sy, L"===", A_YEL);
        }
        break;
    case OB_SLIDE:     // 滑梯
        drawText(sx - 2, sy - 3, L"  __", A_CYAN);
        drawText(sx - 2, sy - 2, L" /  ", A_CYAN);
        drawText(sx - 2, sy - 1, L"/   ", A_CYAN);
        drawText(sx - 2, sy,     L"\\__ ", A_CYAN);
        drawText(sx - 2, sy + 1, L"  \\\\", A_CYAN);
        break;
    }
}

// ------------------------- 小地图：见 Part 3（依赖 NPC 数据） ----------

// =====================================================================
//  Part 3：蛋仔实体 —— 玩家、NPC、精灵绘制、移动与 AI
// =====================================================================
// 跨部分状态（定义在此，Part 4 使用）
static int g_panelType = 0;        // 0 无面板（见 Part 4 面板枚举）
static bool g_showBigMap = false;  // 大地图开关
static void onTrampoline();        // 蹦床回调（Part 4：任务/金币）
static void addMsg(const wstring& s, WORD c = 0x07);   // 消息日志（Part 4）

// ------------------------- 皮肤 --------------------------------------
struct Skin { const wchar_t* name; WORD color; };
static const Skin SKINS[] = {
    { L"原始蛋",  0x0F }, { L"小黄鸭", 0x0E }, { L"兔兔蛋", 0x0D },
    { L"汽水蛋",  0x0B }, { L"抹茶蛋", 0x0A }, { L"葡萄蛋", 0x0D },
    { L"蜜桃蛋",  0x0C }, { L"夜光蛋", 0x07 },
};
static const int N_SKINS = 8;
static bool g_owned[N_SKINS] = { true, false, false, false, false, false, false, false };
static int g_skin = 0;                    // 当前佩戴
static int g_coins = 20;                  // 金币

// ------------------------- 玩家 --------------------------------------
static float g_playerX = 32, g_playerY = 28;   // 出生：广场南侧
static int g_face = 0;                          // 0下 1左 2右 3上
static float g_jumpZ = 0, g_jumpVz = 0;        // 跳跃高度/速度
static bool g_onTramp = false;
static int g_emote = 0;                          // 0无 1挥手 2跳舞 3坐 4爱心
static DWORD g_emoteUntil = 0;
static wstring g_bubble = L"";                   // 玩家气泡
static DWORD g_bubbleUntil = 0;
static bool g_wiggle = false;                    // 走路/跳舞扭动相位

// ------------------------- NPC 蛋仔 ----------------------------------
struct Npc {
    float x, y;
    WORD color;
    wstring name;
    float dirX, dirY;
    DWORD nextThink;
    wstring bubble;
    DWORD bubbleUntil;
    int friendship;
};
static vector<Npc> g_npcs;

static void spawnNpcs() {
    const wchar_t* names[] = { L"团子", L"豆豆", L"泡芙", L"汽水", L"摩卡", L"布丁" };
    const WORD colors[] = { 0x0D, 0x0E, 0x0F, 0x0B, 0x0A, 0x06 };
    float px[] = { 30, 35, 32, 27, 38, 33 };
    float py[] = { 22, 25, 21, 30, 28, 33 };
    for (int i = 0; i < 6; i++)
        g_npcs.push_back({ px[i], py[i], colors[i], names[i], 0, 0, 0, L"", 0, 0 });
}

// ------------------------- 蛋仔精灵 ----------------------------------
// 4 行 5 列的小蛋：眼睛随朝向偏移，坐下变扁，扭动时左右镜像
static void drawEgg(float wx, float wy, WORD color, const wstring& name,
                    int face, float z, bool wiggle, bool sitting) {
    int sx = screenX(wx) - 2, sy = screenY(wy) - (sitting ? 2 : 3) - (int)(z * 2);
    int bob = (wiggle && (g_time / 150) % 2) ? 1 : 0;
    sx += (wiggle && (g_time / 150) % 2) ? 1 : 0;
    // 影子（跳起时留在地面）
    drawText(screenX(wx) - 2, screenY(wy), L" __ ", A_DARK);
    const wchar_t* r0 = sitting ? L" .--. " : L" .-. ";
    const wchar_t* r3 = sitting ? L"(____)" : L"\\___/";
    // 眼睛：朝向决定瞳孔位置
    wstring eyes = L"o o";
    if (face == 1) eyes = L"o .";
    if (face == 2) eyes = L". o";
    if (face == 3) eyes = L"- -";
    wstring r1 = wstring(sitting ? L"( " : L"/") + eyes + (sitting ? L" )" : L"\\");
    const wchar_t* r2 = sitting ? L" `--' " : L" \\_/ ";
    drawText(sx + bob, sy,     r0, color);
    drawText(sx + bob, sy + 1, r1, color);
    drawText(sx + bob, sy + 2, r2, color);
    drawText(sx + bob, sy + 3, r3, color);
    // 名牌
    drawText(screenX(wx) - textWidth(name) / 2, sy - 1, name, A_DARK);
}

// 气泡：画在名牌上方
static void drawBubble(float wx, float wy, const wstring& text, WORD attr) {
    if (text.empty()) return;
    int bx = screenX(wx) - textWidth(text) / 2 - 1;
    int by = screenY(wy) - 6;
    drawText(bx, by, wstring(L"( ") + text + L" )", attr);
}

// ------------------------- NPC AI ------------------------------------
static const wchar_t* NPC_CHAT[] = {
    L"一起玩呀!", L"冲鸭~", L"今天盲盒出了兔兔蛋!",
    L"蹦床超好玩", L"你去过派对之门吗?", L"嘿嘿",
};
static void updateNpcs(float dt) {
    for (auto& n : g_npcs) {
        if (g_time > n.nextThink) {
            n.nextThink = g_time + 1000 + rand() % 2500;
            if (rand() % 3 == 0) { n.dirX = 0; n.dirY = 0; }              // 休息
            else {
                float a = (rand() % 8) * 3.14159f / 4.0f;
                n.dirX = cosf(a); n.dirY = sinf(a);
            }
            if (rand() % 6 == 0) {                                        // 随机闲聊
                n.bubble = NPC_CHAT[rand() % 6];
                n.bubbleUntil = g_time + 2500;
            }
        }
        float spd = 2.2f * dt;
        float nx = n.x + n.dirX * spd, ny = n.y + n.dirY * spd;
        if (!isSolid(nx, n.y) && !isSolid(nx + n.dirX, n.y)) n.x = nx; else n.dirX = 0;
        if (!isSolid(n.x, ny) && !isSolid(n.x, ny + n.dirY)) n.y = ny; else n.dirY = 0;
        if (g_time > n.bubbleUntil) n.bubble.clear();
    }
}

// ------------------------- 玩家更新 ----------------------------------
static void updatePlayer(float dt) {
    bool locked = (g_panelType != 0) || g_showBigMap;
    bool moving = false;
    if (!locked) {
        float spd = (g_held[VK_SHIFT] ? 9.0f : 5.0f) * dt;
        float dx = 0, dy = 0;
        if (g_held['W'] || g_held[VK_UP])    { dy -= spd; g_face = 3; }
        if (g_held['S'] || g_held[VK_DOWN])  { dy += spd; g_face = 0; }
        if (g_held['A'] || g_held[VK_LEFT])  { dx -= spd; g_face = 1; }
        if (g_held['D'] || g_held[VK_RIGHT]) { dx += spd; g_face = 2; }
        moving = (dx != 0 || dy != 0);
        // 轴分离碰撞（可以沿墙滑动）
        if (!isSolid(g_playerX + dx, g_playerY)) g_playerX += dx;
        if (!isSolid(g_playerX, g_playerY + dy)) g_playerY += dy;
        g_wiggle = moving;
    }
    // 跳跃物理
    if (g_jumpZ > 0 || g_jumpVz != 0) {
        g_jumpVz -= 26.0f * dt;
        g_jumpZ += g_jumpVz * dt;
        if (g_jumpZ <= 0) { g_jumpZ = 0; g_jumpVz = 0; }
    }
    // 蹦床：站在上面自动弹起（越高弹得越高）
    g_onTramp = false;
    for (auto& o : g_objs)
        if (o.type == OB_TRAMP) {
            float ddx = g_playerX - o.x, ddy = g_playerY - o.y;
            if (ddx * ddx + ddy * ddy < 1.0f) {
                g_onTramp = true;
                if (g_jumpZ == 0) {
                    g_jumpVz = 9.0f; g_jumpZ = 0.01f;
                    onTrampoline();          // 任务/金币回调（Part 4）
                }
            }
        }
    if (g_time > g_bubbleUntil) g_bubble.clear();
    if (g_time > g_emoteUntil) g_emote = 0;
    // 相机跟随（把玩家放中间，越界钳制）
    g_camX = g_playerX - SW / 4.0f;
    g_camY = g_playerY - SCENE_H / 2.0f;
    if (g_camX < 0) g_camX = 0;
    if (g_camY < 0) g_camY = 0;
    if (g_camX > MW - SW / 2) g_camX = MW - SW / 2;
    if (g_camY > MH - SCENE_H) g_camY = MH - SCENE_H;
}

// ------------------------- 找交互目标 --------------------------------
static Obj* nearestObj() {
    Obj* best = NULL; float bd = 2.2f * 2.2f;
    for (auto& o : g_objs) {
        float dx = g_playerX - o.x, dy = g_playerY - o.y;
        float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = &o; }
    }
    return best;
}
static Npc* nearestNpc() {
    Npc* best = NULL; float bd = 1.6f * 1.6f;
    for (auto& n : g_npcs) {
        float dx = g_playerX - n.x, dy = g_playerY - n.y;
        float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = &n; }
    }
    return best;
}
static wstring objPrompt(Obj* o) {
    switch (o->type) {
    case OB_GASHA:     return L"扭一个盲盒(10金币)";
    case OB_SHOP:      return L"逛商店";
    case OB_BOARD:     return L"看任务板";
    case OB_PORTAL:    return L"前往派对";
    case OB_TRAMP:     return L"蹦床(走上去自动弹)";
    case OB_SWING:     return L"荡秋千";
    case OB_SLIDE:     return L"滑滑梯";
    case OB_FOUNTAIN:  return L"许愿";
    }
    return L"";
}

// ------------------------- 场景绘制（蛋仔层） -------------------------
static void drawEntities() {
    for (auto& n : g_npcs) {
        drawEgg(n.x, n.y, n.color, n.name, 0, 0, false, false);
        drawBubble(n.x, n.y, n.bubble, A_GRAY);
    }
    // 玩家最后画（在最上层）
    bool sitting = (g_emote == 3);
    drawEgg(g_playerX, g_playerY, SKINS[g_skin].color, L"你", g_face,
            g_jumpZ, g_wiggle || g_emote == 2, sitting);
    drawBubble(g_playerX, g_playerY, g_bubble, A_YEL);
}

// ------------------------- 小地图（右上角） --------------------------
static void drawMinimap() {
    const int mw = 26, mh = 9;
    int ox = SW - mw * 2 - 4, oy = SCENE_TOP + 1;
    drawBox(ox - 2, oy - 1, mw * 2 + 4, mh + 2, A_DARK);
    for (int y = 0; y < mh; y++)
        for (int x = 0; x < mw; x++) {
            char c = g_map[y * MH / mh][x * MW / mw];
            wchar_t ch = (c == T_WATER) ? L'~' : (c == T_WALL ? L'#' : (c == T_PATH ? L':' : L'.'));
            WORD a = tileAttr(c);
            put(ox + x * 2, oy + y, ch, a);
            put(ox + x * 2 + 1, oy + y, L' ', a);
        }
    put(ox + (int)(g_playerX * mw / MW) * 2, oy + (int)(g_playerY * mh / MH), L'@', A_RED);
    for (auto& n : g_npcs)
        put(ox + (int)(n.x * mw / MW) * 2, oy + (int)(n.y * mh / MH), L'o', n.color);
}
// =====================================================================
//  Part 4：交互面板、任务系统、HUD 与主循环
// =====================================================================

// ------------------------- 面板枚举 ----------------------------------
enum PanelType { P_NONE = 0, P_GASHA, P_SHOP, P_BOARD, P_PORTAL, P_HELP, P_QUIT };
static bool g_running = true;          // 主循环开关（onKey 要用，提前声明）
static wstring g_gachaResult = L"";    // 盲盒结果文案

// ------------------------- 消息日志 ----------------------------------
static vector<pair<wstring, WORD>> g_log;
static void addMsg(const wstring& s, WORD c) {
    g_log.push_back({ s, c });
    if (g_log.size() > 3) g_log.erase(g_log.begin());
}

// ------------------------- 任务 --------------------------------------
static int tGreet = 0, tTramp = 0, tGacha = 0;
static bool doneGreet = false, doneTramp = false, doneGacha = false;
static wstring taskLine(int i) {
    switch (i) {
    case 0: return wstring(L"和蛋仔打招呼  ") + to_wstring(min(tGreet, 3)) + L"/3" + (doneGreet ? L"  [已完成]" : L"");
    case 1: return wstring(L"在蹦床上弹跳  ") + to_wstring(min(tTramp, 1)) + L"/1" + (doneTramp ? L"  [已完成]" : L"");
    case 2: return wstring(L"开一个盲盒    ") + to_wstring(min(tGacha, 1)) + L"/1" + (doneGacha ? L"  [已完成]" : L"");
    }
    return L"";
}
static void checkTasks() {
    if (!doneGreet && tGreet >= 3) { doneGreet = true; g_coins += 15; addMsg(L"任务完成:和蛋仔打招呼 +15金币", A_YEL); }
    if (!doneTramp && tTramp >= 1) { doneTramp = true; g_coins += 10; addMsg(L"任务完成:蹦床弹跳 +10金币", A_YEL); }
    if (!doneGacha && tGacha >= 1) { doneGacha = true; g_coins += 12; addMsg(L"任务完成:开盲盒 +12金币", A_YEL); }
}
static void onTrampoline() {
    tTramp++;
    if (rand() % 3 == 0) { g_coins++; }
    if (tTramp == 1) addMsg(L"蹦床好高呀~", A_CYAN);
    checkTasks();
}

// ------------------------- 交互动作 ----------------------------------
static void doInteract() {
    Obj* o = nearestObj();      // NPC 打招呼走 C 键，这里只处理场景对象
    if (!o) return;
    switch (o->type) {
    case OB_GASHA:
        if (g_coins < 10) { addMsg(L"金币不足,扭蛋机需要10金币(打招呼赚金币)", A_RED); return; }
        g_coins -= 10;
        tGacha++;
        {   // 随机出一个皮肤；重复的折算 5 金币
            int r = rand() % N_SKINS;
            if (g_owned[r]) {
                g_coins += 5;
                g_gachaResult = wstring(SKINS[r].name) + L"(重复) +5金币";
            } else {
                g_owned[r] = true;
                g_gachaResult = SKINS[r].name;
                addMsg(wstring(L"抽到了新皮肤:") + SKINS[r].name + L"! 按G切换", A_YEL);
            }
            g_panelType = P_GASHA;
        }
        checkTasks();
        break;
    case OB_SHOP:   g_panelType = P_SHOP; break;
    case OB_BOARD:  g_panelType = P_BOARD; break;
    case OB_PORTAL:
        g_panelType = P_PORTAL;
        break;
    case OB_FOUNTAIN:
        g_bubble = L"许愿中...";
        g_bubbleUntil = g_time + 2000;
        g_coins += 1;
        addMsg(L"你对喷泉许了个愿,蛋仔岛的蛋运+1 (+1金币)", A_CYAN);
        break;
    case OB_SWING:
        g_bubble = L"荡呀荡~";
        g_bubbleUntil = g_time + 3000;
        addMsg(L"你荡起了秋千,风从耳边吹过", A_CYAN);
        break;
    case OB_SLIDE:
        g_bubble = L"咻~!";
        g_bubbleUntil = g_time + 2000;
        addMsg(L"你从滑梯上咻地滑了下来", A_CYAN);
        break;
    case OB_TRAMP: break;   // 蹦床靠站上去触发
    }
}

// ------------------------- 按键分发 ----------------------------------
static void greet() {
    Npc* n = nearestNpc();
    if (!n) { addMsg(L"附近没有蛋仔,走近一点再打招呼(按C)", A_DARK); return; }
    if (n->friendship < 99) n->friendship++;
    tGreet++;
    g_coins += 2;
    n->bubble = (n->friendship < 3) ? L"嗨!你好呀" : L"又是你呀~";
    n->bubbleUntil = g_time + 2500;
    g_bubble = L"你好!";
    g_bubbleUntil = g_time + 2000;
    addMsg(wstring(L"你和") + n->name + L"打了招呼 好感+" + to_wstring(n->friendship) + L" (+2金币)", A_GREEN);
    checkTasks();
}
static void doEmote(int e) {
    g_emote = e;
    g_emoteUntil = g_time + 2500;
    switch (e) {
    case 1: g_bubble = L"\x266A 挥手~";   break;      // ♪
    case 2: g_bubble = L"跳舞♪";          break;
    case 3: g_bubble = L"坐下歇会儿";     break;
    case 4: g_bubble = L"\x2665";         break;      // ♥
    }
    g_bubbleUntil = g_time + 2500;
}
static void cycleSkin() {
    int s = g_skin;
    for (int i = 1; i <= N_SKINS; i++) {
        int c = (g_skin + i) % N_SKINS;
        if (g_owned[c]) { s = c; break; }
    }
    g_skin = s;
    addMsg(wstring(L"切换皮肤:") + SKINS[g_skin].name, A_YEL);
}
static void shopBuy(int idx) {
    const int price[3] = { 25, 25, 30 };
    const int id[3]    = { 1, 2, 5 };      // 小黄鸭 兔兔蛋 葡萄蛋
    if (idx < 0 || idx > 2) return;
    if (g_coins < price[idx]) { addMsg(L"金币不足", A_RED); return; }
    if (g_owned[id[idx]]) { addMsg(L"已经拥有这件皮肤了", A_DARK); return; }
    g_coins -= price[idx];
    g_owned[id[idx]] = true;
    g_skin = id[idx];
    addMsg(wstring(L"购买成功,穿上") + SKINS[id[idx]].name, A_YEL);
}

static void onKey(WORD vk) {
    if (g_panelType != P_NONE) {                  // 面板打开时:ESC/E 关闭,商店1-3购买,退出Y确认
        if (g_panelType == P_SHOP) {
            if (vk == '1') shopBuy(0);
            if (vk == '2') shopBuy(1);
            if (vk == '3') shopBuy(2);
        }
        if (g_panelType == P_QUIT && (vk == 'Y' || vk == 'y')) { g_running = false; return; }
        if (vk == VK_ESCAPE || vk == 'E' || vk == 'e') g_panelType = P_NONE;
        return;
    }
    if (g_showBigMap) {                           // 大地图:任意键关闭
        if (vk == 'M' || vk == 'm' || vk == VK_ESCAPE) g_showBigMap = false;
        return;
    }
    switch (vk) {
    case 'E': case 'e':  doInteract();  break;
    case 'C': case 'c':  greet();       break;
    case VK_SPACE: if (g_jumpZ == 0) { g_jumpVz = 5.5f; g_jumpZ = 0.01f; } break;
    case '1': doEmote(1); break;
    case '2': doEmote(2); break;
    case '3': doEmote(3); break;
    case '4': doEmote(4); break;
    case 'G': case 'g': cycleSkin();   break;
    case 'M': case 'm': g_showBigMap = true; break;
    case 'H': case 'h': g_panelType = P_HELP; break;
    case 'Q': case 'q': g_panelType = P_QUIT;  break;
    }
}

// ------------------------- HUD ---------------------------------------
static void drawTopBar() {
    const wchar_t* phaseIcon[] = { L"\x263C", L"\x263C", L"\x263E" };   // ☺ / ☾
    wstring left = wstring(L" 蛋仔岛  ") + clockText() + L" " + phaseIcon[dayPhase()];
    drawText(0, 0, left, dayPhase() == 2 ? A_DARK : A_YEL);
    wstring mid = wstring(L"金币:") + to_wstring(g_coins) + L"  皮肤:" + SKINS[g_skin].name
        + L"  任务:" + to_wstring(doneGreet + doneTramp + doneGacha) + L"/3";
    drawText(28, 0, mid, A_YEL);
    drawText(0, 1, L" WASD/方向键移动  Shift加速  E互动  C打招呼  空格跳  1-4表情  G皮肤  M地图  H帮助", A_DARK);
}
static void drawBottom() {
    for (int x = 0; x < SW; x++) put(x, 34, L'-', A_DARK);
    int row = 35;
    for (auto& m : g_log) drawText(2, row++, L"· " + m.first, m.second);
    while (row < 39) drawText(2, row++, L"", A_DARK);
    // 交互提示
    wstring prompt;
    Obj* o = nearestObj(); Npc* n = nearestNpc();
    if (n) prompt = wstring(L"[C]和") + n->name + L"打招呼   ";
    if (o) prompt += wstring(L"[E]") + objPrompt(o);
    if (!prompt.empty()) drawText(SW - textWidth(prompt) - 2, 39, prompt, A_WHITE);
}

// ------------------------- 大地图 ------------------------------------
static void drawBigMap() {
    fillRect(0, 0, SW, SH, L' ', A_DARK);
    centerText(0, L"—— 蛋仔岛全图 ——", A_YEL);
    int ox = 2, oy = 4;
    for (int y = 0; y < MH; y += 2)
        for (int x = 0; x < MW; x++) {
            char c = g_map[y][x];
            wchar_t ch;
            switch (c) {
            case T_WATER: ch = L'~'; break;
            case T_WALL:  ch = L'#'; break;
            case T_PATH:  ch = L':'; break;
            case T_SAND:  ch = L'.'; break;
            case T_TREE:  ch = L'\x2663'; break;
            case T_FLOWER: ch = L'*'; break;
            default:      ch = L'"'; break;
            }
            put(ox + x, oy + y / 2, ch, tileAttr(c));
        }
    // 对象标记
    const wchar_t* mark[] = { L"G", L"$", L"B", L"P", L"t", L"s", L"L", L"F" };
    for (auto& o : g_objs) put(ox + (int)o.x, oy + (int)o.y / 2, mark[o.type][0], A_YEL);
    for (auto& n : g_npcs) put(ox + (int)n.x, oy + (int)n.y / 2, L'o', n.color);
    put(ox + (int)g_playerX, oy + (int)g_playerY / 2, L'@', A_RED);
    drawText(70, 6,  L"@ 你      o 蛋仔", A_WHITE);
    drawText(70, 8,  L"G 盲盒机  $ 商店", A_YEL);
    drawText(70, 10, L"B 任务板  P 派对门", A_YEL);
    drawText(70, 12, L"t 蹦床    s 秋千", A_YEL);
    drawText(70, 14, L"L 滑梯    F 喷泉", A_YEL);
    drawText(70, 18, L"[M] 返回", A_DARK);
}

// ------------------------- 面板绘制 ----------------------------------
static void drawPanel() {
    if (g_panelType == P_NONE) return;
    int px = 32, py = 8, pw = 56, ph = 18;
    fillRect(px, py, pw, ph, L' ', A_GRAY);
    drawBox(px, py, pw, ph, A_YEL);
    switch (g_panelType) {
    case P_GASHA:
        drawText(px + 4, py + 2, L"* 盲盒机 *", A_MAG);
        drawText(px + 4, py + 5, L"你扭动旋钮,咔哒——", A_GRAY);
        drawText(px + 4, py + 7, L"  " + g_gachaResult, A_YEL);
        drawText(px + 4, py + 14, L"[E] 收下", A_DARK);
        break;
    case P_SHOP:
        drawText(px + 4, py + 2, L"* 蛋仔商店 *", A_YEL);
        drawText(px + 4, py + 5,  L"1. 小黄鸭皮肤   25金币", g_owned[1] ? A_DARK : A_YEL);
        drawText(px + 4, py + 7,  L"2. 兔兔蛋皮肤   25金币", g_owned[2] ? A_DARK : A_YEL);
        drawText(px + 4, py + 9,  L"3. 葡萄蛋皮肤   30金币", g_owned[5] ? A_DARK : A_YEL);
        drawText(px + 4, py + 12, wstring(L"当前金币:") + to_wstring(g_coins), A_GRAY);
        drawText(px + 4, py + 14, L"[1/2/3]购买  [E]离开", A_DARK);
        break;
    case P_BOARD:
        drawText(px + 4, py + 2, L"* 今日任务 *", A_WHITE);
        drawText(px + 4, py + 5,  L"  " + taskLine(0), doneGreet ? A_GREEN : A_GRAY);
        drawText(px + 4, py + 7,  L"  " + taskLine(1), doneTramp ? A_GREEN : A_GRAY);
        drawText(px + 4, py + 9,  L"  " + taskLine(2), doneGacha ? A_GREEN : A_GRAY);
        drawText(px + 4, py + 14, L"[E] 关闭", A_DARK);
        break;
    case P_PORTAL:
        drawText(px + 4, py + 2, L"* 派对之门 *", A_MAG);
        drawText(px + 4, py + 5, L"星门微微发亮,传来了派对的喧闹声...", A_GRAY);
        drawText(px + 4, py + 7, L"派对玩法装修中 —— 本版本先在蛋仔岛尽情玩吧!", A_YEL);
        drawText(px + 4, py + 14, L"[E] 留在岛上", A_DARK);
        break;
    case P_HELP:
        drawText(px + 4, py + 2, L"* 帮助 *", A_WHITE);
        drawText(px + 4, py + 5,  L"WASD/方向键:移动   Shift:加速   空格:跳", A_GRAY);
        drawText(px + 4, py + 7,  L"E:互动(盲盒/商店/任务板/门/秋千/滑梯/喷泉)", A_GRAY);
        drawText(px + 4, py + 9,  L"C:和附近的蛋仔打招呼(涨好感+金币)", A_GRAY);
        drawText(px + 4, py + 11, L"1挥手 2跳舞 3坐下 4爱心  G:切换皮肤  M:大地图", A_GRAY);
        drawText(px + 4, py + 14, L"[E] 关闭", A_DARK);
        break;
    case P_QUIT:
        drawText(px + 4, py + 7, L"确定要离开蛋仔岛吗?", A_WHITE);
        drawText(px + 4, py + 10, L"[Y] 确认离开    [其他键] 取消", A_DARK);
        break;
    }
}

// ------------------------- 渲染一帧 ----------------------------------
static void renderFrame() {
    clearBuffer();
    if (g_showBigMap) {
        drawBigMap();
    } else {
        drawMap();
        for (auto& o : g_objs) drawObj(o);
        drawEntities();
        drawMinimap();
        drawTopBar();
        drawBottom();
        drawPanel();
    }
    present();
}

// ------------------------- 主循环 ------------------------------------
int main() {
    srand((unsigned)time(0));
    initConsole();
    genMap();
    spawnNpcs();
    addMsg(L"欢迎来到蛋仔岛! WASD 移动,按 H 查看帮助", A_YEL);
    addMsg(L"小贴士:走近蛋仔按 C 打招呼可以赚金币", A_GRAY);

    DWORD last = GetTickCount();
    while (g_running) {
        g_time = GetTickCount();
        DWORD now = g_time;
        float dt = (now - last) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f;
        last = now;

        pollInput();
        // 昼夜:每 0.1s 游戏时间 +1 分钟 → 一整天约 2.4 分钟
        static DWORD lastClock = 0;
        if (now - lastClock > 100) { lastClock = now; g_clock = (g_clock + 1) % 1440; }
        updateNpcs(dt);
        updatePlayer(dt);
        renderFrame();
        Sleep(30);
    }
    // 退出画面
    clearBuffer();
    centerText(19, L"下次再来蛋仔岛玩呀!", A_YEL);
    present();
    Sleep(800);
    return 0;
}
