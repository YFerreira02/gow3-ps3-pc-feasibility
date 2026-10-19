#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef void (__cdecl *ProbeLog)(const char *message);
typedef void (__cdecl *ProbeFrame)(const void *pixels, uint32_t byte_count);
static void report(ProbeLog log, const char *message) { if (log) log(message); }
static int strings_equal(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)severity; (void)type;
    ProbeLog log = (ProbeLog)user;
    if (log && data && data->pMessage) log(data->pMessage);
    return VK_FALSE;
}
BOOL WINAPI DllMainCRTStartup(HINSTANCE module, DWORD reason, LPVOID reserved) { return TRUE; }

static int find_memory_type(VkPhysicalDeviceMemoryProperties *properties, uint32_t bits,
    VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred) {
    for (uint32_t i = 0; i < properties->memoryTypeCount; ++i)
        if ((bits & (1u << i)) &&
            (properties->memoryTypes[i].propertyFlags & required) == required &&
            (properties->memoryTypes[i].propertyFlags & preferred) == preferred) return (int)i;
    for (uint32_t i = 0; i < properties->memoryTypeCount; ++i)
        if ((bits & (1u << i)) &&
            (properties->memoryTypes[i].propertyFlags & required) == required) return (int)i;
    return -1;
}

__declspec(dllexport) int __cdecl RunVulkanOffscreenProbe(uint32_t width, uint32_t height,
    uint32_t frames, ProbeLog log, ProbeFrame on_frame) {
    VkInstance instance = VK_NULL_HANDLE; VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE; VkDeviceMemory image_memory = VK_NULL_HANDLE;
    VkBuffer readback = VK_NULL_HANDLE; VkDeviceMemory readback_memory = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE; VkCommandBuffer command = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE; VkQueue queue = VK_NULL_HANDLE;
    void *mapped = NULL; VkBool32 coherent = VK_FALSE; int result = 1;

    if (!width || !height || (uint64_t)width * height * 4 > UINT32_MAX) {
        report(log, "Invalid offscreen dimensions."); return 1;
    }
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "GOW3 Vulkan 1.2 offscreen probe"; app.apiVersion = VK_API_VERSION_1_2;
    uint32_t extension_count = 0; VkBool32 debug_utils_enabled = VK_FALSE;
    VkExtensionProperties extensions[128];
    if (vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL) == VK_SUCCESS && extension_count <= 128 &&
        vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions) == VK_SUCCESS)
        for (uint32_t i = 0; i < extension_count; ++i)
            if (strings_equal(extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) debug_utils_enabled = VK_TRUE;
    const char *instance_extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &app;
    instance_info.enabledExtensionCount = debug_utils_enabled ? 1 : 0;
    instance_info.ppEnabledExtensionNames = instance_extensions;
    VkResult vr = vkCreateInstance(&instance_info, NULL, &instance);
    if (vr != VK_SUCCESS) { report(log, "vkCreateInstance failed."); goto cleanup; }
    if (debug_utils_enabled) {
        VkDebugUtilsMessengerCreateInfoEXT debug_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_info.pfnUserCallback = validation_callback; debug_info.pUserData = (void *)log;
        PFN_vkCreateDebugUtilsMessengerEXT create_messenger =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (create_messenger && create_messenger(instance, &debug_info, NULL, &debug_messenger) == VK_SUCCESS)
            report(log, "Vulkan debug messenger enabled.");
    }

    uint32_t physical_count = 0;
    if (vkEnumeratePhysicalDevices(instance, &physical_count, NULL) != VK_SUCCESS || !physical_count || physical_count > 16) {
        report(log, "No supported Vulkan physical device found."); goto cleanup;
    }
    VkPhysicalDevice physicals[16] = {VK_NULL_HANDLE};
    if (vkEnumeratePhysicalDevices(instance, &physical_count, physicals) != VK_SUCCESS) goto cleanup;
    VkPhysicalDevice physical = VK_NULL_HANDLE; uint32_t family = UINT32_MAX;
    for (uint32_t p = 0; p < physical_count && !physical; ++p) {
        VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(physicals[p], &props);
        if (VK_VERSION_MAJOR(props.apiVersion) < 1 ||
            (VK_VERSION_MAJOR(props.apiVersion) == 1 && VK_VERSION_MINOR(props.apiVersion) < 2)) continue;
        uint32_t count = 0; vkGetPhysicalDeviceQueueFamilyProperties(physicals[p], &count, NULL);
        VkQueueFamilyProperties queues[32] = {0}; if (!count || count > 32) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(physicals[p], &count, queues);
        for (uint32_t q = 0; q < count; ++q) if (queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            physical = physicals[p]; family = q; report(log, props.deviceName); break;
        }
    }
    if (!physical) { report(log, "No Vulkan 1.2 graphics device found."); goto cleanup; }

    float priority = 1.0f; VkDeviceQueueCreateInfo queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = family; queue_info.queueCount = 1; queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1; device_info.pQueueCreateInfos = &queue_info;
    if (vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
        report(log, "vkCreateDevice failed."); goto cleanup;
    }
    vkGetDeviceQueue(device, family, 0, &queue);

    VkFormatProperties format_properties; vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_R8G8B8A8_UNORM, &format_properties);
    VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((format_properties.optimalTilingFeatures & needed) != needed) {
        report(log, "RGBA8 optimal images lack transfer source/destination support."); goto cleanup;
    }
    VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = VK_IMAGE_TYPE_2D; image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width = width; image_info.extent.height = height; image_info.extent.depth = 1;
    image_info.mipLevels = 1; image_info.arrayLayers = 1; image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL; image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &image_info, NULL, &image) != VK_SUCCESS) { report(log, "vkCreateImage failed."); goto cleanup; }

    VkPhysicalDeviceMemoryProperties memory_properties; vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
    VkMemoryRequirements image_requirements; vkGetImageMemoryRequirements(device, image, &image_requirements);
    int image_type = find_memory_type(&memory_properties, image_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (image_type < 0) { report(log, "No suitable device-local image memory type."); goto cleanup; }
    VkMemoryAllocateInfo image_allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    image_allocation.allocationSize = image_requirements.size; image_allocation.memoryTypeIndex = (uint32_t)image_type;
    if (vkAllocateMemory(device, &image_allocation, NULL, &image_memory) != VK_SUCCESS ||
        vkBindImageMemory(device, image, image_memory, 0) != VK_SUCCESS) {
        report(log, "Image memory allocation/bind failed."); goto cleanup;
    }

    VkDeviceSize byte_count = (VkDeviceSize)width * height * 4;
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = byte_count; buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, NULL, &readback) != VK_SUCCESS) { report(log, "vkCreateBuffer failed."); goto cleanup; }
    VkMemoryRequirements buffer_requirements; vkGetBufferMemoryRequirements(device, readback, &buffer_requirements);
    int host_type = find_memory_type(&memory_properties, buffer_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (host_type < 0) { report(log, "No host-visible buffer memory type."); goto cleanup; }
    coherent = (memory_properties.memoryTypes[host_type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    VkMemoryAllocateInfo buffer_allocation = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    buffer_allocation.allocationSize = buffer_requirements.size; buffer_allocation.memoryTypeIndex = (uint32_t)host_type;
    if (vkAllocateMemory(device, &buffer_allocation, NULL, &readback_memory) != VK_SUCCESS ||
        vkBindBufferMemory(device, readback, readback_memory, 0) != VK_SUCCESS ||
        vkMapMemory(device, readback_memory, 0, byte_count, 0, &mapped) != VK_SUCCESS) {
        report(log, "Host readback memory allocation/map failed."); goto cleanup;
    }

    VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family; pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device, &pool_info, NULL, &pool) != VK_SUCCESS) { report(log, "vkCreateCommandPool failed."); goto cleanup; }
    VkCommandBufferAllocateInfo command_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command_info.commandPool = pool; command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command_info.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &command_info, &command) != VK_SUCCESS) { report(log, "vkAllocateCommandBuffers failed."); goto cleanup; }
    VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device, &fence_info, NULL, &fence) != VK_SUCCESS) { report(log, "vkCreateFence failed."); goto cleanup; }
    report(log, "Offscreen Vulkan 1.2 image and host readback buffer ready.");

    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkBufferImageCopy copy = {0}; copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1; copy.imageExtent.width = width; copy.imageExtent.height = height; copy.imageExtent.depth = 1;
    for (uint32_t frame = 0; frame < frames; ++frame) {
        if (frame && vkResetFences(device, 1, &fence) != VK_SUCCESS) goto cleanup;
        if (vkResetCommandBuffer(command, 0) != VK_SUCCESS) goto cleanup;
        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) goto cleanup;
        VkImageMemoryBarrier to_clear = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_clear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_clear.image = image; to_clear.subresourceRange = range; to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &to_clear);
        float shade = (float)(frame % 60) / 60.0f; VkClearColorValue color = {{shade, 0.12f, 0.55f, 1.0f}};
        vkCmdClearColorImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
        VkImageMemoryBarrier to_copy = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_copy.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; to_copy.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to_copy.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; to_copy.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_copy.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; to_copy.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_copy.image = image; to_copy.subresourceRange = range;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &to_copy);
        vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
        if (vkEndCommandBuffer(command) != VK_SUCCESS) goto cleanup;
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS ||
            vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            report(log, "Offscreen queue submission/fence wait failed."); goto cleanup;
        }
        if (!coherent) {
            VkMappedMemoryRange invalidate = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            invalidate.memory = readback_memory; invalidate.offset = 0; invalidate.size = VK_WHOLE_SIZE;
            if (vkInvalidateMappedMemoryRanges(device, 1, &invalidate) != VK_SUCCESS) { report(log, "Readback memory invalidate failed."); goto cleanup; }
        }
        if (on_frame) on_frame(mapped, (uint32_t)byte_count);
        if (frame == 0) report(log, "First offscreen frame rendered and copied to host memory.");
        if ((frame + 1) % 60 == 0) report(log, "Rendered 60 offscreen frames.");
    }
    report(log, "PASS: all requested offscreen frames rendered."); result = 0;

cleanup:
    if (device) vkDeviceWaitIdle(device);
    if (mapped && device && readback_memory) vkUnmapMemory(device, readback_memory);
    if (device && fence) vkDestroyFence(device, fence, NULL);
    if (device && pool) vkDestroyCommandPool(device, pool, NULL);
    if (device && readback) vkDestroyBuffer(device, readback, NULL);
    if (device && readback_memory) vkFreeMemory(device, readback_memory, NULL);
    if (device && image) vkDestroyImage(device, image, NULL);
    if (device && image_memory) vkFreeMemory(device, image_memory, NULL);
    if (device) vkDestroyDevice(device, NULL);
    if (instance && debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_messenger) destroy_messenger(instance, debug_messenger, NULL);
    }
    if (instance) vkDestroyInstance(instance, NULL);
    return result;
}
