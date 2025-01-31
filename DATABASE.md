# SpinningMomo 数据库设计文档

## 数据库概述

SpinningMomo项目使用SQLite3作为数据库管理系统。SQLite3是一个轻量级的、嵌入式的关系型数据库，非常适合桌面应用程序使用。主要用于存储游戏截图的元数据和相册信息。

## 数据库表结构

### 1. screenshots（截图表）
存储游戏截图的基本信息。注意：不存储实际图片文件，只存储元数据。

```sql
CREATE TABLE screenshots (
    id INTEGER PRIMARY KEY,
    filename TEXT NOT NULL,           -- 文件名
    filepath TEXT NOT NULL,           -- 文件路径
    created_at DATETIME NOT NULL,     -- 创建时间
    width INTEGER NOT NULL,           -- 图片宽度
    height INTEGER NOT NULL,          -- 图片高度
    file_size INTEGER NOT NULL,       -- 文件大小(bytes)
    metadata TEXT,                    -- 元数据(JSON格式)
    deleted_at DATETIME,              -- 软删除时间
    updated_at DATETIME NOT NULL      -- 更新时间
);

-- 索引
CREATE INDEX idx_screenshots_created_at ON screenshots(created_at);
CREATE INDEX idx_screenshots_deleted_at ON screenshots(deleted_at);
```

### 2. screenshot_albums（相册表）
存储相册信息。

```sql
CREATE TABLE screenshot_albums (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,               -- 相册名称
    description TEXT,                 -- 相册描述
    cover_screenshot_id INTEGER,      -- 封面截图ID
    created_at DATETIME NOT NULL,     -- 创建时间
    updated_at DATETIME NOT NULL,     -- 更新时间
    deleted_at DATETIME,              -- 软删除时间
    FOREIGN KEY (cover_screenshot_id) REFERENCES screenshots(id)
);

-- 索引
CREATE INDEX idx_screenshot_albums_created_at ON screenshot_albums(created_at);
CREATE INDEX idx_screenshot_albums_deleted_at ON screenshot_albums(deleted_at);
```

### 3. album_screenshots（相册截图关联表）
管理相册和截图的多对多关系。

```sql
CREATE TABLE album_screenshots (
    album_id INTEGER NOT NULL,
    screenshot_id INTEGER NOT NULL,
    position INTEGER NOT NULL,        -- 截图在相册中的位置
    created_at DATETIME NOT NULL,     -- 添加时间
    FOREIGN KEY (album_id) REFERENCES screenshot_albums(id) ON DELETE CASCADE,
    FOREIGN KEY (screenshot_id) REFERENCES screenshots(id) ON DELETE CASCADE,
    PRIMARY KEY (album_id, screenshot_id)
);

-- 索引
CREATE INDEX idx_album_screenshots_album_id ON album_screenshots(album_id);
CREATE INDEX idx_album_screenshots_screenshot_id ON album_screenshots(screenshot_id);
CREATE INDEX idx_album_screenshots_position ON album_screenshots(position);
```

## 数据库迁移

### 迁移文件位置
```
src/media/database/migrations/
├── 001_initial_schema.sql
├── 002_add_indexes.sql
└── 003_add_game_parameters.sql
```

### 迁移命令
```bash
# 创建新的迁移文件
./tools/create_migration.sh "migration_name"

# 应用迁移
./tools/migrate.sh

# 回滚最后一次迁移
./tools/rollback.sh
```

## 数据库维护

### 1. 备份策略
- 自动备份：每天凌晨3点
- 备份位置：用户数据目录下的`backup/`
- 保留时间：最近7天

### 2. 性能优化
1. 索引优化
   - 已创建常用查询字段的索引
   - 避免过度索引
   
2. 查询优化
   - 使用预编译语句
   - 合理使用事务
   - 分页查询限制

### 3. 数据完整性
1. 外键约束
   - 使用ON DELETE CASCADE确保关联数据的一致性
   
2. 数据验证
   - 文件路径必须存在
   - 分辨率和比例设置必须有效

## 注意事项

1. **文件管理**
   - 数据库只存储文件路径，不存储实际文件
   - 定期检查文件完整性
   - 处理游戏删除截图的情况

2. **事务处理**
   - 涉及多表操作时使用事务
   - 批量操作时使用事务提升性能

3. **并发处理**
   - 使用事务确保数据一致性
   - 实现乐观锁避免并发冲突

4. **性能监控**
   - 定期检查索引使用情况
   - 监控查询性能
   - 及时优化慢查询

## 常见问题

### 1. 文件同步
Q: 如何处理游戏删除截图的情况？
A: 定期检查文件是否存在，如果不存在则标记为已删除（软删除）。

### 2. 性能问题
Q: 大量截图时查询性能如何优化？
A: 
1. 使用分页加载
2. 实现缓存机制
3. 优化索引使用

### 3. 数据一致性
Q: 如何确保数据一致性？
A: 
1. 使用事务进行操作
2. 定期进行数据完整性检查
3. 实现定期备份策略 