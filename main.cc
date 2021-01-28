#include <iostream>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#include <vulkan/vulkan.hpp>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>// GLFW should be included after vulkan

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

const char *const kAppName = "Light";
const char *const kEngineName = "Vulkan";
const uint32_t kWidth = 64;
const uint32_t kHeight = 64;

#pragma region classes

struct GlfwContext {
    bool initialized{false};

    GlfwContext() {
        glfwSetErrorCallback([](int err, const char *msg) {
            std::cerr << "glfw: (" << err << ") " << msg << std::endl;
        });
        if (initialized = static_cast<bool>(glfwInit()); !initialized) {
            throw std::runtime_error("glfw init error");
        }
    }

    ~GlfwContext() {
        if (initialized) { glfwTerminate(); }
    }
};

struct Window {
    Window(GLFWwindow *window, const std::string &name,
           const vk::Extent2D &extent);
    ~Window() noexcept;

    GLFWwindow *window;
    std::string name;
    vk::Extent2D extent;
};

Window::Window(GLFWwindow *window, const std::string &name,
               const vk::Extent2D &extent)
    : window(window), name(name), extent(extent) {}

Window::~Window() noexcept { glfwDestroyWindow(window); }

#pragma endregion

#pragma region math utils

template<typename T>
VULKAN_HPP_INLINE constexpr const T &clamp(const T &v, const T &low,
                                           const T &height) {
    return v < low ? low : height < v ? height : v;
}

#pragma endregion

#pragma region vulkan utils

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

    // register glfw required instance extensions (this needs glfwInit() first)
    uint32_t glfwExtensionCount{0};
    auto glfwExtensions =
            glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    assert(glfwExtensionCount > 0);
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
        auto ext = glfwExtensions[i];
        if (std::find(extensions.begin(), extensions.end(), ext) ==
                    extensions.end() &&
            std::find_if(extensionProperties.begin(), extensionProperties.end(),
                         [ext](const auto &ep) {
                             return strcmp(ext, ep.extensionName) == 0;
                         }) != extensionProperties.end()) {
            enabledExtensions.push_back(ext);
        }
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

uint32_t findGraphicsQueueFamilyIndex(
        const std::vector<vk::QueueFamilyProperties> &queueFamilyProperties) {
    // get the first index into queue family properties which support graphics
    size_t graphicsQueueFamilyIndex = std::distance(
            queueFamilyProperties.begin(),
            std::find_if(
                    queueFamilyProperties.begin(), queueFamilyProperties.end(),
                    [](const vk::QueueFamilyProperties &qfp) {
                        return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                    }));
    assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());
    return static_cast<uint32_t>(graphicsQueueFamilyIndex);
}

std::pair<uint32_t, uint32_t>
findGraphicsAndPresentQueueFamilyIndex(vk::PhysicalDevice physicalDevice,
                                       const VkSurfaceKHR &surface) {
    auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    uint32_t graphicsQueueFamilyIndex =
            findGraphicsQueueFamilyIndex(queueFamilyProperties);

    // determine a queue family that supports present
    // first check if the graphics queue family is good enough
    if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex,
                                            surface)) {
        return std::make_pair(graphicsQueueFamilyIndex,
                              graphicsQueueFamilyIndex);
    }

    // the graphics queue doesn't support present, look for an other
    // family that supports both graphics and present
    for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
        if ((queueFamilyProperties[i].queueFlags &
             vk::QueueFlagBits::eGraphics) &&
            physicalDevice.getSurfaceSupportKHR(i, surface)) {
            return std::make_pair(i, i);
        }
    }
    // there's nothing like a single family index that supports both
    // graphics and present, look for an other family that supports
    // present
    for (uint32_t i = 0; i < queueFamilyProperties.size(); i++) {
        if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
            return std::make_pair(graphicsQueueFamilyIndex, i);
        }
    }
    throw std::runtime_error("could not find a queue for graphics or "
                             "present");
}

std::vector<std::string> getInstanceExtensions() {
    std::vector<std::string> extensions;
    extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
    return extensions;
}

std::vector<std::string> getDeviceExtensions() {
    std::vector<std::string> extensions;
    extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifndef NDEBUG
    // debug marker allow the assignment of internal names to Vulkan resources.
    // these internal names will conveniently be visible in debugger like RenderDoc.
    // debug marker are only available if RenderDoc is enabled.
    extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif
    return extensions;
}

