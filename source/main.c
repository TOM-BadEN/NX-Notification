// 采用 libtesla 的绘制逻辑：VI 层、帧缓冲、RGBA4444、块线性 swizzle 与像素混合
// 标准头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/log.h"

// libnx 头文件
#include <switch.h>
#include <switch/display/framebuffer.h>
#include <switch/display/native_window.h>
#include <switch/services/sm.h>
#include <switch/runtime/devices/fs_dev.h>
// Applet type query for NV auto-selection
#include <switch/services/applet.h>
// 字体与系统语言服务
#include <switch/services/pl.h>
#include <switch/services/set.h>
#include <switch/services/pm.h>
#include <switch/runtime/hosversion.h>
// NV 与 NVMAP/FENCE 以便显式初始化与日志
#include <switch/services/nv.h>
#include <switch/nvidia/map.h>
#include <switch/nvidia/fence.h>

// STB TrueType 单文件库实现（在本文件中生成实现）
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// 覆盖 libnx 的弱符号以强制 NV 服务类型和 tmem 大小（避免卡住）
NvServiceType __attribute__((weak)) __nx_nv_service_type = NvServiceType_Application; // 默认 Auto 在 sysmodule 会选 System；强制走 nvdrv(u)
u32 __attribute__((weak)) __nx_nv_transfermem_size = 0x200000; // 将 tmem 从 8MB 降到 2MB，规避大内存问题

// 内部堆大小（按需调整）
#define INNER_HEAP_SIZE 0x400000

// 屏幕分辨率（与 tesla.hpp 对齐）
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

// 配置项（与 tesla cfg 对齐）
static u16 CFG_FramebufferWidth = 448;
static u16 CFG_FramebufferHeight = 720;
static u16 CFG_LayerWidth = 0;
static u16 CFG_LayerHeight = 0;
static u16 CFG_LayerPosX = 0;
static u16 CFG_LayerPosY = 0;

// Renderer 等价的状态
static ViDisplay g_display;
static ViLayer g_layer;
static Event g_vsyncEvent;
static NWindow g_window;
static Framebuffer g_framebuffer;
static void *g_currentFramebuffer = NULL;
static bool g_gfxInitialized = false;

// 字体状态（与 tesla.hpp 的 Renderer::initFonts 等价的 C 实现）
static stbtt_fontinfo g_font_std, g_font_local, g_font_ext;
static bool g_has_local_font = false;
static bool g_fonts_initialized = false;

// 函数声明
static void fonts_exit(void);

// VI 层栈添加（tesla.hpp 使用的辅助函数）
static Result viAddToLayerStack(ViLayer *layer, ViLayerStack stack) {
    const struct {
        u32 stack;
        u64 layerId;
    } in = { stack, layer->layer_id };
    return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
}

// libnx 在 vi.c 中提供的弱符号：用于让 viCreateLayer 关联到已创建的 Managed Layer
extern u64 __nx_vi_layer_id;

// 颜色结构（4bit RGBA）
typedef struct { u8 r, g, b, a; } Color;

static inline u16 color_to_u16(Color c) {
    return (u16)((c.r & 0xF) | ((c.g & 0xF) << 4) | ((c.b & 0xF) << 8) | ((c.a & 0xF) << 12));
}

static inline Color color_from_u16(u16 raw) {
    Color c;
    c.r = (raw >> 0) & 0xF;
    c.g = (raw >> 4) & 0xF;
    c.b = (raw >> 8) & 0xF;
    c.a = (raw >> 12) & 0xF;
    return c;
}

// 像素混合（与 tesla.hpp 的 blendColor 一致）
static inline u8 blendColor(u8 src, u8 dst, u8 alpha) {
    u8 oneMinusAlpha = 0x0F - alpha;
    // 使用浮点以匹配 tesla.hpp 行为
    return (u8)((dst * alpha + src * oneMinusAlpha) / (float)0xF);
}

// 将 x,y 映射为块线性帧缓冲中的偏移（与 tesla.hpp getPixelOffset 一致）
static inline u32 getPixelOffset(s32 x, s32 y) {
    // 边界由调用者保证，这里直接映射
    u32 tmpPos = ((y & 127) / 16) + (x / 32 * 8) + ((y / 16 / 8) * (((CFG_FramebufferWidth / 2) / 16 * 8)));
    tmpPos *= 16 * 16 * 4;
    tmpPos += ((y % 16) / 8) * 512 + ((x % 32) / 16) * 256 + ((y % 8) / 2) * 64 + ((x % 16) / 8) * 32 + (y % 2) * 16 + (x % 8) * 2;
    return tmpPos / 2;
}

