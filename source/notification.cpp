#include "notification.hpp"
#include <cstring>

// 缩放比例（逻辑分辨率 / 物理分辨率 = 1920 / 1280 = 1.5）
#define SCALE 1.5f

// 面板实际显示尺寸（物理像素，屏幕上看到的大小）
#define PANEL_WIDTH  (400 * SCALE)   // 400 物理像素 = 600 逻辑像素
#define PANEL_HEIGHT (100 * SCALE)   // 100 物理像素 = 150 逻辑像素

// Framebuffer 尺寸（必须满足块线性布局要求：宽度是 32 的倍数）
#define FB_WIDTH  (((int)PANEL_WIDTH + 31) / 32 * 32)  // 自动向上取整到 32 的倍数
#define FB_HEIGHT ((int)PANEL_HEIGHT)

// 面板位置配置（物理像素）
#define PANEL_MARGIN_TOP  (50 * SCALE)   // 50 物理像素 = 75 逻辑像素
#define PANEL_MARGIN_SIDE (50 * SCALE)   // 50 物理像素 = 75 逻辑像素

// 屏幕尺寸常量（Layer 逻辑分辨率）
#define SCREEN_WIDTH  1920          // 物理分辨率宽度（物理 1280）
#define SCREEN_HEIGHT 1080          // 物理分辨率高度（物理 720）

#define PANEL_FONT_SIZE  (28 * SCALE)   // 字体大小（逻辑像素）

// libnx 内部全局变量：用于关联 ManagedLayer 和普通 Layer
extern "C" u64 __nx_vi_layer_id;

// 将图层添加到显示栈
Result NotificationManager::ViAddToLayerStack(ViLayer* layer, ViLayerStack stack) {
    const struct {
        u32 stack;
        u64 layerId;
    } in = { stack, layer->layer_id };
    return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
}

// 构造函数：轻量级初始化，不涉及系统服务
NotificationManager::NotificationManager() 
    : m_FramebufferWidth(FB_WIDTH)    // 使用宏定义（自动对齐到 32 的倍数）
    , m_FramebufferHeight(FB_HEIGHT)  // 使用宏定义
    , m_Initialized(false)
    , m_IsVisible(false)
{
}

// 析构函数：清理所有图形资源
NotificationManager::~NotificationManager() {
    if (!m_Initialized) return;
    
    framebufferClose(&m_Framebuffer);
    nwindowClose(&m_Window);
    viDestroyManagedLayer(&m_Layer);
    viCloseDisplay(&m_Display);
    eventClose(&m_VsyncEvent);
    viExit();
    
    m_Initialized = false;
}

