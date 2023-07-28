class StartServer
{
public:
	StartServer(const char* listeningPort); // Constructor
	virtual ~StartServer();
	virtual void serverInitialization();
	virtual void serverHandler() = 0;
private:
	void initSockets(); // Function initiates use of the Winsock DLL
	void bindSocket(); // Function associates a local address with a socket
	void listenState(); // Set socket to listen state
	void stopServer(); // Freeing memory
	StartServer(const StartServer&) = delete; // Copy not allowed
	void operator=(const StartServer&) = delete; // Assignment not allowed
protected:
	void createSockInfo(const char* ip, const char* port, addrinfo** sockInfo); // Create addrinfo and translate host name to address
	void createNewSocket(SOCKET& new_socket, addrinfo* sockInfo); // Create socket with addrinfo parameters
	void acceptConnection();
public:
	HANDLE* serviceStopEvent;
private:
	const char* listeningPort;
	addrinfo* lisSockInfo;
	SOCKET lis_socket;
	WSAEVENT clientConnectionRequest;
	int errState;
	WSADATA wsData;
protected:
	SOCKET client_socket;
};
