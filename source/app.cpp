#include "app.hpp"

App::App() {
    // 初始化通知管理器
    Result rc = m_NotifMgr.Init();
    if (R_FAILED(rc)) {
        fatalThrow(rc);  // 初始化失败，抛出致命错误
    }
}

App::~App() {
}

void App::Loop() {
    // 显示通知（白色文字 "Hello"）
    m_NotifMgr.Show("你好，世界");
    
    // 等待 2 秒
    svcSleepThread(2000000000ULL);  // 2 秒 = 2,000,000,000 纳秒
    
    // 隐藏通知
    m_NotifMgr.Hide();
    
    // 循环结束，程序退出
}
