# 项目编译指南

## 项目说明

**PonyWork** 是一个基于 Qt 5.15.2 的桌面应用程序，提供远程桌面、应用管理、云同步等功能。

### 主要模块

| 模块 | 功能 |
|------|------|
| `modules/core` | 核心功能：数据库、AI配置、日志、应用管理、网络监控、FRPC |
| `modules/user` | 用户系统：登录、菜单、密码修改 |
| `modules/widgets` | UI组件：应用管理、备忘录、工作日志、远程桌面、设置 |
| `modules/dialogs` | 对话框：截图、批量导入、图标选择、AI生成 |
| `modules/update` | 自动更新系统 |

### 项目结构

```
PonyWork/
├── PonyWork.pro          # 主项目文件
├── mainwindow.cpp        # 主窗口
├── modules/              # 功能模块
│   ├── core/            # 核心模块
│   ├── user/            # 用户模块
│   ├── widgets/         # UI组件
│   ├── dialogs/         # 对话框
│   ├── update/          # 更新模块
│   └── ui/              # Qt UI 文件
├── resources.qrc         # 资源文件
└── build.bat            # 编译脚本
```

## 项目信息

- **框架**: Qt 5.15.2
- **编译器**: MinGW 8.1.0 (64-bit)
- **构建工具**: qmake + mingw32-make

## 开发要求

### 代码规范

1. **头文件保护**: 使用 `#ifndef MODULE_NAME_H` / `#define` / `#endif`
2. **命名规范**: 类名 `CamelCase`，函数/变量 `camelCase`
3. **文档**: 公共接口需添加注释说明

### 模块开发

1. 新增模块需要在对应目录创建：
   - `模块名.h` - 头文件
   - `模块名.cpp` - 实现文件
2. 如需 UI，创建 `modules/ui/模块名.ui`
3. 更新 `PonyWork.pro` 的 SOURCES/HEADERS/FORMS

### 提交规范

- 提交前确保编译通过
- 提交信息格式: `type(scope): description`
- type: feat, fix, refactor, chore, docs

## Qt 项目编译

当需要编译 Qt 项目时，**必须**使用 Skill 工具调用 `qt-compile`。

### 调用方式
```
Skill: qt-compile
```

这将自动加载编译指南，包含完整的编译步骤和常见问题解决。

### 快速编译

项目提供了快捷编译脚本，直接运行：

- `build.bat` - Release 版本编译
- `build_test.bat` - 测试版本编译
- `build_memo.bat` - 备忘录模块编译

### 编译命令示例

```bash
# 1. 设置环境（如果需要）
set PATH=D:\Qt\Tools\mingw810_64\bin;D:\Qt\5.15.2\mingw81_64\bin;%PATH%

# 2. 进入项目目录
cd f:\00AI\PonyWork

# 3. 生成 Makefile
D:\Qt\5.15.2\mingw81_64\bin\qmake.exe PonyWork.pro

# 4. 编译
mingw32-make release -j4
```
