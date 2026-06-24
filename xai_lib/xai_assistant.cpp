#include "xai_assistant.h"
#include <QMetaObject>
#include <thread>
#include <sstream>
#include <mutex>
#include <curl/curl.h>
#include <json/json.h>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

using namespace std;
using namespace xdisk;

// AI 日志写到文件（客户端 GUI 无控制台窗口）
static void AILog(const string &msg)
{
    static mutex log_mtx;
    lock_guard<mutex> lk(log_mtx);
    ofstream f("ai_debug.log", ios::app);
    f << msg << "\n";
    f.flush();
    cout << msg << endl;
}

static void InitCurlOnce()
{
    static std::once_flag flag;
    std::call_once(flag, []() { curl_global_init(CURL_GLOBAL_ALL); });
}

static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    reinterpret_cast<string *>(userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

static int CurlProgressCallback(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    auto *alive = reinterpret_cast<std::atomic<bool>*>(clientp);
    return alive->load() ? 0 : 1;
}

#ifdef _WIN32
static CURLcode SafeCurlPerform(CURL *curl)
{
    __try {
        return curl_easy_perform(curl);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return CURLE_FAILED_INIT;
    }
}

static string WinHttpPost(const string &url, const vector<pair<string,string>> &headers,
                          const string &body, std::atomic<bool> *alive)
{
    AILog("[AI] WinHTTP fallback: " + url);
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[1024] = {};
    uc.lpszHostName = host;  uc.dwHostNameLength = 256;
    uc.lpszUrlPath  = path;  uc.dwUrlPathLength  = 1024;

    wstring wurl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
        throw runtime_error("WinHTTP: URL 解析失败");

    HINTERNET session = WinHttpOpen(L"XMSDisk/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) throw runtime_error("WinHTTP: Open 失败");

    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (!connect) { WinHttpCloseHandle(session); throw runtime_error("WinHTTP: Connect 失败"); }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", path, NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) { WinHttpCloseHandle(connect); WinHttpCloseHandle(session); throw runtime_error("WinHTTP: OpenRequest 失败"); }

    if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
        DWORD opts = SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS;
        WinHttpSetOption(request, WINHTTP_OPTION_SECURITY_FLAGS, &opts, sizeof(opts));
    }

    DWORD timeout = 30000;
    WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    for (auto &h : headers) {
        string line = h.first + ": " + h.second;
        wstring wline(line.begin(), line.end());
        WinHttpAddRequestHeaders(request, wline.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        throw runtime_error("WinHTTP: SendRequest 失败"); }

    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request); WinHttpCloseHandle(connect); WinHttpCloseHandle(session);
        throw runtime_error("WinHTTP: ReceiveResponse 失败");
    }

    string resp;
    char buf[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(request, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        if (alive && !alive->load()) break;
        resp.append(buf, bytesRead);
        bytesRead = 0;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return resp;
}
#endif

XAIAssistant::XAIAssistant(QObject *parent) : QObject(parent)
{
    InitCurlOnce();
    alive_ = std::make_shared<std::atomic<bool>>(true);
    AILog("[AI] 构造函数: alive_ 已初始化");
}

XAIAssistant::~XAIAssistant()
{
    alive_->store(false);
    if (worker_thread_.joinable())
        worker_thread_.join();
}

void XAIAssistant::SetFileContext(IFileContext* ctx)
{
    file_ctx_ = ctx;
    // 注册工具（只在首次设置时注册）
    if (tool_registry_.Empty() && ctx) {
        RegisterBuiltinTools(tool_registry_, ctx);
        AILog("[AI] 工具已注册，共 " + to_string(tool_registry_.Size()) + " 个");
    }
}

// ---- 配置 setter/getter（加锁，主/子线程均安全）----
void XAIAssistant::set_api_key(const string &key)  { lock_guard<mutex> l(config_mutex_); config_.api_key  = key;  }
void XAIAssistant::set_api_url(const string &url)  { lock_guard<mutex> l(config_mutex_); config_.api_url  = url;  }
void XAIAssistant::set_api_type(const string &type){ lock_guard<mutex> l(config_mutex_); config_.api_type = type; }
void XAIAssistant::set_model(const string &model)  { lock_guard<mutex> l(config_mutex_); config_.model    = model;}

string XAIAssistant::api_type() const { lock_guard<mutex> l(config_mutex_); return config_.api_type; }
string XAIAssistant::api_key()  const { lock_guard<mutex> l(config_mutex_); return config_.api_key;  }
string XAIAssistant::api_url()  const { lock_guard<mutex> l(config_mutex_); return config_.api_url;  }
string XAIAssistant::model()    const { lock_guard<mutex> l(config_mutex_); return config_.model;    }

bool XAIAssistant::is_configured() const
{
    lock_guard<mutex> l(config_mutex_);
    return config_.api_type == "ollama" || !config_.api_key.empty();
}

XAIAssistant::Config XAIAssistant::GetConfigSnapshot() const
{
    lock_guard<mutex> l(config_mutex_);
    return config_;
}

void XAIAssistant::ClearHistory()
{
    lock_guard<mutex> lock(history_mutex_);
    prompt_builder_.Clear();
}

// ---- 发送消息入口（主线程调用）----
void XAIAssistant::SendUserMessage(const QString &user_input,
                                   const XFileInfoList &file_list,
                                   const string &current_dir)
{
    if (user_input.trimmed().isEmpty()) return;

    if (worker_thread_.joinable())
        worker_thread_.join();

    AILog(string("[AI] SendUserMessage 开始，input=") + user_input.toUtf8().constData());
    emit OnRequestStart();

    string input_str = user_input.toUtf8().constData();

    // 构建 PromptBuilder
    {
        lock_guard<mutex> lock(history_mutex_);
        prompt_builder_.SetSystemPrompt(
            "你是一个智能网盘文件助手，可以帮助用户管理文件、搜索文件、导航目录。",
            tool_registry_);
        prompt_builder_.SetContext(file_list, current_dir);
        prompt_builder_.AddUserMessage(input_str);
        prompt_builder_.TrimHistory(kMaxHistoryRounds);
    }

    Config cfg = GetConfigSnapshot();
    AILog("[AI] 配置快照: api_type=" + cfg.api_type + " model=" + cfg.model);

    auto alive = alive_;
    XAIAssistant *raw_self = this;

    worker_thread_ = std::thread([raw_self, alive, cfg = std::move(cfg)]() {
        AILog("[AI] 子线程启动 (Agent Loop)");

        if (!alive || !alive->load()) return;

        string err;
        AIAction action;

        try {
            XAIPromptBuilder builder_copy;
            {
                lock_guard<mutex> lock(raw_self->history_mutex_);
                builder_copy = raw_self->prompt_builder_;
            }
            action = raw_self->RunAgentLoop(cfg, builder_copy, alive);

            // 保存最终 assistant 回复到历史
            {
                lock_guard<mutex> lock(raw_self->history_mutex_);
                raw_self->prompt_builder_.AddAssistantMessage(action.reply);
            }
        } catch (const exception &e) {
            err = e.what();
            AILog("[AI] Agent Loop 异常: " + err);
        } catch (...) {
            err = "未知异常";
        }

        if (!alive->load()) return;

        QMetaObject::invokeMethod(raw_self, [raw_self, alive, action, err]() {
            if (!alive->load()) return;
            if (!err.empty()) {
                emit raw_self->OnError(QString::fromUtf8(err.c_str()));
                emit raw_self->OnRequestEnd();
                return;
            }
            if (action.reply.empty()) {
                emit raw_self->OnError(QString::fromUtf8("AI 返回了空回复"));
                emit raw_self->OnRequestEnd();
                return;
            }
            raw_self->DispatchAction(action);
            emit raw_self->OnRequestEnd();
            AILog("[AI] Agent Loop 流程完成");
        }, Qt::QueuedConnection);
    });
}

// ---- Agent Loop: 多轮工具调用 ----
XAIAssistant::AIAction XAIAssistant::RunAgentLoop(
    const Config& cfg,
    XAIPromptBuilder& builder,
    std::shared_ptr<std::atomic<bool>> alive)
{
    for (int round = 0; round < kMaxAgentRounds; ++round) {
        if (!alive->load()) throw runtime_error("已取消");

        AILog("[AI] Agent Loop 第 " + to_string(round + 1) + " 轮");

        // 构建 prompt 并调用 LLM
        string prompt = builder.BuildPromptText();
        string raw = CallLLMAPI(cfg, prompt, alive);
        string reply = ExtractReplyText(raw);

        if (reply.empty()) {
            throw runtime_error("AI 返回空回复");
        }

        AILog("[AI] LLM 回复: " + reply.substr(0, min((size_t)100, reply.size())));

        // 对回复进行反转义（API 返回的 content 字段内引号被转义）
        string unescaped_reply = UnescapeJson(reply);

        // 检查是否是工具调用
        ToolCall tool_call;
        if (ParseToolCall(unescaped_reply, tool_call)) {
            AILog("[AI] 检测到工具调用: " + tool_call.tool_name);
            AILog("[AI] 工具参数数量: " + to_string(tool_call.params.size()));
            for (const auto& kv : tool_call.params) {
                AILog("[AI]   参数 " + kv.first + " = " + kv.second.substr(0, min((size_t)50, kv.second.size())));
            }

            // 执行工具
            AILog("[AI] 准备执行工具...");
            string tool_result = tool_registry_.Execute(tool_call);
            AILog("[AI] 工具执行完成");
            AILog("[AI] 工具返回: " + tool_result.substr(0, min((size_t)100, tool_result.size())));

            // 将工具调用和结果加入对话
            builder.AddAssistantMessage(reply);
            builder.AddToolResult(tool_call.tool_name, tool_result);

            continue;  // 再调用一次 LLM
        }

        // 不是工具调用，是最终回复
        AIAction action = ParseAIResponse(unescaped_reply);
        return action;
    }

    // 超过最大轮次，返回最后的状态
    AIAction fallback;
    fallback.type = ActionType::None;
    fallback.reply = "抱歉，我尝试了多次但未能完成任务，请尝试简化问题。";
    return fallback;
}

// ---- 解析工具调用 ----
bool XAIAssistant::ParseToolCall(const string& reply_text, ToolCall& out_call)
{
    // 查找 "tool_call" 字段
    size_t tc_pos = reply_text.find("\"tool_call\"");
    if (tc_pos == string::npos) return false;

    // 找到 tool_call 对象的开始 {
    size_t obj_start = reply_text.find("{", tc_pos + 11);
    if (obj_start == string::npos) return false;

    // 提取 name
    string name = ExtractJsonString(reply_text.substr(obj_start), "name", "");
    if (name.empty()) return false;

    out_call.tool_name = name;
    out_call.params.clear();

    // 查找 params 对象
    size_t params_pos = reply_text.find("\"params\"", obj_start);
    if (params_pos == string::npos) return true;  // 无参数也算有效调用

    size_t params_obj_start = reply_text.find("{", params_pos);
    size_t params_obj_end = reply_text.find("}", params_obj_start);
    if (params_obj_start == string::npos || params_obj_end == string::npos) return true;

    string params_str = reply_text.substr(params_obj_start, params_obj_end - params_obj_start + 1);

    // 简单解析 key-value 对
    size_t pos = 0;
    while (pos < params_str.size()) {
        size_t key_start = params_str.find("\"", pos);
        if (key_start == string::npos) break;
        size_t key_end = params_str.find("\"", key_start + 1);
        if (key_end == string::npos) break;

        string key = params_str.substr(key_start + 1, key_end - key_start - 1);

        // 找冒号后的值
        size_t colon = params_str.find(":", key_end);
        if (colon == string::npos) break;

        // 跳过空格
        size_t val_start = colon + 1;
        while (val_start < params_str.size() && params_str[val_start] == ' ') val_start++;

        string value;
        if (val_start < params_str.size() && params_str[val_start] == '"') {
            // 字符串值
            size_t val_end = val_start + 1;
            while (val_end < params_str.size()) {
                if (params_str[val_end] == '\\' && val_end + 1 < params_str.size()) {
                    val_end += 2; continue;
                }
                if (params_str[val_end] == '"') break;
                val_end++;
            }
            value = params_str.substr(val_start + 1, val_end - val_start - 1);
            pos = val_end + 1;
        } else {
            // 数字或其他
            size_t val_end = params_str.find_first_of(",}", val_start);
            if (val_end == string::npos) val_end = params_str.size();
            value = params_str.substr(val_start, val_end - val_start);
            // 去除首尾空格
            while (!value.empty() && value.back() == ' ') value.pop_back();
            pos = val_end;
        }

        if (!key.empty()) {
            out_call.params[key] = value;
        }
    }

    return true;
}

// ---- JSON 字符串转义（static）----
string XAIAssistant::EscapeJson(const string &s)
{
    AILog("[AI] EscapeJson 进入，长度=" + to_string(s.size()));
    string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '/':  result += "\\/"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c;
        }
    }
    AILog("[AI] EscapeJson 完成");
    return result;
}

