#include "xai_tool_registry.h"
#include <sstream>

void XAIToolRegistry::Register(ToolDef def) {
    tools_.push_back(std::move(def));
}

std::string XAIToolRegistry::Execute(const ToolCall& call) const {
    for (const auto& tool : tools_) {
        if (tool.name == call.tool_name) {
            // 检查必需参数
            for (const auto& p : tool.params) {
                if (p.required && call.params.find(p.name) == call.params.end()) {
                    return "ERROR: 缺少必需参数 " + p.name;
                }
            }
            try {
                return tool.handler(call.params);
            } catch (const std::exception& e) {
                return std::string("ERROR: 工具执行异常: ") + e.what();
            } catch (...) {
                return std::string("ERROR: 工具执行发生未知异常");
            }
        }
    }
    return "ERROR: 未知工具 " + call.tool_name;
}

std::string XAIToolRegistry::BuildToolsDescription() const {
    std::ostringstream ss;
    ss << "你可以使用以下工具来获取更多信息或执行操作。\n";
    ss << "当你需要使用工具时，请返回如下 JSON 格式：\n";
    ss << R"({"tool_call":{"name":"工具名","params":{"参数名":"参数值"}}})" << "\n";
    ss << "当你不需要使用工具、可以直接回复用户时，请返回：\n";
    ss << R"({"reply":"回复内容","action":"none|highlight|navigate|suggest_upload|delete_confirm|search","files":["文件名"],"dir":"目录路径"})" << "\n\n";
    ss << "可用工具列表：\n";

    for (const auto& tool : tools_) {
        ss << "- " << tool.name << ": " << tool.description << "\n";
        ss << "  参数：\n";
        for (const auto& p : tool.params) {
            ss << "    - " << p.name << " (" << p.type << ")";
            if (!p.required) ss << " [可选]";
            ss << ": " << p.description << "\n";
        }
    }
    return ss.str();
}
