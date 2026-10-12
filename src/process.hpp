#pragma once

#include <string>

namespace dsp {

struct CommandResult {
    int exit_code = -1;
    std::string output;
};

CommandResult RunCommand(const std::string& command);

}
