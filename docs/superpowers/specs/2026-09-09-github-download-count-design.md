# GitHub 下载量展示设计

## 目标

在静态下载页展示 PonyWork 安装包在 GitHub Release 中的累计下载量，不依赖已停用的 `server/admin-server.js` 服务，也不采集访问者个人数据。

## 方案

在 `download.html` 的主下载卡片中加入唯一的统计文本元素，并在页面末尾放置内联脚本。脚本请求 GitHub REST API：`https://api.github.com/repos/jiasu1017-beep/ponywork-website/releases/tags/v1.0.9`，在该 Release 的资产中按文件名 `PonyWork-v1.0.9-win64.zip` 找到对应的 `download_count`，并将该数值格式化后显示在下载按钮附近。

每次页面加载最多请求一次 API。成功结果在当前浏览器会话的 `sessionStorage` 中短期缓存，避免同一访客反复浏览时触发匿名 GitHub API 限流。

## 边界与失败处理

- 统计数表示 GitHub 资产累计下载量，不表示本站按钮点击量或成功安装量。
- 不收集 IP、地理位置或其他用户标识。
- API 请求失败、限流或未找到资产时，展示“下载统计暂不可用”；安装包链接仍保持可用。
- 不增加构建工具、服务端或第三方分析服务。

## 验证

使用浏览器检查正常数据渲染，并分别模拟网络/API 失败和资产不存在，确认提示文案正确且下载链接未变化。