vk::UniqueDevice
createDevice(vk::PhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             const std::vector<std::string> &extensions = {},
             const vk::PhysicalDeviceFeatures *physicalDeviceFeatures = nullptr,
             const void *next = nullptr) {
    std::vector<const char *> enabledExtensions;
    enabledExtensions.reserve(extensions.size());
    for (const auto &ext : extensions) {
        enabledExtensions.push_back(ext.c_str());
    }

    float queuePriority = 0.0f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority);
    vk::DeviceCreateInfo deviceCreateInfo(
            vk::DeviceCreateFlags(), deviceQueueCreateInfo, {},
            enabledExtensions, physicalDeviceFeatures);
    deviceCreateInfo.pNext = next;
    return physicalDevice.createDeviceUnique(deviceCreateInfo);
}

Window createWindow(const std::string &windowName, const vk::Extent2D &extent) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, true);
    glfwWindowHint(GLFW_RESIZABLE, true);
    GLFWwindow *w = glfwCreateWindow(extent.width, extent.height,
                                     windowName.c_str(), nullptr, nullptr);
    if (!w) { throw std::runtime_error("glfw create window error"); }
    return Window(w, windowName, extent);
}

uint32_t
findMemoryType(vk::PhysicalDeviceMemoryProperties const &memoryProperties,
               uint32_t typeBits, vk::MemoryPropertyFlags mask) {
    auto typeIndex = uint32_t(~0);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & 1) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & mask) == mask)) {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));
    return typeIndex;
}

vk::UniqueDeviceMemory
allocateMemory(const vk::UniqueDevice &device,
               const vk::PhysicalDeviceMemoryProperties &memoryProperties,
               const vk::MemoryRequirements &memoryRequirements,
               vk::MemoryPropertyFlags memoryPropertyFlags) {
    uint32_t memoryTypeIndex =
            findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits,
                           memoryPropertyFlags);
    return device->allocateMemoryUnique(
            vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
}

glm::mat4x4 createModelViewProjectionClipMatrix(const vk::Extent2D &extent) {
    float fov = glm::radians(45.0f);
    if (extent.width > extent.height) {
        fov *= static_cast<float>(extent.height) /
               static_cast<float>(extent.width);
    }

    glm::mat4x4 model = glm::mat4x4(1.0f);
    glm::mat4x4 view = glm::lookAt(glm::vec3(-5.0f, 3.0f, -10.0f),
                                   glm::vec3(0.0f, 0.0f, 0.0f),
                                   glm::vec3(0.0f, -1.0f, 0.0f));
    glm::mat4x4 projection = glm::perspective(fov, 1.0f, 0.1f, 100.0f);
    // vulkan clip space has inverted y and half z
    // clang-format off
    glm::mat4x4 clip = glm::mat4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f,0.5f, 0.0f,
            0.0f, 0.0f, 0.5f,1.0f);
    // clang-format on

    return clip * projection * view * model;
}

template<typename T>
void copyToDevice(const vk::UniqueDevice &device,
                  const vk::UniqueDeviceMemory &memory, const T *data,
                  size_t size, vk::DeviceSize stride = sizeof(T)) {
    assert(sizeof(T) <= stride);
    uint8_t *deviceData = static_cast<uint8_t *>(
            device->mapMemory(memory.get(), 0, size * stride));
    if (stride == sizeof(T)) {
        memcpy(deviceData, data, size * sizeof(T));
    } else {
        for (size_t i = 0; i < size; ++i) {
            memcpy(deviceData, &data[i], sizeof(T));
            deviceData += stride;
        }
    }
    device->unmapMemory(memory.get());
}

template<typename T>
void copyToDevice(const vk::UniqueDevice &device,
                  const vk::UniqueDeviceMemory &memory, const T &data) {
    copyToDevice<T>(device, memory, &data, 1);
}

#pragma endregion

#pragma region

struct Surface {
    vk::Extent2D extent;
    Window window;
    vk::UniqueSurfaceKHR surface;

    Surface(vk::UniqueInstance &instance, const std::string &windowName,
            const vk::Extent2D &extent);
};

Surface::Surface(vk::UniqueInstance &instance, const std::string &windowName,
                 const vk::Extent2D &extent)
    : extent(extent), window(createWindow(windowName, extent)) {
    VkSurfaceKHR surf;
    if (VkResult r = glfwCreateWindowSurface(instance.get(), window.window,
                                             nullptr, &surf);
        r != VK_SUCCESS) {
        throw std::runtime_error("glfw create window surface error");
    }
    vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> deleter(
            instance.get());
    surface = vk::UniqueSurfaceKHR(surf, deleter);
}

