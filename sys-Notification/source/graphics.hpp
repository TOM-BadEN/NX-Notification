#pragma once

#include <switch.h>

// RGBA4444 颜色结构（每个分量 0-15）
struct Color {
    u8 r, g, b, a;
};

// 图形渲染器：封装所有底层绘制操作
class GraphicsRenderer {
public:
    GraphicsRenderer();
    ~GraphicsRenderer();
    
    // 绑定到 Framebuffer（在 VI/Layer 初始化后调用）
    void Bind(Framebuffer* fb, Event* vsyncEvent, u16 width, u16 height);
    
    // 帧管理
    void StartFrame();
    void EndFrame();
    
    // 圆角矩形的部分区域
    enum class RoundedRectPart {
        ALL,     // 全部（默认）
        TOP,     // 只有顶部（包含上圆角）
        BOTTOM   // 只有底部（包含下圆角）
    };
    
    // 绘制原语
    void SetPixel(s32 x, s32 y, Color color);
    void SetPixelBlend(s32 x, s32 y, Color color);
    void DrawRect(s32 x, s32 y, s32 w, s32 h, Color color);
    void DrawRoundedRect(s32 x, s32 y, s32 w, s32 h, s32 radius, Color color);  // 圆角矩形
    void DrawRoundedRectPartial(s32 x, s32 y, s32 w, s32 h, s32 radius, Color color, RoundedRectPart part);  // 部分圆角矩形
    void FillScreen(Color color);
    
    // 文本水平对齐方式
    enum class TextAlign {
        LEFT,    // 左对齐
        CENTER,  // 居中（默认）
        RIGHT    // 右对齐
    };
    
    // 文本渲染（在矩形区域内，垂直默认居中，水平可选对齐方式）
    // x, y: 矩形左上角坐标
    // w, h: 矩形宽度和高度
    // align: 水平对齐方式（默认居中）
    void DrawText(const char* text, s32 x, s32 y, s32 w, s32 h, float fontSize, Color color, TextAlign align = TextAlign::CENTER);
    
    // 文本测量
    float MeasureTextWidth(const char* text, float fontSize);
    
    // 裁剪区域（Scissoring）
    void EnableScissoring(s32 x, s32 y, s32 w, s32 h);
    void DisableScissoring();
    
    // 颜色工具（静态，可以独立使用）
    static inline u16 ColorToU16(Color c);
    static inline Color ColorFromU16(u16 raw);
    
private:
    // 绑定的资源（不拥有，只使用）
    Framebuffer* m_Framebuffer;
    Event* m_VsyncEvent;
    void* m_CurrentFramebuffer;
    u16 m_Width;
    u16 m_Height;
    
    // 裁剪区域状态
    bool m_ScissorEnabled;
    s32 m_ScissorX, m_ScissorY, m_ScissorW, m_ScissorH;
    
    // 块线性地址计算
    u32 GetPixelOffset(s32 x, s32 y);
    
    // 检查坐标是否在裁剪区域内
    inline bool IsInScissor(s32 x, s32 y) const {
        if (!m_ScissorEnabled) return true;
        return x >= m_ScissorX && x < m_ScissorX + m_ScissorW &&
               y >= m_ScissorY && y < m_ScissorY + m_ScissorH;
    }
    
    // 颜色混合
    static inline u8 BlendColor(u8 src, u8 dst, u8 alpha);
    
    // UTF-8 解码
    static const char* Utf8Next(const char* s, u32* out_cp);
};

