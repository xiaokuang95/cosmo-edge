#pragma once

#include <string>
#include <vector>

namespace cosmo {

// apiKey for third-party OSD snapshot / channel-list endpoints.
// Key file: /userdata/cwaiuserdata/conf/osd_api_key (first line, mode 600).
struct OsdApiKey {
    // Read key from disk; empty if missing/unreadable.
    static std::string Read();
    // Generate a new 64-hex key, persist with mode 600, return it.
    static std::string Generate();
    // Constant-time-ish compare against header value.
    static bool Valid(const std::string& presented);
};

}  // namespace cosmo
