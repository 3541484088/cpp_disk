#include "xshare_handle.h"
#include "xtools.h"
#include "xlog_client.h"
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
using namespace std;
using namespace xdisk;
using namespace xmsg;
using namespace LX;

#define DIR_ROOT (GetDirRoot())
#define FILE_INFO_NAME_PRE ".info_"

// Max lengths for user-controlled string fields (mirrors DB column sizes).
static constexpr size_t MAX_USERNAME_LEN   = 64;
static constexpr size_t MAX_FOLDERNAME_LEN = 128;
static constexpr size_t MAX_PATH_LEN       = 512;
static constexpr size_t MAX_SIDECAR_BYTES  = 4096;  // .info_ sidecar cap

// Escapes a string for safe embedding inside single-quoted MySQL literals.
static string EscStr(const string &s)
{
    string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
        case '\''  : out += "\\'";  break;
        case '\\'  : out += "\\\\"; break;
        case '"'   : out += "\\\""; break;
        case '\0'  : out += "\\0";  break;
        case '\n'  : out += "\\n";  break;
        case '\r'  : out += "\\r";  break;
        case '\x1a': out += "\\Z";  break;
        default    : out += c;      break;
        }
    }
    return out;
}

// Validates a directory path: no absolute paths, no ".." components.
// Splits on '/' and '\' and checks each segment individually,
// so "readme..txt" is allowed but ".." as a segment is not.
static bool IsPathSafe(const string &path)
{
    if (path.empty()) return true;
    if (path[0] == '/' || path[0] == '\\') return false;
    if (path.size() > MAX_PATH_LEN) return false;
    string seg;
    for (char c : path + "/")
    {
        if (c == '/' || c == '\\')
        {
            if (seg == "..") return false;
            seg.clear();
        }
        else { seg += c; }
    }
    return true;
}

// Validates a plain filename: no path separators, not "..".
static bool IsFilenameSafe(const string &name)
{
    if (name.empty() || name.size() > MAX_PATH_LEN) return false;
    if (name.find('/') != string::npos)  return false;
    if (name.find('\\') != string::npos) return false;
    if (name == "..") return false;
    return true;
}

static bool IsValidPermission(const string &p)
{
    return p == "read" || p == "write" || p == "admin";
}

// Verifies that full_path is inside sandbox_dir after resolving symlinks.
// Returns false if the path escapes the sandbox or if canonicalization fails.
static bool IsSandboxed(const string &sandbox_dir, const string &full_path)
{
    namespace fs = std::filesystem;
    error_code ec;
    auto canon_base = fs::weakly_canonical(sandbox_dir, ec);
    if (ec) return false;
    auto canon_path = fs::weakly_canonical(full_path, ec);
    if (ec) return false;
    auto rel = fs::relative(canon_path, canon_base, ec);
    if (ec) return false;
    // If the relative path's first component is "..", the path is outside.
    auto it = rel.begin();
    return it == rel.end() || it->string() != "..";
}

// Thread-safe filetime string.
static string GetFileTimeStr(const std::filesystem::directory_entry &entry)
{
    namespace fs = std::filesystem;
    try
    {
        auto ftime   = entry.last_write_time();
        auto now_ft  = fs::file_time_type::clock::now();
        auto now_sys = std::chrono::system_clock::now();
        auto delta   = ftime - now_ft;
        auto sys_tp  = now_sys +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(delta);
        time_t tt = std::chrono::system_clock::to_time_t(sys_tp);
        char buf[32] = {};
#ifdef _WIN32
        struct tm t; localtime_s(&t, &tt);
#else
        struct tm t; localtime_r(&tt, &t);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        return buf;
    }
    catch (...) { return ""; }
}