// ---- 请求体构建（static，无 this 依赖）----
string XAIAssistant::BuildOpenAIBody(const Config &cfg, const string &prompt)
{
    AILog("[AI] BuildOpenAIBody 进入");
    
    // 直接用字符串拼接，避免 JsonCpp 线程安全问题
    string escaped_prompt = EscapeJson(prompt);
    AILog("[AI] prompt 转义完成");

    string body = R"({"model":")" + cfg.model + R"(","temperature":0.3,"stream":false,"messages":[{"role":"user","content":")" + escaped_prompt + R"("}]})";

    AILog("[AI] BuildOpenAIBody 完成，长度=" + to_string(body.size()));
    return body;
}

string XAIAssistant::BuildClaudeBody(const Config &cfg, const string &prompt)
{
    Json::Value root, msg;
    root["model"]      = cfg.model.empty() ? "claude-3-haiku-20240307" : cfg.model;
    root["max_tokens"] = 1024;
    msg["role"]        = "user";
    msg["content"]     = prompt;
    root["messages"].append(msg);
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return Json::writeString(wb, root);
}

string XAIAssistant::BuildOllamaBody(const Config &cfg, const string &prompt)
{
    Json::Value root, msg;
    root["model"]  = cfg.model;
    root["stream"] = false;
    msg["role"]    = "user";
    msg["content"] = prompt;
    root["messages"].append(msg);
    Json::StreamWriterBuilder wb; wb["indentation"] = "";
    return Json::writeString(wb, root);
}

