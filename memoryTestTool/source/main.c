// 包含 C 标准库的常用头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 包含 libnx 主头文件，用于 Switch 开发
#include <switch.h>

// 系统模块 Program ID
#define SYSMODULE_TID 0x0100000000251020ULL

// 通知配置文件路径前缀
#define NOTIF_FILE_PREFIX "/config/sys-Notification/notif_"

// 全局内存记录
u64 g_initial_free_ram = 0;  // 初始可用内存
u64 g_standby_free_ram = 0;  // 待机内存（按 A 键后）

/**
 * @brief 获取系统内存信息
 * @param out_total 输出：总内存（字节）
 * @param out_used 输出：已使用内存（字节）
 * @param out_free 输出：可用内存（字节）
 */
void GetSystemMemoryInfo(u64* out_total, u64* out_used, u64* out_free) {
    u64 total = 0;
    u64 used = 0;
    
    // 获取系统总内存
    svcGetSystemInfo(&total, 0, INVALID_HANDLE, 2);
    
    // 获取已使用内存
    svcGetSystemInfo(&used, 1, INVALID_HANDLE, 2);
    
    // 输出结果
    if (out_total) *out_total = total;
    if (out_used) *out_used = used;
    if (out_free) *out_free = total - used;
}

/**
 * @brief 启动系统模块
 * @param program_id 系统模块的 Program ID
 * @return Result 结果码
 */
Result LaunchSysmodule(u64 program_id) {
    NcmProgramLocation location = {
        .program_id = program_id,
        .storageID = NcmStorageId_None,
    };
    u64 pid = 0;
    return pmshellLaunchProgram(0, &location, &pid);
}

/**
 * @brief 关闭系统模块
 * @param program_id 系统模块的 Program ID
 * @return Result 结果码
 */
Result TerminateSysmodule(u64 program_id) {
    return pmshellTerminateProgram(program_id);
}

/**
 * @brief 检查系统模块是否在运行
 * @param program_id 系统模块的 Program ID
 * @return true 正在运行, false 未运行
 */
bool IsSysmoduleRunning(u64 program_id) {
    u64 pid = 0;
    if (R_FAILED(pmdmntGetProcessId(&pid, program_id)))
        return false;
    return pid > 0;
}

/**
 * @brief 生成随机通知配置文件路径
 * @return const char* 文件路径（内部静态缓冲区）
 */
const char* GetRandomNotifFilePath() {
    static char filepath[256];
    static bool initialized = false;
    
    if (!initialized) {
        // 使用系统时钟作为种子
        srand((unsigned int)armGetSystemTick());
        initialized = true;
    }
    
    u32 random = (u32)(rand() % 100000000);  // 限制到 8 位数（0-99999999）
    snprintf(filepath, sizeof(filepath), "%s%u.ini.temp", NOTIF_FILE_PREFIX, random);
    
    return filepath;
}

/**
 * @brief 写入通知配置文件（原子操作）
 * @param content 文件内容
 * @return Result 结果码，成功返回 0
 */
Result WriteNotifFile(const char* content) {
    if (!content) return -1;
    
    // 生成临时文件路径
    const char* temp_path = GetRandomNotifFilePath();  // 例如：notif_12345678.ini.temp
    
    // 生成最终文件路径（去掉 .temp 后缀）
    static char final_path[256];
    strncpy(final_path, temp_path, sizeof(final_path) - 1);
    final_path[sizeof(final_path) - 1] = '\0';
    
    // 移除 .temp 后缀
    char* temp_suffix = strstr(final_path, ".temp");
    if (temp_suffix) {
        *temp_suffix = '\0';  // 截断字符串
    }
    
    // 1. 写入临时文件
    FILE* file = fopen(temp_path, "w");
    if (!file) return -2;
    
    fputs(content, file);
    fclose(file);
    
    // 2. 原子重命名：.temp -> .ini
    if (rename(temp_path, final_path) != 0) {
        remove(temp_path);  // 重命名失败，删除临时文件
        return -3;
    }
    
    return 0;
}

/**
 * @brief 生成随机通知配置内容
 * @return const char* 配置内容（内部静态缓冲区）
 */
const char* GenerateRandomNotifConfig() {
    static char config[256];
    
    // 随机类型：INFO 或 ERROR
    const char* types[] = {"INFO", "ERROR"};
    const char* type = types[rand() % 2];
    
    // 随机位置：RIGHT, LEFT, MIDDLE
    const char* positions[] = {"RIGHT", "LEFT", "MIDDLE"};
    const char* position = positions[rand() % 3];
    
    // 固定内容
    const char* text = "这是一个内存测试";
    const char* duration = "5";  // 5 秒
    
    snprintf(config, sizeof(config), 
             "text=%s\ntype=%s\nposition=%s\nduration=%s\n",
             text, type, position, duration);
    
    return config;
}

