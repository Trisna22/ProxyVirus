#include "stdafx.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include <netdb.h>

#ifndef ProxyVirus_H
#define ProxyVirus_H

#define PROXY_NOT_READY		0x00022
#define PROXY_BAD_COMMAND	0x00023

#define PROXY_CONNECT_COMMAND	0xfff10
#define PROXY_TAP_COMMAND	0xfff11
#define PROXY_PING_COMMAND	0xfff12
#define PROXY_CURL_COMMAND	0xfff13

class ProxyVirus
{
public:
	ProxyVirus();
	~ProxyVirus();
	bool Initialize();
	bool StartProxyServer();
	void CloseProxyServer();
private:
	int server;
	string ERROR_MSG = "NO_ERROR";
	bool IsReady = false;

	void ErrorMSG(int);
	bool ProxyHosting();
	void ForkConnection(int);
	int QueryCommand(string);

	bool ProcessConnectInformation(string, string*, int*);
	bool ProcessTapInformation(string, string*);
	bool ProcessWebsiteInformation(string, string*, string*);


	struct ThreadArgs {
		int clientSocket;
		int proxySocket;
		
		pthread_mutex_t mutex;
	};
	int OpenConnection(string, int);
	bool StartConnectionThread(int, int, pthread_t*, pthread_mutex_t);
	static void* ConnectionThread(void*);

	bool TapIPAddress(string);
	bool CurlWebsite(string, string);
};

#endif //! ~ProxyVirus_H

/*   Constructor of class   */
ProxyVirus::ProxyVirus()
{
	if (Initialize() == false)
		return;
}

/*   Deconstructor of class   */
ProxyVirus::~ProxyVirus()
{
	CloseProxyServer();
}

/*   Initialize proxy server   */
bool ProxyVirus::Initialize()
{
	cout << "[ * ] Initializing proxy server..." << endl;

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		cout << "[ - ] Failed to create socket!" << endl;
		ErrorMSG(errno);
		return false;
	}

	sockaddr_in ServerAddr;
	ServerAddr.sin_port = htons(8888);
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = INADDR_ANY;

	int result = bind(sock, (sockaddr*)&ServerAddr, sizeof(ServerAddr));
	if (result == -1)
	{
		cout << "[ - ] Failed to bind the socket!" << endl;
		ErrorMSG(errno);
		return false;
	}

	result = listen(sock, 10);
	if (result == -1)
	{
		cout << "[ - ] Failed to start listeing for connections!" << endl;
		ErrorMSG(errno);
		return false;
	}

	cout << "[ + ] Proxy Server initialized succesfully!" << endl << endl;
	IsReady = true;
	server = sock;
	return true;
}

/*   Starts proxy server   */
bool ProxyVirus::StartProxyServer()
{
	cout << "[ * ] Started creating proxy thread..." << endl;
	if (IsReady == false)
	{
		ErrorMSG(PROXY_NOT_READY);
		return false;
	}

	return ProxyHosting();
}

/*   Closes the proxy server   */
void ProxyVirus::CloseProxyServer()
{
	close(server);
	cout << "\n[ + ] Proxy server closed!" << endl;
	IsReady = false;
}

/*   Prints out the error message   */
void ProxyVirus::ErrorMSG(int errorCode)
{
	ERROR_MSG = "[ - ] Error " + to_string(errorCode) + " occured: ";
	switch (errorCode)
	{
	case EACCES:
	{
		ERROR_MSG += "Access denied!\n";
		break;
	}
	case ENOENT:
	{
		ERROR_MSG += "File/folder not found!\n";
		break;
	}
	case EINVAL:
	{
		ERROR_MSG += "Invalid argument given!\n";
		break;
	}
	case ENOSPC:
	{
		ERROR_MSG += "No space on disk!\n";
		break;
	}
	case EADDRINUSE:
	{
		ERROR_MSG += "Address already in use!\n";
		break;
	}
	case ENETDOWN:
	{
		ERROR_MSG += "Network is down!\n";
		break;
	}
	case ENETUNREACH:
	{
		ERROR_MSG += "Network is unreachable!\n";
		break;
	}
	case ECONNABORTED:
	{
		ERROR_MSG += "Connection aborted!\n";
		break;
	}
	case ECONNRESET:
	{
		ERROR_MSG += "Connection reset by peer!\n";
		break;
	}
	case ETIMEDOUT:
	{
		ERROR_MSG += "Connection timed out!\n";
		break;
	}
	case ECONNREFUSED:
	{
		ERROR_MSG += "Connection refused!\n";
		break;
	}
	case PROXY_NOT_READY:
	{
		ERROR_MSG += "Proxy server not ready!\n";
		break;
	}
	case PROXY_BAD_COMMAND:
	{
		ERROR_MSG += "Client gave bad commands!\n";
		break;
	}

	default:
	{
		ERROR_MSG += "Unknown error code! \n";
		break;
	}
	}

	ERROR_MSG += "\n";
	cout << ERROR_MSG;
}