XShareHandle::XShareHandle()
{
    db_ = new LXMysql();
    if (!db_->Init())
    {
        LOGDEBUG("XShareHandle: DB Init() failed");
        delete db_;
        db_ = nullptr;
        return;
    }
    db_->SetReconnect(true);
    db_->SetConnectTimeout(3);
    if (!db_->InputDBConfig())
    {
        LOGDEBUG("XShareHandle: DB connect failed");
        delete db_;
        db_ = nullptr;
        return;
    }

    // UNIQUE KEY (owner, name) makes the duplicate-check atomic at DB level,
    // eliminating the TOCTOU race between SELECT and INSERT.
    db_->Query(
        "CREATE TABLE IF NOT EXISTS shared_folders ("
        "  id           BIGINT       NOT NULL AUTO_INCREMENT PRIMARY KEY,"
        "  owner        VARCHAR(64)  NOT NULL,"
        "  name         VARCHAR(128) NOT NULL,"
        "  storage_path VARCHAR(256) DEFAULT '',"
        "  created_at   TIMESTAMP    DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE KEY uq_owner_name (owner, name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    db_->Query(
        "CREATE TABLE IF NOT EXISTS shared_folder_permissions ("
        "  folder_id        BIGINT      NOT NULL,"
        "  username         VARCHAR(64) NOT NULL,"
        "  permission_level ENUM('read','write','admin') NOT NULL DEFAULT 'read',"
        "  PRIMARY KEY (folder_id, username),"
        "  FOREIGN KEY (folder_id) REFERENCES shared_folders(id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

}

XShareHandle::~XShareHandle()
{
    delete db_;
}

// Returns 0=none, 1=read, 2=write, 3=admin.
int XShareHandle::GetPermLevel(int64_t folder_id, const string &username)
{
    if (!db_) return 0;
    stringstream ss;
    ss << "SELECT permission_level FROM shared_folder_permissions WHERE folder_id="
       << folder_id << " AND username='" << EscStr(username) << "'";
    auto rows = db_->GetResult(ss.str().c_str());
    if (rows.empty() || rows[0].empty()) return 0;
    string lv = rows[0][0].data ? rows[0][0].data : "";
    if (lv == "admin") return 3;
    if (lv == "write") return 2;
    if (lv == "read")  return 1;
    return 0;
}

// Returns the owner of a shared folder, or "" if not found.
static string GetFolderOwner(LXMysql *db, int64_t folder_id)
{
    if (!db) return "";
    stringstream ss;
    ss << "SELECT owner FROM shared_folders WHERE id=" << folder_id;
    auto rows = db->GetResult(ss.str().c_str());
    if (rows.empty() || rows[0].empty()) return "";
    return rows[0][0].data ? rows[0][0].data : "";
}

void XShareHandle::CreateShareFolderReq(XMsgHead *head, XMsg *msg)
{
    XCreateShareFolderReq req;
    XCreateShareFolderRes res;
    if (!req.ParseFromArray(msg->data, msg->size) || !db_)
    {
        res.set_code(1); res.set_msg("parse error or db unavailable");
        head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
        SendMsg(head, &res); return;
    }

    string name  = req.name();
    string owner = head->username();

    if (name.empty() || name.size() > MAX_FOLDERNAME_LEN)
    {
        res.set_code(1); res.set_msg("invalid folder name");
        head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
        SendMsg(head, &res); return;
    }
    if (owner.empty() || owner.size() > MAX_USERNAME_LEN)
    {
        res.set_code(1); res.set_msg("invalid username");
        head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
        SendMsg(head, &res); return;
    }

    // Use INSERT + UNIQUE KEY to atomically detect duplicates (no TOCTOU race).
    stringstream ins;
    ins << "INSERT INTO shared_folders (owner,name,storage_path) VALUES('"
        << EscStr(owner) << "','" << EscStr(name) << "','')";
    if (!db_->Query(ins.str().c_str()))
    {
        res.set_code(1); res.set_msg("folder name already exists");
        head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
        SendMsg(head, &res); return;
    }
    int64_t folder_id = db_->GetInsertID();
    if (folder_id <= 0)
    {
        res.set_code(1); res.set_msg("failed to retrieve insert ID");
        head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
        SendMsg(head, &res); return;
    }

    // Update storage_path to use the real folder_id (not the name).
    stringstream upd;
    upd << "UPDATE shared_folders SET storage_path='_shared/" << folder_id
        << "' WHERE id=" << folder_id;
    if (!db_->Query(upd.str().c_str()))
        LOGERROR("CreateShareFolderReq: failed to update storage_path");

    string dir = DIR_ROOT + string("_shared/") + to_string(folder_id) + "/";
    XNewDir(dir.c_str());

    string folder_id_str = to_string(folder_id);
    XDATA perm;
    perm["folder_id"]        = folder_id_str.c_str();
    perm["username"]         = owner.c_str();
    perm["permission_level"] = "admin";
    db_->Insert(perm, "shared_folder_permissions");

    for (auto &u : req.users())
    {
        if (u.username().empty() || u.username().size() > MAX_USERNAME_LEN) continue;
        string perm_lv = u.permission_level();
        if (!IsValidPermission(perm_lv)) perm_lv = "read";
        XDATA up;
        up["folder_id"]        = folder_id_str.c_str();
        up["username"]         = u.username().c_str();
        up["permission_level"] = perm_lv.c_str();
        db_->Insert(up, "shared_folder_permissions");
    }

    res.set_code(0);
    res.set_folder_id(folder_id);
    head->set_msg_type((MsgType)CREATE_SHARE_FOLDER_RES);
    SendMsg(head, &res);
}

void XShareHandle::AddShareUserReq(XMsgHead *head, XMsg *msg)
{
    XAddShareUserReq req;
    xmsg::XMessageRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("parse error");
        head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    if (GetPermLevel(req.folder_id(), head->username()) < 3)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("PERMISSION_DENIED");
        head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    for (auto &u : req.users())
    {
        string uname = u.username();
        string perm_lv = u.permission_level();
        if (uname.empty() || uname.size() > MAX_USERNAME_LEN)
        {
            res.set_return_(XMessageRes::ERROR); res.set_msg("invalid username");
            head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
            SendMsg(head, &res); return;
        }
        if (!IsValidPermission(perm_lv))
        {
            res.set_return_(XMessageRes::ERROR);
            res.set_msg("invalid permission_level: " + perm_lv);
            head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
            SendMsg(head, &res); return;
        }
        string esc_user = EscStr(uname);
        string esc_perm = EscStr(perm_lv);
        stringstream ss;
        ss << "INSERT INTO shared_folder_permissions (folder_id,username,permission_level) VALUES("
           << req.folder_id() << ",'" << esc_user << "','"
           << esc_perm << "') ON DUPLICATE KEY UPDATE permission_level='" << esc_perm << "'";
        db_->Query(ss.str().c_str());
    }
    res.set_return_(XMessageRes::OK);
    head->set_msg_type((MsgType)ADD_SHARE_USER_RES);
    SendMsg(head, &res);
}

void XShareHandle::RemoveShareUserReq(XMsgHead *head, XMsg *msg)
{
    XRemoveShareUserReq req;
    xmsg::XMessageRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("parse error");
        head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    if (GetPermLevel(req.folder_id(), head->username()) < 3)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("PERMISSION_DENIED");
        head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
        SendMsg(head, &res); return;
    }
    string owner = GetFolderOwner(db_, req.folder_id());
    for (auto &uname : req.usernames())
    {
        // Prevent removing yourself or the folder owner.
        if (uname == head->username())
        {
            res.set_return_(XMessageRes::ERROR); res.set_msg("cannot remove yourself");
            head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
            SendMsg(head, &res); return;
        }
        if (!owner.empty() && uname == owner)
        {
            res.set_return_(XMessageRes::ERROR); res.set_msg("cannot remove folder owner");
            head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
            SendMsg(head, &res); return;
        }
        stringstream ss;
        ss << "DELETE FROM shared_folder_permissions WHERE folder_id=" << req.folder_id()
           << " AND username='" << EscStr(uname) << "'";
        db_->Query(ss.str().c_str());
    }
    res.set_return_(XMessageRes::OK);
    head->set_msg_type((MsgType)REMOVE_SHARE_USER_RES);
    SendMsg(head, &res);
}

