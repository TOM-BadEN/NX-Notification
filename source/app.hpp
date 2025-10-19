#pragma once

#include <switch.h>
#include "notification.hpp"

class App {
public:
    App();
    ~App();
    
    void Loop();

private:
    NotificationManager m_NotifMgr;
};



