#include "process.hpp"

#include <cctype>
#include <cstdio>

#if defined(_WIN32)
#  define DSP_POPEN  _popen
#  define DSP_PCLOSE _pclose
#  define DSP_PROCESS_SUPPORTED 1
#elif defined(__linux__) || defined(__APPLE__)
#  include <sys/wait.h>
#  define DSP_POPEN  popen
#  define DSP_PCLOSE pclose
#  define DSP_PROCESS_SUPPORTED 1
#else
#  define DSP_PROCESS_SUPPORTED 0
#endif

namespace dsp {
namespace {

#if DSP_PROCESS_SUPPORTED
std::string Sanitize(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (c == '\n' || c == '\t') out.push_back(static_cast<char>(c));
        else if (c == '\r') continue;
        else if (c < 0x20 || c >= 0x7F) out.push_back('?');
        else out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    size_t start = 0;
    while (start < out.size() && std::isspace(static_cast<unsigned char>(out[start]))) ++start;
    return out.substr(start);
}
#endif

}

CommandResult RunCommand(const std::string& command) {
    CommandResult result;
#if DSP_PROCESS_SUPPORTED
    const std::string full = command + " 2>&1";
    FILE* pipe = DSP_POPEN(full.c_str(), "r");
    if (!pipe) {
        result.output = "failed to start process";
        return result;
    }
    std::string raw;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe)) raw += buffer;

    int rc = DSP_PCLOSE(pipe);
#if !defined(_WIN32)
    rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#endif
    result.exit_code = rc;
    result.output = Sanitize(raw);
#else
    (void)command;
    result.output = "unsupported platform";
#endif
    return result;
}

}
