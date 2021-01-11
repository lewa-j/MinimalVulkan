#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <Windows.h>
#include <iostream>
#include <vector>

class Vulkan {
public:
    int Init(HINSTANCE inst, HWND window);
    void Destroy();
    void Draw();
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    VkRenderPass renderPass;
    struct Frame_t {
        VkImageView imageView;
        VkFramebuffer framebuffer;
        VkCommandBuffer commandBuffer;
    };
    std::vector<Frame_t> frames;
    VkCommandPool commandPool;
    VkSemaphore semaphore;
    VkSemaphore signalSemaphore;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Vulkan* pVulkan = nullptr;
    if (uMsg == WM_NCCREATE){
        pVulkan = (Vulkan*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pVulkan);
    }
    else {
        pVulkan = (Vulkan*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (pVulkan) {
        switch (uMsg) {
        case WM_CREATE:
            pVulkan->Init(GetModuleHandle(nullptr), hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            pVulkan->Draw();
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main()
{
    WNDCLASS wndClass{ CS_HREDRAW | CS_VREDRAW | CS_OWNDC,WindowProc,0,0,GetModuleHandle(nullptr),0,0,0,0,L"VulkanWin"};
    RegisterClass(&wndClass);
    Vulkan vulkan;
    HWND window = CreateWindowEx(WS_EX_APPWINDOW,L"VulkanWin",L"Vulkan",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT, 800, 480, nullptr, nullptr, GetModuleHandle(nullptr), &vulkan);
    ShowWindow(window, SW_SHOW);
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    vulkan.Destroy();
    return 0;
}

int Vulkan::Init(HINSTANCE hinst, HWND window)
{
    uint32_t extensionCount{2};
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
    std::vector<const char*> layers;
#if defined(_DEBUG)
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    VkInstanceCreateInfo instInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,0,0,0,(uint32_t)layers.size(),layers.data(),extensionCount,extensions };
    vkCreateInstance(&instInfo, nullptr, &instance);

    uint32_t deviceCount = 2;
    VkPhysicalDevice physicalDevices[2];
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
    physicalDevice = physicalDevices[0];
    uint32_t queueFamilyIndex = 0;
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,0,0,queueFamilyIndex,1,&queuePriority };
    const char* deviceExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo deviceInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,0,0,1,&queueInfo,0,0,1,&deviceExtension,0 };
    vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkWin32SurfaceCreateInfoKHR surfaceInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,0,0,hinst,window};
    vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
    VkBool32 supported; vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &supported);
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
    VkSwapchainCreateInfoKHR swapchainInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,0,0,surface,surfaceCapabilities.minImageCount,VK_FORMAT_B8G8R8A8_UNORM,
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,surfaceCapabilities.currentExtent,1,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_SHARING_MODE_EXCLUSIVE,0,0,
        surfaceCapabilities.currentTransform,VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_PRESENT_MODE_IMMEDIATE_KHR,VK_TRUE,0 };
    vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    VkAttachmentDescription attachment{ 0,VK_FORMAT_B8G8R8A8_UNORM,VK_SAMPLE_COUNT_1_BIT,VK_ATTACHMENT_LOAD_OP_CLEAR,VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
    VkAttachmentReference colorAttachmentRef{ 0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass{ 0,VK_PIPELINE_BIND_POINT_GRAPHICS,0,0,1,&colorAttachmentRef,0,0,0,0 };
    VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,0,0,1,&attachment,1,&subpass,0,0 };
    vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);

    VkCommandPoolCreateInfo commandPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,0,VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,queueFamilyIndex };
    vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool);
    VkCommandBufferAllocateInfo commandBufferInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,0,commandPool,VK_COMMAND_BUFFER_LEVEL_PRIMARY,1 };
    frames.resize(swapchainImageCount);
    for (int i = 0; i < swapchainImageCount; i++) {
        VkImageViewCreateInfo imageViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,0,0,swapchainImages[i],VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_B8G8R8A8_UNORM,{},{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,VK_REMAINING_ARRAY_LAYERS} };
        vkCreateImageView(device, &imageViewInfo, nullptr, &frames[i].imageView);
        VkFramebufferCreateInfo framebufferInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,0,0,renderPass,1,&frames[i].imageView,surfaceCapabilities.currentExtent.width,surfaceCapabilities.currentExtent.height,1 };
        vkCreateFramebuffer(device, &framebufferInfo, nullptr, &frames[i].framebuffer);
        vkAllocateCommandBuffers(device, &commandBufferInfo, &frames[i].commandBuffer);
        VkCommandBufferBeginInfo cbBeginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,0,VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,0 };
        vkBeginCommandBuffer(frames[i].commandBuffer, &cbBeginInfo);
        VkClearValue clearValue{ {0.2f,0.3f,0.7f,1.0f} };
        VkRenderPassBeginInfo rpBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, 0, renderPass, frames[i].framebuffer,{{0,0},surfaceCapabilities.currentExtent},1,&clearValue };
        vkCmdBeginRenderPass(frames[i].commandBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(frames[i].commandBuffer);
        vkEndCommandBuffer(frames[i].commandBuffer);
    }
    VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,0,0 };
    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &signalSemaphore);
    return 0;
}

void Vulkan::Destroy()
{
    vkDestroySemaphore(device, semaphore, nullptr);
    vkDestroySemaphore(device, signalSemaphore, nullptr);
    for (int i = 0; i < frames.size(); i++) {
        vkFreeCommandBuffers(device, commandPool, 1, &frames[i].commandBuffer);
        vkDestroyFramebuffer(device, frames[i].framebuffer, nullptr);
        vkDestroyImageView(device, frames[i].imageView, nullptr);
    }
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void Vulkan::Draw()
{
    uint32_t imgIndex{ 0 };
    vkAcquireNextImageKHR(device, swapchain, 0, semaphore, VK_NULL_HANDLE, &imgIndex);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;// VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO,0,1,&semaphore,&waitStage,1,&frames[imgIndex].commandBuffer,1,&signalSemaphore };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,0,1,&signalSemaphore,1,&swapchain,&imgIndex,0 };
    vkQueuePresentKHR(queue, &presentInfo);
    vkDeviceWaitIdle(device);
}