// 绘制基本原语
static inline void setPixel(s32 x, s32 y, Color color) {
    if (x < 0 || y < 0 || x >= (s32)CFG_FramebufferWidth || y >= (s32)CFG_FramebufferHeight || g_currentFramebuffer == NULL) return;
    u32 offset = getPixelOffset(x, y);
    ((u16*)g_currentFramebuffer)[offset] = color_to_u16(color);
}

static inline void setPixelBlendDst(s32 x, s32 y, Color color) {
    if (x < 0 || y < 0 || x >= (s32)CFG_FramebufferWidth || y >= (s32)CFG_FramebufferHeight || g_currentFramebuffer == NULL) return;
    u32 offset = getPixelOffset(x, y);
    Color src = color_from_u16(((u16*)g_currentFramebuffer)[offset]);
    Color dst = color;
    Color out = {0,0,0,0};
    out.r = blendColor(src.r, dst.r, dst.a);
    out.g = blendColor(src.g, dst.g, dst.a);
    out.b = blendColor(src.b, dst.b, dst.a);
    // alpha 叠加并限制到 0xF
    u16 sumA = (u16)dst.a + (u16)src.a;
    out.a = (sumA > 0xF) ? 0xF : (u8)sumA;
    setPixel(x, y, out);
}

static inline void drawRect(s32 x, s32 y, s32 w, s32 h, Color color) {
    s32 x2 = x + w;
    s32 y2 = y + h;
    if (x2 < 0 || y2 < 0) return;
    if (x >= (s32)CFG_FramebufferWidth || y >= (s32)CFG_FramebufferHeight) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (s32)CFG_FramebufferWidth) x2 = CFG_FramebufferWidth;
    if (y2 > (s32)CFG_FramebufferHeight) y2 = CFG_FramebufferHeight;
    for (s32 xi = x; xi < x2; ++xi) {
        for (s32 yi = y; yi < y2; ++yi) {
            setPixelBlendDst(xi, yi, color);
        }
    }
}

static inline void fillScreen(Color color) {
    drawRect(0, 0, CFG_FramebufferWidth, CFG_FramebufferHeight, color);
}

// 帧控制
static inline void startFrame(void) {
    g_currentFramebuffer = framebufferBegin(&g_framebuffer, NULL);
}

static inline void endFrame(void) {
    eventWait(&g_vsyncEvent, UINT64_MAX);
    framebufferEnd(&g_framebuffer);
    g_currentFramebuffer = NULL;
}

