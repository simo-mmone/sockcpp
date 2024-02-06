#pragma once

#include <vector>
#include <string>

std::string base64_encode(char const* buf, unsigned int bufLen);
std::vector<char> base64_decode(std::string const&);