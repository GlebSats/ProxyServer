#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "ProxyConnection.h"
#include <thread>

void proxyServer(std::unique_ptr<ProxyConnection> server, std::unique_ptr<ProxyConnection> client, HANDLE* stopEvent);

#endif