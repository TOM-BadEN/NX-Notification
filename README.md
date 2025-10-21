
![演示](./image/demo.gif)

# NX-Notification

Nintendo Switch 通知弹窗系统模块及 C 库

## 项目简介

**NX-Notification** 是一个适用于 Nintendo Switch 的通知弹窗解决方案，包含系统模块和配套的 C 库，主要用于系统模块的通知显示。

### 项目组成

- **[sys-Notification](./sys-Notification/)** - 通知弹窗系统模块
- **[libnotification](./libnotification/)** - 配套便于调用的库

## 功能特性

- 自定义通知文本（支持 Unicode）
- 可选通知类型（INFO / ERROR）
- 可选显示位置（LEFT / MIDDLE / RIGHT）
- 可配置显示时长（1-10 秒）
- 调用时自动启动系统模块，调用完成系统模块自动关闭

## 注意

- 这是面向switch自制插件开发者的弹窗插件
- 系统模块需要使用者先编译，直接用 make 就行
- 这个插件主要是用于给其他系统模块提供通知弹窗的
- 对于ovl和nro插件，不应该使用该插件
- 因底层限制，无法与 Tesla 覆盖层共存，系统模块运行时 Tesla 会暂时隐藏且无法打开，弹窗结束后恢复
- 因为要通用设计，该模块启用时内存占用有1.33MB，对于20+HOS，可能大部分用户不愿使用
- 所以如果你的插件使用该模块，应向用户提供配置关闭的选项


## 使用方法

1. 编译好系统模块后，将其与你的插件一同发放
2. 你的插件项目中引入[libnotification](./libnotification/)后，调用创建通知弹窗即可
3. libnotification 详细文档：[README](./libnotification/README.md)

## 目录结构

```
NX-Notification/
├── sys-Notification/          # 系统模块源码
│   ├── source/               # C++ 源文件
│   ├── include/              # 头文件
│   ├── sys-json/             # 系统模块配置
│   └── Makefile              # 编译配置
│
├── libnotification/           # C 库
│   ├── libnotification.h     # 源码
│   ├── example/              # 示例程序
│   └── README.md             # 库文档
│
└── README.md                  # 本文件
```

