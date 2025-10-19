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
    
    // 绘制原语
    void SetPixel(s32 x, s32 y, Color color);
    void SetPixelBlend(s32 x, s32 y, Color color);
    void DrawRect(s32 x, s32 y, s32 w, s32 h, Color color);
    void FillScreen(Color color);
    
    // 文本渲染
    void DrawText(const char* text, s32 x, s32 y, float fontSize, Color color);
    
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
    
    // 块线性地址计算
    u32 GetPixelOffset(s32 x, s32 y);
    
    // 颜色混合
    static inline u8 BlendColor(u8 src, u8 dst, u8 alpha);
    
    // UTF-8 解码
    static const char* Utf8Next(const char* s, u32* out_cp);
};

