#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// 工具参数定义（JSON Schema 简化版）
struct ToolParam {
    std::string name;
    std::string type;        // "string" | "integer" | "number"
    std::string description;
    bool        required = true;
};

// 工具定义
struct ToolDef {
    std::string name;
    std::string description;
    std::vector<ToolParam> params;
    std::function<std::string(const std::unordered_map<std::string, std::string>&)> handler;
};

// 工具调用请求（由 LLM 返回的 JSON 解析得到）
struct ToolCall {
    std::string tool_name;
    std::unordered_map<std::string, std::string> params;
};

class XAIToolRegistry {
public:
    void Register(ToolDef def);

    // 执行工具，返回结果字符串；失败时返回 "ERROR: ..."
    std::string Execute(const ToolCall& call) const;

    // 生成工具描述文本（嵌入 system prompt 使用）
    std::string BuildToolsDescription() const;

    // 判断是否有注册工具
    bool Empty() const { return tools_.empty(); }

    // 获取注册工具数量
    int Size() const { return (int)tools_.size(); }

private:
    std::vector<ToolDef> tools_;
};
