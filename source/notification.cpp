#include "notification.hpp"

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
    : m_FramebufferWidth(448)
    , m_FramebufferHeight(720)
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
    
    // 1. 计算图层尺寸和位置（保持宽高比，屏幕居中）
    m_LayerHeight = 1080;  // 填满屏幕高度
    m_LayerWidth = (u16)(1080 * ((float)m_FramebufferWidth / (float)m_FramebufferHeight));
    m_LayerPosX = (1280 - m_LayerWidth) / 2;  // 水平居中
    m_LayerPosY = 0;
    
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

// 显示通知弹窗
void NotificationManager::Show(const char* text) {
    if (!m_Initialized) return;
    
    // 计算面板布局（业务逻辑）
    s32 panelX = 100;
    s32 panelY = 300;
    s32 panelW = 248;  // 448 - 100*2
    s32 panelH = 120;
    
    // 绘制
    m_Renderer.StartFrame();
    m_Renderer.FillScreen({0, 0, 0, 0});
    m_Renderer.DrawRect(panelX, panelY, panelW, panelH, {0, 0, 0, 12});
    m_Renderer.DrawRect(panelX, panelY, panelW, 2, {15, 15, 15, 15});
    m_Renderer.DrawRect(panelX, panelY + panelH - 2, panelW, 2, {15, 15, 15, 15});
    m_Renderer.DrawRect(panelX, panelY, 2, panelH, {15, 15, 15, 15});
    m_Renderer.DrawRect(panelX + panelW - 2, panelY, 2, panelH, {15, 15, 15, 15});
    
    // 渲染文本
    m_Renderer.DrawText(text, panelX + 10, panelY + 10, 28.0f, {15, 15, 15, 15});
    
    m_Renderer.EndFrame();
}

// 隐藏通知弹窗
void NotificationManager::Hide() {
    if (!m_Initialized) return;
    
    // 绘制全透明帧，清空显示
    m_Renderer.StartFrame();
    m_Renderer.FillScreen({0, 0, 0, 0});
    m_Renderer.EndFrame();
    
    m_IsVisible = false;
}
