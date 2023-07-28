class TCPConnection: public StartServer {
public:
	TCPConnection(const char* listeningPort, const char* serverIP, const char* serverPort);
	~TCPConnection();
	void serverHandler() override;
private:
	void connectToServer();
	void sockCommunication();
	void createSocketEvents();
	void closeConnection();
	TCPConnection(const TCPConnection&) = delete; // Copy not allowed
	void operator=(const TCPConnection&) = delete; // Assignment not allowed
private:
	const char* serverIP;
	const char* serverPort;
	addrinfo* serverSockInfo;
	SOCKET server_socket;
	WSAEVENT bufToClientHasData;
	WSAEVENT bufToServHasData;
	WSAEVENT clientReadySend;
	WSAEVENT serverReadySend;
	int errState;
};
