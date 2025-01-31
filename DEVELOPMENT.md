# SpinningMomo 开发文档

## 项目结构

```
SpinningMomo/
├── frontend/                # 前端项目目录
│   ├── src/
│   │   ├── views/          # 页面组件
│   │   │   ├── Album/      # 相册相关页面
│   │   │   └── Settings/   # 设置页面
│   │   ├── components/     # 通用组件
│   │   ├── api/           # API接口封装
│   │   ├── store/         # 状态管理
│   │   └── utils/         # 工具函数
│   └── public/            # 静态资源
├── backend/                # 后端项目目录
│   ├── api/               # RESTful API处理
│   ├── database/          # 数据库操作
│   ├── models/            # 数据模型
│   └── services/          # 业务逻辑
├── src/                   # 原有的C++源代码
└── docs/                  # 项目文档
```

## 技术栈

### 前端
- Vue 3 + TypeScript
- Naive UI
- Pinia (状态管理)
- Vite (构建工具)

### 后端
- C++ 17
- cpp-httplib (HTTP服务器)
- SQLite3 (数据库)
- nlohmann/json (JSON处理)
- spdlog (日志处理)

## 开发规范

### 代码风格
- 使用TypeScript进行前端开发
- 使用ESLint + Prettier进行代码格式化
- C++代码遵循项目现有的代码风格
- 使用clang-format进行C++代码格式化

### Git提交规范
提交信息格式：
```
<type>(<scope>): <subject>

<body>
```

type类型：
- feat: 新功能
- fix: 修复bug
- docs: 文档更新
- style: 代码格式调整
- refactor: 重构
- test: 测试相关
- chore: 构建/工具链相关

### 前端开发规范

#### 组件命名
- 使用PascalCase命名组件
- 页面组件以Page结尾
- 通用组件使用描述性名称

#### 状态管理
- 使用Pinia进行状态管理
- 按功能模块划分store
- 避免滥用全局状态

### 后端开发规范

#### API设计
- 遵循RESTful设计原则
- 使用JSON格式进行数据交换
- 统一的错误处理机制

#### 错误码规范
- 200: 成功
- 400: 请求参数错误
- 401: 未授权
- 403: 禁止访问
- 404: 资源不存在
- 500: 服务器内部错误

## API接口文档

### 照片相关接口

#### 获取照片列表
```
GET /api/photos
```

请求参数：
```typescript
{
  page?: number;      // 页码
  pageSize?: number;  // 每页数量
  albumId?: number;   // 相册ID
  tags?: string[];    // 标签列表
}
```

响应：
```typescript
{
  total: number;      // 总数
  items: Photo[];     // 照片列表
}
```

#### 获取照片详情
```
GET /api/photos/:id
```

响应：
```typescript
{
  id: number;
  filename: string;
  filepath: string;
  width: number;
  height: number;
  createdAt: string;
  tags: string[];
}
```

### 相册相关接口

#### 创建相册
```
POST /api/albums
```

请求体：
```typescript
{
  name: string;
  description?: string;
}
```

响应：
```typescript
{
  id: number;
  name: string;
  description: string;
  createdAt: string;
}
```

## 开发流程

### 1. 环境搭建
1. 安装Node.js和npm
2. 安装C++开发环境
3. 安装SQLite3
4. 克隆项目并安装依赖

### 2. 开发步骤
1. 从main分支创建新的功能分支
2. 进行开发和测试
3. 提交代码并创建Pull Request
4. 代码审查
5. 合并到主分支

### 3. 测试规范
- 单元测试覆盖率要求：>80%
- 提交前必须通过所有测试
- 新功能必须包含测试用例

### 4. 部署流程
1. 构建前端项目
2. 构建后端项目
3. 运行数据库迁移
4. 部署更新

## 注意事项

1. 安全性考虑
   - 所有用户输入必须进行验证
   - 防止SQL注入
   - 防止XSS攻击
   - 文件上传限制

2. 性能优化
   - 图片懒加载
   - 合理的缓存策略
   - 数据库索引优化
   - 大文件处理优化

3. 兼容性考虑
   - WebView兼容性
   - 不同分辨率适配
   - 响应式设计

## 常见问题

### 1. 开发环境配置
Q: 如何配置开发环境？
A: 参考项目根目录的README.md文件进行环境配置。

### 2. 数据库操作
Q: 如何进行数据库迁移？
A: 使用项目提供的迁移工具，详见DATABASE.md。

### 3. API调试
Q: 如何调试API接口？
A: 使用Postman或项目提供的API调试工具。 