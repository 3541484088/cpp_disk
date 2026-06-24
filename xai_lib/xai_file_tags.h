#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// 文件标签条目
struct FileTagEntry {
    std::string filename;       // 文件名
    std::string filedir;        // 文件所在目录
    std::vector<std::string> tags;  // 标签列表
    std::string description;    // 用户描述
    std::string add_time;       // 添加时间
};

// 文件标签管理器（本地 JSON 文件存储）
class XAIFileTags {
public:
    static XAIFileTags& Instance();

    // 添加/更新文件标签
    void SetTags(const std::string& filename, const std::string& filedir,
                 const std::vector<std::string>& tags, const std::string& description = "");

    // 按标签搜索文件
    std::vector<FileTagEntry> SearchByTag(const std::string& tag) const;

    // 按描述关键词搜索
    std::vector<FileTagEntry> SearchByDescription(const std::string& keyword) const;

    // 获取某文件的标签
    FileTagEntry GetFileTags(const std::string& filename, const std::string& filedir) const;

    // 删除文件标签
    void RemoveTags(const std::string& filename, const std::string& filedir);

    // 获取所有标签条目
    std::vector<FileTagEntry> GetAllEntries() const;

    // 加载/保存
    void Load();
    void Save();

private:
    XAIFileTags() { Load(); }
    mutable std::mutex mutex_;
    std::vector<FileTagEntry> entries_;
    std::string GetStoragePath() const;
};