// ---- HTTP 请求（static，无 this 依赖）----
string XAIAssistant::CallLLMAPI(const Config &cfg, const string &prompt,
                                std::shared_ptr<std::atomic<bool>> alive)
{
    AILog("[AI] CallLLMAPI 进入");
    
    // 确保 curl 已初始化
    InitCurlOnce();
    AILog("[AI] curl 已初始化");
    
    CURL *curl = curl_easy_init();
    if (!curl) { 
        AILog("[AI] curl_easy_init 返回 null"); 
        throw runtime_error("curl_easy_init 失败"); 
    }
    AILog("[AI] curl_easy_init 成功");

    struct Guard {
        CURL *curl; curl_slist *headers = nullptr;
        ~Guard() { curl_slist_free_all(headers); curl_easy_cleanup(curl); }
    } g{curl};

    string body, url;
    g.headers = curl_slist_append(g.headers, "Content-Type: application/json");
    AILog("[AI] Content-Type header 已添加");

    if (cfg.api_type == "openai") {
        url = cfg.api_url;
        AILog("[AI] openai api_url_原始=" + url);
        
        // 检查 url 是否有问题
        AILog("[AI] url 长度=" + to_string(url.size()));
        for (size_t i = 0; i < min(url.size(), (size_t)20); ++i) {
            unsigned char c = (unsigned char)url[i];
            if (c < 32 || c > 126) {
                AILog("[AI] url 第" + to_string(i) + "个字符是控制字符: " + to_string((int)c));
            }
        }
        
        if (url.empty()) {
            AILog("[AI] url为空，使用默认dashscope地址");
            url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
        } else if (url.find("/chat/completions") == string::npos) {
            AILog("[AI] url需要追加 /chat/completions");
            url += "/chat/completions";
        } else {
            AILog("[AI] url已包含 /chat/completions，无需追加");
        }
        AILog("[AI] url最终拼接完成=" + url);
        
        AILog("[AI] 准备调用 BuildOpenAIBody");
        body = BuildOpenAIBody(cfg, prompt);
        AILog("[AI] openai body 长度=" + to_string(body.size()));
        
        AILog("[AI] 准备构建 auth header");
        string auth = "Authorization: Bearer " + cfg.api_key;
        AILog("[AI] auth header 构建完成");
        g.headers = curl_slist_append(g.headers, auth.c_str());
        AILog("[AI] auth header 添加成功");
    } else if (cfg.api_type == "claude") {
        AILog("[AI] claude 分支进入");
        url = cfg.api_url;
        AILog("[AI] claude api_url_原始=" + url);
        if (url.empty()) {
            url = "https://api.anthropic.com/v1/messages";
            AILog("[AI] claude url为空，使用默认地址");
        } else if (url.find("/v1/") == string::npos && url.find("/messages") == string::npos) {
            url += "/v1/messages";
            AILog("[AI] claude url追加 /v1/messages");
        }
        AILog("[AI] claude url最终=" + url);
        body = BuildClaudeBody(cfg, prompt);
        AILog("[AI] claude body 长度=" + to_string(body.size()));
        string auth = "x-api-key: " + cfg.api_key;
        g.headers = curl_slist_append(g.headers, auth.c_str());
        g.headers = curl_slist_append(g.headers, "anthropic-version: 2023-06-01");
        AILog("[AI] claude headers 设置完成");
    } else {
        url  = cfg.api_url.empty() ? "http://localhost:11434/api/chat" : cfg.api_url;
        body = BuildOllamaBody(cfg, prompt);
    }
    
    AILog("[AI] url最终=" + url);
    AILog("[AI] body前100=" + body.substr(0, min((size_t)100, body.size())));

    string resp;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    g.headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    AILog("[AI] curl_easy_setopt 全部完成");

    AILog("[AI] 准备执行 curl_easy_perform");
    CURLcode res = curl_easy_perform(curl);
    AILog("[AI] curl_easy_perform 返回 res=" + to_string((int)res));

    if (res != CURLE_OK) {
        string err_msg = curl_easy_strerror(res);
        AILog("[AI] curl 错误: " + err_msg + " (code=" + to_string((int)res) + ")");
        throw runtime_error("curl 请求失败: " + err_msg + " (URL: " + url + ")");
    }
    
    AILog("[AI] curl 请求成功，响应长度=" + to_string(resp.size()));
    return resp;
}

