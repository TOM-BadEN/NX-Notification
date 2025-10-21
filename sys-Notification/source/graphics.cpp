#include "graphics.hpp"
#include "font_manager.hpp"

// 构造函数：轻量级初始化
GraphicsRenderer::GraphicsRenderer() 
    : m_Framebuffer(nullptr)
    , m_VsyncEvent(nullptr)
    , m_CurrentFramebuffer(nullptr)
    , m_Width(0)
    , m_Height(0)
    , m_ScissorEnabled(false)
    , m_ScissorX(0)
    , m_ScissorY(0)
    , m_ScissorW(0)
    , m_ScissorH(0)
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
    if (!IsInScissor(x, y)) return;  // 裁剪检查
    u32 offset = GetPixelOffset(x, y);
    ((u16*)m_CurrentFramebuffer)[offset] = ColorToU16(color);
}

// 设置像素（与目标混合）（透明实现）
void GraphicsRenderer::SetPixelBlend(s32 x, s32 y, Color color) {
    if (x < 0 || y < 0 || x >= (s32)m_Width || y >= (s32)m_Height || m_CurrentFramebuffer == nullptr) return;
    if (!IsInScissor(x, y)) return;  // 裁剪检查
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

// 绘制矩形（混合模式）
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

// 绘制圆角矩形
void GraphicsRenderer::DrawRoundedRect(s32 x, s32 y, s32 w, s32 h, s32 radius, Color color) {
    DrawRoundedRectPartial(x, y, w, h, radius, color, RoundedRectPart::ALL);
}

// 绘制部分圆角矩形（只有顶部或底部）
void GraphicsRenderer::DrawRoundedRectPartial(s32 x, s32 y, s32 w, s32 h, s32 radius, Color color, RoundedRectPart part) {
    // 先绘制主体矩形
    DrawRect(x, y, w, h, color);
    
    // 根据 part 决定处理哪些角
    bool processTopLeft = (part == RoundedRectPart::ALL || part == RoundedRectPart::TOP);
    bool processTopRight = (part == RoundedRectPart::ALL || part == RoundedRectPart::TOP);
    bool processBottomLeft = (part == RoundedRectPart::ALL || part == RoundedRectPart::BOTTOM);
    bool processBottomRight = (part == RoundedRectPart::ALL || part == RoundedRectPart::BOTTOM);
    
    // 在四个角绘制透明遮罩，实现圆角效果
    for (s32 cy = 0; cy < radius; cy++) {
        for (s32 cx = 0; cx < radius; cx++) {
            float radiusSq = radius * radius;
            
            // 左上角
            if (processTopLeft) {
                float dx_tl = cx - radius + 0.5f;
                float dy_tl = cy - radius + 0.5f;
                float dist_tl = dx_tl * dx_tl + dy_tl * dy_tl;
                if (dist_tl > radiusSq)
                    SetPixel(x + cx, y + cy, {0, 0, 0, 0});
            }
            
            // 右上角
            if (processTopRight) {
                float dx_tr = cx + 0.5f;
                float dy_tr = cy - radius + 0.5f;
                float dist_tr = dx_tr * dx_tr + dy_tr * dy_tr;
                if (dist_tr > radiusSq)
                    SetPixel(x + w - radius + cx, y + cy, {0, 0, 0, 0});
            }
            
            // 左下角
            if (processBottomLeft) {
                float dx_bl = cx - radius + 0.5f;
                float dy_bl = cy + 0.5f;
                float dist_bl = dx_bl * dx_bl + dy_bl * dy_bl;
                if (dist_bl > radiusSq)
                    SetPixel(x + cx, y + h - radius + cy, {0, 0, 0, 0});
            }
            
            // 右下角
            if (processBottomRight) {
                float dx_br = cx + 0.5f;
                float dy_br = cy + 0.5f;
                float dist_br = dx_br * dx_br + dy_br * dy_br;
                if (dist_br > radiusSq)
                    SetPixel(x + w - radius + cx, y + h - radius + cy, {0, 0, 0, 0});
            }
        }
    }
}

// 填充整个屏幕
void GraphicsRenderer::FillScreen(Color color) {
    // 使用 SetPixel 逐像素填充（处理块线性布局）
    if (!m_CurrentFramebuffer) return;
    
    for (s32 y = 0; y < (s32)m_Height; y++) {
        for (s32 x = 0; x < (s32)m_Width; x++) {
            SetPixel(x, y, color);
        }
    }
}

// 启用裁剪区域
void GraphicsRenderer::EnableScissoring(s32 x, s32 y, s32 w, s32 h) {
    m_ScissorEnabled = true;
    m_ScissorX = x;
    m_ScissorY = y;
    m_ScissorW = w;
    m_ScissorH = h;
}

// 禁用裁剪区域
void GraphicsRenderer::DisableScissoring() {
    m_ScissorEnabled = false;
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

// 文本渲染：在矩形区域内，垂直居中，水平可选对齐
void GraphicsRenderer::DrawText(const char* text, s32 x, s32 y, s32 w, s32 h, float fontSize, Color color, TextAlign align) {
    if (!text || !m_CurrentFramebuffer) return;
    
    FontManager& fontMgr = FontManager::Instance();
    stbtt_fontinfo* font = fontMgr.GetStdFont();
    
    // 测量文本宽度
    float textWidth = MeasureTextWidth(text, fontSize);
    
    // 根据对齐方式计算水平起始位置
    s32 startX;
    switch (align) {
        case TextAlign::LEFT:
            startX = x;  // 左对齐：从左边界开始
            break;
        case TextAlign::RIGHT:
            startX = x + w - (s32)textWidth;  // 右对齐：从右边界减去文本宽度
            break;
        case TextAlign::CENTER:
        default:
            startX = x + (w - (s32)textWidth) / 2;  // 居中：中间位置
            break;
    }
    
    // 计算垂直居中位置（考虑字体的实际度量）
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);
    
    // 使用与 RenderGlyph 相同的缩放方式
    float scale = fontMgr.CalculateScaleForVisibleHeight(font, fontSize);
    
    // 实际的 ascent 和 descent（像素，descent 是负值）
    float actualAscent = ascent * scale;
    float actualDescent = descent * scale;  // 保持负值
    
    // 字体的视觉中心距离基线的偏移（向上为正）
    float visualCenterOffset = (actualAscent + actualDescent) / 2.0f;
    
    // 基线位置 = 面板中心 + 视觉中心偏移
    s32 startY = y + h / 2 + (s32)visualCenterOffset;
    
    s32 cursorX = startX;
    s32 cursorY = startY;
    
    // 逐字符解析和渲染
    while (*text) {
        u32 codepoint;
        text = Utf8Next(text, &codepoint);
        if (codepoint == 0) break;
        
        // 使用 FontManager 渲染字形
        auto glyph = fontMgr.RenderGlyph(codepoint, fontSize);
        
        // 如果有位图数据，绘制字形
        if (glyph.data) {
            // 绘制位图到屏幕（带抗锯齿 + 边界裁剪）
            for (int by = 0; by < glyph.height; by++) {
                for (int bx = 0; bx < glyph.width; bx++) {
                    // 计算像素位置
                    s32 px = cursorX + bx + glyph.xoffset;
                    s32 py = cursorY + by + glyph.yoffset;
                    
                    // 边界裁剪：超出矩形区域的像素不渲染
                    if (px < x || px >= x + w || py < y || py >= y + h) {
                        continue;
                    }
                    
                    // 获取灰度值（0-255）
                    u8 coverage = glyph.data[by * glyph.width + bx];
                    if (coverage == 0) continue;  // 完全透明，跳过
                    
                    // 转换为 RGBA4444 的 alpha（0-15）
                    u8 alpha = coverage / 17;  // 255 / 15 ≈ 17
                    
                    // 创建带抗锯齿的颜色
                    Color textColor = color;
                    textColor.a = (alpha * color.a) / 15;  // 混合原始透明度
                    
                    // 绘制像素
                    SetPixelBlend(px, py, textColor);
                }
            }
            
            // 释放字形位图
            fontMgr.FreeGlyph(glyph);
        }
        
        // 移动光标到下一个字符位置（无论是否有位图数据，都要前进）
        // 空格字符不添加额外间距（空格本身就是间距）
        s32 spacing = (codepoint == ' ') ? 0 : (s32)(3.0f);
        cursorX += glyph.advance + spacing; 
    }
}

// 测量文本宽度
float GraphicsRenderer::MeasureTextWidth(const char* text, float fontSize) {
    if (!text) return 0.0f;
    
    float totalWidth = 0.0f;
    FontManager& fontMgr = FontManager::Instance();
    
    // 逐字符测量
    while (*text) {
        u32 codepoint;
        text = Utf8Next(text, &codepoint);
        if (codepoint == 0) break;
        
        // 获取字形信息
        auto glyph = fontMgr.RenderGlyph(codepoint, fontSize);
        
        // 无论是否有位图数据，都要累加 advance（空格也占宽度）
        // 空格字符不添加额外间距（空格本身就是间距）
        s32 spacing = (codepoint == ' ') ? 0 : (s32)(3.0f);
        totalWidth += glyph.advance + spacing;
        
        // 如果有位图数据，释放它
        if (glyph.data) {
            fontMgr.FreeGlyph(glyph);
        }
    }
    
    return totalWidth;
}

