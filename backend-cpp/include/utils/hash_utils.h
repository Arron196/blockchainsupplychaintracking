#pragma once

#include <string>
#include <string_view>

#ifndef AGRI_USE_OPENSSL
#define AGRI_USE_OPENSSL 0
#endif

namespace agri {

std::string Sha256Hex(std::string_view input);
std::string CurrentUtcIso8601();

}
