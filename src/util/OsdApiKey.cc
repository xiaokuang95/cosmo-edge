#include "util/OsdApiKey.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "util/PathUtil.h"

namespace cosmo {
namespace {
    std::string KeyPath() {
        return (std::filesystem::path(cosmo::path::GetCfgPath()) / "osd_api_key").string();
    }

    std::string RandomHex64() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned long long> dist;
        std::ostringstream oss;
        oss << std::hex;
        for (int i = 0; i < 4; ++i) {
            oss.width(16);
            oss.fill('0');
            oss << dist(gen);
        }
        return oss.str();
    }
}  // namespace

std::string OsdApiKey::Read() {
    std::ifstream ifs(KeyPath());
    if (!ifs) {
        return {};
    }
    std::string key;
    std::getline(ifs, key);
    while (!key.empty() && (key.back() == '\r' || key.back() == '\n' || key.back() == ' ')) {
        key.pop_back();
    }
    return key;
}

std::string OsdApiKey::Generate() {
    const std::string key = RandomHex64();
    const std::string path = KeyPath();
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        ::write(fd, key.data(), key.size());
        ::write(fd, "\n", 1);
        ::close(fd);
        ::chmod(path.c_str(), 0600);
    }
    return key;
}

bool OsdApiKey::Valid(const std::string& presented) {
    const std::string expected = Read();
    if (expected.empty() || presented.empty() || expected.size() != presented.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        diff |= static_cast<unsigned char>(expected[i] ^ presented[i]);
    }
    return diff == 0;
}

}  // namespace cosmo
