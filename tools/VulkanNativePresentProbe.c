#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdint.h>

typedef void (__cdecl *ProbeLog)(const char *message);
static void report(ProbeLog log, const char *message) { if (log) log(message); }
static int strings_equal(const char *left, const char *right) {
    while (*left && *right && *left == *right) { ++left; ++right; }
    return *left == *right;
}
static int string_contains(const char *text, const char *fragment) {
    if (!fragment || !*fragment) return 1;
    for (const char *start = text; *start; ++start) {
        const char *a = start, *b = fragment;
        while (*a && *b && *a == *b) { ++a; ++b; }
        if (!*b) return 1;
    }
    return 0;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    (void)type;
    ProbeLog log = (ProbeLog)user;
    (void)severity;
    if (log && data && data->pMessage) log(data->pMessage);
    return VK_FALSE;
}
#define CHECK(call) do { VkResult result_ = (call); if (result_ != VK_SUCCESS) { \
    if (log) log(#call " failed"); goto cleanup; } } while (0)

BOOL WINAPI DllMainCRTStartup(HINSTANCE module, DWORD reason, LPVOID reserved) { return TRUE; }

__declspec(dllexport) int __cdecl RunNativeVulkanProbe(HINSTANCE hinstance, HWND hwnd, uint32_t frame_limit,
    const char *preferred_device, const char *preferred_present_mode, VkBool32 acquire_only,
    VkBool32 synchronous_present, ProbeLog log) {
    VkInstance instance = VK_NULL_HANDLE; VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE; VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkBool32 debug_utils_enabled = VK_FALSE;
    VkCommandPool pool = VK_NULL_HANDLE; VkCommandBuffer commands[8] = {VK_NULL_HANDLE};
    VkQueue queue = VK_NULL_HANDLE; VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkSemaphore acquire_for_image[8] = {VK_NULL_HANDLE};
    VkSemaphore recycled_acquire[8] = {VK_NULL_HANDLE}; uint32_t recycled_count = 0;
    VkSemaphore current_acquire = VK_NULL_HANDLE;
    VkSemaphore ready[8] = {VK_NULL_HANDLE}; VkImage images[8] = {VK_NULL_HANDLE};
    VkBool32 initialized[8] = {VK_FALSE}; uint32_t image_count = 0, family = UINT32_MAX;
    VkBool32 wsi_stalled = VK_FALSE;
    int exit_code = 1;
    report(log, "Native probe entry point reached.");

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "GOW3 native Vulkan WSI probe"; app.apiVersion = VK_API_VERSION_1_2;
    uint32_t instance_extension_count = 0;
    VkExtensionProperties available_instance_extensions[128];
    if (vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL) == VK_SUCCESS &&
        instance_extension_count <= 128 &&
        vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, available_instance_extensions) == VK_SUCCESS) {
        for (uint32_t i = 0; i < instance_extension_count; ++i)
            if (strings_equal(available_instance_extensions[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
                debug_utils_enabled = VK_TRUE;
    }
    const char *instance_extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    VkInstanceCreateInfo instance_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &app; instance_info.enabledExtensionCount = debug_utils_enabled ? 3 : 2;
    instance_info.ppEnabledExtensionNames = instance_extensions;
    CHECK(vkCreateInstance(&instance_info, NULL, &instance));
    report(log, "Vulkan instance created.");
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = validation_callback; debug_info.pUserData = (void *)log;
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_messenger = debug_utils_enabled ?
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT") : NULL;
    if (create_debug_messenger) {
        VkResult debug_result = create_debug_messenger(instance, &debug_info, NULL, &debug_messenger);
        if (debug_result == VK_SUCCESS) report(log, "Vulkan debug messenger enabled.");
        else report(log, "Vulkan debug messenger unavailable; validation output may be incomplete.");
    } else report(log, "VK_EXT_debug_utils unavailable; validation output may be incomplete.");

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
        if (!string_contains(properties.deviceName, preferred_device)) continue;
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
    uint32_t mode_count = 0; CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, NULL));
    VkPresentModeKHR modes[16]; if (!mode_count || mode_count > 16) { report(log, "Unsupported present-mode count"); goto cleanup; }
    CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count, modes));
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    VkPresentModeKHR requested_mode = strings_equal(preferred_present_mode, "immediate") ? VK_PRESENT_MODE_IMMEDIATE_KHR :
        (strings_equal(preferred_present_mode, "mailbox") ? VK_PRESENT_MODE_MAILBOX_KHR :
        (strings_equal(preferred_present_mode, "relaxed") ? VK_PRESENT_MODE_FIFO_RELAXED_KHR : VK_PRESENT_MODE_FIFO_KHR));
    if (!strings_equal(preferred_present_mode, "fifo")) {
        for (uint32_t i = 0; i < mode_count; ++i) if (modes[i] == requested_mode) present_mode = requested_mode;
        if (present_mode != requested_mode) report(log, "Requested present mode unavailable; using FIFO.");
    }
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) { extent.width = 960; extent.height = 540; }
    uint32_t requested_images = caps.minImageCount + 2;
    if (caps.maxImageCount && requested_images > caps.maxImageCount) requested_images = caps.maxImageCount;
    VkSwapchainCreateInfoKHR swap_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swap_info.surface = surface; swap_info.minImageCount = requested_images;
    swap_info.imageFormat = formats[0].format; swap_info.imageColorSpace = formats[0].colorSpace;
    swap_info.imageExtent = extent; swap_info.imageArrayLayers = 1; swap_info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swap_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; swap_info.preTransform = caps.currentTransform;
    swap_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; swap_info.presentMode = present_mode;
    report(log, present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "Present mode: immediate" :
        (present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "Present mode: mailbox" :
        (present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR ? "Present mode: fifo-relaxed" : "Present mode: FIFO")));
    swap_info.clipped = VK_TRUE;
    CHECK(vkCreateSwapchainKHR(device, &swap_info, NULL, &swapchain));
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL));
    if (!image_count || image_count > 8) { report(log, "Unsupported swapchain image count"); goto cleanup; }
    report(log, image_count == 2 ? "Swapchain image count: 2" : (image_count == 3 ? "Swapchain image count: 3" :
        (image_count == 4 ? "Swapchain image count: 4" : "Swapchain image count: other")));
    CHECK(vkGetSwapchainImagesKHR(device, swapchain, &image_count, images));

    VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family; pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CHECK(vkCreateCommandPool(device, &pool_info, NULL, &pool));
    VkCommandBufferAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.commandPool = pool; alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc_info.commandBufferCount = image_count;
    CHECK(vkAllocateCommandBuffers(device, &alloc_info, commands));
    VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (uint32_t i = 0; i < image_count; ++i) CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &ready[i]));
    report(log, "Native Vulkan 1.2 swapchain created.");

    for (uint32_t frame = 0; frame < frame_limit; ++frame) {
        if (frame < 5) report(log, "Beginning frame.");
        uint32_t image_index = 0;
        if (recycled_count) current_acquire = recycled_acquire[--recycled_count];
        else CHECK(vkCreateSemaphore(device, &semaphore_info, NULL, &current_acquire));
        // Finite waits preserve forward progress if the window/surface cannot return an image.
        VkResult ar = vkAcquireNextImageKHR(device, swapchain, 2000000000ULL, current_acquire, VK_NULL_HANDLE, &image_index);
        if (ar != VK_SUCCESS && ar != VK_SUBOPTIMAL_KHR) {
            if (ar == VK_TIMEOUT) {
                report(log, "vkAcquireNextImageKHR timed out after 2 seconds; entering WSI-stall cleanup.");
                wsi_stalled = VK_TRUE; exit_code = 2; goto cleanup;
            }
            report(log, ar == VK_ERROR_OUT_OF_DATE_KHR ? "vkAcquireNextImageKHR returned OUT_OF_DATE" :
                (ar == VK_ERROR_SURFACE_LOST_KHR ? "vkAcquireNextImageKHR returned SURFACE_LOST" :
                "vkAcquireNextImageKHR returned an unexpected error"));
            goto cleanup;
        }
        if (acquire_only) {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkSubmitInfo consume = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            consume.waitSemaphoreCount = 1; consume.pWaitSemaphores = &current_acquire;
            consume.pWaitDstStageMask = &wait_stage;
            CHECK(vkQueueSubmit(queue, 1, &consume, VK_NULL_HANDLE));
            CHECK(vkQueueWaitIdle(queue));
            vkDestroySemaphore(device, current_acquire, NULL); current_acquire = VK_NULL_HANDLE;
            report(log, "Image acquired and acquire semaphore consumed without present.");
            continue;
        }
        if (acquire_for_image[image_index]) recycled_acquire[recycled_count++] = acquire_for_image[image_index];
        acquire_for_image[image_index] = current_acquire; current_acquire = VK_NULL_HANDLE;
        if (frame < 5) report(log, "Image acquired.");
        if (frame == 0) report(log, "First image acquired.");
        VkCommandBuffer command = commands[image_index];
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
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquire_for_image[image_index]; submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
        if (!synchronous_present) {
            submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &ready[image_index];
        }
        CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        if (frame < 5) report(log, "Queue submitted.");
        if (frame == 0) report(log, "First command buffer submitted.");
        if (synchronous_present) CHECK(vkQueueWaitIdle(queue));
        VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = synchronous_present ? 0 : 1;
        present.pWaitSemaphores = synchronous_present ? NULL : &ready[image_index];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain; present.pImageIndices = &image_index;
        VkResult pr = vkQueuePresentKHR(queue, &present);
        if (pr != VK_SUCCESS && pr != VK_SUBOPTIMAL_KHR) { report(log, "vkQueuePresentKHR failed"); goto cleanup; }
        if (frame < 5) report(log, "Queue presented.");
        if (frame == 0) report(log, "First image presented.");
        initialized[image_index] = VK_TRUE;
        if (frame < 5) report(log, synchronous_present ? "Synchronous render completed before present." : "Frame submitted without global queue idle.");
        if ((frame + 1) % 60 == 0) report(log, "Presented 60-frame milestone.");
    }
    CHECK(vkDeviceWaitIdle(device)); report(log, "PASS: all requested native frames presented."); exit_code = 0;

