#include "notification.hpp"

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

// 显示通知弹窗
void NotificationManager::Show(const char* text, NotificationPosition position) {
    if (!m_Initialized) return;

    // 恢复系统输入焦点
    RestoreSystemInput();
    
    // 1. 根据位置计算最终 Layer 坐标
    s32 targetX;
    s32 targetY = PANEL_MARGIN_TOP;
    
    switch (position) {
        case LEFT:  // 左对齐
            targetX = PANEL_MARGIN_SIDE;
            break;
        case MIDDLE:  // 居中
            targetX = (SCREEN_WIDTH - PANEL_WIDTH) / 2;
            break;
        case RIGHT:  // 右对齐
            targetX = SCREEN_WIDTH - PANEL_WIDTH - PANEL_MARGIN_SIDE;
            break;
        default:
            targetX = (SCREEN_WIDTH - PANEL_WIDTH) / 2;  // 默认居中
    }
    
    // 3. 面板布局（使用宏定义）
    s32 panelX = 0;              // 从 Framebuffer 左边开始
    s32 panelY = 0;              // 从 Framebuffer 顶部开始
    s32 panelW = PANEL_WIDTH;    // 实际显示宽度（右侧可能有像素留空用于块线性对齐）
    s32 panelH = PANEL_HEIGHT;   // 实际显示高度
    
    // 4. 解析图标和文本
    char iconStr[4] = "\uE137";  // 默认图标 \uE137 (UTF-8: 3字节)
    const char* displayText = text;
    
    // 检查文本开头是否有 UTF-8 编码的私有区字符（U+E000-U+EFFF: \xEE\x8X\xXX）
    if (text && (u8)text[0] == 0xEE && ((u8)text[1] & 0xF0) == 0x80) {
        // 提取图标的 3 个 UTF-8 字节
        iconStr[0] = text[0];
        iconStr[1] = text[1];
        iconStr[2] = text[2];
        iconStr[3] = '\0';
        
        // 跳过图标的 3 个字节
        displayText = text + 3;
        
        // 跳过图标后面的空格
        while (*displayText == ' ') displayText++;
    }
    
    // 8. 绘制
    m_Renderer.StartFrame();
    
    // 背景（圆角矩形）
    s32 cornerRadius = (s32)(8 * SCALE);  
    m_Renderer.DrawRoundedRect(panelX, panelY, panelW, panelH, cornerRadius, {13, 13, 13, 15});
    
    // 顶部高光（微妙的亮边，增加立体感，带圆角）
    s32 highlightH = (s32)(4 * SCALE);
    m_Renderer.DrawRoundedRectPartial(panelX, panelY, panelW, highlightH, cornerRadius, 
                                       {15, 15, 15, 8}, GraphicsRenderer::RoundedRectPart::TOP);
    
    // 底部微阴影（极淡的暗边，带圆角）
    s32 shadowH = (s32)(4 * SCALE);
    s32 shadowY = panelY + panelH - shadowH;
    m_Renderer.DrawRoundedRectPartial(panelX, shadowY, panelW, shadowH, cornerRadius,
                                       {0, 0, 0, 2}, GraphicsRenderer::RoundedRectPart::BOTTOM);
    
    // 图标（固定在左边距 15 逻辑像素）
    s32 iconX = (s32)(15 * SCALE);
    s32 iconW = (s32)(40 + 15 + 15) * SCALE;
    s32 iconSize = (s32)(40 * SCALE);
    m_Renderer.DrawText(iconStr, iconX, panelY, iconW, panelH, iconSize, {4, 4, 4, 15});
    
    // 文本（图标右边，左对齐，右边留15像素边距）
    s32 textX = iconX + iconW + (s32)(3 * SCALE);
    s32 textW = panelW - textX - (s32)(15 * SCALE);  // 总宽度 - 文本起始X - 右边距
    m_Renderer.DrawText(displayText, textX, panelY, textW, panelH, PANEL_FONT_SIZE, {5, 5, 5, 15}, GraphicsRenderer::TextAlign::LEFT); 
    
    m_Renderer.EndFrame();
    
    // 2. 执行动画（根据位置选择不同动画）
    switch (position) {
        case LEFT:
            AnimateFromLeft(targetX, targetY);
            break;
        case RIGHT:
            AnimateFromRight(targetX, targetY);
            break;
        case MIDDLE:
            AnimateExpand(targetX, targetY);  // 暂时只是设置位置
            break;
        default:
            viSetLayerPosition(&m_Layer, targetX, targetY);
            break;
    }
    
    m_IsVisible = true;
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

// ==================== 动画函数 ====================

// 缓动函数：快进慢出（EaseOutCubic）
float NotificationManager::EaseOutCubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}
// 左边滑入动画
void NotificationManager::AnimateFromLeft(s32 targetX, s32 targetY) {
    const int ANIMATION_FRAMES = 120;
    const u64 FRAME_TIME = 16666667;  // 16.6ms (60fps)
    
    s32 startX = -PANEL_WIDTH;  // 从左边屏幕外开始
    
    for (int i = 0; i <= ANIMATION_FRAMES; i++) {
        float t = (float)i / ANIMATION_FRAMES;
        float progress = EaseOutCubic(t);
        
        s32 currentX = startX + (s32)((targetX - startX) * progress);
        
        viSetLayerPosition(&m_Layer, currentX, targetY);
        svcSleepThread(FRAME_TIME);
    }
    
    // 确保最终位置准确
    viSetLayerPosition(&m_Layer, targetX, targetY);
}

// 右边滑入动画
void NotificationManager::AnimateFromRight(s32 targetX, s32 targetY) {
    const int ANIMATION_FRAMES = 15;
    const u64 FRAME_TIME = 16666667;  // 16.6ms (60fps)
    
    s32 startX = SCREEN_WIDTH;  // 从右边屏幕外开始
    
    for (int i = 0; i <= ANIMATION_FRAMES; i++) {
        float t = (float)i / ANIMATION_FRAMES;
        float progress = EaseOutCubic(t);
        
        s32 currentX = startX + (s32)((targetX - startX) * progress);
        
        viSetLayerPosition(&m_Layer, currentX, targetY);
        svcSleepThread(FRAME_TIME);
    }
    
    // 确保最终位置准确
    viSetLayerPosition(&m_Layer, targetX, targetY);
}

// 中间展开动画（未实现，占位）
void NotificationManager::AnimateExpand(s32 targetX, s32 targetY) {
    // TODO: 实现展开动画
    // 暂时直接设置位置，不做动画
    viSetLayerPosition(&m_Layer, targetX, targetY);
}

