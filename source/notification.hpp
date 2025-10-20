#pragma once

#include <switch.h>
#include "graphics.hpp"

// 通知管理器：负责通知弹窗的显示和管理
class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();
    
    // 初始化图形资源，返回 Result 以便优雅处理失败
    Result Init();
    
    // 显示通知弹窗
    void Show(const char* text);
    
    // 隐藏通知弹窗
    void Hide();
    
private:
    // 核心图形资源
    ViDisplay m_Display;              // VI 显示对象
    ViLayer m_Layer;                  // 图层对象
    Event m_VsyncEvent;               // 垂直同步事件
    NWindow m_Window;                 // 原生窗口
    Framebuffer m_Framebuffer;        // 帧缓冲
    
    // 图形渲染器
    GraphicsRenderer m_Renderer;
    
    // 配置参数
    u16 m_FramebufferWidth;           // 帧缓冲宽度 (448)
    u16 m_FramebufferHeight;          // 帧缓冲高度 (720)
    u16 m_LayerWidth;                 // 图层宽度（动态计算）
    u16 m_LayerHeight;                // 图层高度（动态计算）
    u16 m_LayerPosX;                  // 图层 X 位置（居中）
    u16 m_LayerPosY;                  // 图层 Y 位置
    
    // 状态标志
    bool m_Initialized;               // 是否已初始化
    bool m_IsVisible;                 // 是否可见
    
    // 将图层添加到显示栈
    static Result ViAddToLayerStack(ViLayer* layer, ViLayerStack stack);
    
    // 恢复系统输入焦点（模拟触屏点击）
    void RestoreSystemInput();
};
