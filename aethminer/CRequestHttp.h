// Copyright 2012.  All rights reserved.
// http://skycpp.cn
// Author: zhaolei
// E-mail: qjzl2008@163.com
#define WINDOWS
#include <string>
#include <list>

#ifdef WINDOWS
#include <winsock2.h> 
#include <ws2tcpip.h>
#pragma comment(lib, "WS2_32")   
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
typedef int SOCKET;
#endif

#define MAXBUFFERSIZE 1024    //maximum size of recv buffer
#define MAXMALLOCSIZE 5000000   //maximum size of content for malloc

class CRequestHttp
{
private:
	CRequestHttp(const CRequestHttp&);
	CRequestHttp& operator=(const CRequestHttp&);

public:
	CRequestHttp(void);
	~CRequestHttp(void);
/*���׵��ýӿ�,δ������ձ���������get_response_content��ȡ*/
inline bool request_get(const char* url); 
inline bool request_get(const char* url,std::string& response_content);
inline bool request_post(const char* url,const char* post_content);
inline bool request_post(const char* url,const char* post_content,std::string& response_content);
//��ȡ���󷵻����ݣ�����Ϊ���ݳ���
unsigned char* get_response_content(unsigned int &length) const;
//��ȡ������Ϣͷ
std::string get_request_headers(void)const;
//��ȡ�����Ӧ��Ϣͷ
std::string get_response_headers(void)const;
//��ȡ���һ�δ�����Ϣ
std::string get_error_info(void)const;
//��ȡ����״̬��
int get_status(void)const;

/*�Զ�������*/
//���������Ϣͷ
void add_part_request_headers(const char* key,const char* value);
//ɾ�������Ϣͷ���ָ���Ĭ����Ϣͷ
void clear_addheaders(void);
//ɾ��ָ��������Ϣͷ
void delete_part_request_headers(const char* key);
//�����Ƿ񱣴�cookies
void save_cookies(bool flag);
//�Ƿ����Referer
bool care_referer;
//���ñ��ֳ�����
void set_keepalive() { _is_keepalive = true; }

private:
void init(void);
//��װurl����ϸ��
bool request_web(bool is_post,const char* url);
bool socket_request(const char* host,const char* sub_directory,const int port =80);
bool parse_url(const char*url,std::string& host,std::string& sub_directory,int& port);
bool check_host_valid(const std::string& host);
unsigned long get_host_address(const char* host);
//��װsend
void send_string(SOCKET& sock,const char* content);
void set_error(const char* content);
//������Ӧ��Ϣͷcookies
void set_cookies(const char* cookies);
//��ȡ��Ϣͷ�е�״̬��
bool set_status(std::string& headers);
private:
std::list<std::string> _default_headers; 
std::list<std::string> _add_headers; 
std::string _part_request_headers;
std::string _response_headers;
std::string _cookies;
std::string _err_info;
std::string _post_content;
unsigned char* _response_content;//���ݻ�����
unsigned int _content_size;//��������С
unsigned int _content_pos;
bool _is_post;
bool _set_cookies;
bool _is_keepalive;
bool _is_first_connect;
int _status;
int _sock;
};
/*
eg.
url:
	"http://www.google.com","www.google.com","www.google.com/test"
	"www.google.com:80","www.google.com:80/test","www.google.com:8080/test"
	.....
	����http:// Ĭ�Ͻ���Ϊhttp
post_content:
	"key=value&key2=value2" ...
	
*/
inline bool
CRequestHttp::request_post(const char* url,const char* post_content)
{
	_post_content = post_content;
	return request_web(true,url);
}

inline bool
CRequestHttp::request_post(const char* url,const char* post_content,std::string& response_content)
{
	_post_content = post_content;
	bool flag = request_web(true,url);
	if(flag)
		response_content = std::string((char*)_response_content,_content_pos);
	return flag;
}

inline bool
CRequestHttp::request_get(const char* url) 
{
	return request_web(false,url);
}

inline bool
CRequestHttp::request_get(const char* url,std::string& response_content) 
{
	bool flag = request_web(false,url);
	if(flag)
		response_content = std::string((char*)_response_content,_content_pos);
	return flag;
}