// 图形初始化与释放（移植 tesla Renderer::init/exit 的核心）
static Result gfx_init(void) {
    // 计算 Layer 尺寸，保持纵向填满屏幕并水平居中
    CFG_LayerHeight = SCREEN_HEIGHT;
    CFG_LayerWidth  = (u16)(SCREEN_HEIGHT * ((float)CFG_FramebufferWidth / (float)CFG_FramebufferHeight));
    CFG_LayerPosX = (u16)((SCREEN_WIDTH - CFG_LayerWidth) / 2);
    CFG_LayerPosY = (u16)((SCREEN_HEIGHT - CFG_LayerHeight) / 2); // 等于 0

    log_info("viInitialize(ViServiceType_Manager)");
    Result rc = viInitialize(ViServiceType_Manager);
    if (R_FAILED(rc)) return rc;

    log_info("viOpenDefaultDisplay...");
    rc = viOpenDefaultDisplay(&g_display);
    if (R_FAILED(rc)) return rc;

    log_info("viGetDisplayVsyncEvent...");
    rc = viGetDisplayVsyncEvent(&g_display, &g_vsyncEvent);
    if (R_FAILED(rc)) return rc;

    // 确保显示全局 Alpha 为不透明
    log_info("viSetDisplayAlpha(1.0f)...");
    viSetDisplayAlpha(&g_display, 1.0f);

    log_info("viCreateManagedLayer...");
    rc = viCreateManagedLayer(&g_display, (ViLayerFlags)0, 0, &__nx_vi_layer_id);
    if (R_FAILED(rc)) return rc;

    log_info("viCreateLayer...");
    rc = viCreateLayer(&g_display, &g_layer);
    if (R_FAILED(rc)) return rc;

    log_info("viSetLayerScalingMode(FitToLayer)...");
    rc = viSetLayerScalingMode(&g_layer, ViScalingMode_FitToLayer);
    if (R_FAILED(rc)) return rc;

    s32 layerZ = 250;
    log_info("viSetLayerZ(%d)...", layerZ);
    rc = viSetLayerZ(&g_layer, layerZ);
    if (R_FAILED(rc)) return rc;

    // 添加到图层栈
    log_info("viAddToLayerStack(Default and Screenshot)...");
    rc = viAddToLayerStack(&g_layer, ViLayerStack_Default);
    if (R_FAILED(rc)) return rc;
    rc = viAddToLayerStack(&g_layer, ViLayerStack_Screenshot);
    if (R_FAILED(rc)) return rc;

    log_info("viSetLayerSize(%u,%u)...", CFG_LayerWidth, CFG_LayerHeight);
    rc = viSetLayerSize(&g_layer, CFG_LayerWidth, CFG_LayerHeight);
    if (R_FAILED(rc)) return rc;
    log_info("viSetLayerPosition(%u,%u) 屏幕居中", CFG_LayerPosX, CFG_LayerPosY);
    rc = viSetLayerPosition(&g_layer, CFG_LayerPosX, CFG_LayerPosY);
    if (R_FAILED(rc)) return rc;

    log_info("nwindowCreateFromLayer...");
    rc = nwindowCreateFromLayer(&g_window, &g_layer);
    if (R_FAILED(rc)) return rc;

    log_info("framebufferCreate(%u,%u,RGBA_4444,2)...", CFG_FramebufferWidth, CFG_FramebufferHeight);
    rc = framebufferCreate(&g_framebuffer, &g_window, CFG_FramebufferWidth, CFG_FramebufferHeight, PIXEL_FORMAT_RGBA_4444, 2);
    if (R_FAILED(rc)) return rc;

    g_gfxInitialized = true;
    log_info("gfx_init 完成");
    return 0;
}

static void gfx_exit(void) {
    if (!g_gfxInitialized) return;
    
    log_info("开始清理图形资源...");
    
    // 首先清理字体资源
    fonts_exit();
    
    // 清理图形相关资源
    framebufferClose(&g_framebuffer);
    nwindowClose(&g_window);
    
    // 安全清理VI资源，避免与Status-Monitor-Overlay退出冲突
    log_info("安全清理VI资源...");
    
    // 检查VI服务是否仍然可用
    Result rc = 0;
    
    // 尝试销毁Managed Layer
    rc = viDestroyManagedLayer(&g_layer);
    if (R_FAILED(rc)) {
        log_info("viDestroyManagedLayer失败 (可能已被其他程序清理): 0x%x", rc);
    }
    
    // 尝试关闭Display
    rc = viCloseDisplay(&g_display);
    if (R_FAILED(rc)) {
        log_info("viCloseDisplay失败 (可能已被其他程序清理): 0x%x", rc);
    }
    
    eventClose(&g_vsyncEvent);
    
    // 最后尝试退出VI服务
    // 如果其他程序（如Status-Monitor-Overlay）已经调用了viExit()，
    // 这里的调用可能会失败，但不会导致程序崩溃
    viExit();
    
    g_gfxInitialized = false;
    
    log_info("图形资源清理完成");
}

// UTF-8 解析为 Unicode 码点（简易实现）
static inline const char* utf8_next(const char *s, u32 *out_cp) {
    const unsigned char *us = (const unsigned char*)s;
    if (!*us) { *out_cp = 0; return s; }
    if (us[0] < 0x80) { *out_cp = us[0]; return s + 1; }
    if ((us[0] & 0xE0) == 0xC0) { *out_cp = ((us[0] & 0x1F) << 6) | (us[1] & 0x3F); return s + 2; }
    if ((us[0] & 0xF0) == 0xE0) { *out_cp = ((us[0] & 0x0F) << 12) | ((us[1] & 0x3F) << 6) | (us[2] & 0x3F); return s + 3; }
    if ((us[0] & 0xF8) == 0xF0) { *out_cp = ((us[0] & 0x07) << 18) | ((us[1] & 0x3F) << 12) | ((us[2] & 0x3F) << 6) | (us[3] & 0x3F); return s + 4; }
    // 非法字节，跳过
    *out_cp = us[0];
    return s + 1;
}

