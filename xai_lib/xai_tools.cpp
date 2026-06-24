#include "xai_tools.h"
#include "xai_file_tags.h"
#include "xai_content_index.h"
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string FormatFileList(const xdisk::XFileInfoList& list) {
    std::ostringstream ss;
    ss << "共 " << list.files_size() << " 个文件：\n";
    for (int i = 0; i < list.files_size(); ++i) {
        const auto& f = list.files(i);
        ss << "  - " << f.filename();
        if (f.is_dir()) ss << "/ (目录)";
        else ss << " (" << f.filesize() << " 字节, " << f.filetime() << ")";
        ss << "\n";
    }
    return ss.str();
}

void RegisterBuiltinTools(XAIToolRegistry& registry, IFileContext* ctx) {

    // ---- list_directory ----
    registry.Register({
        "list_directory",
        "列出指定目录下的文件和子目录，不填则列出当前目录",
        {{"path", "string", "目录路径，留空为当前目录", false}},
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string path;
            auto it = p.find("path");
            if (it == p.end() || it->second.empty())
                path = ctx->GetCurrentDir();
            else
                path = it->second;

            xdisk::XFileInfoList files = ctx->GetDirFiles(path);
            if (files.files_size() == 0)
                return std::string("目录为空或路径不存在：") + path;
            return "目录 [" + path + "] " + FormatFileList(files);
        }
    });

    // ---- search_files ----
    registry.Register({
        "search_files",
        "在当前目录中按条件搜索文件（文件名关键词、类型、大小范围）",
        {
            {"keyword",   "string",  "文件名关键词（不区分大小写）", false},
            {"file_type", "string",  "文件扩展名，如 txt、jpg",      false},
            {"min_size",  "integer", "最小大小（字节）",              false},
            {"max_size",  "integer", "最大大小（字节）",              false},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            auto get = [&](const std::string& k) -> std::string {
                auto it = p.find(k);
                return it != p.end() ? it->second : "";
            };
            std::string keyword   = to_lower(get("keyword"));
            std::string file_type = to_lower(get("file_type"));
            int64_t min_size = get("min_size").empty() ? -1 : std::stoll(get("min_size"));
            int64_t max_size = get("max_size").empty() ? -1 : std::stoll(get("max_size"));

            xdisk::XFileInfoList all = ctx->GetCurrentFiles();
            std::ostringstream ss;
            int count = 0;
            for (int i = 0; i < all.files_size(); ++i) {
                const auto& f = all.files(i);
                std::string name = to_lower(f.filename());

                if (!keyword.empty() && name.find(keyword) == std::string::npos) continue;

                if (!file_type.empty()) {
                    auto dot = name.rfind('.');
                    if (dot == std::string::npos || name.substr(dot + 1) != file_type) continue;
                }
                if (min_size >= 0 && f.filesize() < min_size) continue;
                if (max_size >= 0 && f.filesize() > max_size) continue;

                ss << "  - " << f.filename();
                if (f.is_dir()) ss << "/ (目录)";
                else ss << " (" << f.filesize() << " 字节)";
                ss << "\n";
                ++count;
            }
            if (count == 0) return std::string("未找到符合条件的文件");
            return "找到 " + std::to_string(count) + " 个文件：\n" + ss.str();
        }
    });

    // ---- get_file_info ----
    registry.Register({
        "get_file_info",
        "获取当前目录中某个文件的详细信息",
        {{"filename", "string", "文件名", true}},
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string target = to_lower(p.at("filename"));
            xdisk::XFileInfoList all = ctx->GetCurrentFiles();
            for (int i = 0; i < all.files_size(); ++i) {
                const auto& f = all.files(i);
                if (to_lower(f.filename()) == target) {
                    std::ostringstream ss;
                    ss << "文件名: " << f.filename() << "\n"
                       << "类型: " << (f.is_dir() ? "目录" : "文件") << "\n"
                       << "大小: " << f.filesize() << " 字节\n"
                       << "修改时间: " << f.filetime() << "\n"
                       << "目录: " << f.filedir();
                    return ss.str();
                }
            }
            return std::string("未找到文件: ") + p.at("filename");
        }
    });

    // ---- get_transfer_tasks ----
    registry.Register({
        "get_transfer_tasks",
        "查看当前的上传、下载和已完成的传输任务列表",
        {{"type", "string", "任务类型：upload(上传中)、download(下载中)、completed(已完成)，留空返回全部", false}},
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string type;
            auto it = p.find("type");
            if (it != p.end()) type = to_lower(it->second);

            std::ostringstream ss;

            auto format_tasks = [](const std::vector<TransferTaskSnapshot>& tasks, const std::string& title) {
                std::ostringstream s;
                s << title << "（共 " << tasks.size() << " 个）：\n";
                if (tasks.empty()) {
                    s << "  （无）\n";
                } else {
                    for (const auto& t : tasks) {
                        s << "  - " << t.filename << " (" << t.filesize << " 字节)";
                        if (!t.tasktime.empty()) s << " [" << t.tasktime << "]";
                        if (t.is_complete) s << " ✓已完成";
                        s << "\n";
                    }
                }
                return s.str();
            };

            if (type.empty() || type == "upload") {
                ss << format_tasks(ctx->GetUploadTasks(), "上传任务");
            }
            if (type.empty() || type == "download") {
                ss << format_tasks(ctx->GetDownloadTasks(), "下载任务");
            }
            if (type.empty() || type == "completed") {
                ss << format_tasks(ctx->GetCompletedTasks(), "已完成任务");
            }
            return ss.str();
        }
    });

    // ---- get_disk_info ----
    registry.Register({
        "get_disk_info",
        "查看网盘的磁盘空间使用情况（总空间、已用、可用）",
        {},
        [ctx](const std::unordered_map<std::string, std::string>&) {
            DiskInfoSnapshot info = ctx->GetDiskInfo();
            std::ostringstream ss;

            auto format_size = [](int64_t bytes) -> std::string {
                if (bytes < 1024) return std::to_string(bytes) + " B";
                if (bytes < 1024*1024) return std::to_string(bytes/1024) + " KB";
                if (bytes < 1024LL*1024*1024) return std::to_string(bytes/(1024*1024)) + " MB";
                return std::to_string(bytes/(1024LL*1024*1024)) + " GB";
            };

            ss << "磁盘空间信息：\n"
               << "  总空间: " << format_size(info.total) << "\n"
               << "  可用空间: " << format_size(info.avail) << "\n"
               << "  空闲空间: " << format_size(info.free_space) << "\n"
               << "  当前目录大小: " << format_size(info.dir_size) << "\n";

            if (info.total > 0) {
                int used_percent = (int)((info.total - info.free_space) * 100 / info.total);
                ss << "  使用率: " << used_percent << "%\n";
            }
            return ss.str();
        }
    });

    // ---- get_shared_folders ----
    registry.Register({
        "get_shared_folders",
        "列出当前用户的所有共享文件夹信息",
        {},
        [ctx](const std::unordered_map<std::string, std::string>&) {
            auto folders = ctx->GetSharedFolders();
            if (folders.empty()) return std::string("当前没有共享文件夹");

            std::ostringstream ss;
            ss << "共享文件夹列表（共 " << folders.size() << " 个）：\n";
            for (const auto& f : folders) {
                ss << "  - " << f.name << " (所有者: " << f.owner
                   << ", 成员: " << f.user_count
                   << "人, 文件: " << f.file_count << "个";
                if (!f.created_at.empty()) ss << ", 创建于: " << f.created_at;
                ss << ")\n";
            }
            return ss.str();
        }
    });

    // ---- tag_file ----
    registry.Register({
        "tag_file",
        "为文件添加标签和描述，方便后续按标签或描述搜索文件",
        {
            {"filename", "string", "文件名", true},
            {"tags",     "string", "标签列表，用逗号分隔，如: 医疗,报告,重要", true},
            {"description", "string", "文件描述（用自然语言描述文件内容）", false},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            auto fn_it = p.find("filename");
            auto tags_it = p.find("tags");
            if (fn_it == p.end() || fn_it->second.empty())
                return std::string("错误：缺少 filename 参数");
            if (tags_it == p.end() || tags_it->second.empty())
                return std::string("错误：缺少 tags 参数");

            std::string filename = fn_it->second;
            std::string tags_str = tags_it->second;
            std::string desc;
            auto it = p.find("description");
            if (it != p.end()) desc = it->second;

            // 解析逗号分隔的标签
            std::vector<std::string> tags;
            std::istringstream ss(tags_str);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                // 去除首尾空格
                size_t start = tag.find_first_not_of(" ");
                size_t end = tag.find_last_not_of(" ");
                if (start != std::string::npos)
                    tags.push_back(tag.substr(start, end - start + 1));
            }

            if (tags.empty())
                return std::string("错误：标签列表为空");

            try {
                std::string filedir = ctx->GetCurrentDir();
                XAIFileTags::Instance().SetTags(filename, filedir, tags, desc);
                return std::string("已为文件 [") + filename + "] 添加标签: " + tags_str +
                       (desc.empty() ? "" : "，描述: " + desc);
            } catch (const std::exception& e) {
                return std::string("标记文件失败: ") + e.what();
            }
        }
    });

    // ---- search_by_tag ----
    registry.Register({
        "search_by_tag",
        "按标签或描述关键词搜索已标记的文件",
        {
            {"keyword", "string", "搜索关键词（匹配标签或描述）", true},
        },
        [](const std::unordered_map<std::string, std::string>& p) {
            std::string keyword = p.at("keyword");

            // 先按标签搜
            auto by_tag = XAIFileTags::Instance().SearchByTag(keyword);
            // 再按描述搜
            auto by_desc = XAIFileTags::Instance().SearchByDescription(keyword);

            // 合并去重
            std::vector<FileTagEntry> results = by_tag;
            for (const auto& d : by_desc) {
                bool dup = false;
                for (const auto& r : results) {
                    if (r.filename == d.filename && r.filedir == d.filedir) { dup = true; break; }
                }
                if (!dup) results.push_back(d);
            }

            if (results.empty())
                return std::string("未找到与 [" + keyword + "] 相关的已标记文件");

            std::ostringstream ss;
            ss << "找到 " << results.size() << " 个匹配文件：\n";
            for (const auto& r : results) {
                ss << "  - " << r.filename << " (目录: " << r.filedir << ")\n";
                if (!r.tags.empty()) {
                    ss << "    标签: ";
                    for (size_t i = 0; i < r.tags.size(); ++i) {
                        if (i > 0) ss << ", ";
                        ss << r.tags[i];
                    }
                    ss << "\n";
                }
                if (!r.description.empty())
                    ss << "    描述: " << r.description << "\n";
            }
            return ss.str();
        }
    });

    // ---- batch_delete ----
    registry.Register({
        "batch_delete",
        "批量删除当前目录中的多个文件（需要用户确认）。返回待删除列表供确认。",
        {
            {"filenames", "string", "要删除的文件名列表，用逗号分隔", true},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string filenames_str = p.at("filenames");
            std::vector<std::string> names;
            std::istringstream ss(filenames_str);
            std::string name;
            while (std::getline(ss, name, ',')) {
                size_t start = name.find_first_not_of(" ");
                size_t end = name.find_last_not_of(" ");
                if (start != std::string::npos)
                    names.push_back(name.substr(start, end - start + 1));
            }

            // 验证文件是否存在
            xdisk::XFileInfoList all = ctx->GetCurrentFiles();
            std::vector<std::string> found, not_found;
            for (const auto& n : names) {
                bool exists = false;
                std::string n_lower = to_lower(n);
                for (int i = 0; i < all.files_size(); ++i) {
                    if (to_lower(all.files(i).filename()) == n_lower) {
                        exists = true;
                        found.push_back(all.files(i).filename());
                        break;
                    }
                }
                if (!exists) not_found.push_back(n);
            }

            std::ostringstream result;
            if (!found.empty()) {
                result << "以下 " << found.size() << " 个文件将被删除：\n";
                for (const auto& f : found) result << "  - " << f << "\n";
                result << "（请在最终回复中使用 action:delete_confirm 和 files 数组来触发删除确认）";
            }
            if (!not_found.empty()) {
                result << "未找到以下文件：\n";
                for (const auto& f : not_found) result << "  - " << f << "\n";
            }
            return result.str();
        }
    });

    // ---- search_all_dirs ----
    registry.Register({
        "search_all_dirs",
        "在当前目录及其所有子目录中递归搜索文件（按文件名关键词）",
        {
            {"keyword", "string", "文件名关键词（不区分大小写）", true},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string keyword = to_lower(p.at("keyword"));
            std::ostringstream ss;
            int count = 0;
            const int max_depth = 3;  // 最多递归3层

            // BFS 遍历目录树
            struct DirEntry { std::string path; int depth; };
            std::vector<DirEntry> queue;
            queue.push_back({ctx->GetCurrentDir(), 0});

            for (size_t qi = 0; qi < queue.size() && qi < 50; ++qi) {
                const std::string cur_path = queue[qi].path;
                int depth = queue[qi].depth;

                xdisk::XFileInfoList files;
                if (depth == 0) {
                    files = ctx->GetCurrentFiles();
                } else {
                    files = ctx->GetDirFilesSync(cur_path);
                }

                for (int i = 0; i < files.files_size(); ++i) {
                    const auto& f = files.files(i);
                    // 将子目录加入队列继续搜索
                    if (f.is_dir() && depth < max_depth) {
                        std::string sub = cur_path;
                        if (!sub.empty() && sub.back() != '/') sub += "/";
                        sub += f.filename();
                        queue.push_back({sub, depth + 1});
                    }
                    // 匹配文件名
                    std::string name_lower = to_lower(f.filename());
                    if (name_lower.find(keyword) != std::string::npos) {
                        ss << "  - " << f.filename();
                        if (f.is_dir()) ss << "/ (目录)";
                        else ss << " (" << f.filesize() << " 字节)";
                        ss << " [" << cur_path << "]\n";
                        ++count;
                    }
                }
            }

            if (count == 0) return std::string("未找到包含 [" + p.at("keyword") + "] 的文件");
            return "找到 " + std::to_string(count) + " 个文件：\n" + ss.str();
        }
    });

    // ---- search_by_content ----
    registry.Register({
        "search_by_content",
        "按文件内容关键词搜索已索引的文本文件（搜索文件内容预览和摘要）",
        {
            {"keyword", "string", "要搜索的内容关键词", true},
        },
        [](const std::unordered_map<std::string, std::string>& p) {
            std::string keyword = p.at("keyword");
            auto results = XAIContentIndex::Instance().SearchByContent(keyword);

            if (results.empty())
                return std::string("未找到内容包含 [" + keyword + "] 的已索引文件");

            std::ostringstream ss;
            ss << "找到 " << results.size() << " 个文件内容匹配：\n";
            for (const auto& r : results) {
                ss << "  - " << r.filename << " (目录: " << r.filedir << ")\n";
                if (!r.summary.empty())
                    ss << "    摘要: " << r.summary << "\n";
                else if (!r.preview.empty()) {
                    // 显示预览的前 100 个字符
                    std::string prev = r.preview.substr(0, 100);
                    if (r.preview.size() > 100) prev += "...";
                    ss << "    预览: " << prev << "\n";
                }
            }
            return ss.str();
        }
    });

    // ---- get_file_preview ----
    registry.Register({
        "get_file_preview",
        "获取已索引文本文件的内容预览（前50行）",
        {
            {"filename", "string", "文件名", true},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string filename = p.at("filename");
            std::string filedir = ctx->GetCurrentDir();
            auto entry = XAIContentIndex::Instance().GetFileContent(filename, filedir);

            if (entry.filename.empty())
                return std::string("文件 [" + filename + "] 尚未建立内容索引");

            std::ostringstream ss;
            ss << "文件: " << entry.filename << "\n";
            if (!entry.summary.empty())
                ss << "摘要: " << entry.summary << "\n";
            ss << "--- 内容预览 ---\n" << entry.preview;
            return ss.str();
        }
    });

    // ---- index_file_content ----
    registry.Register({
        "index_file_content",
        "为文本文件建立内容索引（读取文件前50行存入索引，支持后续按内容搜索）",
        {
            {"filename", "string", "文件名", true},
            {"local_path", "string", "文件本地路径（上传前的本地路径）", true},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string filename = p.at("filename");
            std::string local_path = p.at("local_path");
            std::string filedir = ctx->GetCurrentDir();

            XAIContentIndex::Instance().IndexTextFile(filename, filedir, local_path);
            return "已为文件 [" + filename + "] 建立内容索引";
        }
    });

    // ---- search_images ----
    registry.Register({
        "search_images",
        "按描述关键词搜索已索引的图片（搜索图片的AI生成描述和标签）",
        {
            {"keyword", "string", "搜索关键词（如：风景、证件照、合照、建筑等）", true},
        },
        [](const std::unordered_map<std::string, std::string>& p) {
            std::string keyword = p.at("keyword");
            auto results = XAIContentIndex::Instance().SearchImageByDesc(keyword);

            if (results.empty())
                return std::string("未找到与 [" + keyword + "] 相关的已索引图片。提示：需要先对图片进行描述索引。");

            std::ostringstream ss;
            ss << "找到 " << results.size() << " 张匹配图片：\n";
            for (const auto& r : results) {
                ss << "  - " << r.filename << " (目录: " << r.filedir << ")\n";
                ss << "    描述: " << r.description << "\n";
                if (!r.tags.empty()) {
                    ss << "    标签: ";
                    for (size_t i = 0; i < r.tags.size(); ++i) {
                        if (i > 0) ss << ", ";
                        ss << r.tags[i];
                    }
                    ss << "\n";
                }
            }
            return ss.str();
        }
    });

    // ---- index_image_desc ----
    registry.Register({
        "index_image_desc",
        "为图片添加描述和标签索引（通常由AI分析图片内容后调用）",
        {
            {"filename", "string", "图片文件名", true},
            {"description", "string", "图片内容描述（如：蓝天白云的海边风景照）", true},
            {"tags", "string", "图片标签，逗号分隔（如：风景,海边,蓝天）", false},
        },
        [ctx](const std::unordered_map<std::string, std::string>& p) {
            std::string filename = p.at("filename");
            std::string description = p.at("description");
            std::string filedir = ctx->GetCurrentDir();

            std::vector<std::string> tags;
            auto it = p.find("tags");
            if (it != p.end() && !it->second.empty()) {
                std::istringstream ss(it->second);
                std::string tag;
                while (std::getline(ss, tag, ',')) {
                    size_t start = tag.find_first_not_of(" ");
                    size_t end = tag.find_last_not_of(" ");
                    if (start != std::string::npos)
                        tags.push_back(tag.substr(start, end - start + 1));
                }
            }

            XAIContentIndex::Instance().IndexImage(filename, filedir, description, tags);
            return "已为图片 [" + filename + "] 建立描述索引: " + description;
        }
    });

    // ---- suggest_organize ----
    registry.Register({
        "suggest_organize",
        "分析当前目录的文件结构，生成智能分类整理建议。返回文件类型统计和建议的分类方案。",
        {},
        [ctx](const std::unordered_map<std::string, std::string>&) {
            xdisk::XFileInfoList all = ctx->GetCurrentFiles();
            if (all.files_size() == 0)
                return std::string("当前目录为空，无需整理");

            // 统计文件类型
            std::unordered_map<std::string, std::vector<std::string>> by_ext;
            std::vector<std::string> dirs;
            int total_files = 0;
            int64_t total_size = 0;

            for (int i = 0; i < all.files_size(); ++i) {
                const auto& f = all.files(i);
                if (f.is_dir()) {
                    dirs.push_back(f.filename());
                    continue;
                }
                total_files++;
                total_size += f.filesize();

                std::string name = f.filename();
                std::string ext = "无扩展名";
                size_t dot = name.rfind('.');
                if (dot != std::string::npos) {
                    ext = name.substr(dot);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                }
                by_ext[ext].push_back(name);
            }

            std::ostringstream ss;
            ss << "当前目录分析：\n";
            ss << "  文件总数: " << total_files << " 个\n";
            ss << "  子目录数: " << dirs.size() << " 个\n";
            ss << "  总大小: " << (total_size / (1024*1024)) << " MB\n\n";

            ss << "文件类型分布：\n";

            // 定义分类规则
            struct Category {
                std::string name;
                std::vector<std::string> extensions;
            };
            std::vector<Category> categories = {
                {"文档", {".doc", ".docx", ".pdf", ".txt", ".md", ".rtf", ".odt", ".xls", ".xlsx", ".ppt", ".pptx", ".csv"}},
                {"图片", {".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp", ".svg", ".ico", ".tiff"}},
                {"视频", {".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"}},
                {"音频", {".mp3", ".wav", ".flac", ".aac", ".ogg", ".wma"}},
                {"压缩包", {".zip", ".rar", ".7z", ".tar", ".gz", ".bz2"}},
                {"代码", {".cpp", ".h", ".py", ".js", ".ts", ".java", ".c", ".cs", ".go", ".rs"}},
            };

            std::unordered_map<std::string, std::vector<std::string>> categorized;
            std::vector<std::string> uncategorized;

            for (const auto& [ext, files] : by_ext) {
                bool found = false;
                for (const auto& cat : categories) {
                    for (const auto& ce : cat.extensions) {
                        if (ext == ce) {
                            for (const auto& f : files)
                                categorized[cat.name].push_back(f);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) {
                    for (const auto& f : files)
                        uncategorized.push_back(f);
                }
            }

            ss << "建议分类方案：\n";
            for (const auto& [cat_name, files] : categorized) {
                ss << "  📁 " << cat_name << "/ (" << files.size() << " 个文件)\n";
                for (size_t i = 0; i < std::min(files.size(), (size_t)5); ++i) {
                    ss << "      " << files[i] << "\n";
                }
                if (files.size() > 5)
                    ss << "      ... 还有 " << (files.size() - 5) << " 个\n";
            }
            if (!uncategorized.empty()) {
                ss << "  📁 其他/ (" << uncategorized.size() << " 个文件)\n";
                for (size_t i = 0; i < std::min(uncategorized.size(), (size_t)5); ++i) {
                    ss << "      " << uncategorized[i] << "\n";
                }
            }

            if (categorized.size() > 1) {
                ss << "\n建议：当前目录有 " << categorized.size() << " 种不同类型的文件混在一起，"
                   << "建议按上述方案创建子文件夹进行归类整理。";
            } else if (total_files > 20) {
                ss << "\n建议：当前目录文件较多(" << total_files << " 个)，"
                   << "建议按时间或用途进一步分类。";
            } else {
                ss << "\n当前目录文件结构相对整齐，暂无需要整理。";
            }

            return ss.str();
        }
    });

    // ---- create_folder ----
    registry.Register({
        "create_folder",
        "在当前目录下创建一个新文件夹。参数 name 为文件夹名称。",
        {{"name", "要创建的文件夹名称"}},
        [ctx](const std::unordered_map<std::string, std::string>& params) {
            auto it = params.find("name");
            if (it == params.end() || it->second.empty())
                return std::string("错误：缺少 name 参数");

            std::string folder_name = it->second;
            std::string cur_dir = ctx->GetCurrentDir();
            std::string full_path;
            if (cur_dir.empty() || cur_dir == "/") {
                full_path = "/" + folder_name;
            } else {
                if (cur_dir.back() == '/')
                    full_path = cur_dir + folder_name;
                else
                    full_path = cur_dir + "/" + folder_name;
            }

            bool ok = ctx->CreateFolder(full_path);
            if (ok)
                return std::string("已成功创建文件夹: ") + folder_name;
            else
                return std::string("创建文件夹失败（可能已存在或权限不足）: ") + folder_name;
        }
    });

    // ---- move_file ----
    registry.Register({
        "move_file",
        "将文件从当前目录移动到指定子目录。参数 filename 为文件名，dst_dir 为目标目录路径。",
        {{"filename", "要移动的文件名"}, {"dst_dir", "目标目录路径"}},
        [ctx](const std::unordered_map<std::string, std::string>& params) {
            auto fn_it = params.find("filename");
            auto dst_it = params.find("dst_dir");
            if (fn_it == params.end() || fn_it->second.empty())
                return std::string("错误：缺少 filename 参数");
            if (dst_it == params.end() || dst_it->second.empty())
                return std::string("错误：缺少 dst_dir 参数");

            std::string filename = fn_it->second;
            std::string src_dir = ctx->GetCurrentDir();
            std::string dst_dir = dst_it->second;

            bool ok = ctx->MoveFile(filename, src_dir, dst_dir);
            if (ok)
                return std::string("已将 ") + filename + " 移动到 " + dst_dir;
            else
                return std::string("移动失败：服务端暂不支持文件移动操作，请手动移动文件");
        }
    });
}
