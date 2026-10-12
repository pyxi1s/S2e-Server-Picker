#pragma once

#include <string>
#include <vector>

#include "servers.hpp"

namespace dsp {

struct OperationResult {
    bool ok = false;
    std::string message;
};

class FirewallManager {
public:
    static bool IsElevated();

    static std::string PlatformName();

    static std::string ElevationHint();

    static bool RelaunchElevated(int argc, char** argv);

    OperationResult BlockRegion(const Region& region);
    OperationResult UnblockRegion(const Region& region);
    OperationResult ResetAll(const std::vector<Region>& regions);
    std::vector<bool> QueryBlocked(const std::vector<Region>& regions);
};

}