// 将共享字体初始化为 STB 字体对象
static Result fonts_init(void) {
    PlFontData stdFontData, extFontData, localFontData;

    Result rc = plGetSharedFontByType(&stdFontData, PlSharedFontType_Standard);
    if (R_FAILED(rc)) { log_error("plGetSharedFontByType(Standard) 失败: 0x%x", rc); return rc; }
    u8 *fontBuffer = (u8*)stdFontData.address;
    stbtt_InitFont(&g_font_std, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

    // 根据系统语言尝试加载本地化字体（中文/韩文/繁体等）
    u64 languageCode;
    if (R_SUCCEEDED(setGetSystemLanguage(&languageCode))) {
        SetLanguage setLanguage;
        if (R_SUCCEEDED(setMakeLanguage(languageCode, &setLanguage))) {
            g_has_local_font = true;
            Result rcLoc = 0;
            switch (setLanguage) {
            case SetLanguage_ZHCN:
            case SetLanguage_ZHHANS:
                rcLoc = plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseSimplified);
                break;
            case SetLanguage_ZHTW:
            case SetLanguage_ZHHANT:
                rcLoc = plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseTraditional);
                break;
            case SetLanguage_KO:
                rcLoc = plGetSharedFontByType(&localFontData, PlSharedFontType_KO);
                break;
            default:
                g_has_local_font = false;
                break;
            }
            if (g_has_local_font && R_SUCCEEDED(rcLoc)) {
                fontBuffer = (u8*)localFontData.address;
                stbtt_InitFont(&g_font_local, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
            } else {
                g_has_local_font = false;
            }
        }
    }

    rc = plGetSharedFontByType(&extFontData, PlSharedFontType_NintendoExt);
    if (R_FAILED(rc)) { log_error("plGetSharedFontByType(NintendoExt) 失败: 0x%x", rc); return rc; }
    fontBuffer = (u8*)extFontData.address;
    stbtt_InitFont(&g_font_ext, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

    g_fonts_initialized = true;
    log_info("字体初始化完成（标准%s，本地化%s，扩展已加载）",
             "已加载",
             g_has_local_font ? "已加载" : "未加载");
    return 0;
}

// 字体资源清理函数
static void fonts_exit(void) {
    if (!g_fonts_initialized) return;
    
    log_info("清理字体资源...");
    
    // 重置字体状态标志
    g_fonts_initialized = false;
    g_has_local_font = false;
    
    // 注意：STB TrueType 使用的是系统共享字体缓冲区，
    // 这些缓冲区由 pl 服务管理，不需要手动释放
    // 只需要重置 fontinfo 结构体即可
    memset(&g_font_std, 0, sizeof(stbtt_fontinfo));
    memset(&g_font_local, 0, sizeof(stbtt_fontinfo));
    memset(&g_font_ext, 0, sizeof(stbtt_fontinfo));
    
    log_info("字体资源清理完成");
}

// 使用指定字体在帧缓冲上渲染 UTF-8 字符串（旧实现，现由回退版本替代）
static __attribute__((unused)) void draw_string_stb(stbtt_fontinfo *font, const char *text, float pixel_height, s32 x, s32 y, Color color) {
    if (!font || !text || !g_currentFramebuffer) return;
    int ascent, descent, lineGap;
    float scale = stbtt_ScaleForPixelHeight(font, pixel_height);
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    float baseline = (float)y + scale * (float)ascent;

    float xpos = (float)x;
    u32 prev_cp = 0;
    const char *p = text;
    while (*p) {
        u32 cp = 0; p = utf8_next(p, &cp);
        if (cp == 0) break;

        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(font, (int)cp, &advanceWidth, &leftSideBearing);
        int kern = prev_cp ? stbtt_GetCodepointKernAdvance(font, (int)prev_cp, (int)cp) : 0;

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBoxSubpixel(font, (int)cp, scale, scale, 0.0f, 0.0f, &x0, &y0, &x1, &y1);
        int gw = x1 - x0;
        int gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            u8 *bitmap = (u8*)malloc((size_t)gw * (size_t)gh);
            if (bitmap) {
                memset(bitmap, 0, (size_t)gw * (size_t)gh);
                stbtt_MakeCodepointBitmapSubpixel(font, bitmap, gw, gh, gw, scale, scale, 0.0f, 0.0f, (int)cp);
                // 将灰度位图写入 RGBA4444 帧缓冲，使用目标混合
                for (int by = 0; by < gh; ++by) {
                    for (int bx = 0; bx < gw; ++bx) {
                        u8 cov = bitmap[by * gw + bx];
                        if (cov) {
                            u8 a4 = (u8)((cov * 15) / 255); // 0..255 -> 0..15
                            Color out = { color.r, color.g, color.b, a4 };
                            s32 px = (s32)(xpos + x0 + bx);
                            s32 py = (s32)(baseline + y0 + by);
                            setPixelBlendDst(px, py, out);
                        }
                    }
                }
                free(bitmap);
            }
        }

        xpos += scale * (float)advanceWidth;
        xpos += scale * (float)kern;
        prev_cp = cp;
    }
}