void XShareHandle::GetSharedFoldersReq(XMsgHead *head, XMsg *msg)
{
    XGetSharedFoldersReq req;
    XGetSharedFoldersRes res;
    if (!req.ParseFromArray(msg->data, msg->size) || !db_)
    {
        head->set_msg_type((MsgType)GET_SHARED_FOLDERS_RES);
        SendMsg(head, &res); return;
    }
    // Always use the gateway-injected username; ignore req.username() from the client
    // to prevent enumeration of other users' shared folders.
    string username = head->username();
    if (username.empty() || username.size() > MAX_USERNAME_LEN)
    {
        head->set_msg_type((MsgType)GET_SHARED_FOLDERS_RES);
        SendMsg(head, &res); return;
    }
    stringstream ss;
    ss << "SELECT f.id,f.owner,f.name,f.created_at,"
       << "(SELECT COUNT(*) FROM shared_folder_permissions WHERE folder_id=f.id) AS uc"
       << " FROM shared_folders f"
       << " JOIN shared_folder_permissions p ON p.folder_id=f.id"
       << " WHERE p.username='" << EscStr(username) << "'";
    auto rows = db_->GetResult(ss.str().c_str());
    for (auto &row : rows)
    {
        if (row.size() < 5) continue;
        int64_t fid = row[0].data ? atoll(row[0].data) : 0;
        auto *fi = res.add_folders();
        fi->set_id(fid);
        fi->set_owner(row[1].data ? row[1].data : "");
        fi->set_name(row[2].data ? row[2].data : "");
        fi->set_created_at(row[3].data ? row[3].data : "");
        fi->set_user_count(row[4].data ? atoi(row[4].data) : 0);
        // Count files by scanning the shared directory on disk.
        int file_count = 0;
        if (fid > 0)
        {
            namespace fs = std::filesystem;
            string dir = DIR_ROOT + string("_shared/") + to_string(fid) + "/";
            std::error_code ec;
            for (auto &entry : fs::directory_iterator(dir, ec))
            {
                if (entry.is_regular_file())
                {
                    string fn = entry.path().filename().u8string();
                    if (fn.rfind(FILE_INFO_NAME_PRE, 0) != 0)
                        ++file_count;
                }
            }
        }
        fi->set_file_count(file_count);
    }
    head->set_msg_type((MsgType)GET_SHARED_FOLDERS_RES);
    SendMsg(head, &res);
}

