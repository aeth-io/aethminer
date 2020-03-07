#include "stdafx.h"
#include "network.h"
#include "Client.h"

int network_quality = 100;
size_t send_interval = 100;
size_t update_interval = 10000; //10s
std::string nodeaddr = "localhost";
std::string nodeport = "8125";

std::map <u_long, unsigned long long> satellite_size;
std::thread showWinner;

char str_signature[65];
extern std::string Format(const char* fmt, ...);


void hostname_to_ip(char const *const  in_addr, char* out_addr)
{
	struct addrinfo *result = nullptr;
	struct addrinfo *ptr = nullptr;
	struct addrinfo hints;
	DWORD dwRetval;
	struct sockaddr_in  *sockaddr_ipv4;

	RtlSecureZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	dwRetval = getaddrinfo(in_addr, NULL, &hints, &result);
	if (dwRetval != 0) {
		Log("\n getaddrinfo failed with error: "); Log_llu(dwRetval);
		WSACleanup();
		exit(-1);
	}
	for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {

		if (ptr->ai_family == AF_INET)
		{
			sockaddr_ipv4 = (struct sockaddr_in *) ptr->ai_addr;
			char str[INET_ADDRSTRLEN];
			inet_ntop(hints.ai_family, &(sockaddr_ipv4->sin_addr), str, INET_ADDRSTRLEN);
			//strcpy(out_addr, inet_ntoa(sockaddr_ipv4->sin_addr));
			strcpy_s(out_addr, 50, str);
			Log("\nAddress: "); Log(in_addr); Log(" defined as: "); Log(out_addr);
		}
	}
	freeaddrinfo(result);
}