// 按字符选择字体：优先扩展图标，其次本地化（当标准缺失时），最后标准
static inline stbtt_fontinfo* pick_font_for_codepoint(u32 cp) {
    if (stbtt_FindGlyphIndex(&g_font_ext, (int)cp))
        return &g_font_ext;
    if (g_has_local_font && stbtt_FindGlyphIndex(&g_font_std, (int)cp) == 0 && stbtt_FindGlyphIndex(&g_font_local, (int)cp))
        return &g_font_local;
    return &g_font_std;
}

// 支持多字体回退的字符串绘制
static void draw_string_stb_fallback(const char *text, float pixel_height, s32 x, s32 y, Color color) {
    if (!text || !g_currentFramebuffer) return;

    int ascent_std, descent_std, lineGap_std;
    float scale_std = stbtt_ScaleForPixelHeight(&g_font_std, pixel_height);
    stbtt_GetFontVMetrics(&g_font_std, &ascent_std, &descent_std, &lineGap_std);
    float baseline = (float)y + scale_std * (float)ascent_std;

    float xpos = (float)x;
    u32 prev_cp = 0;
    const char *p = text;
    while (*p) {
        u32 cp = 0; p = utf8_next(p, &cp);
        if (cp == 0) break;

        stbtt_fontinfo *font = pick_font_for_codepoint(cp);
        float scale = stbtt_ScaleForPixelHeight(font, pixel_height);

        int advanceWidth = 0, leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(font, (int)cp, &advanceWidth, &leftSideBearing);
        int kern = prev_cp ? stbtt_GetCodepointKernAdvance(font, (int)prev_cp, (int)cp) : 0;

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBoxSubpixel(font, (int)cp, scale, scale, 0.0f, 0.0f, &x0, &y0, &x1, &y1);
        int gw = x1 - x0;
        int gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            u8 *bitmap = (u8*)malloc((size_t)gw * (size_t)gh);
            if (bitmap) {
                memset(bitmap, 0, (size_t)gw * (size_t)gh);
                stbtt_MakeCodepointBitmapSubpixel(font, bitmap, gw, gh, gw, scale, scale, 0.0f, 0.0f, (int)cp);
                for (int by = 0; by < gh; ++by) {
                    for (int bx = 0; bx < gw; ++bx) {
                        u8 cov = bitmap[by * gw + bx];
                        if (cov) {
                            u8 a4 = (u8)((cov * 15) / 255);
                            Color out = { color.r, color.g, color.b, a4 };
                            s32 px = (s32)(xpos + x0 + bx);
                            s32 py = (s32)(baseline + y0 + by);
                            setPixelBlendDst(px, py, out);
                        }
                    }
                }
                free(bitmap);
            }
        }

        xpos += scale * (float)advanceWidth;
        xpos += scale * (float)kern;
        prev_cp = cp;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

// 后台程序：不使用 Applet 环境
u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

// 配置 newlib 堆（使 malloc/free 可用）
void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void *fake_heap_start;
    extern void *fake_heap_end;
    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

// 必要服务初始化（最小化）
void __appInit(void)
{
    log_info("应用程序初始化开始...");
    
    Result rc = 0;
    
    // 基础服务初始化
    rc = smInitialize();
    if (R_FAILED(rc)) {
        log_error("smInitialize失败: 0x%x", rc);
        fatalThrow(rc);
    }
    
    rc = fsInitialize();
    if (R_FAILED(rc)) {
        log_error("fsInitialize失败: 0x%x", rc);
        fatalThrow(rc);
    }
    
    fsdevMountSdmc();
    
    // 其他服务初始化
    rc = hidInitialize();
    if (R_FAILED(rc)) {
        log_error("hidInitialize失败: 0x%x", rc);
        fatalThrow(rc);
    }
    
    rc = plInitialize(PlServiceType_User);
    if (R_FAILED(rc)) {
        log_error("plInitialize失败: 0x%x", rc);
        fatalThrow(rc);
    }
    
    rc = setInitialize();
    if (R_FAILED(rc)) {
        log_error("setInitialize失败: 0x%x", rc);
        fatalThrow(rc);
    }
    
    log_info("应用程序初始化完成");
}

// 服务释放
void __appExit(void)
{
    log_info("应用程序退出开始...");
    
    // 优先清理图形资源，避免与其他叠加层冲突
    gfx_exit();
    
    // 清理字体资源
    fonts_exit();
    
    // 清理其他服务
    plExit();
    setExit();
    hidExit();
    
    // 最后清理基础服务
    fsdevUnmountAll();
    fsExit();
    smExit();
    
    log_info("应用程序退出完成");
}

#ifdef __cplusplus
}
#endif

