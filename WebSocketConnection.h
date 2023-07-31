class WebSocketConnection : public StartServer {
public:
	WebSocketConnection(const char* listeningPort, LPCWSTR serverIP, INTERNET_PORT serverPort);
	~WebSocketConnection();
	void serverInitialization() override;
	void serverHandler() override;
private:
	void WaitResponseFromServer();
	void connectToServer();
	void stopServer();
private:
	bool SendResponseStatus;
	bool ResponseStatus;
	HANDLE serverSendResponse;
	HINTERNET SessionHandle;
	HINTERNET ConnectionHandle;
	HINTERNET RequestHandle;
	HINTERNET WebSocketHandle;
	LPCWSTR serverIP;
	INTERNET_PORT serverPort;
};