// ---- 解析响应（static）----
string XAIAssistant::ExtractReplyText(const string &json_response)
{
    AILog("[AI] ExtractReplyText 进入，长度=" + to_string(json_response.size()));
    AILog("[AI] 响应前200=" + json_response.substr(0, min((size_t)200, json_response.size())));
    
    // 纯字符串解析，避免 JsonCpp 线程安全问题
    // 查找 "content":"..." 模式
    size_t content_pos = json_response.find("\"content\"");
    if (content_pos == string::npos) {
        // 尝试查找 "message":{"content":"..."
        size_t message_pos = json_response.find("\"message\"");
        if (message_pos != string::npos) {
            content_pos = json_response.find("\"content\"", message_pos);
        }
    }
    
    if (content_pos == string::npos) {
        AILog("[AI] 未找到 content 字段");
        // 检查是否有 error
        size_t error_pos = json_response.find("\"error\"");
        if (error_pos != string::npos) {
            size_t msg_start = json_response.find("\"message\"", error_pos);
            if (msg_start != string::npos) {
                size_t colon = json_response.find(":", msg_start);
                size_t quote1 = json_response.find("\"", colon + 1);
                size_t quote2 = json_response.find("\"", quote1 + 1);
                if (quote1 != string::npos && quote2 != string::npos) {
                    string err_msg = json_response.substr(quote1 + 1, quote2 - quote1 - 1);
                    AILog("[AI] API 返回错误: " + err_msg);
                    throw runtime_error("API 返回错误: " + err_msg);
                }
            }
        }
        return "";
    }
    
    // 找到 "content" 后，找到冒号和引号
    size_t colon = json_response.find(":", content_pos);
    if (colon == string::npos) {
        AILog("[AI] 未找到 content 冒号");
        return "";
    }
    
    size_t quote1 = json_response.find("\"", colon + 1);
    if (quote1 == string::npos) {
        AILog("[AI] 未找到 content 开始引号");
        return "";
    }
    
    // 查找结束引号（处理转义）
    size_t quote2 = quote1 + 1;
    while (quote2 < json_response.size()) {
        if (json_response[quote2] == '\\' && quote2 + 1 < json_response.size()) {
            quote2 += 2;
            continue;
        }
        if (json_response[quote2] == '"') {
            break;
        }
        quote2++;
    }
    
    string content = json_response.substr(quote1 + 1, quote2 - quote1 - 1);
    AILog("[AI] ExtractReplyText 完成，content 长度=" + to_string(content.size()));
    return content;
}