void XShareHandle::GetSharedDirReq(XMsgHead *head, XMsg *msg)
{
    XGetSharedDirReq req;
    XGetSharedDirRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_code(1); res.set_msg("parse error");
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_code(1); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }
    if (GetPermLevel(req.folder_id(), head->username()) < 1)
    {
        res.set_code(1); res.set_msg("PERMISSION_DENIED");
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }
    if (!IsPathSafe(req.path()))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }
    string sandbox = DIR_ROOT + string("_shared/") + to_string(req.folder_id()) + "/";
    string dir = sandbox;
    if (!req.path().empty()) dir += req.path() + "/";

    // Symlink escape check.
    if (!IsSandboxed(sandbox, dir))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }

    namespace fs = std::filesystem;
    if (!fs::exists(dir))
    {
        res.set_code(0);
        head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
        SendMsg(head, &res); return;
    }
    for (auto &entry : fs::directory_iterator(dir))
    {
        string fname = entry.path().filename().u8string();
        if (fname.rfind(FILE_INFO_NAME_PRE, 0) == 0) continue;
        auto *fi = res.add_files();
        fi->set_filename(fname);
        fi->set_filedir(req.path());
        fi->set_is_dir(entry.is_directory());
        fi->set_filetime(GetFileTimeStr(entry));
        if (!entry.is_directory())
            fi->set_filesize((int64_t)entry.file_size());
        // Load .info_ sidecar, but cap to MAX_SIDECAR_BYTES to prevent OOM.
        string info_path = dir + FILE_INFO_NAME_PRE + fname;
        error_code ec;
        auto sidecar_size = fs::file_size(info_path, ec);
        if (!ec && sidecar_size <= MAX_SIDECAR_BYTES)
        {
            ifstream ifs(info_path, ios::binary);
            if (ifs.is_open())
            {
                XFileInfo info;
                if (info.ParseFromIstream(&ifs))
                {
                    fi->set_md5(info.md5());
                    fi->set_is_enc(info.is_enc());
                    fi->set_ori_size(info.ori_size());
                }
            }
        }
    }
    res.set_code(0);
    head->set_msg_type((MsgType)GET_SHARED_DIR_RES);
    SendMsg(head, &res);
}

