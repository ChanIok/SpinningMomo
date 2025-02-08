#pragma once

#include "media/db/models.hpp"
#include "media/repositories/album_repository.hpp"
#include <string>
#include <vector>

// 相册服务类 - 处理业务逻辑
class AlbumService {
public:
    // 获取单例实例
    static AlbumService& get_instance();

    // 创建新的相册
    Album create_album(const std::string& name, const std::string& description = "");

    // 获取相册列表
    std::vector<Album> get_albums(bool include_deleted = false);

    // 获取单个相册
    Album get_album(int64_t id);

    // 更新相册信息
    bool update_album(Album& album);

    // 删除相册
    bool delete_album(int64_t id);

    // 相册-截图关联操作
    bool add_screenshot_to_album(int64_t album_id, int64_t screenshot_id);
    bool add_screenshots_to_album(int64_t album_id, const std::vector<int64_t>& screenshot_ids);
    bool remove_screenshot_from_album(int64_t album_id, int64_t screenshot_id);
    std::vector<Screenshot> get_album_screenshots(int64_t album_id);
    bool set_album_cover(int64_t album_id, int64_t screenshot_id);

private:
    AlbumService() = default;
    ~AlbumService() = default;

    // 禁止拷贝和赋值
    AlbumService(const AlbumService&) = delete;
    AlbumService& operator=(const AlbumService&) = delete;

    // 获取相册中最后一个位置编号
    int get_last_position(int64_t album_id);

    // 数据访问层
    AlbumRepository& repository_ = AlbumRepository::get_instance();
}; 
