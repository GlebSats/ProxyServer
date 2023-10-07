#ifndef PROXYSERVER_H
#define PROXYSERVER_H

#include "ProxyConnection.h"
#include <thread>

void proxyServer(ProxyConnection& server, ProxyConnection& client, HANDLE* stopEvent);

#endif