// 主入口：初始化绘制并执行一次演示帧，然后进入后台循环
int main(int argc, char *argv[])
{
    log_info("后台程序启动（移植 tesla 绘制逻辑）");

    Result rc = gfx_init();
    if (R_SUCCEEDED(rc)) {
        // 字体初始化
        if (R_SUCCEEDED(fonts_init())) {
            log_info("共享字体加载成功，开始文本绘制示例...");
        } else {
            log_error("共享字体加载失败，跳过文本示例");
        }
        log_info("开始首帧绘制：framebufferBegin...");
        // 示例：绘制一次半透明面板与边框
        startFrame();
        fillScreen((Color){0,0,0,0});
        s32 px = (s32)(CFG_FramebufferWidth * 0.5f) - 100;
        s32 py = (s32)(CFG_FramebufferHeight * 0.5f) - 50;
        // 修复弹窗透明：将面板 Alpha 设为 0xF（不透明）
        drawRect(px, py, 200, 100, (Color){0,0,0,15});
        // 边框
        drawRect(px, py, 200, 2, (Color){15,15,15,15});
        drawRect(px, py+98, 200, 2, (Color){15,15,15,15});
        drawRect(px, py, 2, 100, (Color){15,15,15,15});
        drawRect(px+198, py, 2, 100, (Color){15,15,15,15});
        // 文本：优先使用本地化字体，否则标准字体
        if (g_fonts_initialized) {
            const char *msg1 = "LibTesla 字体渲染";
            const char *msg2 = "STB TrueType";
            draw_string_stb_fallback(msg1, 28.0f, px + 10, py + 12, (Color){15,15,15,15});
            draw_string_stb_fallback(msg2, 24.0f, px + 10, py + 50, (Color){15,15,15,15});
        }
        log_info("提交首帧：framebufferEnd...");
        endFrame();

        // 停留 2 秒后清屏
        svcSleepThread(2000000000ULL);
        log_info("清屏并提交一帧...");
        startFrame();
        fillScreen((Color){0,0,0,0});
        endFrame();

        // 弹窗结束后立刻释放图层与 VI 资源，避免占用最前景导致 Ultrahand 无法显示
        log_info("弹窗结束，释放图层/窗口/帧缓冲与 VI 服务...");
        gfx_exit();
        
        // 终止指定程序
        const uint64_t titleID = 0x0100000000251020;
        pmshellInitialize();
        pmshellTerminateProgram(titleID);
    } else {
        log_error("图形初始化失败: 0x%x", rc);
    }

    // 后台循环（此处保持为日志输出与休眠）
    u32 loop_count = 0;
    while (true) {
        loop_count++;
        log_info("后台运行中 - 循环 %u", loop_count);
        svcSleepThread(30000000000ULL); // 30 秒
    }

    // 注意：由于上面是无限循环，这里的代码永远不会执行
    // 但保留作为安全措施，以防将来修改循环逻辑
    log_info("程序即将退出，执行最终清理...");
    gfx_exit();
    return 0;
}