// ---- 解析动作（static）----
XAIAssistant::AIAction XAIAssistant::ParseAIResponse(const string &reply_text)
{
    AILog("[AI] ParseAIResponse 进入");
    AIAction action;
    action.reply = reply_text;
    
    size_t js = reply_text.find('{'), je = reply_text.rfind('}');
    if (js == string::npos || je == string::npos || je <= js) {
        AILog("[AI] 未找到 JSON，返回原始文本");
        action.type = ActionType::None;
        return action;
    }
    
    string json_str = reply_text.substr(js, je - js + 1);
    AILog("[AI] JSON 片段: " + json_str.substr(0, min((size_t)100, json_str.size())));
    
    // 对 JSON 字符串进行反转义（处理 API 返回的转义内容）
    json_str = UnescapeJson(json_str);
    AILog("[AI] JSON 反转义后: " + json_str.substr(0, min((size_t)100, json_str.size())));
    
    // 解析 reply 字段（处理嵌套 JSON 情况）
    string raw_reply = ExtractJsonString(json_str, "reply", reply_text);
    AILog("[AI] 原始 reply: " + raw_reply.substr(0, min((size_t)50, raw_reply.size())));
    
    // 检查 reply 是否又是一个 JSON 字符串（被转义的）
    if (!raw_reply.empty() && raw_reply[0] == '{') {
        AILog("[AI] reply 是嵌套 JSON，再次解析");
        action.reply = ExtractJsonString(raw_reply, "reply", raw_reply);
    } else {
        action.reply = raw_reply;
    }
    AILog("[AI] 最终 reply: " + action.reply.substr(0, min((size_t)50, action.reply.size())));
    
    // 解析 dir 字段
    action.dir_path = ExtractJsonString(json_str, "dir", "");
    AILog("[AI] 解析 dir: " + action.dir_path);
    
    // 解析 files 数组
    size_t files_start = json_str.find("\"files\"");
    if (files_start != string::npos) {
        size_t arr_start = json_str.find("[", files_start);
        size_t arr_end = json_str.find("]", arr_start);
        if (arr_start != string::npos && arr_end != string::npos) {
            string files_arr = json_str.substr(arr_start + 1, arr_end - arr_start - 1);
            // 分割数组元素
            size_t pos = 0;
            while (pos < files_arr.size()) {
                size_t quote1 = files_arr.find("\"", pos);
                if (quote1 == string::npos) break;
                size_t quote2 = quote1 + 1;
                while (quote2 < files_arr.size()) {
                    if (files_arr[quote2] == '\\' && quote2 + 1 < files_arr.size()) {
                        quote2 += 2;
                        continue;
                    }
                    if (files_arr[quote2] == '"') break;
                    quote2++;
                }
                if (quote2 < files_arr.size()) {
                    action.file_list.push_back(files_arr.substr(quote1 + 1, quote2 - quote1 - 1));
                }
                pos = quote2 + 1;
            }
        }
    }
    AILog("[AI] 解析 files 数量: " + to_string(action.file_list.size()));
    
    // 解析 action 字段
    string act = ExtractJsonString(json_str, "action", "none");
    AILog("[AI] 解析 action: " + act);
    
    if      (act == "highlight")       action.type = ActionType::HighlightFiles;
    else if (act == "navigate")        action.type = ActionType::NavigateDir;
    else if (act == "suggest_upload")  action.type = ActionType::SuggestUpload;
    else if (act == "delete_confirm")  action.type = ActionType::DeleteConfirm;
    else if (act == "search")          action.type = ActionType::SearchFiles;
    else                               action.type = ActionType::None;
    
    // 如果是搜索动作，解析搜索条件
    if (action.type == ActionType::SearchFiles) {
        action.search_criteria.keyword = ExtractJsonString(json_str, "keyword", "");
        action.search_criteria.content_keyword = ExtractJsonString(json_str, "content", "");
        action.search_criteria.file_type = ExtractJsonString(json_str, "file_type", "");
        
        // 解析大小条件
        string min_size_str = ExtractJsonString(json_str, "min_size", "-1");
        string max_size_str = ExtractJsonString(json_str, "max_size", "-1");
        try {
            action.search_criteria.min_size = stoll(min_size_str);
            action.search_criteria.max_size = stoll(max_size_str);
        } catch (...) {
            action.search_criteria.min_size = -1;
            action.search_criteria.max_size = -1;
        }
        
        action.search_criteria.date_from = ExtractJsonString(json_str, "date_from", "");
        action.search_criteria.date_to = ExtractJsonString(json_str, "date_to", "");
        
        AILog("[AI] 搜索条件: keyword=" + action.search_criteria.keyword + 
              ", type=" + action.search_criteria.file_type +
              ", min_size=" + to_string(action.search_criteria.min_size) +
              ", max_size=" + to_string(action.search_criteria.max_size));
    }
    
    AILog("[AI] ParseAIResponse 完成");
    return action;
}

