#include "graphics.hpp"
#include "font_manager.hpp"

// 构造函数：轻量级初始化
GraphicsRenderer::GraphicsRenderer() 
    : m_Framebuffer(nullptr)
    , m_VsyncEvent(nullptr)
    , m_CurrentFramebuffer(nullptr)
    , m_Width(0)
    , m_Height(0)
{
    // 预初始化字体管理器（确保在绘制前字体已加载）
    FontManager::Instance();
}

// 析构函数：不拥有资源，无需清理
GraphicsRenderer::~GraphicsRenderer() {
}

// 绑定到 Framebuffer
void GraphicsRenderer::Bind(Framebuffer* fb, Event* vsyncEvent, u16 width, u16 height) {
    m_Framebuffer = fb;
    m_VsyncEvent = vsyncEvent;
    m_Width = width;
    m_Height = height;
}

// 开始绘制帧
void GraphicsRenderer::StartFrame() {
    if (m_Framebuffer) {
        m_CurrentFramebuffer = framebufferBegin(m_Framebuffer, nullptr);
    }
}

// 提交并显示帧
void GraphicsRenderer::EndFrame() {
    if (m_Framebuffer && m_VsyncEvent) {
        eventWait(m_VsyncEvent, UINT64_MAX);
        framebufferEnd(m_Framebuffer);
        m_CurrentFramebuffer = nullptr;
    }
}

// 将 Color 结构转换为 RGBA4444 格式（16位）
inline u16 GraphicsRenderer::ColorToU16(Color c) {
    return (u16)((c.r & 0xF) | ((c.g & 0xF) << 4) | ((c.b & 0xF) << 8) | ((c.a & 0xF) << 12));
}

// 将 RGBA4444 格式转换为 Color 结构
inline Color GraphicsRenderer::ColorFromU16(u16 raw) {
    Color c;
    c.r = (raw >> 0) & 0xF;
    c.g = (raw >> 4) & 0xF;
    c.b = (raw >> 8) & 0xF;
    c.a = (raw >> 12) & 0xF;
    return c;
}

// 颜色混合（Alpha 混合）
inline u8 GraphicsRenderer::BlendColor(u8 src, u8 dst, u8 alpha) {
    u8 oneMinusAlpha = 0x0F - alpha;
    return (u8)((dst * alpha + src * oneMinusAlpha) / (float)0xF);
}

// 将 x,y 坐标映射为块线性帧缓冲中的偏移
u32 GraphicsRenderer::GetPixelOffset(s32 x, s32 y) {
    u32 tmpPos = ((y & 127) / 16) + (x / 32 * 8) + ((y / 16 / 8) * (((m_Width / 2) / 16 * 8)));
    tmpPos *= 16 * 16 * 4;
    tmpPos += ((y % 16) / 8) * 512 + ((x % 32) / 16) * 256 + ((y % 8) / 2) * 64 + ((x % 16) / 8) * 32 + (y % 2) * 16 + (x % 8) * 2;
    return tmpPos / 2;
}

// 直接设置像素（不混合）
void GraphicsRenderer::SetPixel(s32 x, s32 y, Color color) {
    if (x < 0 || y < 0 || x >= (s32)m_Width || y >= (s32)m_Height || m_CurrentFramebuffer == nullptr) return;
    u32 offset = GetPixelOffset(x, y);
    ((u16*)m_CurrentFramebuffer)[offset] = ColorToU16(color);
}

// 设置像素（与目标混合）（透明实现）
void GraphicsRenderer::SetPixelBlend(s32 x, s32 y, Color color) {
    if (x < 0 || y < 0 || x >= (s32)m_Width || y >= (s32)m_Height || m_CurrentFramebuffer == nullptr) return;
    u32 offset = GetPixelOffset(x, y);
    Color src = ColorFromU16(((u16*)m_CurrentFramebuffer)[offset]);
    Color dst = color;
    Color out = {0, 0, 0, 0};
    out.r = BlendColor(src.r, dst.r, dst.a);
    out.g = BlendColor(src.g, dst.g, dst.a);
    out.b = BlendColor(src.b, dst.b, dst.a);
    // Alpha 叠加并限制到 0xF
    u16 sumA = (u16)dst.a + (u16)src.a;
    out.a = (sumA > 0xF) ? 0xF : (u8)sumA;
    SetPixel(x, y, out);
}

