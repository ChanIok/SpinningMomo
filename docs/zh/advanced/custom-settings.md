# 自定义设置

## ⚙️ 配置文件说明

### 📂 文件位置

配置文件 `config.ini` 位于程序目录下，首次运行程序时会自动创建。

::: tip 快速访问
可以通过托盘菜单的"打开配置文件"快速打开。
:::

## 🔧 配置项说明

### 🎯 窗口设置

```ini
[Window]
# 目标窗口标题，程序会优先调整此标题的窗口
Title=
```

### ⌨️ 快捷键设置

```ini
[Hotkey]
# 修饰键组合值：
# 1 = Alt
# 2 = Ctrl
# 3 = Ctrl + Alt
# 4 = Shift
# 5 = Alt + Shift
# 6 = Ctrl + Shift
# 7 = Ctrl + Alt + Shift
Modifiers=3

# 主键的虚拟键码
# 82 = R键
# 65-90 = A-Z
# 48-57 = 0-9
# 112-123 = F1-F12
Key=82
```

::: tip 默认快捷键
默认快捷键是 `Ctrl + Alt + R`，对应 `Modifiers=3` 和 `Key=82`。
建议通过托盘菜单的子菜单选择快捷键，也可以自行查询 KeyCode 对照表进行设置。
:::

### 📐 自定义比例

```ini
[CustomRatio]
# 用逗号分隔多个比例，比例用冒号连接
# 例如：16:10,17:10,1.618:1
RatioList=
```

格式说明：
- 用冒号(`:`)连接宽高比
- 用逗号(`,`)分隔多个比例
- 比例值可以是整数或小数

示例：
```ini
# 单个比例
RatioList=16:10

# 多个比例
RatioList=16:10,17:10,18:10
```

### 📏 自定义分辨率

```ini
[CustomResolution]
# 用逗号分隔多个分辨率，宽高用x连接
# 例如：6000x4000,7680x4320
ResolutionList=
```

格式说明：
- 用字母x或X连接宽高
- 用逗号(`,`)分隔多个分辨率
- 分辨率必须是整数

示例：
```ini
# 单个分辨率
ResolutionList=5120x2880

# 多个分辨率
ResolutionList=5120x2880,6000x4000,7680x4320
```

### 📸 相册目录设置

```ini
[Screenshot]
# 游戏相册目录路径，用于快速打开游戏截图文件夹
# 可以修改为其他游戏的相册目录
GameAlbumPath=
```

示例：
```ini
# 自定义目录
GameAlbumPath=D:\Games\Screenshots
```

### 🌐 语言设置

```ini
[Language]
# 支持的语言：
# zh-CN = 简体中文
# en-US = English
Current=zh-CN
```

### 🔽 任务栏设置

```ini
[Taskbar]
# 是否自动隐藏任务栏
# 0 = 不隐藏
# 1 = 自动隐藏
AutoHide=0

# 调整窗口时是否将任务栏置底
# 0 = 不置底
# 1 = 自动置底
LowerOnResize=1
```

::: warning 注意事项
- 修改配置文件后需要重启程序才能生效
- 可手动将旧的配置文件复制到新版本中，以保留自定义设置
:::