// ---- 辅助函数：反转义 JSON 字符串（static）----
string XAIAssistant::UnescapeJson(const string &s) {
    string result;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\\': result += '\\'; i++; break;
                case '"': result += '"'; i++; break;
                case 'n': result += '\n'; i++; break;
                case 'r': result += '\r'; i++; break;
                case 't': result += '\t'; i++; break;
                default: result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

// ---- 辅助函数：从 JSON 字符串中提取字段值（static）----
string XAIAssistant::ExtractJsonString(const string &json_str, const string &key, const string &default_val)
{
    size_t key_pos = json_str.find("\"" + key + "\"");
    if (key_pos == string::npos) return default_val;
    
    size_t colon = json_str.find(":", key_pos);
    if (colon == string::npos) return default_val;
    
    size_t quote1 = json_str.find("\"", colon + 1);
    if (quote1 == string::npos) return default_val;
    
    size_t quote2 = quote1 + 1;
    while (quote2 < json_str.size()) {
        if (json_str[quote2] == '\\' && quote2 + 1 < json_str.size()) {
            quote2 += 2;
            continue;
        }
        if (json_str[quote2] == '"') break;
        quote2++;
    }
    
    string value = json_str.substr(quote1 + 1, quote2 - quote1 - 1);
    return UnescapeJson(value);
}

// ---- Dispatch（主线程执行）----
void XAIAssistant::DispatchAction(const AIAction &action)
{
    emit OnAIReply(QString::fromUtf8(action.reply.c_str()));

    switch (action.type) {
    case ActionType::HighlightFiles: {
        QStringList ql;
        for (auto &f : action.file_list) ql << QString::fromUtf8(f.c_str());
        if (!ql.isEmpty()) emit OnHighlightFiles(ql);
        break;
    }
    case ActionType::NavigateDir:
        if (!action.dir_path.empty())
            emit OnNavigateDir(QString::fromUtf8(action.dir_path.c_str()));
        break;
    case ActionType::SuggestUpload:
        if (!action.dir_path.empty())
            emit OnSuggestUploadDir(QString::fromUtf8(action.dir_path.c_str()));
        break;
    case ActionType::DeleteConfirm: {
        QStringList ql;
        for (auto &f : action.file_list) ql << QString::fromUtf8(f.c_str());
        if (!ql.isEmpty()) emit OnDeleteConfirm(ql);
        break;
    }
    case ActionType::SearchFiles:
        emit OnSearchFiles(action.search_criteria);
        break;
    default: break;
    }
}