void send_i(void)
{
	Log("\nSender: started thread");

	int iResult = 0;

	char tbuffer[9];
	std::string jsondata;

	struct addrinfo *result = nullptr;

	for (; !exit_flag;)
	{
		if (stopThreads == 1)
		{
			return;
		}

		for (auto iter = shares.begin(); iter != shares.end();)
		{
			if ((iter->best / baseTarget) > bests[Get_index_acc(iter->account_id)].targetDeadline)
			{
				if (use_debug)
				{
					_strtime_s(tbuffer);
					bm_wattron(2);
					bm_wprintw("%s [%20llu]\t%llu > %llu  discarded\n", tbuffer, iter->account_id, iter->best / baseTarget, bests[Get_index_acc(iter->account_id)].targetDeadline, 0);
					bm_wattroff(2);
				}
				EnterCriticalSection(&sharesLock);
				iter = shares.erase(iter);
				LeaveCriticalSection(&sharesLock);
				continue;
			}

			if(true)
			{
				freeaddrinfo(result);

				int bytes = 0;
				if (miner_mode == 1)
				{
					unsigned long long total = total_size / 1024 / 1024 / 1024;
					for (auto It = satellite_size.begin(); It != satellite_size.end(); ++It) total = total + It->second;
					// bytes = sprintf_s(buffer, buffer_size, "POST /burst?requestType=submitNonce&accountId=%llu&nonce=%llu&deadline=%llu HTTP/1.0\r\nHost: %s:%s\r\nX-Miner: Blago %s\r\nX-Capacity: %llu\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", iter->account_id, iter->nonce, iter->best, nodeaddr.c_str(), nodeport.c_str(), version, total);

					//提交给矿池
					char result[1000];
					sprintf_s(result, "{\"jsonrpc\":\"2.0\",\"method\":\"miner_submitNonce\",\"params\":[\"%llu\",\"%llu\",%d,\"\",true],\"id\":0}", iter->nonce, iter->account_id, height);
					jsondata = result;
				}

				std::string url = "http://";
				url += nodeaddr.c_str();
				url += ":";
				url += nodeport.c_str();

				std::string result;
				bool ret = ClientPost(url.c_str(), jsondata.c_str(), result);

				if (ret == false)
				{
					if (network_quality > 0) network_quality--;
					Log("\nSender: ! Error deadline's sending: "); Log_u(WSAGetLastError());
					bm_wattron(12);
					bm_wprintw("SENDER: send failed: %ld\n", WSAGetLastError(), 0);
					bm_wattroff(12);
					continue;
				}
				else
				{
					unsigned long long dl = iter->best / baseTarget;
					_strtime_s(tbuffer);
					if (network_quality < 100) network_quality++;
					bm_wattron(9);
					bm_wprintw("%s [%20llu] sent DL: %15llu %5llud %02llu:%02llu:%02llu\n", tbuffer, iter->account_id, dl, (dl) / (24 * 60 * 60), (dl % (24 * 60 * 60)) / (60 * 60), (dl % (60 * 60)) / 60, dl % 60, 0);
					bm_wattroff(9);

					EnterCriticalSection(&sessionsLock);
					sessions.push_back({ dl, *iter,  result });
					LeaveCriticalSection(&sessionsLock);

					bests[Get_index_acc(iter->account_id)].targetDeadline = dl;
					EnterCriticalSection(&sharesLock);
					iter = shares.erase(iter);
					LeaveCriticalSection(&sharesLock);
				}
			}
		}

		if (!sessions.empty())
		{
			EnterCriticalSection(&sessionsLock);
			for (auto iter = sessions.begin(); iter != sessions.end() && !stopThreads;)
			{

				if (network_quality < 100) network_quality++;
				// 处理矿池返回数据
				std::string str_resp1 = iter->result;
				size_t pos1 = str_resp1.find("\"result\":{");
				if (pos1 != std::string::npos)
				{
					size_t pos2 = str_resp1.find("}", pos1 + 9);
					if (pos2 != std::string::npos)
					{
						std::string jsondata = str_resp1.substr(pos1 + 9, pos2 + 1 - pos1 - 9);
						unsigned long long ndeadline;
						unsigned long long naccountId = 0;
						unsigned long long ntargetDeadline = 0;
						rapidjson::Document answ;
						// burst.ninja        {"requestProcessingTime":0,"result":"success","block":216280,"deadline":304917,"deadlineString":"3 days, 12 hours, 41 mins, 57 secs","targetDeadline":304917}
						// pool.burst-team.us {"requestProcessingTime":0,"result":"success","block":227289,"deadline":867302,"deadlineString":"10 days, 55 mins, 2 secs","targetDeadline":867302}
						// proxy              {"result": "proxy","accountId": 17930413153828766298,"deadline": 1192922,"targetDeadline": 197503}
						if (!answ.Parse<0>(jsondata.c_str()).HasParseError())
						{
							if (answ.IsObject())
							{
								if (answ.HasMember("deadline")) {
									if (answ["deadline"].IsString())	ndeadline = _strtoui64(answ["deadline"].GetString(), 0, 10);
									else
										if (answ["deadline"].IsInt64()) ndeadline = answ["deadline"].GetInt64();
									Log("\nSender: confirmed deadline: "); Log_llu(ndeadline);

									if (answ.HasMember("targetDeadline")) {
										if (answ["targetDeadline"].IsString())	ntargetDeadline = _strtoui64(answ["targetDeadline"].GetString(), 0, 10);
										else
											if (answ["targetDeadline"].IsInt64()) ntargetDeadline = answ["targetDeadline"].GetInt64();
									}
									if (answ.HasMember("accountId")) {
										if (answ["accountId"].IsString())	naccountId = _strtoui64(answ["accountId"].GetString(), 0, 10);
										else
											if (answ["accountId"].IsInt64()) naccountId = answ["accountId"].GetInt64();
									}

									unsigned long long days = (ndeadline) / (24 * 60 * 60);
									unsigned hours = (ndeadline % (24 * 60 * 60)) / (60 * 60);
									unsigned min = (ndeadline % (60 * 60)) / 60;
									unsigned sec = ndeadline % 60;
									_strtime_s(tbuffer);
									bm_wattron(10);
									if ((naccountId != 0) && (ntargetDeadline != 0))
									{
										EnterCriticalSection(&bestsLock);
										bests[Get_index_acc(naccountId)].targetDeadline = ntargetDeadline;
										LeaveCriticalSection(&bestsLock);
										bm_wprintw("%s [%20llu] confirmed DL: %10llu %5llud %02u:%02u:%02u\n", tbuffer, naccountId, ndeadline, days, hours, min, sec, 0);
										if (use_debug) bm_wprintw("%s [%20llu] set targetDL: %10llu\n", tbuffer, naccountId, ntargetDeadline, 0);
									}
									else bm_wprintw("%s [%20llu] confirmed DL: %10llu %5llud %02u:%02u:%02u\n", tbuffer, iter->body.account_id, ndeadline, days, hours, min, sec, 0);
									bm_wattroff(10);
									if (ndeadline < deadline || deadline == 0)  deadline = ndeadline;

									if (ndeadline != iter->deadline)
									{
										bm_wattron(6);
										bm_wprintw("----Fast block or corrupted file?----\nSent deadline:\t%llu\nServer's deadline:\t%llu \n----\n", iter->deadline, ndeadline, 0); //shares[i].file_name.c_str());
										bm_wattroff(6);
									}
								}
								else {
									if (answ.HasMember("errorDescription")) {
										bm_wattron(15);
										bm_wprintw("[ERROR %u] %s\n", answ["errorCode"].GetInt(), answ["errorDescription"].GetString(), 0);
										bm_wattroff(15);
										bm_wattron(12);
										if (answ["errorCode"].GetInt() == 1004) bm_wprintw("You need change reward assignment and wait 4 blocks (~16 minutes)\n"); //error 1004
										bm_wattroff(12);
									}
									else {
										bm_wattron(15);
										bm_wprintw("%s\n", jsondata.c_str());
										bm_wattroff(15);
									}
								}
							}
						}
						else
						{
							if (strstr(jsondata.c_str(), "Received share") != nullptr)
							{
								_strtime_s(tbuffer);
								deadline = bests[Get_index_acc(iter->body.account_id)].DL;
								// if(deadline > iter->deadline) deadline = iter->deadline;
								bm_wattron(10);
								bm_wprintw("%s [%20llu] confirmed DL   %10llu\n", tbuffer, iter->body.account_id, iter->deadline, 0);
								bm_wattroff(10);
							}
							else
							{
								int minor_version;
								int status = 0;
								const char* msg;
								size_t msg_len;
								struct phr_header headers[12];
								size_t num_headers = sizeof(headers) / sizeof(headers[0]);
								phr_parse_response(iter->result.c_str(), iter->result.size(), &minor_version, &status, &msg, &msg_len, headers, &num_headers, 0);

								if (status != 0)
								{
									bm_wattron(6);
									//wprintw("%s [%20llu] NOT confirmed DL %10llu\n", tbuffer, iter->body.account_id, iter->deadline, 0);
									std::string error_str(msg, msg_len);
									bm_wprintw("Server error: %d %s\n", status, error_str.c_str());
									bm_wattroff(6);
									Log("\nSender: server error for DL: "); Log_llu(iter->deadline);
									shares.push_back({ iter->body.file_name, iter->body.account_id, iter->body.best, iter->body.nonce });
								}
								else
								{
									bm_wattron(7);
									bm_wattroff(7);
								}
							}
						}
					}
				}
				iter = sessions.erase(iter);
				if (iter != sessions.end()) ++iter;
			}
			LeaveCriticalSection(&sessionsLock);
		}
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(send_interval));
	}
	return;
}

