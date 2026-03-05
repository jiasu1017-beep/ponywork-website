# 小马办公官方网站 - 部署文档

## 概述

本文档详细介绍如何将小马办公官方网站部署到GitHub Pages。

## 网站架构

```
website/
├── index.html          # 首页
├── features.html       # 功能介绍页
├── download.html       # 下载中心页
├── guide.html          # 使用指南页
├── about.html          # 关于我们页
├── contact.html        # 联系我们页
├── css/
│   └── style.css       # 主样式文件
├── js/
│   └── main.js         # 交互脚本
├── images/
│   └── favicon.ico     # 网站图标
└── .github/
    └── workflows/
        └── deploy.yml  # 自动部署工作流
```

## 部署方式

### 方式一：GitHub Actions 自动部署（推荐）

1. **创建GitHub仓库**（如果还没有）
   - 登录GitHub
   - 创建新仓库，例如：`ponywork-website`

2. **推送网站文件**
   ```bash
   cd website
   git init
   git add .
   git commit -m "Initial commit"
   git branch -M main
   git remote add origin https://github.com/yourusername/ponywork-website.git
   git push -u origin main
   ```

3. **启用GitHub Pages**
   - 进入仓库设置 (Settings)
   - 找到 "Pages" 左侧菜单
   - 在 "Build and deployment" 部分：
     - Source 选择 "Deploy from a branch"
     - Branch 选择 "gh-pages"
     - Folder 选择 "/ (root)"
   - 点击 "Save"

4. **等待部署完成**
   - 访问 `https://yourusername.github.io/ponywork-website/`

### 方式二：手动部署

1. 在仓库中创建 `gh-pages` 分支：
   ```bash
   git checkout -b gh-pages
   git push origin gh-pages
   ```

2. 在GitHub仓库设置中启用Pages，选择gh-pages分支

### 方式三：使用GitHub Actions（自动）

1. 将 `.github/workflows/deploy.yml` 文件推送到仓库
2. 每次推送到main分支时会自动触发部署
3. 部署完成后访问生成的URL

## 自定义域名配置

### 前提条件
- 已购买域名
- 域名已完成备案（国内服务器需要）

### 配置步骤

1. **创建CNAME文件**
   在website目录创建CNAME文件，内容为您的域名：
   ```
   www.yourdomain.com
   ```

2. **配置DNS**
   在域名提供商处添加CNAME记录：
   - 主机记录: www
   - 记录值: yourusername.github.io

3. **启用HTTPS**
   在GitHub Pages设置中，勾选"Enforce HTTPS"

## SSL证书

GitHub Pages自动为所有页面提供Let's Encrypt免费SSL证书。

## 测试检查清单

部署完成后，请检查：

- [ ] 首页正常显示
- [ ] 所有页面导航链接正常
- [ ] CSS样式正确加载
- [ ] JavaScript交互功能正常
- [ ] 响应式布局在移动端正常
- [ ] 图片资源正常加载
- [ ] 自定义域名（如使用）正常访问
- [ ] HTTPS正常工作

## 故障排除

### 页面显示404
- 检查文件路径是否正确
- 确认index.html在正确位置

### CSS/JS不加载
- 检查文件引用路径
- 确认文件已推送到仓库

### 自定义域名不工作
- 确认DNS配置正确
- 等待DNS生效（可能需要24-48小时）
- 检查CNAME文件内容

## 相关链接

- [GitHub Pages 文档](https://docs.github.com/en/pages)
- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Let's Encrypt](https://letsencrypt.org/)
