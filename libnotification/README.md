# libnotification

适用于 Nintendo Switch 的通知系统模块 C 库

## 简介

`libnotification` 是一个单头文件 C 库，用于向 `sys-Notification` 系统模块发送通知。

- **单头文件**：只需包含一个 `.h` 文件
- **自动管理**：自动启动系统模块、创建配置目录
- **完全容错**：自动校正非法参数，不会因参数错误而失败
- **原子操作**：使用原子文件写入，避免竞态条件

## 安装

1. 确保已安装 `sys-Notification 
2. 将 `libnotification.h` 复制到你的项目中
3. 在代码中包含头文件：`#include "libnotification.h"`


## 注意事项

### 使用须知
1. **Header-Only 库**：只需包含头文件即可使用，参考 example 示例
2. **系统模块必须安装**：Program ID `0x0100000000251020`
3. **服务依赖**：使用前必须初始化 `pmdmnt` 和 `pmshell` 服务

### 功能限制
1. **文本长度**：最大 7 个中文字符（31 字节），超出自动截断
2. **显示时长**：1-10 秒，超出范围自动校正，非必要请缩短时间
3. **换行符**：`\n` 和 `\r` 会被自动替换为空格
4. **Unicode 支持**：支持 Unicode 字符（与 Tesla 用法相同）

### 重要说明
1. **内存占用**：系统模块运行时占用 1.33MB，弹窗完成后自动关闭
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

### 完整示例

```c
#include <switch.h>
#include "libnotification.h"

int main(int argc, char* argv[]) {
    // 初始化服务
    pmdmntInitialize();
    pmshellInitialize();
    
    // 发送不同类型的通知
    createNotification("信息通知", 3, INFO, RIGHT);
    createNotification("错误通知", 5, ERROR, MIDDLE);
    createNotification("左侧通知", 4, INFO, LEFT);
    
    // 清理
    pmshellExit();
    pmdmntExit();
    
    return 0;
}
```

## API 文档

### createNotification

```c
Result createNotification(const char* text, int duration, NotificationType type, NotificationPosition position);
```

发送一个通知到 sys-Notification 系统模块。

**参数：**
- `text` - 通知文本（最大 31 字符，超出自动截断）
- `duration` - 显示时长（秒，范围 1-10，超出自动校正）
- `type` - 通知类型（`INFO` 或 `ERROR`，非法值自动改为 `INFO`）
- `position` - 通知位置（`LEFT`、`MIDDLE`、`RIGHT`，非法值自动改为 `RIGHT`）

**返回值：**
- `0` - 成功
- `-1` - 参数错误（文本为空）
- `-2` - 创建配置目录失败
- `-3` - 文件操作失败
- `-4` - 原子重命名失败
- 其他 - 系统模块启动失败（libnx 错误码）

**特性：**
- 文本超过 31 字符会自动截断
- 换行符 `\n` 和 `\r` 会自动替换为空格
- `duration` 超出 1-10 范围会自动校正
- 非法的枚举值会自动改为默认值
- 系统模块未运行时会自动启动
- 配置目录不存在时会自动创建

### 枚举类型

#### NotificationType - 通知类型

```c
typedef enum {
    INFO = 0,    // 信息通知
    ERROR = 1    // 错误通知
} NotificationType;
```

#### NotificationPosition - 通知位置

```c
typedef enum {
    LEFT = 0,    // 左对齐
    MIDDLE = 1,  // 居中
    RIGHT = 2    // 右对齐
} NotificationPosition;
```

## 编译配置

### Makefile 示例

```makefile
# 包含路径
INCLUDES := -I./path/to/libnotification

# 链接库
LIBS := -lnx

# 编译
$(CC) $(CFLAGS) $(INCLUDES) your_source.c -o output $(LIBS)
```

### 必需的服务

使用此库前，必须初始化以下 libnx 服务：

```c
pmdmntInitialize();   // 用于检查系统模块是否运行
pmshellInitialize();  // 用于启动系统模块
```

使用完毕后记得清理：

```c
pmshellExit();
pmdmntExit();
```



