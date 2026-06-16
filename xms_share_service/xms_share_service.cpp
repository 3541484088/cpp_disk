#include "xshare_server.h"
#include <iostream>
using namespace std;

int main(int argc, char *argv[])
{
    XShareServer server;
    server.main(argc, argv);
    server.Start();
    cout << SHARE_NAME << endl;
    server.Wait();
    return 0;
}