cleanup:
    if (device) vkDeviceWaitIdle(device);
    report(log, "Cleanup: destroying acquire semaphores.");
    if (device && current_acquire) vkDestroySemaphore(device, current_acquire, NULL);
    if (device) for (uint32_t i = 0; i < image_count; ++i) if (acquire_for_image[i]) vkDestroySemaphore(device, acquire_for_image[i], NULL);
    if (device) for (uint32_t i = 0; i < recycled_count; ++i) if (recycled_acquire[i]) vkDestroySemaphore(device, recycled_acquire[i], NULL);
    report(log, "Cleanup: acquire semaphores destroyed.");
    if (!wsi_stalled && device) {
        for (uint32_t i = 0; i < image_count; ++i) if (ready[i]) vkDestroySemaphore(device, ready[i], NULL);
    } else if (wsi_stalled) report(log, "Cleanup: defer present semaphore reclamation to device teardown.");
    if (!wsi_stalled) {
        report(log, "Cleanup: destroying command pool.");
        if (device && pool) vkDestroyCommandPool(device, pool, NULL);
        report(log, "Cleanup: command pool destroyed.");
    } else report(log, "Cleanup: defer command-pool reclamation to device teardown.");
    if (!wsi_stalled) {
        report(log, "Cleanup: destroying swapchain.");
        if (device && swapchain) vkDestroySwapchainKHR(device, swapchain, NULL);
    } else report(log, "Cleanup: skip explicit swapchain destruction after WSI timeout.");
    report(log, "Cleanup: destroying device and instance.");
    if (device) vkDestroyDevice(device, NULL);
    if (instance && surface) vkDestroySurfaceKHR(instance, surface, NULL);
    if (instance && debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_messenger =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug_messenger) destroy_debug_messenger(instance, debug_messenger, NULL);
    }
    if (instance) vkDestroyInstance(instance, NULL);
    return exit_code;
}
