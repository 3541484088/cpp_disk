#include "xai_content_index.h"
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

XAIContentIndex& XAIContentIndex::Instance() {
    static XAIContentIndex instance;
    return instance;
}

static std::string GetCurrentTimeStr2() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return buf;
}

static std::string to_lower_ci(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string XAIContentIndex::GetStoragePath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path) == S_OK) {
        return std::string(path) + "\\xms_disk\\content_index.json";
    }
    return "content_index.json";
#else
    return std::string(getenv("HOME")) + "/.xms_disk/content_index.json";
#endif
}

void XAIContentIndex::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    text_entries_.clear();
    image_entries_.clear();

    std::string path = GetStoragePath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) return;

    // 加载文本索引
    if (root.isMember("text") && root["text"].isArray()) {
        for (const auto& item : root["text"]) {
            FileContentEntry entry;
            entry.filename   = item.get("filename", "").asString();
            entry.filedir    = item.get("filedir", "").asString();
            entry.preview    = item.get("preview", "").asString();
            entry.summary    = item.get("summary", "").asString();
            entry.filesize   = item.get("filesize", 0).asInt64();
            entry.index_time = item.get("index_time", "").asString();
            text_entries_.push_back(entry);
        }
    }

    // 加载图片索引
    if (root.isMember("images") && root["images"].isArray()) {
        for (const auto& item : root["images"]) {
            ImageDescEntry entry;
            entry.filename    = item.get("filename", "").asString();
            entry.filedir     = item.get("filedir", "").asString();
            entry.description = item.get("description", "").asString();
            entry.index_time  = item.get("index_time", "").asString();
            if (item.isMember("tags") && item["tags"].isArray()) {
                for (const auto& tag : item["tags"])
                    entry.tags.push_back(tag.asString());
            }
            image_entries_.push_back(entry);
        }
    }
}

void XAIContentIndex::Save() {
    // 注意：调用时必须已持有 mutex_，此处不再加锁
    std::string path = GetStoragePath();

#ifdef _WIN32
    std::string dir = path.substr(0, path.rfind('\\'));
    CreateDirectoryA(dir.c_str(), NULL);
#else
    std::string dir = path.substr(0, path.rfind('/'));
    mkdir(dir.c_str(), 0755);
#endif

    // 手动拼接 JSON，避免 JsonCpp 在子线程中的线程安全问题
    auto escapeStr = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:   out += c; break;
            }
        }
        return out;
    };

    std::ostringstream oss;
    oss << "{\n";

    // 文本索引
    oss << "  \"text\": [\n";
    for (size_t i = 0; i < text_entries_.size(); ++i) {
        const auto& e = text_entries_[i];
        oss << "    {\n";
        oss << "      \"filename\": \"" << escapeStr(e.filename) << "\",\n";
        oss << "      \"filedir\": \"" << escapeStr(e.filedir) << "\",\n";
        oss << "      \"preview\": \"" << escapeStr(e.preview) << "\",\n";
        oss << "      \"summary\": \"" << escapeStr(e.summary) << "\",\n";
        oss << "      \"filesize\": " << e.filesize << ",\n";
        oss << "      \"index_time\": \"" << escapeStr(e.index_time) << "\"\n";
        oss << "    }";
        if (i + 1 < text_entries_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ],\n";

    // 图片索引
    oss << "  \"images\": [\n";
    for (size_t i = 0; i < image_entries_.size(); ++i) {
        const auto& e = image_entries_[i];
        oss << "    {\n";
        oss << "      \"filename\": \"" << escapeStr(e.filename) << "\",\n";
        oss << "      \"filedir\": \"" << escapeStr(e.filedir) << "\",\n";
        oss << "      \"description\": \"" << escapeStr(e.description) << "\",\n";
        oss << "      \"index_time\": \"" << escapeStr(e.index_time) << "\",\n";
        oss << "      \"tags\": [";
        for (size_t j = 0; j < e.tags.size(); ++j) {
            oss << "\"" << escapeStr(e.tags[j]) << "\"";
            if (j + 1 < e.tags.size()) oss << ", ";
        }
        oss << "]\n";
        oss << "    }";
        if (i + 1 < image_entries_.size()) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";

    oss << "}\n";

    std::ofstream out(path);
    if (out.is_open()) {
        out << oss.str();
    }
}

void XAIContentIndex::IndexTextFile(const std::string& filename, const std::string& filedir,
                                     const std::string& local_path, int max_lines) {
    std::ifstream file(local_path);
    if (!file.is_open()) return;

    // 读取前 max_lines 行
    std::string preview;
    std::string line;
    int count = 0;
    while (std::getline(file, line) && count < max_lines) {
        preview += line + "\n";
        ++count;
        if (preview.size() > 4096) break;  // 限制最大 4KB
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // 更新或新增
    for (auto& entry : text_entries_) {
        if (entry.filename == filename && entry.filedir == filedir) {
            entry.preview = preview;
            entry.index_time = GetCurrentTimeStr2();
            Save();
            return;
        }
    }
    FileContentEntry entry;
    entry.filename   = filename;
    entry.filedir    = filedir;
    entry.preview    = preview;
    entry.index_time = GetCurrentTimeStr2();
    text_entries_.push_back(entry);
    Save();
}

void XAIContentIndex::SetFileSummary(const std::string& filename, const std::string& filedir,
                                      const std::string& summary) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : text_entries_) {
        if (entry.filename == filename && entry.filedir == filedir) {
            entry.summary = summary;
            Save();
            return;
        }
    }
}