/*   Thread hosts proxy server   */
bool ProxyVirus::ProxyHosting()
{
	cout << "[ + ] Thread created succesfully!" << endl << endl;
	int SOCKET = server;
	sockaddr_in ClientAddr;
	socklen_t addrlen;

	while (true)
	{
		int connection = accept(SOCKET, (sockaddr*)&ClientAddr, &addrlen);
		if (connection == -1)
		{
			close(SOCKET);
			cout << "[ - ] Failed to accept connection!" << endl;
			ErrorMSG(errno);
			return false;
		}

		cout << "[ ! ] Proxy client " << inet_ntoa(ClientAddr.sin_addr) << ":" << to_string(ClientAddr.sin_port) << endl;

		ForkConnection(connection);
		close(connection);
	}
}

/*   Forks the connection and handles input   */
void ProxyVirus::ForkConnection(int connection)
{
	if (!fork())
	{
		timeval tv;
		tv.tv_sec = 10;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(connection, &fds);
		int retval = select(connection +1, &fds, 0, 0, &tv);
		if (retval == 0)
		{
			ERROR_MSG = "Failed to wait for clients initializing message! Error code: " + to_string(errno) + "\n\n";
			send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(connection);
			return;
		}

		// First receive the init message.
		int bytes;
		char buffer[1024];
		if ((bytes = recv(connection, buffer, 1024, 0)) == -1)
		{
			ERROR_MSG = "Receiving init message failed! Error code: " + to_string(errno) + "\n\n";
			send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(connection);
			return;
		}

		// Check if init message is correct.
		buffer[bytes] = '\0';
		string initClient = buffer;
		if (initClient != "PROXY_SERVER_CLIENT_HELLO\n")
		{
			ERROR_MSG = "Failed to receive the init message! Error code: " + to_string(errno) + "\n\n";
			send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(connection);
			return;
		}

		// Send init message.
		string init = "PROXY_SERVER_SERVER_HELLO\n";
		send(connection, init.c_str(), init.length(), 0);

		// Get connect information.
		if ((bytes = recv(connection, buffer, 1024, 0)) == -1)
		{
			ERROR_MSG = "Receiving connect information failed! Error code: " + to_string(errno) + "\n\n";
			send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(connection);
			return;
		}

		// Find the command the user wants to use.
		buffer[bytes] = '\0';
		string COMMAND = buffer;
		int ACTION = QueryCommand(COMMAND);

		switch (ACTION)
		{
			case PROXY_CONNECT_COMMAND:
			{
				// Process information.
				int PORT;
				string IP;
				if (ProcessConnectInformation(COMMAND, &IP, &PORT) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				volatile int SOCKET;
				// Open connection to remote target.
				if ((SOCKET = OpenConnection(IP, PORT)) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				ERROR_MSG = "CONNECT 100 OK\n\n";
				send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);

				pthread_t thread1;
				pthread_mutex_t mutex;
				if (pthread_mutex_init(&mutex, NULL) != 0)
				{
					ERROR_MSG = "\nFailed to create a mutex object! Error code: " + to_string(errno) + "\n\n";
					return;
				}

				if (StartConnectionThread(SOCKET, connection, &thread1, mutex) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				// Listen for packets from client
				fd_set fds;
				while (true)
				{
					FD_ZERO(&fds);
					FD_SET(connection, &fds);

					// Wait for commands.
					int result = select(connection +1, &fds, 0, 0, 0);
					if (result == 0)
					{
						close(connection);
						pthread_cancel(thread1);
						return;
					}

					// Get mutex object.
					if (pthread_mutex_lock(&mutex) != 0)
					{
						ERROR_MSG = "\nFailed to lock the mutex object!\n\n";
						send(connection , ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
						pthread_cancel(thread1);
						close(connection);
						return;
					}

					// Receive the client data for proxyserver.
					char data[1024];
					int bytes = result = recv(connection, data, sizeof(data), 0);
					if (bytes == -1)
					{
						close(connection);
						pthread_cancel(thread1);
						return;
					}

					data[bytes] = '\0';
					string dataProxyServer = data;
					printf("clientdata TO BOUNCE: %s\n", dataProxyServer.c_str());			
					
					// Send the data to proxyserver.
					send(SOCKET, dataProxyServer.c_str(), dataProxyServer.length(), 0);

					// Release mutex object.
					if (pthread_mutex_unlock(&mutex) != 0)
					{
						ERROR_MSG ="\nFailed to unlock the mutex object!\n\n";
						send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
						pthread_cancel(thread1);
						close(connection);
						return;
					}
				}
				return;
			}

			case PROXY_TAP_COMMAND:
			{
				// Process information.
				string IP;
				if (ProcessTapInformation(COMMAND, &IP) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				// Tap IP address.
				if (TapIPAddress(IP) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				ERROR_MSG = "TAP 100 OK\n\n";
				send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
				return;
			}

			case PROXY_CURL_COMMAND:
			{
				// Retrieve website.
				string WEBSITE, REQUEST_METHOD;
				if (ProcessWebsiteInformation(COMMAND, &WEBSITE, &REQUEST_METHOD) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				// Curl IP address.
				if (CurlWebsite(WEBSITE, REQUEST_METHOD) == false)
				{
					send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
					close(connection);
					return;
				}

				ERROR_MSG = "CURL 100 OK\n\n";
				send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
				return;
			}

			default:
			{
				ErrorMSG(PROXY_BAD_COMMAND);
				send(connection, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
				return;
			}

		}

		close(connection);
	}
}

/*   Gets the command ID from string   */
int ProxyVirus::QueryCommand(string data)
{
	if (data.find("CONNECT ") != string::npos)
	{
		return PROXY_CONNECT_COMMAND;
	}
	else if (data.find("TAP ") != string::npos)
	{
		return PROXY_TAP_COMMAND;
	}
	else if (data.find("CURL ") != string::npos)
	{
		return PROXY_CURL_COMMAND;
	}
	else
	{
		return PROXY_BAD_COMMAND;
	}
}

/*   Processes the connection information   */
bool ProxyVirus::ProcessConnectInformation(string DATA, string* IP, int* PORT)
{
	if (DATA.find("CONNECT ") == string::npos || DATA.find(":") == string::npos)
	{
		ERROR_MSG = "Client sended invalid CONNECT request!\nUsage: CONNECT {IP}:{PORT}\n\n\n";
		return false;
	}

	// IP address.
	string a = DATA.substr(8);
	*IP = a.substr(0, a.find(":"));

	// Check if string is actually an IP address.
	// If not resolve hostname to IP address.
	string HOSTNAME = *IP;
    	struct sockaddr_in sa;
	if (inet_pton(AF_INET, HOSTNAME.c_str(), &(sa.sin_addr)) != 1)
	{
		hostent* record = gethostbyname(HOSTNAME.c_str());
		if (record == 0)
		{
			ERROR_MSG = "Failed to resolve hostname to IP!\n\n";
			return false;
		}

		int i = 0;
		while (record->h_addr_list[i] != 0)
		{
			in_addr* address = (in_addr *)record->h_addr_list[i++];
			*IP = inet_ntoa(*address);
		}
	}

	// PORT number.
	string b = a.substr(a.find(":") +1);
	string p = b.substr(0, b.length() -1);
	*PORT = atoi(p.c_str());
	return true;
}

/*   Processes the tap information   */
bool ProxyVirus::ProcessTapInformation(string DATA, string* IP) 
{
	if (DATA.find("TAP ") == string::npos || DATA.length() < 11)
	{
		ERROR_MSG = "Client sended invalid TAP request!\nUsage: TAP {IP}\n\n";
		return false;
	}

	// IP address.
	string a = DATA.substr(4);
	*IP = a.substr(0, a.length() -1);

	// Check if string is actually an IP address.
	// If not resolve hostname to IP address.
	string HOSTNAME = *IP;
	struct sockaddr_in sa;
	if (inet_pton(AF_INET, HOSTNAME.c_str(), &(sa.sin_addr)) != 1)
	{
		hostent* record = gethostbyname(HOSTNAME.c_str());
		if (record == 0)
		{
			ERROR_MSG = "Failed to resolve hostname to IP!\n\n";
			return false;
		}

		int i = 0;
		while (record->h_addr_list[i] != 0)
		{
			in_addr* address = (in_addr *)record->h_addr_list[i++];
			*IP = inet_ntoa(*address);
		}
		return true;
	}
	
	return true;
}

/*   Processes the curl information   */
bool ProxyVirus::ProcessWebsiteInformation(string DATA, string* WEBSITE, string* RESPOSE_HEADER)
{
	return false;
}

/*   Opens connection to other side   */
int ProxyVirus::OpenConnection(string IP, int PORT)
{
	int SOCKET = socket(AF_INET, SOCK_STREAM, 0);
	if (SOCKET == -1)
	{
		ERROR_MSG = "Failed to create socket for opening connection! Error code: " + to_string(errno) + "\n\n";
		return false;
	}

	sockaddr_in TargetAddr;
	TargetAddr.sin_port = htons(PORT);
	TargetAddr.sin_family = AF_INET;
	TargetAddr.sin_addr.s_addr = inet_addr(IP.c_str());
	socklen_t addrlen;

	int result = connect(SOCKET, (sockaddr*)&TargetAddr, sizeof(TargetAddr));
	if (result == -1)
	{
		if (errno == EACCES || errno == EPERM)
		{
			ERROR_MSG = "Failed to connect! Permission denied!\n\n";
			return false;
		}
		else if (errno == ECONNREFUSED)
		{
			ERROR_MSG = "Failed to connect! Connection refused!\n\n";
			return false;
		}
		else if (errno == ETIMEDOUT)
		{
			ERROR_MSG = "Failed to connect! Connection timed out!\n\n";
			return false;
		}
		else
		{
			ERROR_MSG = "Failed to connect! Error code: " + to_string(errno) + "\n\n";
			return false;
		}
		return false;
	}

	cout << "[ + ] Succesfully connected with " << IP << ":" << to_string(PORT) << endl;
	return SOCKET;
}

/*   Starts thread that holds connection   */
bool ProxyVirus::StartConnectionThread(int ProxySocket, int ClientSocket, 
	pthread_t* thread1, pthread_mutex_t mutex)
{
	ThreadArgs args;
	args.proxySocket = ProxySocket;
	args.clientSocket = ClientSocket;

	args.mutex = mutex;

	if (pthread_create(thread1, 0, ProxyVirus::ConnectionThread, &args) != 0)
	{
		ERROR_MSG = "\nFailed to start thread that holds the connection! Error code:" + to_string(errno) + "\n\n";
		return false;
	}

	return true;
}

/*   Thread that holds connection   */
void* ProxyVirus::ConnectionThread(void* param)
{
	ThreadArgs* args = (ThreadArgs*)param;
	pthread_mutex_t mutex = args->mutex;
	string ERROR_MSG;

	// Listening for packets from proxy target.
	fd_set fds;
	while (true)
	{
		// Receive packets.
		FD_ZERO(&fds);
		FD_SET(args->proxySocket, &fds);

		// Wait for commands.
		int result = select(args->proxySocket +1, &fds, 0, 0, 0);
		if (result == 0)
		{
			ERROR_MSG ="\nFailed to read data from proxysocket!\n\n";	
			send(args->clientSocket, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(args->proxySocket);
			close(args->clientSocket);
			pthread_exit((void*)0);
		}

		// Receive the client data for proxyserver.
		char data[1024];
		int bytes = result = recv(args->proxySocket, data, sizeof(data), 0);
		if (bytes == -1)
		{
			ERROR_MSG ="\nFailed to lock the mutex object!\n\n";	
			send(args->clientSocket, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(args->clientSocket);
			close(args->proxySocket);
			pthread_exit((void*)0);
		}

		data[bytes] = '\0';
		string dataClientServer = data;
		printf("proxydata TO BOUNCE: %s\n", dataClientServer.c_str());

		// Get mutex object.
		if (pthread_mutex_lock(&mutex) != 0)
		{
			ERROR_MSG = "\nFailed to lock the mutex object!\n\n";
			printf("Locking mutex failed!!! error code: %d\n", errno);
			send(args->clientSocket , ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(args->proxySocket);
			close(args->clientSocket);
			pthread_exit((void*)0);
		}


		// Send the data to client.
		send(args->clientSocket, dataClientServer.c_str(), dataClientServer.length(), 0);

		// Release mutex object.
		if (pthread_mutex_unlock(&mutex) != 0)
		{
			ERROR_MSG ="\nFailed to unlock the mutex object!\n\n";	
			send(args->clientSocket, ERROR_MSG.c_str(), ERROR_MSG.length(), 0);
			close(args->proxySocket);
			close(args->clientSocket);
			pthread_exit((void*)0);
		}
	}
}

/*   Taps the IP address to check if it is alive   */
bool ProxyVirus::TapIPAddress(string IP)
{
	string command = "ping -c 4 " + IP;
	int returnValue = system(command.c_str());
	if (returnValue == 0) 
		return true;
	else if (returnValue == 256)
	{
		ERROR_MSG = "TAP 101 Host Unreachable!\n\n";
		return false;
	}
	else if (returnValue == 512)
	{
		ERROR_MSG = "TAP 102 Network Unreachable!\n\n";
		return false;
	}
	else if (returnValue == -1)
	{
		ERROR_MSG = "TAP 104 system() error occured: " + to_string(errno) + "\n\n";
		return false;
	}
	else
	{
		ERROR_MSG = "TAP 103 Unknown system() error: " + to_string(returnValue) + "\n\n";
		return false;
	}
}

/*   Curls the website and get the request method   */
bool ProxyVirus::CurlWebsite(string WEBSITE, string REQUEST_METHOD)
{
	return false;
}