struct Buffer {
    Buffer(const vk::PhysicalDevice &physicalDevice,
           const vk::UniqueDevice &device, vk::DeviceSize size,
           vk::BufferUsageFlags usage,
           vk::MemoryPropertyFlags propertyFlags =
                   vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory deviceMemory;

#ifndef NDEBUG
private:
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::MemoryPropertyFlags propertyFlags;
#endif
};

Buffer::Buffer(const vk::PhysicalDevice &physicalDevice,
               const vk::UniqueDevice &device, vk::DeviceSize size,
               vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags propertyFlags)
#ifndef NDEBUG
    : size(size), usage(usage), propertyFlags(propertyFlags)
#endif
{
    buffer = device->createBufferUnique(
            vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage));
    deviceMemory = allocateMemory(
            device, physicalDevice.getMemoryProperties(),
            device->getBufferMemoryRequirements(buffer.get()), propertyFlags);
    device->bindBufferMemory(buffer.get(), deviceMemory.get(), 0);
}

#pragma endregion

int main() {
    try {
        static auto glfwCtx = GlfwContext();

        vk::UniqueInstance instance =
                createInstance(kAppName, kEngineName, 1, 1, VK_API_VERSION_1_2,
                               {}, getInstanceExtensions());
#ifndef NDEBUG
        vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger =
                createDebugUtilsMessenger(instance);
#endif

        vk::PhysicalDevice physicalDevice =
                instance->enumeratePhysicalDevices().front();

        Surface surface(instance, kAppName, {kWidth, kHeight});

        const auto &[graphicsQueueFamilyIndex, presentQueueFamilyIndex] =
                findGraphicsAndPresentQueueFamilyIndex(physicalDevice,
                                                       *surface.surface);

        vk::UniqueDevice device =
                createDevice(physicalDevice, graphicsQueueFamilyIndex,
                             getDeviceExtensions());

        // create a command pool to allocate a command buffer from
        vk::UniqueCommandPool commandPool = device->createCommandPoolUnique(
                vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlags(),
                                          graphicsQueueFamilyIndex));
        // allocate command buffer from the command pool
        vk::UniqueCommandBuffer commandBuffer = std::move(
                device->allocateCommandBuffersUnique(
                              vk::CommandBufferAllocateInfo(
                                      commandPool.get(),
                                      vk::CommandBufferLevel::ePrimary, 1))
                        .front());

        // get the supported surface formats
        std::vector<vk::SurfaceFormatKHR> formats =
                physicalDevice.getSurfaceFormatsKHR(*surface.surface);
        assert(!formats.empty());
        vk::Format format = (formats[0].format == vk::Format::eUndefined)
                                    ? vk::Format::eB8G8R8A8Unorm
                                    : formats[0].format;

        vk::SurfaceCapabilitiesKHR surfaceCapabilities =
                physicalDevice.getSurfaceCapabilitiesKHR(*surface.surface);
        VkExtent2D swapchainExtent;
        if (surfaceCapabilities.currentExtent.width ==
            std::numeric_limits<uint32_t>::max()) {
            // if the surface size is undefined, the size is set to the size of
            // the images requested
            swapchainExtent.width =
                    clamp(kWidth, surfaceCapabilities.minImageExtent.width,
                          surfaceCapabilities.maxImageExtent.width);
            swapchainExtent.height =
                    clamp(kHeight, surfaceCapabilities.minImageExtent.height,
                          surfaceCapabilities.maxImageExtent.height);
        } else {
            // if the surface size is defined, the swap chain size must match
            swapchainExtent = surfaceCapabilities.currentExtent;
        }

        // FIFO present mode is guaranteed by the spec to be supported
        vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

        vk::SurfaceTransformFlagBitsKHR transform =
                (surfaceCapabilities.supportedTransforms &
                 vk::SurfaceTransformFlagBitsKHR::eIdentity)
                        ? vk::SurfaceTransformFlagBitsKHR::eIdentity
                        : surfaceCapabilities.currentTransform;

        vk::CompositeAlphaFlagBitsKHR compositeAlpha =
                (surfaceCapabilities.supportedCompositeAlpha &
                 vk::CompositeAlphaFlagBitsKHR::ePreMultiplied)
                        ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
                : (surfaceCapabilities.supportedCompositeAlpha &
                   vk::CompositeAlphaFlagBitsKHR::ePostMultiplied)
                        ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
                : (surfaceCapabilities.supportedCompositeAlpha &
                   vk::CompositeAlphaFlagBitsKHR::eInherit)
                        ? vk::CompositeAlphaFlagBitsKHR::eInherit
                        : vk::CompositeAlphaFlagBitsKHR::eOpaque;

        vk::SwapchainCreateInfoKHR swapchainCreateInfo(
                vk::SwapchainCreateFlagsKHR(), *surface.surface,
                surfaceCapabilities.minImageCount, format,
                vk::ColorSpaceKHR::eSrgbNonlinear, swapchainExtent, 1,
                vk::ImageUsageFlagBits::eColorAttachment,
                vk::SharingMode::eExclusive, {}, transform, compositeAlpha,
                swapchainPresentMode, true, nullptr);

        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            // if the graphics and present queues are from different queue
            // families, we either have to explicitly transfer ownership of
            // images between the queues, or we have to create the swapchain
            // with imageSharingMode as VK_SHARING_MODE_CONCURRENT

            uint32_t queueFamilyIndices[2] = {
                    static_cast<uint32_t>(graphicsQueueFamilyIndex),
                    static_cast<uint32_t>(presentQueueFamilyIndex)};

            swapchainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            swapchainCreateInfo.queueFamilyIndexCount = 2;
            swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        vk::UniqueSwapchainKHR swapchain =
                device->createSwapchainKHRUnique(swapchainCreateInfo);

        std::vector<vk::Image> swapchainImages =
                device->getSwapchainImagesKHR(swapchain.get());

        std::vector<vk::UniqueImageView> imageViews;
        imageViews.reserve(swapchainImages.size());
        vk::ComponentMapping componentMapping(
                vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
        vk::ImageSubresourceRange subresourceRange(
                vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
        for (const auto &image : swapchainImages) {
            vk::ImageViewCreateInfo imageViewCreateInfo(
                    vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D,
                    format, componentMapping, subresourceRange);
            imageViews.push_back(
                    device->createImageViewUnique(imageViewCreateInfo));
        }

        // init depth buffer
        const vk::Format depthFormat = vk::Format::eD16Unorm;
        vk::FormatProperties formatProperties =
                physicalDevice.getFormatProperties(depthFormat);

        vk::ImageTiling tiling;
        if (formatProperties.linearTilingFeatures &
            vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            tiling = vk::ImageTiling::eLinear;
        } else if (formatProperties.optimalTilingFeatures &
                   vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            tiling = vk::ImageTiling::eOptimal;
        } else {
            throw std::runtime_error("DepthStencilAttachment is not supported "
                                     "for D16Unorm depth format");
        }
        vk::ImageCreateInfo imageCreateInfo(
                vk::ImageCreateFlags(), vk::ImageType::e2D, depthFormat,
                vk::Extent3D(surface.extent, 1), 1, 1,
                vk::SampleCountFlagBits::e1, tiling,
                vk::ImageUsageFlagBits::eDepthStencilAttachment);
        vk::UniqueImage depthImage = device->createImageUnique(imageCreateInfo);

        vk::PhysicalDeviceMemoryProperties memoryProperties =
                physicalDevice.getMemoryProperties();
        vk::MemoryRequirements memoryRequirements =
                device->getImageMemoryRequirements(depthImage.get());
        uint32_t typeBits = memoryRequirements.memoryTypeBits;
        uint32_t typeIndex = findMemoryType(
                memoryProperties, memoryRequirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal);
        vk::UniqueDeviceMemory depthMemory = device->allocateMemoryUnique(
                vk::MemoryAllocateInfo(memoryRequirements.size, typeIndex));

        device->bindImageMemory(depthImage.get(), depthMemory.get(), 0);

        vk::ImageSubresourceRange subResourceRange(
                vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);
        vk::UniqueImageView depthView =
                device->createImageViewUnique(vk::ImageViewCreateInfo(
                        vk::ImageViewCreateFlags(), depthImage.get(),
                        vk::ImageViewType::e2D, depthFormat, componentMapping,
                        subResourceRange));

        // init uniform buffer
        Buffer uniformBuffer(physicalDevice, device, sizeof(glm::mat4x4),
                             vk::BufferUsageFlagBits::eUniformBuffer);
        copyToDevice(device, uniformBuffer.deviceMemory,
                     createModelViewProjectionClipMatrix(vk::Extent2D(0, 0)));

        // init pipeline
        // create a DescriptorSetLayout
        vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding(
                0, vk::DescriptorType::eUniformBuffer, 1,
                vk::ShaderStageFlagBits::eVertex);
        vk::UniqueDescriptorSetLayout descriptorSetLayout =
                device->createDescriptorSetLayoutUnique(
                        vk::DescriptorSetLayoutCreateInfo(
                                vk::DescriptorSetLayoutCreateFlags(),
                                descriptorSetLayoutBinding));

        // create a PipelineLayout using that DescriptorSetLayout
        vk::UniquePipelineLayout pipelineLayout =
                device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo(
                        vk::PipelineLayoutCreateFlags(), *descriptorSetLayout));
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