// 初始化图形资源：失败时返回错误码并清理已分配资源
Result NotificationManager::Init() {
    if (m_Initialized) return 0;
    
    Result rc = 0;
    // 资源初始化状态标志（用于错误时的精确清理）
    bool viInited = false;          // VI 服务是否已初始化
    bool displayOpened = false;     // 显示器是否已打开
    bool vsyncGot = false;          // VSync 事件是否已获取
    bool layerCreated = false;      // 图层是否已创建
    bool windowCreated = false;     // 窗口是否已创建
    
    // 1. 设置图层尺寸和位置（使用宏定义）
    m_LayerWidth = FB_WIDTH;                         // 实际显示宽度
    m_LayerHeight = FB_HEIGHT;                       // 实际显示高度
    m_LayerPosX = (SCREEN_WIDTH - FB_WIDTH) / 2;    // 初始居中位置
    m_LayerPosY = PANEL_MARGIN_TOP;                     // 距屏幕顶部距离
    
    // 2. 初始化 VI 服务
    rc = viInitialize(ViServiceType_Manager);
    if (R_FAILED(rc)) goto cleanup;
    viInited = true;
    
    // 3. 打开默认显示
    rc = viOpenDefaultDisplay(&m_Display);
    if (R_FAILED(rc)) goto cleanup;
    displayOpened = true;
    
    // 4. 获取垂直同步事件（用于帧同步）
    rc = viGetDisplayVsyncEvent(&m_Display, &m_VsyncEvent);
    if (R_FAILED(rc)) goto cleanup;
    vsyncGot = true;
    
    // 5. 设置显示为不透明
    viSetDisplayAlpha(&m_Display, 1.0f);
    
    // 6. 创建托管图层（直接写入全局 Layer ID）
    rc = viCreateManagedLayer(&m_Display, (ViLayerFlags)0, 0, &__nx_vi_layer_id);
    if (R_FAILED(rc)) goto cleanup;
    
    // 7. 创建普通图层（关联到托管图层）
    rc = viCreateLayer(&m_Display, &m_Layer);
    if (R_FAILED(rc)) goto cleanup;
    layerCreated = true;
    
    // 8. 设置图层缩放模式
    rc = viSetLayerScalingMode(&m_Layer, ViScalingMode_FitToLayer);
    if (R_FAILED(rc)) goto cleanup;
    
    // 9. 设置图层深度（Z 值越大越靠前）
    rc = viSetLayerZ(&m_Layer, 250);
    if (R_FAILED(rc)) goto cleanup;
    
    // 10. 将图层添加到默认显示栈（必须在设置尺寸和位置之前）
    rc = ViAddToLayerStack(&m_Layer, ViLayerStack_Default);
    if (R_FAILED(rc)) goto cleanup;
    
    // 11. 将图层添加到截图栈
    rc = ViAddToLayerStack(&m_Layer, ViLayerStack_Screenshot);
    if (R_FAILED(rc)) goto cleanup;
    
    // 12. 设置图层尺寸
    rc = viSetLayerSize(&m_Layer, m_LayerWidth, m_LayerHeight);
    if (R_FAILED(rc)) goto cleanup;
    
    // 13. 设置图层位置
    rc = viSetLayerPosition(&m_Layer, m_LayerPosX, m_LayerPosY);
    if (R_FAILED(rc)) goto cleanup;
    
    // 14. 从图层创建原生窗口
    rc = nwindowCreateFromLayer(&m_Window, &m_Layer);
    if (R_FAILED(rc)) goto cleanup;
    windowCreated = true;
    
    // 15. 创建帧缓冲（RGBA4444 格式，双缓冲）
    rc = framebufferCreate(&m_Framebuffer, &m_Window, 
                          m_FramebufferWidth, m_FramebufferHeight, 
                          PIXEL_FORMAT_RGBA_4444, 2);
    if (R_FAILED(rc)) goto cleanup;
    
    // 16. 绑定图形渲染器
    m_Renderer.Bind(&m_Framebuffer, &m_VsyncEvent, 
                    m_FramebufferWidth, m_FramebufferHeight);
    
    // 17. 初始化完成
    m_Initialized = true;
    m_IsVisible = true;
    return 0;

cleanup:
    // 统一清理资源（按初始化的逆序）
    if (windowCreated) nwindowClose(&m_Window);
    if (layerCreated) viDestroyManagedLayer(&m_Layer);
    if (vsyncGot) eventClose(&m_VsyncEvent);
    if (displayOpened) viCloseDisplay(&m_Display);
    if (viInited) viExit();
    return rc;
}

// 恢复系统输入焦点（模拟触屏点击屏幕右上角）
void NotificationManager::RestoreSystemInput() {
    HidTouchState touch = {0};
    touch.x = 1280 - 50;  // 屏幕右上角 X（距离右边 50 像素）
    touch.y = 50;         // 屏幕右上角 Y（距离顶部 50 像素）
    touch.finger_id = 0;
    touch.diameter_x = 15;
    touch.diameter_y = 15;
    
    hiddbgSetTouchScreenAutoPilotState(&touch, 1);
    svcSleepThread(20000000ULL);  // 20ms
    hiddbgUnsetTouchScreenAutoPilotState();
}

// 绘制通知内容（不包含动画）
void NotificationManager::DrawNotificationContent(s32 drawX, s32 drawY, const char* iconStr, const char* displayText) {
    // 面板布局
    s32 panelW = PANEL_WIDTH;
    s32 panelH = PANEL_HEIGHT;
    
    // 背景（圆角矩形）
    s32 cornerRadius = (s32)(8 * SCALE);  
    m_Renderer.DrawRoundedRect(drawX, drawY, panelW, panelH, cornerRadius, {13, 13, 13, 15});
    
    // 顶部高光
    s32 highlightH = (s32)(4 * SCALE);
    m_Renderer.DrawRoundedRectPartial(drawX, drawY, panelW, highlightH, cornerRadius, 
                                       {15, 15, 15, 8}, GraphicsRenderer::RoundedRectPart::TOP);
    
    // 底部微阴影
    s32 shadowH = (s32)(4 * SCALE);
    s32 shadowY = drawY + panelH - shadowH;
    m_Renderer.DrawRoundedRectPartial(drawX, shadowY, panelW, shadowH, cornerRadius,
                                       {0, 0, 0, 2}, GraphicsRenderer::RoundedRectPart::BOTTOM);
    
    // 图标
    s32 iconX = drawX + (s32)(15 * SCALE);
    s32 iconW = (s32)(40 + 15 + 15) * SCALE;
    s32 iconSize = (s32)(40 * SCALE);
    m_Renderer.DrawText(iconStr, iconX, drawY, iconW, panelH, iconSize, {4, 4, 4, 15});
    
    // 文本
    s32 textX = iconX + iconW + (s32)(3 * SCALE);
    s32 textW = panelW - (textX - drawX) - (s32)(15 * SCALE);
    m_Renderer.DrawText(displayText, textX, drawY, textW, panelH, PANEL_FONT_SIZE, {5, 5, 5, 15}, GraphicsRenderer::TextAlign::LEFT);
}

