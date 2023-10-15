#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "ProxyConnection.h"
#include <thread>

void proxyServer(std::shared_ptr<ProxyConnection> server, std::shared_ptr<ProxyConnection> client, HANDLE* stopEvent);

#endif
