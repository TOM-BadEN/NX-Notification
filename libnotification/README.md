# libnotification

适用于 Nintendo Switch 的通知系统模块 C 库

## 简介

`libnotification` 是一个单头文件 C 库，用于向 `sys-Notification` 系统模块申请向switch发送通知弹窗，主要设计用于其他系统模块使用。

## 安装

1. 确保已安装 [sys-Notification](https://github.com/TOM-BadEN/NX-Notification/tree/main/sys-Notification) 系统模块
2. 将 `libnotification.h` 复制到你的项目中
3. 在代码中包含头文件：`#include "libnotification.h"`


## 注意事项

### 使用须知
1. **Header-Only 库**：只需包含头文件即可使用
2. **系统模块必须安装**：[sys-Notification](https://github.com/TOM-BadEN/NX-Notification/tree/main/sys-Notification) 
3. **服务依赖**：使用前必须初始化 `pmdmnt` 和 `pmshell` 服务

### 功能限制
1. **文本长度**：最大 7 个中文字符（31 字节），超出自动截断
2. **显示时长**：1-10 秒，超出范围自动校正，非必要请缩短时间
3. **换行符**：`\n` 和 `\r` 会被自动替换为空格
4. **Unicode 支持**：支持 Unicode 字符（与 Tesla 用法相同）

### 重要说明
1. **内存占用**：系统模块运行时占用 688KB，弹窗完成后自动关闭
2. **覆盖层冲突**：无法与 Tesla 覆盖层共存，系统模块运行时 Tesla 会暂时隐藏且无法打开，弹窗结束后恢复
3. **使用建议**：
   - 主要用于其他系统模块的通知，请勿滥用
   - NRO 程序不建议使用（example 仅为演示）
   - Tesla 插件请使用 libultrahand 的内置弹窗功能
   - 如果你的插件使用该模块，应向用户提供配置关闭的选项


## 快速开始

### 最小示例

```c
#include <switch.h>
#include "libnotification.h"

int main(int argc, char* argv[]) {
    // 初始化必需的服务
    pmdmntInitialize();
    pmshellInitialize();
    
    // 发送通知
    createNotification("Hello World!", 3, INFO, RIGHT);
    
    // 清理
    pmshellExit();
    pmdmntExit();
    
    return 0;
}
```

# 示例项目

- [按键连发](https://github.com/TOM-BadEN/AutoKeyLoop)       AutoKeyLoop
- [完整示例](https://github.com/TOM-BadEN/NX-Notification/tree/main/libnotification/example/memoryTestTool)     一个内存小工具