void updater_i(void)
{
	for (; !exit_flag;) {
		pollLocal();
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(update_interval*10));
	}
}


void pollLocal(void) {

	size_t const buffer_size = 1000;
	char *buffer = (char*)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, buffer_size);
	if (buffer == nullptr) ShowMemErrorExit();

	int bytes = sprintf_s(buffer, buffer_size, "{\"jsonrpc\":\"2.0\",\"method\":\"miner_getMiningInfo\",\"params\":[],\"id\":0}");

	std::string url = "http://";
	url += nodeaddr.c_str();
	url += ":";
	url += nodeport.c_str();

	std::string result;
	bool ret = ClientPost(url.c_str(), buffer, result);

	if (ret == false)
		return;

	if (network_quality < 100) network_quality++;

	strcpy(buffer, result.c_str());

	char* buff_p = strstr(buffer, "\"result\":") + 9;
	char* end_p = strstr(buffer, "}");
	char temp_p[256] = { 0 };
	memcpy(temp_p, buff_p, (end_p + 1) - buff_p);
	char* find = temp_p;
	if (find == nullptr)	Log("\n*! GMI: error message from pool");
	rapidjson::Document gmi;
	if (gmi.Parse<0>(find).HasParseError()) Log("\n*! GMI: error parsing JSON message from pool");
	else {
		if (gmi.IsObject())
		{
			if (gmi.HasMember("baseTarget")) {
				if (gmi["baseTarget"].IsString())	baseTarget = _strtoui64(gmi["baseTarget"].GetString(), 0, 10);
				else
					if (gmi["baseTarget"].IsInt64()) baseTarget = gmi["baseTarget"].GetInt64();
			}

			if (gmi.HasMember("height")) {
				if (gmi["height"].IsString())	height = _strtoui64(gmi["height"].GetString(), 0, 10);
				else
					if (gmi["height"].IsInt64()) height = gmi["height"].GetInt64();
			}

			//POC2 determination
			if (height >= POC2StartBlock) {
				POC2 = true;
			}

			if (gmi.HasMember("generationSignature")) {
				strcpy_s(str_signature, gmi["generationSignature"].GetString());
				if (xstr2strr(signature, 33, gmi["generationSignature"].GetString()) == 0)	Log("\n*! GMI: Node response: Error decoding generationsignature\n");
			}
			if (gmi.HasMember("targetDeadline")) {
				if (gmi["targetDeadline"].IsString())
					targetDeadlineInfo = _strtoui64(gmi["targetDeadline"].GetString(), 0, 10);
				else
					if (gmi["targetDeadline"].IsInt64())
						targetDeadlineInfo = gmi["targetDeadline"].GetInt64();
			}
		}
	}
	HeapFree(hHeap, 0, buffer);
}

void ShowWinner(unsigned long long const num_block)
{
}

// Helper routines taken from http://stackoverflow.com/questions/1557400/hex-to-char-array-in-c
int xdigit(char const digit) {
	int val;
	if ('0' <= digit && digit <= '9') val = digit - '0';
	else if ('a' <= digit && digit <= 'f') val = digit - 'a' + 10;
	else if ('A' <= digit && digit <= 'F') val = digit - 'A' + 10;
	else val = -1;
	return val;
}

size_t xstr2strr(char *buf, size_t const bufsize, const char *const in) {
	if (!in) return 0; // missing input string

	size_t inlen = (size_t)strlen(in);
	if (inlen % 2 != 0) inlen--; // hex string must even sized

	size_t i, j;
	for (i = 0; i<inlen; i++)
		if (xdigit(in[i])<0) return 0; // bad character in hex string

	if (!buf || bufsize<inlen / 2 + 1) return 0; // no buffer or too small

	for (i = 0, j = 0; i<inlen; i += 2, j++)
		buf[j] = xdigit(in[i]) * 16 + xdigit(in[i + 1]);

	buf[inlen / 2] = '\0';
	return inlen / 2 + 1;
}