// 主程序入口点
int main(int argc, char* argv[])
{
    // 使用文本控制台，作为向屏幕输出文本的简单方式
    consoleInit(NULL);
    
    // 初始化系统服务
    pmdmntInitialize();   // 进程管理服务（用于检查系统模块状态）
    pmshellInitialize();  // 进程Shell服务（用于启动/停止系统模块）

    // 配置支持的输入布局：单人玩家，标准手柄样式
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // 初始化默认手柄（读取掌机模式输入以及第一个连接的手柄）
    PadState pad;
    padInitializeDefault(&pad);

    printf("Memory Test Tool\n\n");
    
    // 保存初始内存到全局变量
    u64 total_ram, used_ram;
    GetSystemMemoryInfo(&total_ram, &used_ram, &g_initial_free_ram);
    
    printf(CONSOLE_GREEN "Initial Free: %.2f MB\n\n" CONSOLE_RESET, (float)g_initial_free_ram / (1024.0f * 1024.0f));
    printf("Press A to launch sysmodule\n");
    printf("Press X to send random notification\n");
    printf("Press + to exit.\n");

    // 主循环
    while (appletMainLoop())
    {
        // 扫描手柄输入，每帧执行一次
        padUpdate(&pad);

        // padGetButtonsDown 返回在本帧中新按下的按键集合
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_A) {
            // 检查系统模块是否已经在运行
            if (IsSysmoduleRunning(SYSMODULE_TID)) {
                continue;
            }
            
            // 启动系统模块
            Result rc = LaunchSysmodule(SYSMODULE_TID);
            
            // 等待 1 秒后获取新内存
            svcSleepThread(1000000000ULL);
            
            GetSystemMemoryInfo(&total_ram, &used_ram, &g_standby_free_ram);
            
            float standby_mb = (float)g_standby_free_ram / (1024.0f * 1024.0f);
            float initial_mb = (float)g_initial_free_ram / (1024.0f * 1024.0f);
            
            if (R_SUCCEEDED(rc)) {
                // 计算内存占用
                float usage_mb = (float)(g_initial_free_ram - g_standby_free_ram) / (1024.0f * 1024.0f);
                
                printf(CONSOLE_YELLOW "sysmodule ON  Usage: %.2f MB  Current: %.2f MB  Initial: %.2f MB\n" CONSOLE_RESET, 
                       usage_mb, standby_mb, initial_mb);
            } else {
                printf(CONSOLE_RED "Launch failed: 0x%x  Current: %.2f MB  Initial: %.2f MB\n" CONSOLE_RESET, 
                       rc, standby_mb, initial_mb);
            }
        }



        if (kDown & HidNpadButton_X) {
            // 检查系统模块是否正在运行
            if (!IsSysmoduleRunning(SYSMODULE_TID)) {
                continue;
            }
            
            // 获取写入前的内存
            u64 before_free_ram;
            GetSystemMemoryInfo(&total_ram, &used_ram, &before_free_ram);
            float before_mb = (float)before_free_ram / (1024.0f * 1024.0f);
            
            // 生成随机配置并写入文件
            const char* config = GenerateRandomNotifConfig();
            Result rc = WriteNotifFile(config);
            
            if (R_FAILED(rc)) {
                printf("Write file failed: 0x%x\n", rc);
                continue;
            }
            
            printf("Notification sent!\n");

            // 多次检查内存变化
            for (int i = 0; i < 5; i++) {
                svcSleepThread(1000000000ULL);  // 等待 1 秒
                
                u64 current_free_ram;
                GetSystemMemoryInfo(&total_ram, &used_ram, &current_free_ram);
                
                // 计算各项数据
                float usage_mb = (float)(g_initial_free_ram - current_free_ram) / (1024.0f * 1024.0f);
                float diff_mb = (float)(before_free_ram - current_free_ram) / (1024.0f * 1024.0f);
                float standby_mb = (g_standby_free_ram > 0) ? (float)g_standby_free_ram / (1024.0f * 1024.0f) : 0;
                float initial_mb = (float)g_initial_free_ram / (1024.0f * 1024.0f);
                
                // 格式：Usage xx (Diff xx), Standby xx (or Null), Initial xx
                if (g_standby_free_ram > 0) {
                    printf(CONSOLE_CYAN "[%d] Usage: %.2f MB (Diff: %+.3f MB), Standby: %.2f MB, Initial: %.2f MB\n" CONSOLE_RESET, 
                           i + 1, usage_mb, diff_mb, standby_mb, initial_mb);
                } else {
                    printf(CONSOLE_CYAN "[%d] Usage: %.2f MB (Diff: %+.3f MB), Standby: Null, Initial: %.2f MB\n" CONSOLE_RESET, 
                           i + 1, usage_mb, diff_mb, initial_mb);
                }
            }
        }

        if (kDown & HidNpadButton_Plus)
            break; // 退出并返回到 hbmenu

        // 更新控制台，向显示器发送新帧
        consoleUpdate(NULL);
    }

    // 清理系统服务
    pmshellExit();
    pmdmntExit();
    
    // 清理控制台使用的资源（重要！）
    consoleExit(NULL);
    return 0;
}
