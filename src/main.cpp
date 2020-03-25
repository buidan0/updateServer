/*
 ============================================================================
 Name        : UpdateServer.cpp
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C++,
 ============================================================================
 */

#include <csignal>
#include <cstdio>
#include "UpgradeServer.h"
#include <cstring>

using namespace std;

#define VERSION "v1.0.1"


void SingHandler(int signalNum) {
    cout << "Program exit!\n" << endl;
    exit(0);
}

int main(int argc, char **argv) {
    cout << "version is " << VERSION << endl;
    int port = 0;
    if (argc < 2) {
        cout << "use default port:60000" << endl;
        port = 60000;
    } else if (argc == 2) {
        if (strlen(argv[1]) > 5) {
            cout << "argv[1] is error" << endl;
            return -1;
        }
        port = atoi(argv[1]);
        if (port < 1000 || port >= 65535) {
            cout << "port is error" << endl;
            return -1;
        }
    } else if (argc > 2) {
        cout << "usage: UpgradeServer port" << endl;
        return 0;
    }

    signal(SIGINT, SingHandler);
    UpgradeServer server(port);
    server.start();
    return 0;
}
