#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

const char *const kAppName = "Light";
const char *const kEngineName = "Vulkan";

// ======= utils begin =======

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        VkDebugUtilsMessengerCallbackDataEXT const *callbackData,
        void * /*userData*/) {
    std::cerr << vk::to_string(
                         static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(
                                 messageSeverity))
              << ": "
              << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(
                         messageTypes))
              << ":\n";
    std::cerr << "\t"
              << "messageIDName   = <" << callbackData->pMessageIdName << ">\n";
    std::cerr << "\t"
              << "messageIdNumber = " << callbackData->messageIdNumber << "\n";
    std::cerr << "\t"
              << "message         = <" << callbackData->pMessage << ">\n";
    if (0 < callbackData->queueLabelCount) {
        std::cerr << "\t"
                  << "Queue Labels:\n";
        for (uint8_t i = 0; i < callbackData->queueLabelCount; i++) {
            std::cerr << "\t\t"
                      << "labelName = <"
                      << callbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < callbackData->cmdBufLabelCount) {
        std::cerr << "\t"
                  << "CommandBuffer Labels:\n";
        for (uint8_t i = 0; i < callbackData->cmdBufLabelCount; i++) {
            std::cerr << "\t\t"
                      << "labelName = <"
                      << callbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < callbackData->objectCount) {
        std::cerr << "\t"
                  << "Objects:\n";
        for (uint8_t i = 0; i < callbackData->objectCount; i++) {
            std::cerr << "\t\t"
                      << "Object " << i << "\n";
            std::cerr << "\t\t\t"
                      << "objectType   = "
                      << vk::to_string(static_cast<vk::ObjectType>(
                                 callbackData->pObjects[i].objectType))
                      << "\n";
            std::cerr << "\t\t\t"
                      << "objectHandle = "
                      << callbackData->pObjects[i].objectHandle << "\n";
            if (callbackData->pObjects[i].pObjectName) {
                std::cerr << "\t\t\t"
                          << "objectName   = <"
                          << callbackData->pObjects[i].pObjectName << ">\n";
            }
        }
    }
    return VK_TRUE;
}

vk::UniqueInstance
createInstance(const std::string &appName, const std::string &engineName,
               uint32_t appVersion, uint32_t engineVersion, uint32_t apiVersion,
               const std::vector<std::string> &layers = {},
               const std::vector<std::string> &extensions = {}) {
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    static vk::DynamicLoader dl;
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
            "vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif

#ifndef NDEBUG
    std::vector<vk::LayerProperties> layerProperties =
            vk::enumerateInstanceLayerProperties();
    std::vector<vk::ExtensionProperties> extensionProperties =
            vk::enumerateInstanceExtensionProperties();
#endif

    std::vector<const char *> enabledLayers;
    enabledLayers.reserve(layers.size());
    for (const auto &layer : layers) {
        assert(std::find_if(layerProperties.begin(), layerProperties.end(),
                            [layer](const auto &lp) {
                                return layer == lp.layerName;
                            }) != layerProperties.end());
        enabledLayers.push_back(layer.data());
    }

#ifndef NDEBUG
    // enable validation layer to find as much errors as possible
    std::vector<std::string> instanceDebugLayers = {
            // standard validation layer
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_LUNARG_assistant_layer",
            // RenderDoc
            "VK_LAYER_RENDERDOC_Capture",
    };
    for (const auto &layer : instanceDebugLayers) {
        if (std::find(layers.begin(), layers.end(), layer) == layers.end() &&
            std::find_if(layerProperties.begin(), layerProperties.end(),
                         [layer](const auto &lp) {
                             return layer == lp.layerName;
                         }) != layerProperties.end()) {
            enabledLayers.push_back(layer.c_str());
        }
    }
#endif

    std::vector<const char *> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (const auto &ext : extensions) {
        assert(std::find_if(extensionProperties.begin(),
                            extensionProperties.end(), [ext](const auto &ep) {
                                return ext == ep.extensionName;
                            }) != extensionProperties.end());
        enabledExtensions.push_back(ext.data());
    }

#if !defined(NDEBUG)
    // in debug mode, use the following instance extensions
    std::vector<std::string> instanceDebugExtensions = {
            // assign internal names to Vulkan resources
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            // set up a Vulkan debug report callback function
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    };
    for (const auto &ext : instanceDebugExtensions) {
        if (std::find(extensions.begin(), extensions.end(), ext) ==
                    extensions.end() &&
            std::find_if(extensionProperties.begin(), extensionProperties.end(),
                         [ext](const auto &ep) {
                             return ext == ep.extensionName;
                         }) != extensionProperties.end()) {
            enabledExtensions.push_back(ext.c_str());
        }
    }
#endif

    vk::ApplicationInfo applicationInfo(appName.c_str(), appVersion,
                                        engineName.c_str(), engineVersion,
                                        apiVersion);
#if defined(NDEBUG)
    // in non-debug mode just use the InstanceCreateInfo for instance creation
    vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfo(
            {{}, &applicationInfo, enabledLayers, enabledExtensions});
#else
    // in debug mode, additionally use the debugUtilsMessengerCallback in instance creation
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    vk::StructureChain<vk::InstanceCreateInfo,
                       vk::DebugUtilsMessengerCreateInfoEXT>
            instanceCreateInfo(
                    {{}, &applicationInfo, enabledLayers, enabledExtensions},
                    {{},
                     severityFlags,
                     messageTypeFlags,
                     &debugUtilsMessengerCallback});
#endif

    auto instance = vk::createInstanceUnique(
            instanceCreateInfo.get<vk::InstanceCreateInfo>());
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    // initialize function pointers for instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
#endif
    return instance;
}

vk::UniqueDebugUtilsMessengerEXT
createDebugUtilsMessenger(vk::UniqueInstance &instance) {
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    return instance->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags,
                                                 messageTypeFlags,
                                                 &debugUtilsMessengerCallback));
}

// ======= utils end =======

int main() {
    try {
        vk::UniqueInstance instance =
                createInstance(kAppName, kEngineName, 1, 1, VK_API_VERSION_1_2);
#ifndef NDEBUG
        vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger =
                createDebugUtilsMessenger(instance);
#endif

        vk::PhysicalDevice physicalDevice =
                instance->enumeratePhysicalDevices().front();
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
