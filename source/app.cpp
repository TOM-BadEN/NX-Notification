#include "app.hpp"
#include <sstream>
#include <cstdlib>
#include "SimpleFs.hpp"


#define NOTIFICATION_PATH "/config/notification"

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
    
    const u64 timeout_ns = 20000000000ULL;        // 20 秒超时（纳秒）
    const u64 min_display_ns = 1000000000ULL;     // 最小显示时长 1 秒（纳秒）
    const u64 sleep_ns = 200000000ULL;            // 每次循环休眠 200ms
    
    while (true) {
        // 获取当前时间
        u64 now = armGetSystemTick();
        
        // 扫描第一个 INI 文件
        std::string file = SimpleFs::GetFirstIniFile(NOTIFICATION_PATH);
        
        // 如果有文件
        if (!file.empty()) {
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
            std::string content = SimpleFs::ReadFileContent(file);
            // 解析出来通知所需的结构体
            NotificationConfig config = ParseIni(content);
            
            // 立即删除文件
            SimpleFs::DeleteFile(file);
            
            // 检查解析出来的通知配置项，无效则跳过
            if (config.text.empty()) {
                svcSleepThread(sleep_ns);
                continue;
            }
            
            // 显示新通知
            m_NotifMgr.Show(config.text.c_str(), config.position, config.type);
            // 记录通知开始显示的时间
            show_start_time = now;
            
            // 检查是否还有其他文件（判断显示时长）（如果有，则显示时长为1秒，没有就按配置项中的时长）
            std::string next_file = SimpleFs::GetFirstIniFile(NOTIFICATION_PATH);
            u64 display_duration = next_file.empty() ? config.duration : min_display_ns;
            
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




NotificationConfig App::ParseIni(const std::string& content) {
    NotificationConfig config;
    config.duration = 0;
    
    bool has_text = false;
    bool has_type = false;
    bool has_position = false;
    bool has_duration = false;
    
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        // 跳过空行
        if (line.empty()) continue;
        
        // 查找 '=' 分隔符
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        
        // 提取 key 和 value
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // 匹配对应字段
        if (key == "text") {
            config.text = value;
            has_text = true;
        } 
        else if (key == "duration") {
            int seconds = atoi(value.c_str());
            // 限制范围：1秒 ~ 10秒
            if (seconds < 1) seconds = 2;
            else if (seconds > 10) seconds = 10;
            // 转换为纳秒
            config.duration = (u64)seconds * 1000000000ULL;
            has_duration = true;
        } 
        else if (key == "position") {
            if (value == "LEFT")        { config.position = LEFT;   has_position = true; }
            else if (value == "MIDDLE") { config.position = MIDDLE; has_position = true; }
            else if (value == "RIGHT")  { config.position = RIGHT;  has_position = true; }
        } 
        else if (key == "type") {
            if (value == "INFO")  { config.type = INFO;  has_type = true; }
            else if (value == "ERROR") { config.type = ERROR; has_type = true; }
        }
    }
    
    // 如果任何一项缺失，清空 text（标记无效）
    if (!has_text || !has_type || !has_position || !has_duration) {
        config.text = "";
    }
    
    return config;
}
