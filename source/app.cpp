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
    // 测试左对齐位置
    // m_NotifMgr.Show("\uE140居中测试HAH", MIDDLE);
    // svcSleepThread(5000000000ULL);  // 2 秒
    
    // 测试居中位置
    m_NotifMgr.Show("\uE137左对齐测试HAH", LEFT);
    svcSleepThread(2000000000ULL);  // 2 秒
    m_NotifMgr.Hide();
    svcSleepThread(5000000000ULL);  // 2 秒
    m_NotifMgr.Show("\uE137左对齐测试HAH", LEFT);
    svcSleepThread(2000000000ULL);  // 2 秒
    m_NotifMgr.Hide();
    svcSleepThread(5000000000ULL);  // 2 秒
    m_NotifMgr.Show("\uE137左对齐测试HAH", LEFT);
    svcSleepThread(5000000000ULL);  // 2 秒
    m_NotifMgr.Hide();
    svcSleepThread(5000000000ULL);  // 2 秒
    m_NotifMgr.Show("\uE137左对齐测试HAH", LEFT);
    svcSleepThread(5000000000ULL);  // 2 秒
    m_NotifMgr.Hide();
    svcSleepThread(5000000000ULL);  // 2 秒
    
    // 测试右对齐位置
    m_NotifMgr.Show("右对齐测试HAHDd打撒多少啊", RIGHT);
    svcSleepThread(5000000000ULL);  // 2 秒
    
    // 隐藏通知
    m_NotifMgr.Hide();
    
    // 循环结束，程序退出
}
