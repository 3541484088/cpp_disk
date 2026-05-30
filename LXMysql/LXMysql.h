#ifndef LXMYSQL_H
#define LXMYSQL_H

#include "LXData.h"

namespace LX 
{
	class LXMysql
	{
	public:
		LXMysql() {}
		~LXMysql() { Close(); }

		bool Init();
		void Close();

		bool Connect(const char* host, const char* user, const char* pass,
			const char* db, unsigned short port = 3306, unsigned long flag = 0);
		bool Query(const char* sql, unsigned long sqllen = 0);
		bool Options(LX_OPT opt, const void* arg);
		bool SetConnectTimeout(int sec);
		bool SetReconnect(bool isre);
		bool StoreResult();
		bool UseResult();
		void FreeResult();
		std::vector<LXData> FetchRow();
		std::string GetInsertSql(XDATA kv, std::string table);
		bool Insert(XDATA kv, std::string table);
		bool InsertBin(XDATA kv, std::string table);
		int GetInsertID();
		std::string GetUpdateSql(XDATA kv, std::string table, std::string where);
		int Update(XDATA kv, std::string table, std::string where);
		int UpdateBin(XDATA kv, std::string table, std::string where);
		bool StartTransaction();
		bool StopTransaction();
		bool Commit();
		bool Rollback();
		XROWS GetResult(const char* sql);
		bool InputDBConfig();

		void* mysql = 0;
		void* result = 0;
	};
}

#endif