std::vector<FileContentEntry> XAIContentIndex::SearchByContent(const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FileContentEntry> result;
    std::string kw = to_lower_ci(keyword);
    for (const auto& entry : text_entries_) {
        if (to_lower_ci(entry.preview).find(kw) != std::string::npos ||
            to_lower_ci(entry.summary).find(kw) != std::string::npos ||
            to_lower_ci(entry.filename).find(kw) != std::string::npos) {
            result.push_back(entry);
        }
    }
    return result;
}

FileContentEntry XAIContentIndex::GetFileContent(const std::string& filename, const std::string& filedir) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : text_entries_) {
        if (entry.filename == filename && entry.filedir == filedir)
            return entry;
    }
    return {};
}

void XAIContentIndex::IndexImage(const std::string& filename, const std::string& filedir,
                                  const std::string& description, const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : image_entries_) {
        if (entry.filename == filename && entry.filedir == filedir) {
            entry.description = description;
            entry.tags = tags;
            entry.index_time = GetCurrentTimeStr2();
            Save();
            return;
        }
    }
    ImageDescEntry entry;
    entry.filename    = filename;
    entry.filedir     = filedir;
    entry.description = description;
    entry.tags        = tags;
    entry.index_time  = GetCurrentTimeStr2();
    image_entries_.push_back(entry);
    Save();
}

std::vector<ImageDescEntry> XAIContentIndex::SearchImageByDesc(const std::string& keyword) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ImageDescEntry> result;
    std::string kw = to_lower_ci(keyword);
    for (const auto& entry : image_entries_) {
        bool match = to_lower_ci(entry.description).find(kw) != std::string::npos;
        if (!match) {
            for (const auto& tag : entry.tags) {
                if (to_lower_ci(tag).find(kw) != std::string::npos) {
                    match = true;
                    break;
                }
            }
        }
        if (!match) {
            match = to_lower_ci(entry.filename).find(kw) != std::string::npos;
        }
        if (match) result.push_back(entry);
    }
    return result;
}

ImageDescEntry XAIContentIndex::GetImageDesc(const std::string& filename, const std::string& filedir) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : image_entries_) {
        if (entry.filename == filename && entry.filedir == filedir)
            return entry;
    }
    return {};
}

std::vector<ImageDescEntry> XAIContentIndex::GetAllImages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return image_entries_;
}

std::vector<FileContentEntry> XAIContentIndex::GetAllTextEntries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return text_entries_;
}
