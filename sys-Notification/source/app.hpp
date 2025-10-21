#pragma once

#include <switch.h>
#include "notification.hpp"

// 通知配置结构体
struct NotificationConfig {
    char text[32];                      // 通知内容
    NotificationType type;              // 通知类型 (info/warning/error)
    NotificationPosition position;      // 弹窗位置 (left/middle/right)
    u64 duration;                       // 持续时间 (纳秒)
};

class App {
public:
    App();
    ~App();
    
    void Loop();

private:
    NotificationManager m_NotifMgr;
    
    // 解析 INI 内容
    NotificationConfig ParseIni(const char* content);
};



