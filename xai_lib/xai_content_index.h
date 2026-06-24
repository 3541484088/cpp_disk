#pragma once

#include <string>
#include <vector>
#include <mutex>

// 文件内容索引条目
struct FileContentEntry {
    std::string filename;       // 文件名
    std::string filedir;        // 文件所在目录
    std::string preview;        // 文件前N行内容预览
    std::string summary;        // AI 生成的摘要（可选）
    int64_t     filesize = 0;   // 文件大小
    std::string index_time;     // 索引时间
};

// 图片描述索引条目
struct ImageDescEntry {
    std::string filename;       // 文件名
    std::string filedir;        // 文件所在目录
    std::string description;    // AI 生成的图片描述
    std::vector<std::string> tags;  // 自动标签
    std::string index_time;     // 索引时间
};

// 文件内容索引管理器
class XAIContentIndex {
public:
    static XAIContentIndex& Instance();

    // 文件内容索引
    void IndexTextFile(const std::string& filename, const std::string& filedir,
                       const std::string& local_path, int max_lines = 50);
    void SetFileSummary(const std::string& filename, const std::string& filedir,
                        const std::string& summary);
    std::vector<FileContentEntry> SearchByContent(const std::string& keyword) const;
    FileContentEntry GetFileContent(const std::string& filename, const std::string& filedir) const;

    // 图片描述索引
    void IndexImage(const std::string& filename, const std::string& filedir,
                    const std::string& description, const std::vector<std::string>& tags = {});
    std::vector<ImageDescEntry> SearchImageByDesc(const std::string& keyword) const;
    ImageDescEntry GetImageDesc(const std::string& filename, const std::string& filedir) const;
    std::vector<ImageDescEntry> GetAllImages() const;

    // 获取所有文本索引
    std::vector<FileContentEntry> GetAllTextEntries() const;

    void Load();
    void Save();

private:
    XAIContentIndex() { Load(); }
    mutable std::mutex mutex_;
    std::vector<FileContentEntry> text_entries_;
    std::vector<ImageDescEntry>   image_entries_;
    std::string GetStoragePath() const;
};
