#include <iostream>
#include <vulkan/vulkan.hpp>

const char *const kAppName = "Light";
const char *const kEngineName = "Vulkan";

int main() {
    try {
        vk::ApplicationInfo applicationInfo(kAppName, 1, kEngineName, 1,
                                            VK_API_VERSION_1_2);
        vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);
        vk::UniqueInstance instance =
                vk::createInstanceUnique(instanceCreateInfo);
    } catch (vk::SystemError &err) {
        std::cerr << "vk::SystemError: " << err.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (std::exception &ex) {
        std::cerr << "std::exception: " << ex.what() << std::endl;
        exit(EXIT_FAILURE);
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
