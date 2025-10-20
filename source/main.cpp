#include <switch.h>
#include "app.hpp"

// 定义一个错误处理宏，如果结果失败，则抛出错误
#define ASSERT_FATAL(x) if (Result res = x; R_FAILED(res)) fatalThrow(res)

// NV 服务配置（避免系统模块卡住）
NvServiceType __attribute__((weak)) __nx_nv_service_type = NvServiceType_Application;

extern "C" {

// 内部堆的大小（根据需要调整） 4MB
#define INNER_HEAP_SIZE 0x400000

// 系统模块不应使用applet相关功能
u32 __nx_applet_type = AppletType_None;

// 设置用于 NVIDIA 显卡操作的共享内存大小（2MB）
u32 __nx_nv_transfermem_size = 0x200000;

// 系统模块通常只需要使用一个文件系统会话
u32 __nx_fs_num_sessions = 1;

// Newlib堆配置函数（使malloc/free能够工作）
void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // 配置newlib堆
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    ASSERT_FATAL(smInitialize());                         // 初始化系统管理服务
    ASSERT_FATAL(fsInitialize());                         // 初始化文件系统服务
    fsdevMountSdmc();                                     // 挂载SD卡(非核心依赖，所以不检查)
    ASSERT_FATAL(plInitialize(PlServiceType_User));       // 初始化本地化服务(字体)
    ASSERT_FATAL(setInitialize());                        // 初始化设置服务(系统语言)
    ASSERT_FATAL(hiddbgInitialize());                     // 初始化 HID 调试服务(模拟触屏)
}

void __appExit(void) {
    hiddbgExit();
    setExit();          
    plExit();           
    fsdevUnmountAll();  
    fsExit();           
    smExit();           
}

}

int main() {
  App app;
  app.Loop();
  return 0;
}
