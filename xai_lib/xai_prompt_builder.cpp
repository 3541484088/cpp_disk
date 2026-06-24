#include "xai_prompt_builder.h"
#include <sstream>

void XAIPromptBuilder::SetSystemPrompt(const std::string& base_prompt,
                                        const XAIToolRegistry& registry) {
    std::ostringstream ss;
    ss << base_prompt << "\n\n";
    if (!registry.Empty()) {
        ss << registry.BuildToolsDescription();
    }
    ss << "\n重要规则：\n";
    ss << "1. 每次回复只能返回一个 JSON 对象，不要输出任何 JSON 以外的内容\n";
    ss << "2. 如果需要更多信息才能回答，使用工具获取\n";
    ss << "3. 每次只能调用一个工具\n";
    ss << "4. 获得工具结果后，综合信息回答用户问题\n";
    system_prompt_ = ss.str();
}

void XAIPromptBuilder::SetContext(const xdisk::XFileInfoList& files,
                                   const std::string& cur_dir) {
    std::ostringstream ss;
    ss << "当前目录：" << (cur_dir.empty() ? "/" : cur_dir) << "\n";
    ss << "目录文件列表（共 " << files.files_size() << " 个）：\n";
    for (int i = 0; i < files.files_size(); ++i) {
        const auto& f = files.files(i);
        ss << "  - " << f.filename();
        if (f.is_dir()) ss << "/ (目录)";
        else ss << " (" << f.filesize() << " 字节)";
        ss << "\n";
    }
    context_text_ = ss.str();
}

void XAIPromptBuilder::AddUserMessage(const std::string& content) {
    messages_.push_back({MsgRole::User, content, ""});
}

void XAIPromptBuilder::AddAssistantMessage(const std::string& content) {
    messages_.push_back({MsgRole::Assistant, content, ""});
}

void XAIPromptBuilder::AddToolResult(const std::string& tool_name,
                                      const std::string& result) {
    messages_.push_back({MsgRole::Tool, result, tool_name});
}

std::string XAIPromptBuilder::BuildPromptText() const {
    std::ostringstream ss;

    // System prompt
    ss << "[系统指令]\n" << system_prompt_ << "\n\n";

    // 上下文
    if (!context_text_.empty()) {
        ss << "[当前环境]\n" << context_text_ << "\n\n";
    }

    // 对话历史
    ss << "[对话历史]\n";
    for (const auto& msg : messages_) {
        switch (msg.role) {
        case MsgRole::User:
            ss << "用户: " << msg.content << "\n";
            break;
        case MsgRole::Assistant:
            ss << "助手: " << msg.content << "\n";
            break;
        case MsgRole::Tool:
            ss << "工具[" << msg.tool_name << "]返回: " << msg.content << "\n";
            break;
        default:
            break;
        }
    }

    ss << "\n请根据以上信息回复（只输出 JSON）：\n";
    return ss.str();
}

std::string XAIPromptBuilder::BuildMessagesJson() const {
    std::ostringstream ss;
    ss << "[";

    // system message
    ss << R"({"role":"system","content":")" << EscapeJsonStr(system_prompt_ + "\n\n" + context_text_) << R"("})";

    // conversation messages
    for (const auto& msg : messages_) {
        ss << ",";
        std::string role = RoleToString(msg.role);
        ss << R"({"role":")" << role << R"(","content":")" << EscapeJsonStr(msg.content) << R"("})";
    }

    ss << "]";
    return ss.str();
}

void XAIPromptBuilder::Clear() {
    messages_.clear();
    context_text_.clear();
}

void XAIPromptBuilder::TrimHistory(int max_rounds) {
    // 保留最近 max_rounds*2 条消息（一轮 = user + assistant）
    // 但不能从中间截断工具调用链（assistant tool_call + tool result 必须成对）
    int max_msgs = max_rounds * 2;
    while ((int)messages_.size() > max_msgs) {
        // 如果第一条是 Tool 类型，说明前面的 assistant 已被删除，这条也要删
        // 如果第一条是 Assistant，检查下一条是否是 Tool，如果是则一起删
        if (messages_.size() >= 2 &&
            messages_[0].role == MsgRole::Assistant &&
            messages_[1].role == MsgRole::Tool) {
            messages_.erase(messages_.begin(), messages_.begin() + 2);
        } else if (messages_[0].role == MsgRole::Tool) {
            messages_.erase(messages_.begin());
        } else {
            messages_.erase(messages_.begin());
        }
    }
}

std::string XAIPromptBuilder::RoleToString(MsgRole role) {
    switch (role) {
    case MsgRole::System:    return "system";
    case MsgRole::User:      return "user";
    case MsgRole::Assistant: return "assistant";
    case MsgRole::Tool:      return "tool";
    }
    return "user";
}

std::string XAIPromptBuilder::EscapeJsonStr(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '\\': result += "\\\\"; break;
        case '"':  result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:   result += c;
        }
    }
    return result;
}