// 显示通知弹窗
void NotificationManager::Show(const char* text, NotificationPosition position, NotificationType type) {
    if (!m_Initialized) return;
    // 代表弹窗显示中
    m_IsVisible = true;
    // 恢复系统输入焦点
    RestoreSystemInput();
    
    // 提前解析图标和文本
    char iconStr[4];
    if (type == INFO) strcpy(iconStr, "\uE137");       // 信息图标
    else if (type == ERROR) strcpy(iconStr, "\uE140"); // 错误图标
     
    const char* displayText = text;
    
    if (text && (u8)text[0] == 0xEE && ((u8)text[1] & 0xF0) == 0x80) {
        iconStr[0] = text[0];
        iconStr[1] = text[1];
        iconStr[2] = text[2];
        iconStr[3] = '\0';
        displayText = text + 3;
        while (*displayText == ' ') displayText++;
    }
    
    // 根据位置执行对应的动画
    s32 targetY = PANEL_MARGIN_TOP;
    
    switch (position) {
        case LEFT: {
            s32 targetX = PANEL_MARGIN_SIDE;
            AnimateFromLeft(targetX, targetY, iconStr, displayText);
            break;
        }
        case RIGHT: {
            s32 targetX = SCREEN_WIDTH - PANEL_WIDTH - PANEL_MARGIN_SIDE;
            AnimateFromRight(targetX, targetY, iconStr, displayText);
            break;
        }
        case MIDDLE: {
            s32 targetX = (SCREEN_WIDTH - PANEL_WIDTH) / 2;
            AnimateExpand(targetX, targetY, iconStr, displayText);
            break;
        }
        default: {
            s32 targetX = (SCREEN_WIDTH - PANEL_WIDTH) / 2;
            viSetLayerPosition(&m_Layer, targetX, targetY);
            break;
        }
    }
    
    
}

// 隐藏通知弹窗
void NotificationManager::Hide() {
    if (!m_Initialized) return;
    
    // 清空双缓冲（确保彻底清除）
    for (int i = 0; i < 2; i++) {
        m_Renderer.StartFrame();
        m_Renderer.FillScreen({0, 0, 0, 0});
        m_Renderer.EndFrame();
    }
    
    m_IsVisible = false;
}



// 缓动函数：快进慢出（EaseOutCubic）
float NotificationManager::EaseOutCubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}
// 左边滑入动画（特斯拉逐帧绘制方式）
void NotificationManager::AnimateFromLeft(s32 targetX, s32 targetY, const char* iconStr, const char* displayText) {
    const int ANIMATION_FRAMES = 15;
    const s32 SLIDE_DURATION_MS = 250;  // 250ms 滑入时间
    
    // Layer 固定在目标位置
    viSetLayerPosition(&m_Layer, targetX, targetY);
    eventWait(&m_VsyncEvent, UINT64_MAX);
    
    // 动画循环
    u64 startTime = armTicksToNs(armGetSystemTick());
    for (int i = 0; i <= ANIMATION_FRAMES; i++) {
        u64 now = armTicksToNs(armGetSystemTick());
        u64 elapsedMs = (now - startTime) / 1'000'000ULL;
        float t = (float)elapsedMs / SLIDE_DURATION_MS;
        if (t > 1.0f) t = 1.0f;
        
        float progress = EaseOutCubic(t);
        
        // 计算绘制坐标（从 -PANEL_WIDTH 滑到 0）
        s32 drawX = (s32)(-PANEL_WIDTH + progress * PANEL_WIDTH);
        
        // 计算裁剪区域
        s32 scissorX = (drawX < 0) ? 0 : drawX;
        s32 scissorW = (drawX < 0) ? (PANEL_WIDTH + drawX) : PANEL_WIDTH;
        if (scissorW <= 0) {
            svcSleepThread(16666667);  // 16.6ms
            continue;
        }
        
        // 每帧清空+绘制
        m_Renderer.StartFrame();
        m_Renderer.FillScreen({0, 0, 0, 0});
        m_Renderer.EnableScissoring(scissorX, 0, scissorW, PANEL_HEIGHT);
        DrawNotificationContent(drawX, 0, iconStr, displayText);
        m_Renderer.DisableScissoring();
        m_Renderer.EndFrame();
        
        if (t >= 1.0f) break;
        svcSleepThread(16666667);  // 16.6ms
    }
}

