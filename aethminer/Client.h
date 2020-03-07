#pragma once

#include "CRequestHttp.h"
#include <string>

bool ClientPost(const char* url, const char* data,std::string& result);

void InitClient();

void UniniClient();