#include "app.hpp"
#include <cstring>
#include "SimpleFs.hpp"


#define NOTIFICATION_PATH "/config/sys-Notification"

App::App() {

    // 检查并创建通知目录
    if (!SimpleFs::DirectoryExists(NOTIFICATION_PATH)) {
        SimpleFs::CreateDirectory(NOTIFICATION_PATH);
    }

    // 初始化通知管理器
    Result rc = m_NotifMgr.Init();
    if (R_FAILED(rc)) fatalThrow(rc);  // 初始化失败，抛出致命错误
        
}

App::~App() {
}

void App::Loop() {

    // 状态变量
    enum State { IDLE, SHOWING };
    State state = IDLE;
    
    u64 last_activity_time = armGetSystemTick();  // 最后一次活动时间
    u64 show_start_time = 0;                      // 当前通知开始显示的时间
    u64 hide_time = 0;                            // 应该隐藏的时间点
    
    const u64 timeout_ns = 1000000000ULL;         // 1 秒超时（纳秒）
    const u64 min_display_ns = 1000000000ULL;     // 最小显示时长 1 秒（纳秒）
    const u64 sleep_ns = 200000000ULL;            // 每次循环休眠 200ms
    
    while (true) {
        // 获取当前时间
        u64 now = armGetSystemTick();
        
        // 扫描第一个 INI 文件
        const char* file = SimpleFs::GetFirstIniFile(NOTIFICATION_PATH);
        
        // 如果有文件
        if (file) {
            // 重置超时计时器
            last_activity_time = now;  
            
            // 如果有旧的通知正在显示，检查是否满 1 秒
            if (state == SHOWING) {
                u64 elapsed_ns = armTicksToNs(now - show_start_time);
                if (elapsed_ns < min_display_ns) {
                    // 未满 1 秒，等待
                    svcSleepThread(sleep_ns);
                    continue;
                }
                // 满 1 秒，删除旧的通知
                m_NotifMgr.Hide();
                state = IDLE;
            }
            
            // 读取并解析文件
            const char* content = SimpleFs::ReadFileContent(file);
            // 解析出来通知所需的结构体
            NotificationConfig config = ParseIni(content);
            
            // 立即删除文件
            SimpleFs::DeleteFile(file);
            
            // 检查解析出来的通知配置项，无效则跳过
            if (config.text[0] == '\0') {
                svcSleepThread(sleep_ns);
                continue;
            }
            
            // 显示新通知
            m_NotifMgr.Show(config.text, config.position, config.type);
            // 记录通知开始显示的时间
            show_start_time = now;
            
            // 检查是否还有其他文件（判断显示时长）（如果有，则显示时长为1秒，没有就按配置项中的时长）
            const char* next_file = SimpleFs::GetFirstIniFile(NOTIFICATION_PATH);
            u64 display_duration = next_file ? min_display_ns : config.duration;
            
            // 计算删除这个通知的时间点
            hide_time = show_start_time + armNsToTicks(display_duration);
            state = SHOWING;
            
            svcSleepThread(sleep_ns);
            continue;
        }
        
        // 没有新文件，检查当前通知是否到期
        if (state == SHOWING) {
            if (now >= hide_time) {
                m_NotifMgr.Hide();
                state = IDLE;
                last_activity_time = now;  // 更新超时计时起点（从Hide后开始计时）
            }
            svcSleepThread(sleep_ns);
            continue;
        }
        
        // 完全空闲，检查超时
        u64 idle_ns = armTicksToNs(now - last_activity_time);
        if (idle_ns > timeout_ns) {
            break;  // 20 秒没活动，退出
        }
        
        svcSleepThread(sleep_ns);
    }
}




NotificationConfig App::ParseIni(const char* content) {
    
    NotificationConfig config;
    config.text[0] = '\0';
    config.duration = 0;
    config.type = INFO;      
    config.position = RIGHT;

    if (!content) return config;
    const char* p = content;
    
    while (*p) {
        // 跳过空白字符
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;
        
        const char* line_start = p;
        const char* equal_pos = nullptr;
        
        // 查找 '=' 分隔符
        while (*p && *p != '\n') {
            if (*p == '=' && !equal_pos) equal_pos = p;
            p++;
        }
        
        // 没有找到 '='，跳过这行
        if (!equal_pos) {
            if (*p == '\n') p++;
            continue;
        }
        
        // 提取 key 和 value 的范围
        const char* key_start = line_start;
        const char* key_end = equal_pos;
        const char* value_start = equal_pos + 1;
        const char* value_end = p;
        
        // 去除 key 两端的空格和制表符
        while (key_start < key_end && (*key_start == ' ' || *key_start == '\t'))
            key_start++;
        while (key_end > key_start && (*(key_end-1) == ' ' || *(key_end-1) == '\t'))
            key_end--;
        
        // 去除 value 两端的空格、制表符和 \r
        while (value_start < value_end && (*value_start == ' ' || *value_start == '\t'))
            value_start++;
        while (value_end > value_start && (*(value_end-1) == ' ' || *(value_end-1) == '\t' || *(value_end-1) == '\r'))
            value_end--;
        
        int key_len = key_end - key_start;
        int value_len = value_end - value_start;
        
        // 匹配 "text"
        if (key_len == 4 && strncmp(key_start, "text", 4) == 0) {
            if (value_len <= 0) continue;
            int copy_len = (value_len < 31) ? value_len : 31;
            strncpy(config.text, value_start, copy_len);
            config.text[copy_len] = '\0';
        }
        // 匹配 "duration"
        else if (key_len == 8 && strncmp(key_start, "duration", 8) == 0) {
            int seconds = 0;
            for (const char* d = value_start; d < value_end && *d >= '0' && *d <= '9'; d++)
                seconds = seconds * 10 + (*d - '0');
            
            if (seconds < 1) seconds = 2;
            else if (seconds > 10) seconds = 10;
            config.duration = (u64)seconds * 1000000000ULL;
        }
        // 匹配 "position"
        else if (key_len == 8 && strncmp(key_start, "position", 8) == 0) {
            if (value_len == 4 && strncmp(value_start, "LEFT", 4) == 0)
                config.position = LEFT;
            else if (value_len == 6 && strncmp(value_start, "MIDDLE", 6) == 0)
                config.position = MIDDLE;
            else if (value_len == 5 && strncmp(value_start, "RIGHT", 5) == 0)
                config.position = RIGHT;
        }
        // 匹配 "type"
        else if (key_len == 4 && strncmp(key_start, "type", 4) == 0) {
            if (value_len == 4 && strncmp(value_start, "INFO", 4) == 0)
                config.type = INFO;
            else if (value_len == 5 && strncmp(value_start, "ERROR", 5) == 0)
                config.type = ERROR;
        }
        
        if (*p == '\n') p++;
    }
    
    
    return config;
}
