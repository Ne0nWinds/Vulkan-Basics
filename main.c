#define _AMD64_
#include <windef.h>
#include <WinUser.h>
#include <synchapi.h>
#include <debugapi.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan.h>
#include <vulkan_win32.h>

#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

typedef int8_t s8;
typedef int32_t s32;

typedef float f32;
typedef double f64;

enum {
	false,
	true
} typedef bool;

static bool Running = true;

static VkInstance Instance;
static VkDevice Device;
static VkPhysicalDevice PhysicalDevice;
static u32 QueueFamilyIndex;
static VkQueue Queue;
static VkSurfaceKHR Surface;
static VkSwapchainKHR Swapchain;
static VkExtent2D WindowSize;
static VkSemaphore ImageAvailableSemaphore;
static VkSemaphore RenderingFinishedSemaphore;
static VkCommandPool CommandPool;
static VkCommandBuffer *CommandBuffers;
static VkImage *SwapchainImages;
static u32 SwapchainImageCount;

static u8 _TempMem[1024 * 1024 * 1];
static void *TempMem = _TempMem;

static void TempMemReset() {
	TempMem = _TempMem;
}

static u8 _PermanentMem[1024 * 1024 * 1];
static void *PermanentMem = _PermanentMem;

static void *AllocPermanentMem (u32 size)
__attribute__((assume_aligned (4))) __attribute__((alloc_size(1)));

static void *AllocPermanentMem (u32 size)
{
	void *Value = PermanentMem;
	size = (size + 3) & (-4);
	PermanentMem += size;
	return Value;
}

LRESULT CALLBACK WndProc(HWND HWND, UINT Message, WPARAM Wparam, LPARAM Lparam) {
	switch (Message) {
		case WM_DESTROY:
		case WM_CLOSE:
			PostQuitMessage(0);
			Running = false;
			return 0;
		default:
			return DefWindowProcA(HWND, Message, Wparam, Lparam);
	}
}

