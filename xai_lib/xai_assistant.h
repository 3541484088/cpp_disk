#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include "xms_disk_client_gui.pb.h"
#include "xai_tool_registry.h"
#include "xai_tools.h"
#include "xai_prompt_builder.h"

class XAIAssistant : public QObject
{
    Q_OBJECT

public:
    enum class ActionType { 
        None, 
        HighlightFiles, 
        NavigateDir, 
        SuggestUpload, 
        DeleteConfirm,
        SearchFiles  // 新增：文件搜索
    };

    // 搜索条件结构体
    struct SearchCriteria {
        std::string keyword;           // 文件名关键词
        std::string content_keyword;   // 文件内容关键词
        std::string file_type;         // 文件类型（扩展名，如 .txt）
        int64_t min_size = -1;         // 最小大小（字节），-1表示不限制
        int64_t max_size = -1;         // 最大大小（字节），-1表示不限制
        std::string date_from;         // 日期范围开始（格式：YYYY-MM-DD）
        std::string date_to;           // 日期范围结束（格式：YYYY-MM-DD）
    };

    struct AIAction {
        ActionType               type = ActionType::None;
        std::string              reply;
        std::vector<std::string> file_list;
        std::string              dir_path;
        SearchCriteria           search_criteria;  // 新增：搜索条件
    };

    explicit XAIAssistant(QObject *parent = nullptr);
    ~XAIAssistant() override;

    void set_api_key(const std::string &key);
    void set_api_url(const std::string &url);
    void set_api_type(const std::string &type);
    void set_model(const std::string &model);
    std::string api_type() const;
    std::string api_key() const;
    std::string api_url() const;
    std::string model() const;
    bool is_configured() const;
    void ClearHistory();

    // 设置文件上下文接口（由 UI 层注入）
    void SetFileContext(IFileContext* ctx);

    void SendUserMessage(const QString &user_input,
                         const xdisk::XFileInfoList &file_list,
                         const std::string &current_dir);

signals:
    void OnAIReply(QString text);
    void OnHighlightFiles(QStringList file_names);
    void OnNavigateDir(QString dir_path);
    void OnSuggestUploadDir(QString dir_path);
    void OnDeleteConfirm(QStringList file_names);
    void OnSearchFiles(const SearchCriteria& criteria);  // 新增：搜索信号
    void OnRequestStart();
    void OnRequestEnd();
    void OnError(QString error_msg);

private:
    struct Config {
        std::string api_key, api_url, api_type = "ollama", model = "qwen2.5:7b";
    };

    Config  GetConfigSnapshot() const;

    // Agent Loop：多轮工具调用，直到 LLM 返回最终回复
    AIAction RunAgentLoop(const Config& cfg,
                          XAIPromptBuilder& builder,
                          std::shared_ptr<std::atomic<bool>> alive);

    static std::string CallLLMAPI(const Config &cfg, const std::string &prompt,
                                  std::shared_ptr<std::atomic<bool>> alive);
    static std::string BuildOpenAIBody(const Config &cfg, const std::string &prompt);
    static std::string BuildClaudeBody(const Config &cfg, const std::string &prompt);
    static std::string BuildOllamaBody(const Config &cfg, const std::string &prompt);
    static std::string ExtractReplyText(const std::string &json_response);
    static AIAction    ParseAIResponse(const std::string &reply_text);

    // 判断响应是否是工具调用，如果是则填充 ToolCall
    static bool ParseToolCall(const std::string& reply_text, ToolCall& out_call);

    static std::string EscapeJson(const std::string &s);
    static std::string UnescapeJson(const std::string &s);
    static std::string ExtractJsonString(const std::string &json_str, const std::string &key, const std::string &default_val);
    void               DispatchAction(const AIAction &action);

    mutable std::mutex   config_mutex_;
    Config               config_;

    mutable std::mutex   history_mutex_;
    XAIPromptBuilder     prompt_builder_;
    static constexpr int kMaxHistoryRounds = 6;
    static constexpr int kMaxAgentRounds   = 5;  // Agent Loop 最大工具调用轮次

    XAIToolRegistry      tool_registry_;
    IFileContext*        file_ctx_ = nullptr;

    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    std::thread worker_thread_;
};
