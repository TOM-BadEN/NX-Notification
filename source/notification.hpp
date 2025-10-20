#pragma once

#include <switch.h>
#include "graphics.hpp"

// 通知位置枚举
enum NotificationPosition {
    LEFT = 0,    // 左对齐
    MIDDLE = 1,  // 居中
    RIGHT = 2    // 右对齐
};

// 通知管理器：负责通知弹窗的显示和管理
class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager();
    
    // 初始化图形资源，返回 Result 以便优雅处理失败
    Result Init();
    
    // 显示通知弹窗
    // position: LEFT=左对齐, MIDDLE=居中, RIGHT=右对齐
    void Show(const char* text, NotificationPosition position = RIGHT);
    
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
    u16 m_FramebufferWidth;           // 帧缓冲宽度 (400)
    u16 m_FramebufferHeight;          // 帧缓冲高度 (130)
    u16 m_LayerWidth;                 // 图层宽度 (400)
    u16 m_LayerHeight;                // 图层高度 (130)
    u16 m_LayerPosX;                  // 图层 X 位置（动态）
    u16 m_LayerPosY;                  // 图层 Y 位置 (50)
    
    // 状态标志
    bool m_Initialized;               // 是否已初始化
    bool m_IsVisible;                 // 是否可见
    
    // 将图层添加到显示栈
    static Result ViAddToLayerStack(ViLayer* layer, ViLayerStack stack);
    
    // 恢复系统输入焦点（模拟触屏点击）
    void RestoreSystemInput();
    
    // 绘制通知内容（不包含动画）
    void DrawNotificationContent(s32 drawX, s32 drawY, const char* iconStr, const char* displayText);
    
    // 动画函数
    void AnimateFromLeft(s32 targetX, s32 targetY, const char* iconStr, const char* displayText);   // 左边滑入
    void AnimateFromRight(s32 targetX, s32 targetY, const char* iconStr, const char* displayText);  // 右边滑入
    void AnimateExpand(s32 targetX, s32 targetY, const char* iconStr, const char* displayText);     // 中间展开
    
    // 缓动函数
    static float EaseOutCubic(float t);
};
