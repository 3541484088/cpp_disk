#include "xai_file_tags.h"
#include <json/json.h>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

XAIFileTags& XAIFileTags::Instance() {
    static XAIFileTags instance;
    return instance;
}

std::string XAIFileTags::GetStoragePath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
        return std::string(path) + "\\xms_disk\\file_tags.json";
    }
    return "file_tags.json";
#else
    return std::string(getenv("HOME")) + "/.xms_disk/file_tags.json";
#endif
}

static std::string GetCurrentTimeStr() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buf;
}

static std::string to_lower_tag(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

void XAIFileTags::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();

    std::string path = GetStoragePath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) return;

    if (!root.isArray()) return;
    for (const auto& item : root) {
        FileTagEntry entry;
        entry.filename = item.get("filename", "").asString();
        entry.filedir = item.get("filedir", "").asString();
        entry.description = item.get("description", "").asString();
        entry.add_time = item.get("add_time", "").asString();
        if (item.isMember("tags") && item["tags"].isArray()) {
            for (const auto& tag : item["tags"])
                entry.tags.push_back(tag.asString());
        }
        entries_.push_back(entry);
    }
}

void XAIFileTags::Save() {
    // 注意：调用时必须已持有 mutex_，此处不再加锁
    std::string path = GetStoragePath();

    // 确保目录存在
#ifdef _WIN32
    std::string dir = path.substr(0, path.rfind('\\'));
    CreateDirectoryA(dir.c_str(), NULL);
#else
    std::string dir = path.substr(0, path.rfind('/'));
    mkdir(dir.c_str(), 0755);
#endif

    // 手动拼接 JSON，避免 JsonCpp 线程安全问题
    std::ostringstream oss;
    oss << "[\n";
    for (size_t i = 0; i < entries_.size(); ++i) {
        const auto& entry = entries_[i];
        oss << "  {\n";
        oss << "    \"filename\": \"" << entry.filename << "\",\n";
        oss << "    \"filedir\": \"" << entry.filedir << "\",\n";
        oss << "    \"description\": \"" << entry.description << "\",\n";
        oss << "    \"add_time\": \"" << entry.add_time << "\",\n";
        oss << "    \"tags\": [";
        for (size_t j = 0; j < entry.tags.size(); ++j) {
            oss << "\"" << entry.tags[j] << "\"";
            if (j + 1 < entry.tags.size()) oss << ", ";
        }
        oss << "]\n";
        oss << "  }";
        if (i + 1 < entries_.size()) oss << ",";
        oss << "\n";
    }
    oss << "]\n";

    std::ofstream out(path);
    if (out.is_open()) {
        out << oss.str();
    }
}

void XAIFileTags::SetTags(const std::string& filename, const std::string& filedir,
                           const std::vector<std::string>& tags, const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 查找已有条目
    for (auto& entry : entries_) {
        if (entry.filename == filename && entry.filedir == filedir) {
            entry.tags = tags;
            if (!description.empty()) entry.description = description;
            entry.add_time = GetCurrentTimeStr();
            Save();
            return;
        }
    }
    // 新条目
    FileTagEntry entry;
    entry.filename = filename;
    entry.filedir = filedir;
    entry.tags = tags;
    entry.description = description;
    entry.add_time = GetCurrentTimeStr();
    entries_.push_back(entry);
    Save();
}

std::vector<FileTagEntry> XAIFileTags::SearchByTag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FileTagEntry> result;
    std::string tag_lower = to_lower_tag(tag);
    for (const auto& entry : entries_) {
        for (const auto& t : entry.tags) {
            if (to_lower_tag(t).find(tag_lower) != std::string::npos) {
                result.push_back(entry);
                break;
            }
        }
    }
    return result;
}

std::vector<FileTagEntry> XAIFileTags::SearchByDescription(const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FileTagEntry> result;
    std::string kw_lower = to_lower_tag(keyword);
    for (const auto& entry : entries_) {
        if (to_lower_tag(entry.description).find(kw_lower) != std::string::npos) {
            result.push_back(entry);
        }
    }
    return result;
}

FileTagEntry XAIFileTags::GetFileTags(const std::string& filename, const std::string& filedir) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : entries_) {
        if (entry.filename == filename && entry.filedir == filedir)
            return entry;
    }
    return {};
}

void XAIFileTags::RemoveTags(const std::string& filename, const std::string& filedir) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&](const FileTagEntry& e) { return e.filename == filename && e.filedir == filedir; }),
        entries_.end());
    Save();
}

std::vector<FileTagEntry> XAIFileTags::GetAllEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}
