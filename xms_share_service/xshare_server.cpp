#include "xshare_server.h"
#include "xshare_handle.h"
#include "xlog_client.h"
#include "xregister_client.h"
#include "xtools.h"
#include <sstream>
using namespace std;

void XShareServer::main(int argc, char *argv[])
{
    XShareHandle::RegMsgCallback();

    int service_port = SHARE_PORT;
    int register_port = REGISTER_PORT;
    string register_ip = XGetHostByName(API_REGISTER_SERVER_NAME);
    if (argc > 1) register_ip = argv[1];
    if (argc > 2) register_port = atoi(argv[2]);
    if (argc > 3) service_port = atoi(argv[3]);

    set_server_port(service_port);
    XRegisterClient::Get()->set_server_ip(register_ip.c_str());
    XRegisterClient::Get()->set_server_port(register_port);
    // is_find=false: the gateway does NOT expose this service to external clients.
    // Clients talk to the share service only via the gateway's forwarding path,
    // not by direct connection (unlike upload/download which use is_find=true).
    XRegisterClient::Get()->RegisterServer(SHARE_NAME, service_port, "127.0.0.1", false);

    auto log = XLogClient::Get();
    log->set_service_name(SHARE_NAME);
    log->set_is_print(true);
    stringstream ss;
    ss << SHARE_NAME << "_" << service_port << ".log";
    log->set_local_file(ss.str());
    log->StartLog();
}

XServiceHandle* XShareServer::CreateServiceHandle()
{
    return new XShareHandle();
}
