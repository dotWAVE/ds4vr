#include <openvr_driver.h>

#include <cstring>

#include "server_driver.h"

#if defined(_WIN32)
    #define DS4VR_EXPORT extern "C" __declspec(dllexport)
#else
    #define DS4VR_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
ds4vr::ServerDriver g_server_driver;
}

DS4VR_EXPORT void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode)
{
    if (pInterfaceName != nullptr &&
        std::strcmp(pInterfaceName, vr::IServerTrackedDeviceProvider_Version) == 0)
    {
        return static_cast<vr::IServerTrackedDeviceProvider *>(&g_server_driver);
    }

    if (pReturnCode != nullptr) {
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    }
    return nullptr;
}
