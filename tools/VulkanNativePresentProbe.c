#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef void (__cdecl *ProbeLog)(const char *message);
static void report(ProbeLog log, const char *message) { if (log) log(message); }
#define CHECK(call) do { VkResult result_ = (call); if (result_ != VK_SUCCESS) { \
    if (log) log(#call " failed"); goto cleanup; } } while (0)

BOOL WINAPI DllMainCRTStartup(HINSTANCE module, DWORD reason, LPVOID reserved) { return TRUE; }

__declspec(dllexport) int __cdecl RunNativeVulkanProbe(HINSTANCE hinstance, HWND hwnd, uint32_t frame_limit, ProbeLog log) {
    VkInstance instance = VK_NULL_HANDLE; VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE; VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE; VkCommandBuffer command = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE; VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkSemaphore acquire[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkSemaphore ready[8] = {VK_NULL_HANDLE}; VkImage images[8] = {VK_NULL_HANDLE};
    VkBool32 initialized[8] = {VK_FALSE}; uint32_t image_count = 0, family = UINT32_MAX;
    int exit_code = 1;
    report(log, "Native probe entry point reached.");

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "GOW3 native Vulkan WSI probe"; app.apiVersion = VK_API_VERSION_1_2;
    const char *instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &app; instance_info.enabledExtensionCount = 2;
    instance_info.ppEnabledExtensionNames = instance_extensions;
    CHECK(vkCreateInstance(&instance_info, NULL, &instance));
    report(log, "Vulkan instance created.");

    VkWin32SurfaceCreateInfoKHR surface_info = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    surface_info.hinstance = hinstance; surface_info.hwnd = hwnd;
    CHECK(vkCreateWin32SurfaceKHR(instance, &surface_info, NULL, &surface));
    report(log, "Win32 surface created.");

    uint32_t physical_count = 0; CHECK(vkEnumeratePhysicalDevices(instance, &physical_count, NULL));
    VkPhysicalDevice physicals[16] = {VK_NULL_HANDLE};
    if (!physical_count || physical_count > 16) { report(log, "Unsupported physical-device count"); goto cleanup; }
    CHECK(vkEnumeratePhysicalDevices(instance, &physical_count, physicals));
    VkPhysicalDeviceProperties selected_properties = {0};
    for (uint32_t p = 0; p < physical_count && !physical; ++p) {
        VkPhysicalDeviceProperties properties; vkGetPhysicalDeviceProperties(physicals[p], &properties);
        if (VK_VERSION_MAJOR(properties.apiVersion) < 1 ||
            (VK_VERSION_MAJOR(properties.apiVersion) == 1 && VK_VERSION_MINOR(properties.apiVersion) < 2)) continue;
        uint32_t queue_count = 0; vkGetPhysicalDeviceQueueFamilyProperties(physicals[p], &queue_count, NULL);
        VkQueueFamilyProperties queues[32] = {0}; if (!queue_count || queue_count > 32) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(physicals[p], &queue_count, queues);
        for (uint32_t q = 0; q < queue_count; ++q) {
            VkBool32 can_present = VK_FALSE;
            if ((queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                vkGetPhysicalDeviceSurfaceSupportKHR(physicals[p], q, surface, &can_present) == VK_SUCCESS && can_present) {
                physical = physicals[p]; family = q; selected_properties = properties; break;
            }
        }
    }
    if (!physical) { report(log, "No Vulkan 1.2 graphics/present device"); goto cleanup; }
    report(log, selected_properties.deviceName);

    float priority = 1.0f; VkDeviceQueueCreateInfo queue_info = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = family; queue_info.queueCount = 1; queue_info.pQueuePriorities = &priority;
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo device_info = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1; device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1; device_info.ppEnabledExtensionNames = device_extensions;
    CHECK(vkCreateDevice(physical, &device_info, NULL, &device)); vkGetDeviceQueue(device, family, 0, &queue);

    VkSurfaceCapabilitiesKHR caps; CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps));
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) { report(log, "Swapchain lacks transfer-destination support"); goto cleanup; }
    uint32_t format_count = 0; CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, NULL));
    VkSurfaceFormatKHR formats[32]; if (!format_count || format_count > 32) { report(log, "Unsupported surface format count"); goto cleanup; }
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, formats));
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) { extent.width = 960; extent.height = 540; }
    uint32_t requested_images = caps.minImageCount + 1;
    if (caps.maxImageCount && requested_images > caps.maxImageCount) requested_images = caps.maxImageCount;
    VkSwapchainCreateInfoKHR swap_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swap_info.surface = surface; swap_info.minImageCount = requested_images;
    swap_info.imageFormat = formats[0].format; swap_info.imageColorSpace = formats[0].colorSpace;
    swap_info.imageExtent = extent; swap_info.imageArrayLayers = 1; swap_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swap_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; swap_info.preTransform = caps.currentTransform;
    swap_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; swap_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swap_info.clipped = VK_TRUE;
    CHECK(vkCreateSwapchainKHR(device, &swap_info, NULL, &swapchain));
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL));
    if (!image_count || image_count > 8) { report(log, "Unsupported swapchain image count"); goto cleanup; }
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images));

    VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family; pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK(vkCreateCommandPool(device, &pool_info, NULL, &pool));
    VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.commandPool = pool; alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc_info.commandBufferCount = 1;
    CHECK(vkAllocateCommandBuffers(device, &alloc_info, &command));
    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (int i = 0; i < 2; ++i) CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &acquire[i]));
    for (uint32_t i = 0; i < image_count; ++i) CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &ready[i]));
    report(log, "Native Vulkan 1.2 swapchain created.");

    for (uint32_t frame = 0; frame < frame_limit; ++frame) {
        if (frame < 5) report(log, "Beginning frame.");
        uint32_t image_index = 0;
        VkResult ar = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquire[frame % 2], VK_NULL_HANDLE, &image_index);
        if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) { report(log, "vkAcquireNextImageKHR failed"); goto cleanup; }
        if (frame < 5) report(log, "Image acquired.");
        if (frame == 0) report(log, "First image acquired.");
        CHECK(vkResetCommandBuffer(command, 0));
        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; CHECK(vkBeginCommandBuffer(command, &begin));
        VkImageMemoryBarrier to_clear = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_clear.oldLayout = initialized[image_index] ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_UNDEFINED;
        to_clear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_clear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; to_clear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_clear.image = images[image_index]; to_clear.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_clear.subresourceRange.levelCount = 1; to_clear.subresourceRange.layerCount = 1;
        to_clear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, NULL, 0, NULL, 1, &to_clear);
        VkClearColorValue color = {{0.06f, 0.10f, 0.18f, 1.0f}};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(command, images[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
        VkImageMemoryBarrier to_present = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; to_present.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_present.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; to_present.image = images[image_index]; to_present.subresourceRange = range;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, NULL, 0, NULL, 1, &to_present); CHECK(vkEndCommandBuffer(command));
        if (frame < 5) report(log, "Command buffer recorded.");
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquire[frame % 2]; submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &ready[image_index];
        CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        if (frame < 5) report(log, "Queue submitted.");
        if (frame == 0) report(log, "First command buffer submitted.");
        VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &ready[image_index]; present.swapchainCount = 1;
        present.pSwapchains = &swapchain; present.pImageIndices = &image_index;
        VkResult pr = vkQueuePresentKHR(queue, &present);
        if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) { report(log, "vkQueuePresentKHR failed"); goto cleanup; }
        if (frame < 5) report(log, "Queue presented.");
        if (frame == 0) report(log, "First image presented.");
        initialized[image_index] = VK_TRUE;
        CHECK(vkQueueWaitIdle(queue));
        if (frame < 5) report(log, "Frame queue idle complete.");
        if ((frame + 1) % 60 == 0) report(log, "Presented 60-frame milestone.");
    }
    CHECK(vkDeviceWaitIdle(device)); report(log, "PASS: all requested native frames presented."); exit_code = 0;

cleanup:
    if (device) vkDeviceWaitIdle(device);
    report(log, "Cleanup: destroying semaphores and command pool.");
    if (device) for (uint32_t i = 0; i < image_count; ++i) if (ready[i]) vkDestroySemaphore(device, ready[i], NULL);
    if (device) for (int i = 0; i < 2; ++i) if (acquire[i]) vkDestroySemaphore(device, acquire[i], NULL);
    if (device && pool) vkDestroyCommandPool(device, pool, NULL);
    report(log, "Cleanup: destroying swapchain.");
    if (device && swapchain) vkDestroySwapchainKHR(device, swapchain, NULL);
    report(log, "Cleanup: destroying device and instance.");
    if (device) vkDestroyDevice(device, NULL);
    if (instance && surface) vkDestroySurfaceKHR(instance, surface, NULL);
    if (instance) vkDestroyInstance(instance, NULL);
    return exit_code;
}
