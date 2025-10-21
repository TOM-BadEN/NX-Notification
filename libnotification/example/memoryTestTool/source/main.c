


#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libnotification.h"


// 全局内存记录
u64 g_initial_free_ram = 0;  // 初始可用内存

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
    printf("Press X to send notification\n");
    printf("Press + to exit.\n");

    // 主循环
    while (appletMainLoop())
    {
        // 扫描手柄输入，每帧执行一次
        padUpdate(&pad);

        // padGetButtonsDown 返回在本帧中新按下的按键集合
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_X) {
            // 获取写入前的内存
            u64 before_free_ram;
            GetSystemMemoryInfo(&total_ram, &used_ram, &before_free_ram);
            
            // 申请弹窗通知
            Result rc = createNotification("这是一次测试", 3, INFO, RIGHT);
            
            if (R_FAILED(rc)) {
                printf("Failed: 0x%x\n", rc);
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
                float initial_mb = (float)g_initial_free_ram / (1024.0f * 1024.0f);
                
                printf(CONSOLE_CYAN "[%d] Usage: %.2f MB (Diff: %+.3f MB), Initial: %.2f MB\n" CONSOLE_RESET, 
                           i + 1, usage_mb, diff_mb, initial_mb);

            }
        }

        if (kDown & HidNpadButton_Plus)
            break; // 退出

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