// 右边滑入动画（特斯拉逐帧绘制方式）
void NotificationManager::AnimateFromRight(s32 targetX, s32 targetY, const char* iconStr, const char* displayText) {
    const int ANIMATION_FRAMES = 15;
    const s32 SLIDE_DURATION_MS = 250;  // 250ms 滑入时间
    
    // Layer 固定在目标位置
    viSetLayerPosition(&m_Layer, targetX, targetY);
    eventWait(&m_VsyncEvent, UINT64_MAX);
    
    // 动画循环
    u64 startTime = armTicksToNs(armGetSystemTick());
    for (int i = 0; i <= ANIMATION_FRAMES; i++) {
        u64 now = armTicksToNs(armGetSystemTick());
        u64 elapsedMs = (now - startTime) / 1'000'000ULL;
        float t = (float)elapsedMs / SLIDE_DURATION_MS;
        if (t > 1.0f) t = 1.0f;
        
        float progress = EaseOutCubic(t);
        
        // 计算绘制坐标（从 PANEL_WIDTH 滑到 0）
        s32 drawX = (s32)(PANEL_WIDTH - progress * PANEL_WIDTH);
        
        // 计算裁剪区域
        s32 scissorX = drawX;
        s32 scissorW = PANEL_WIDTH - drawX;
        if (scissorW <= 0 || scissorX >= (s32)PANEL_WIDTH) {
            svcSleepThread(16666667);  // 16.6ms
            continue;
        }
        
        // 每帧清空+绘制
        m_Renderer.StartFrame();
        m_Renderer.FillScreen({0, 0, 0, 0});
        m_Renderer.EnableScissoring(scissorX, 0, scissorW, PANEL_HEIGHT);
        DrawNotificationContent(drawX, 0, iconStr, displayText);
        m_Renderer.DisableScissoring();
        m_Renderer.EndFrame();
        
        if (t >= 1.0f) break;
        svcSleepThread(16666667);  // 16.6ms
    }
}

// 中间展开动画（从中心向两边扩展）
void NotificationManager::AnimateExpand(s32 targetX, s32 targetY, const char* iconStr, const char* displayText) {
    const int ANIMATION_FRAMES = 15;
    const s32 EXPAND_DURATION_MS = 400;  // 400ms 展开时间
    
    // Layer 固定在目标位置
    viSetLayerPosition(&m_Layer, targetX, targetY);
    eventWait(&m_VsyncEvent, UINT64_MAX);
    
    // 动画循环
    u64 startTime = armTicksToNs(armGetSystemTick());
    for (int i = 0; i <= ANIMATION_FRAMES; i++) {
        u64 now = armTicksToNs(armGetSystemTick());
        u64 elapsedMs = (now - startTime) / 1'000'000ULL;
        float t = (float)elapsedMs / EXPAND_DURATION_MS;
        if (t > 1.0f) t = 1.0f;
        
        float progress = EaseOutCubic(t);
        
        // 计算当前宽度（从 0 扩展到 PANEL_WIDTH）
        s32 currentWidth = (s32)(progress * PANEL_WIDTH);
        if (currentWidth <= 0) {
            svcSleepThread(16666667);
            continue;
        }
        
        // 计算绘制坐标（从中心开始，向左扩展）
        s32 drawX = (PANEL_WIDTH - currentWidth) / 2;
        
        // 计算裁剪区域（从中心向两边扩展）
        s32 scissorX = drawX;
        s32 scissorW = currentWidth;
        
        // 每帧清空+绘制
        m_Renderer.StartFrame();
        m_Renderer.FillScreen({0, 0, 0, 0});
        m_Renderer.EnableScissoring(scissorX, 0, scissorW, PANEL_HEIGHT);
        DrawNotificationContent(0, 0, iconStr, displayText);  // 完整内容在 x=0 处
        m_Renderer.DisableScissoring();
        m_Renderer.EndFrame();
        
        if (t >= 1.0f) break;
        svcSleepThread(16666667);  // 16.6ms
    }
}

