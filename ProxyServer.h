#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "ProxyConnection.h"
#include <thread>

void proxyServer(ProxyConnection& client, ProxyConnection& targetServer, HANDLE* stopEvent);

#endif