void XShareHandle::DownloadSharedFileReq(XMsgHead *head, XMsg *msg)
{
    XDownloadSharedFileReq req;
    XDownloadSharedFileRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_code(1); res.set_msg("parse error");
        head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_code(1); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    int perm = GetPermLevel(req.folder_id(), head->username());
    if (perm != 1 && perm != 3)
    {
        res.set_code(1); res.set_msg("PERMISSION_DENIED: read or admin required");
        head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!IsPathSafe(req.filedir()) || !IsFilenameSafe(req.filename()))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    string sandbox  = DIR_ROOT + string("_shared/") + to_string(req.folder_id()) + "/";
    string rel_dir  = string("_shared/") + to_string(req.folder_id()) + "/";
    if (!req.filedir().empty()) rel_dir += req.filedir() + "/";
    string full_path = DIR_ROOT + rel_dir + req.filename();

    // Symlink escape check.
    if (!IsSandboxed(sandbox, full_path))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }

    string info_path = DIR_ROOT + rel_dir + FILE_INFO_NAME_PRE + req.filename();
    XFileInfo fi;
    ifstream ifs(info_path, ios::binary);
    if (ifs.is_open()) fi.ParseFromIstream(&ifs);
    fi.set_filename(req.filename());
    fi.set_filedir(rel_dir);
    fi.set_local_path(req.local_path());

    res.set_code(0);
    res.set_authorized_path(rel_dir);
    res.mutable_file_info()->CopyFrom(fi);
    head->set_msg_type((MsgType)DOWNLOAD_SHARED_FILE_RES);
    SendMsg(head, &res);
}

void XShareHandle::UploadSharedFileReq(XMsgHead *head, XMsg *msg)
{
    XUploadSharedFileReq req;
    XUploadSharedFileRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_code(1); res.set_msg("parse error");
        head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_code(1); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (GetPermLevel(req.folder_id(), head->username()) < 2)
    {
        res.set_code(1); res.set_msg("PERMISSION_DENIED: write or admin required");
        head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!IsPathSafe(req.filedir()))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    string sandbox = DIR_ROOT + string("_shared/") + to_string(req.folder_id()) + "/";
    string filedir = string("_shared/") + to_string(req.folder_id()) + "/";
    if (!req.filedir().empty()) filedir += req.filedir() + "/";
    if (!IsSandboxed(sandbox, DIR_ROOT + filedir))
    {
        res.set_code(1); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    res.set_code(0);
    res.set_authorized_filedir(filedir);
    head->set_msg_type((MsgType)UPLOAD_SHARED_FILE_RES);
    SendMsg(head, &res);
}

void XShareHandle::DeleteSharedFileReq(XMsgHead *head, XMsg *msg)
{
    XDeleteSharedFileReq req;
    xmsg::XMessageRes res;
    if (!req.ParseFromArray(msg->data, msg->size))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("parse error");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!db_)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("DB unavailable");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (GetPermLevel(req.folder_id(), head->username()) < 3)
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("PERMISSION_DENIED: admin required");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    if (!IsPathSafe(req.filedir()) || !IsFilenameSafe(req.filename()))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    string sandbox   = DIR_ROOT + string("_shared/") + to_string(req.folder_id()) + "/";
    string dir       = sandbox;
    if (!req.filedir().empty()) dir += req.filedir() + "/";
    string file_path = dir + req.filename();
    string info_path = dir + FILE_INFO_NAME_PRE + req.filename();

    // Symlink escape check.
    if (!IsSandboxed(sandbox, file_path))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("INVALID_PATH");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }

    namespace fs = std::filesystem;
    if (fs::is_directory(file_path))
    {
        res.set_return_(XMessageRes::ERROR); res.set_msg("cannot delete a directory");
        head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
        SendMsg(head, &res); return;
    }
    error_code ec;
    fs::remove(file_path, ec);
    fs::remove(info_path, ec);

    res.set_return_(XMessageRes::OK);
    head->set_msg_type((MsgType)DELETE_SHARED_FILE_RES);
    SendMsg(head, &res);
}
