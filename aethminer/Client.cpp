#include "Client.h"	

CRequestHttp *g_http = NULL;
HANDLE g_httpMutex = 0;

void InitClient()
{
#ifdef WINDOWS
	WSADATA         WsaData;
	int err = WSAStartup(0x0101, &WsaData);                            // Init Winsock 
#endif

	g_http = new CRequestHttp();
	g_httpMutex = CreateMutex(NULL, FALSE, NULL);
}

void UniniClient()
{
	delete g_http;
	g_http = NULL;
	CloseHandle(g_httpMutex);
}

bool ClientPost(const char* url, const char* data, std::string& result)
{
	WaitForSingleObject(g_httpMutex, INFINITE);

	//g_http->set_keepalive();
	if (g_http->request_post(url, data) == false)
	{
		delete g_http;
		g_http = new CRequestHttp();
		ReleaseMutex(g_httpMutex);
		return false;
	}
	unsigned int length;
	result = (char*)g_http->get_response_content(length);

	
	ReleaseMutex(g_httpMutex);
	return true;
}