// 绘制矩形
void GraphicsRenderer::DrawRect(s32 x, s32 y, s32 w, s32 h, Color color) {
    s32 x2 = x + w;
    s32 y2 = y + h;
    if (x2 < 0 || y2 < 0) return;
    if (x >= (s32)m_Width || y >= (s32)m_Height) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > (s32)m_Width) x2 = m_Width;
    if (y2 > (s32)m_Height) y2 = m_Height;
    for (s32 xi = x; xi < x2; ++xi) {
        for (s32 yi = y; yi < y2; ++yi) {
            SetPixelBlend(xi, yi, color);
        }
    }
}

// 填充整个屏幕
void GraphicsRenderer::FillScreen(Color color) {
    DrawRect(0, 0, m_Width, m_Height, color);
}

// UTF-8 解码：将 UTF-8 字符串解析为 Unicode 码点
const char* GraphicsRenderer::Utf8Next(const char* s, u32* out_cp) {
    const unsigned char* us = (const unsigned char*)s;
    if (!*us) { *out_cp = 0; return s; }
    if (us[0] < 0x80) { *out_cp = us[0]; return s + 1; }
    if ((us[0] & 0xE0) == 0xC0) { *out_cp = ((us[0] & 0x1F) << 6) | (us[1] & 0x3F); return s + 2; }
    if ((us[0] & 0xF0) == 0xE0) { *out_cp = ((us[0] & 0x0F) << 12) | ((us[1] & 0x3F) << 6) | (us[2] & 0x3F); return s + 3; }
    if ((us[0] & 0xF8) == 0xF0) { *out_cp = ((us[0] & 0x07) << 18) | ((us[1] & 0x3F) << 12) | ((us[2] & 0x3F) << 6) | (us[3] & 0x3F); return s + 4; }
    *out_cp = us[0];
    return s + 1;
}

// 文本渲染：将文本绘制到屏幕
void GraphicsRenderer::DrawText(const char* text, s32 x, s32 y, float fontSize, Color color) {
    if (!text || !m_CurrentFramebuffer) return;
    
    s32 cursorX = x;
    s32 cursorY = y;
    
    FontManager& fontMgr = FontManager::Instance();
    
    // 逐字符解析和渲染
    while (*text) {
        u32 codepoint;
        text = Utf8Next(text, &codepoint);
        if (codepoint == 0) break;
        
        // 使用 FontManager 渲染字形
        auto glyph = fontMgr.RenderGlyph(codepoint, fontSize);
        if (!glyph.data) continue;
        
        // 绘制位图到屏幕（带抗锯齿）
        for (int by = 0; by < glyph.height; by++) {
            for (int bx = 0; bx < glyph.width; bx++) {
                // 获取灰度值（0-255）
                u8 coverage = glyph.data[by * glyph.width + bx];
                if (coverage == 0) continue;  // 完全透明，跳过
                
                // 转换为 RGBA4444 的 alpha（0-15）
                u8 alpha = coverage / 17;  // 255 / 15 ≈ 17
                
                // 创建带抗锯齿的颜色
                Color textColor = color;
                textColor.a = (alpha * color.a) / 15;  // 混合原始透明度
                
                // 绘制像素
                s32 px = cursorX + bx + glyph.xoffset;
                s32 py = cursorY + by + glyph.yoffset;
                SetPixelBlend(px, py, textColor);
            }
        }
        
        // 释放字形位图
        fontMgr.FreeGlyph(glyph);
        
        // 移动光标到下一个字符位置
        cursorX += glyph.advance;
    }
}