int WINAPI wWinMain(HINSTANCE HInstance, HINSTANCE HPrevInstance, PWSTR CMDLine, int CMDShow) {

	WNDCLASSEXA WindowClass = {0};
	WindowClass.cbSize = sizeof(WNDCLASSEXA);
	WindowClass.lpszClassName = "Vulkan Demo";
	WindowClass.lpfnWndProc = WndProc;

	RegisterClassExA(&WindowClass);
	HWND WindowHandle = CreateWindowExA(
		0, WindowClass.lpszClassName, "", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, HInstance, NULL
	);

	{
		VkApplicationInfo AppInfo = {0};
		AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		AppInfo.pApplicationName = "Vulkan Demo";
		AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.pEngineName = "Parker Engine";
		AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		AppInfo.apiVersion = VK_API_VERSION_1_2;

		char *InstanceExtensions[] = { "VK_KHR_win32_surface", "VK_KHR_surface" };

		VkInstanceCreateInfo InstanceCreateInfo = {0};
		InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		InstanceCreateInfo.pApplicationInfo = &AppInfo;
		InstanceCreateInfo.ppEnabledExtensionNames = InstanceExtensions;
		InstanceCreateInfo.enabledExtensionCount = 2;

		vkCreateInstance(&InstanceCreateInfo, NULL, &Instance);
	}

	{
		VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {0};
		SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		SurfaceCreateInfo.hinstance = HInstance;
		SurfaceCreateInfo.hwnd = WindowHandle;

		vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, NULL, &Surface);
	}

	{
		u32 DeviceCount = 0;
		VkPhysicalDevice *PhysicalDevices = TempMem;
		vkEnumeratePhysicalDevices(Instance, &DeviceCount, NULL);
		vkEnumeratePhysicalDevices(Instance, &DeviceCount, PhysicalDevices);
		VkPhysicalDeviceProperties DeviceProps = {0};
		PhysicalDevice = PhysicalDevices[0];
		for (u32 i = 1; i < DeviceCount; ++i) {
			vkGetPhysicalDeviceProperties(PhysicalDevices[i], &DeviceProps);
			if (DeviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				PhysicalDevice = PhysicalDevices[i];
				break;
			}
		}

		u32 QueueFamilyCount = 0;
		VkQueueFamilyProperties *FamilyProps = TempMem;
		vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, NULL);
		vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, FamilyProps);
		for (u32 i = 0; i < QueueFamilyCount; ++i) {
			VkBool32 Supported = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &Supported);
			if (FamilyProps[i].queueCount && (FamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && Supported) {
				QueueFamilyIndex = i;
			}
		}

		f32 QueuePriorites[] = { 1.0f };

		VkDeviceQueueCreateInfo QueueCreateInfo = {0};
		QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		QueueCreateInfo.queueFamilyIndex = QueueFamilyIndex;
		QueueCreateInfo.pQueuePriorities = QueuePriorites;
		QueueCreateInfo.queueCount = 1;

		char *DeviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		VkDeviceCreateInfo DeviceCreateInfo = {0};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.flags = 1;
		DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;
		DeviceCreateInfo.queueCreateInfoCount = 1;
		DeviceCreateInfo.ppEnabledExtensionNames = (const char **)DeviceExtensions;
		DeviceCreateInfo.enabledExtensionCount = 1;

		vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, NULL, &Device);
		vkGetDeviceQueue(Device, QueueFamilyIndex, 0, &Queue);
	}

	{
		VkSemaphoreCreateInfo SemaphoreCreateInfo = {0};
		SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &ImageAvailableSemaphore);
		vkCreateSemaphore(Device, &SemaphoreCreateInfo, NULL, &RenderingFinishedSemaphore);
	}

	{
		u32 PresentModeCount = 0;
		VkPresentModeKHR *PresentModes = TempMem;
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, NULL);
		vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, PresentModes);

		VkPresentModeKHR SelectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (u32 i = 0; i < PresentModeCount; ++i) {
			if (PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				SelectedPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}

		VkSurfaceCapabilitiesKHR SurfaceCaps = {0};
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCaps);

		u32 ImageCount = SurfaceCaps.minImageCount + (SelectedPresentMode == VK_PRESENT_MODE_MAILBOX_KHR);
		if (ImageCount > SurfaceCaps.maxImageCount) ImageCount = SurfaceCaps.maxImageCount;

		WindowSize = SurfaceCaps.currentExtent;

		u32 FormatsCount = 0;
		VkSurfaceFormatKHR *SurfaceFormats = TempMem;
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, NULL);
		vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatsCount, SurfaceFormats);

		VkSurfaceFormatKHR SurfaceFormat = {0};
		for (u32 i = 0; i < FormatsCount; ++i) {
			if (SurfaceFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM || SurfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
				SurfaceFormat = SurfaceFormats[i];
				break;
			}
		}

		VkSwapchainCreateInfoKHR SwapchainCreateInfo = {0};
		SwapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		SwapchainCreateInfo.surface = Surface;
		SwapchainCreateInfo.minImageCount = ImageCount;
		SwapchainCreateInfo.imageFormat = SurfaceFormat.format;
		SwapchainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
		SwapchainCreateInfo.imageArrayLayers = 1;
		SwapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		SwapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		SwapchainCreateInfo.presentMode = SelectedPresentMode;
		SwapchainCreateInfo.clipped = VK_TRUE;
		SwapchainCreateInfo.imageExtent = WindowSize;
		SwapchainCreateInfo.imageUsage = SurfaceCaps.supportedUsageFlags;

		vkCreateSwapchainKHR(Device, &SwapchainCreateInfo, NULL, &Swapchain);
	}

	{
		vkGetSwapchainImagesKHR(Device, Swapchain, &SwapchainImageCount, NULL);
		SwapchainImages = AllocPermanentMem(sizeof(VkImage) * SwapchainImageCount);
		vkGetSwapchainImagesKHR(Device, Swapchain, &SwapchainImageCount, SwapchainImages);

		VkCommandPoolCreateInfo CmdPoolCreateInfo = {0};
		CmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		CmdPoolCreateInfo.queueFamilyIndex = QueueFamilyIndex;
		vkCreateCommandPool(Device, &CmdPoolCreateInfo, NULL, &CommandPool);

		CommandBuffers = AllocPermanentMem(sizeof(VkCommandBuffer) * SwapchainImageCount);
		VkCommandBufferAllocateInfo CmdBufferAllocInfo = {0};
		CmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		CmdBufferAllocInfo.commandPool = CommandPool;
		CmdBufferAllocInfo.commandBufferCount = SwapchainImageCount;
		CmdBufferAllocInfo.level = SwapchainImageCount;

		vkAllocateCommandBuffers(Device, &CmdBufferAllocInfo, CommandBuffers);

		VkCommandBufferBeginInfo CmdBufferBeginInfo = {0};
		CmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		CmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		VkClearColorValue ClearColor = { 1.0f, 0.8f, 0.4f, 0.0f };

		VkImageSubresourceRange ImageSubresourceRange = {0};
		ImageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ImageSubresourceRange.baseMipLevel = 0;
		ImageSubresourceRange.levelCount = 1;
		ImageSubresourceRange.baseArrayLayer = 0;
		ImageSubresourceRange.layerCount = 1;

		VkImageMemoryBarrier BarrierFromPresentToClear = {0};
		BarrierFromPresentToClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		BarrierFromPresentToClear.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		BarrierFromPresentToClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;;
		BarrierFromPresentToClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		BarrierFromPresentToClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		BarrierFromPresentToClear.srcQueueFamilyIndex = QueueFamilyIndex;
		BarrierFromPresentToClear.dstQueueFamilyIndex = QueueFamilyIndex;
		BarrierFromPresentToClear.subresourceRange = ImageSubresourceRange;

		VkImageMemoryBarrier BarrierFromClearToPresent = {0};
		BarrierFromClearToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		BarrierFromClearToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		BarrierFromClearToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		BarrierFromClearToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		BarrierFromClearToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		BarrierFromClearToPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;;
		BarrierFromClearToPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;;
		BarrierFromClearToPresent.subresourceRange = ImageSubresourceRange;

		for (u32 i = 0; i < SwapchainImageCount; ++i) {
			BarrierFromPresentToClear.image = SwapchainImages[i];
			BarrierFromClearToPresent.image = SwapchainImages[i];
			vkBeginCommandBuffer(CommandBuffers[i], &CmdBufferBeginInfo);
			vkCmdPipelineBarrier(CommandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &BarrierFromPresentToClear);
			vkCmdClearColorImage(CommandBuffers[i], SwapchainImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColor, 1, &ImageSubresourceRange);
			vkCmdPipelineBarrier(CommandBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &BarrierFromClearToPresent);
			vkEndCommandBuffer(CommandBuffers[i]);
		}

	}

	MSG msg;
	while (Running) {
		while (PeekMessageA(&msg, WindowHandle, 0, 0, PM_REMOVE))
			DispatchMessage(&msg);


		u32 ImageIndex = 0;
		s32 Result = vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX, ImageAvailableSemaphore, NULL, &ImageIndex);
		if (Result != VK_SUCCESS) ExitProcess(0);

		VkPipelineStageFlags WaitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkSubmitInfo SubmitInfo = {0};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.waitSemaphoreCount = 1;
		SubmitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
		SubmitInfo.pWaitDstStageMask = &WaitDstStageMask;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &CommandBuffers[ImageIndex];
		SubmitInfo.signalSemaphoreCount = 1;
		SubmitInfo.pSignalSemaphores = &RenderingFinishedSemaphore;


		s32 result = vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE);

		VkPresentInfoKHR PresentInfo = {0};
		PresentInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_PRESENT_INFO_KHR;
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = &RenderingFinishedSemaphore;
		PresentInfo.swapchainCount = 1;
		PresentInfo.pSwapchains = &Swapchain;
		PresentInfo.pImageIndices = &ImageIndex;

		vkQueuePresentKHR(Queue, &PresentInfo);

		Sleep(100);
	}

	vkDeviceWaitIdle(Device);
	vkDestroyDevice(Device, NULL);
	vkDestroyInstance(Instance, NULL);
}
