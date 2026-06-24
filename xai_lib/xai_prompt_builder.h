#pragma once

#include <string>
#include <vector>
#include "xai_tool_registry.h"
#include "xms_disk_client_gui.pb.h"

// 消息角色
enum class MsgRole {
    System,
    User,
    Assistant,
    Tool
};

// 单条消息
struct ChatMessage {
    MsgRole     role;
    std::string content;
    std::string tool_name;  // 仅当 role == Tool 时有效
};

class XAIPromptBuilder {
public:
    // 设置 system prompt（含工具描述）
    void SetSystemPrompt(const std::string& base_prompt,
                         const XAIToolRegistry& registry);

    // 设置当前目录上下文
    void SetContext(const xdisk::XFileInfoList& files, const std::string& cur_dir);

    // 添加用户消息
    void AddUserMessage(const std::string& content);

    // 添加 AI 回复（包含 tool_call）
    void AddAssistantMessage(const std::string& content);

    // 添加工具执行结果
    void AddToolResult(const std::string& tool_name, const std::string& result);

    // 构建最终 prompt 文本（适用于非原生 function calling 模型）
    // 将所有消息拼接为一个 prompt 字符串
    std::string BuildPromptText() const;

    // 构建 OpenAI messages JSON 数组字符串
    // 用于原生支持 function calling 的 API
    std::string BuildMessagesJson() const;

    // 获取历史消息数量
    int MessageCount() const { return (int)messages_.size(); }

    // 清空
    void Clear();

    // 限制历史长度（保留 system + 最近 N 轮）
    void TrimHistory(int max_rounds);

private:
    std::string system_prompt_;
    std::string context_text_;
    std::vector<ChatMessage> messages_;

    static std::string RoleToString(MsgRole role);
    static std::string EscapeJsonStr(const std::string& s);
};
