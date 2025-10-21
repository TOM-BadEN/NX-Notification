#include <switch.h>
#include <stdlib.h>
#include "app.hpp"

// 定义一个错误处理宏，如果结果失败，则抛出错误
#define ASSERT_FATAL(x) if (Result res = x; R_FAILED(res)) fatalThrow(res)

// NV 服务配置（避免系统模块卡住）
NvServiceType __attribute__((weak)) __nx_nv_service_type = NvServiceType_Application;

extern "C" {

/*

    AI给的代码，将堆内存转移到静态内存，
    实测堆内存申请确实可以大幅减少，但是内存不会消失，总的内存占用还是一样的，
    这段代码超出了我的水平，
    我不知道会不会出现问题，所以注释掉不使用，
    不过也不删了，就放在这里吧。

// 静态帧缓冲区（约 608 KB，移出堆）
static u8 __attribute__((aligned(0x1000))) g_framebuffer_memory[0x98000];  // 622,592 字节
static bool g_framebuffer_allocated = false;

// 自定义分配器：将帧缓冲区分配到静态内存
void* __libnx_aligned_alloc(size_t alignment, size_t size) {
    // 检测是否是帧缓冲区分配（0x1000 对齐 + 大小约 600 KB）
    if (alignment == 0x1000 && size >= 0x90000 && size <= 0xA0000 && !g_framebuffer_allocated) {
        g_framebuffer_allocated = true;
        return g_framebuffer_memory;
    }
    // 其他分配仍使用堆
    size = (size + alignment - 1) & ~(alignment - 1);
    return aligned_alloc(alignment, size);
}

void __libnx_free(void* p) {
    // 如果是静态帧缓冲区，标记为未使用
    if (p == g_framebuffer_memory) {
        g_framebuffer_allocated = false;
        return;
    }
    // 其他内存正常释放
    free(p);
}


#define INNER_HEAP_SIZE 0x80000          // 512 KB 


*/

// 堆的大小
#define INNER_HEAP_SIZE 0xE1000          // 0.9 MB

// 系统模块不应使用applet相关功能
u32 __nx_applet_type = AppletType_None;

// 设置用于 NVIDIA 显卡操作的共享内存大小
u32 __nx_nv_transfermem_size = 0x15000;                   // 86 KB

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
    fsdevUnmountAll();  
    plExit();  
    fsExit();           
    smExit();           
}

}

int main() {
  App app;
  app.Loop();
  return 0;
}
