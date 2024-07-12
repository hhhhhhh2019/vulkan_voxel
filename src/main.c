// almost all of code is hardcoded


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


const int output_width  = 32 * 16 * 3; // 1280
const int output_height = 32 * 9  * 3; // 736


GLFWwindow* window;


#define vk_assert(FUNC, ...) do { \
	VkResult result = FUNC(__VA_ARGS__); \
	if (result != VK_SUCCESS) { \
		printf("%s failed: %s\n", #FUNC, string_VkResult(result)); \
		exit(1); \
	} \
} while (0)


VkInstance instance;
VkPhysicalDevice phys_device;
struct {
	unsigned int compute;
	unsigned int present;
} queue_fam_ids;
VkQueue queue;
VkDevice device;
VkSurfaceKHR surface;

VkShaderModule render_shader;

VkExtent2D swap_image_extent;
VkSwapchainKHR swapchain;
unsigned int swap_images_count;
VkImage* swap_images;

VkImage output_image;
VkSampler output_image_sampler;
VkImageView output_image_view;
VkDeviceMemory output_image_mem;

VkBuffer camera_buffer;
VkDeviceMemory camera_buffer_mem;

VkPipelineLayout pipeline_layout;
VkPipeline pipeline;
VkCommandPool command_pool;
VkCommandBuffer command_buffer;
VkDescriptorSetLayout descr_layout;
VkDescriptorPool descriptor_pool;
VkDescriptorSet set;

VkFence fence;


void create_instance();
void select_phys_device();
void create_device();
void create_swapchain();
void create_buffers();
void create_pipeline_layout();
void create_pipeline();
void create_command_pool();
void create_command_buffer();
void create_descr_pool();
void alloc_descr();

void create_shader(char*, VkShaderModule*);



int main() {
	if (!glfwInit()) {
		perror("glfwInit");
		exit(errno);
	}

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(output_width, output_height, "voxels", glfwGetPrimaryMonitor(), NULL);


	create_instance();
	glfwCreateWindowSurface(instance, window, NULL, &surface);
	select_phys_device();
	create_device();
	
	create_shader("build/render.comp.spv", &render_shader);

	create_swapchain();

	create_buffers();
	create_pipeline_layout();
	create_pipeline();
	create_command_pool();
	create_command_buffer();
	create_descr_pool();
	alloc_descr();


	VkFenceCreateInfo fence_create_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
	};
	vkCreateFence(device, &fence_create_info, NULL, &fence);


	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = NULL,
		.waitSemaphoreCount = 0,
		.commandBufferCount = 1,
		.pCommandBuffers = &command_buffer,
		.signalSemaphoreCount = 0,
		.pWaitDstStageMask = NULL,
	};


	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = NULL,
	};

	VkImageCopy image_copy = {
		.extent = (VkExtent3D){output_width, output_height, 1},
		.srcOffset = (VkOffset3D){0,0,0},
		.dstOffset = (VkOffset3D){0,0,0},
		.srcSubresource = (VkImageSubresourceLayers){
			.mipLevel = 0,
			.layerCount = 1,
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseArrayLayer = 0,
		},
		.dstSubresource = (VkImageSubresourceLayers){
			.mipLevel = 0,
			.layerCount = 1,
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseArrayLayer = 0,
		},
	};



	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		unsigned int image_id;
		vkResetFences(device, 1, &fence);
		vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, VK_NULL_HANDLE, fence, &image_id);
		vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);


		// render
		vkResetCommandBuffer(command_buffer, 0);
		vkBeginCommandBuffer(command_buffer, &begin_info);
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, NULL);
		vkCmdDispatch(command_buffer, output_width / 32, output_height / 32, 1);
		vkEndCommandBuffer(command_buffer);

		/*printf("\n\n-----1-----\n\n\n");*/
		vkResetFences(device, 1, &fence);
		/*printf("\n\n-----2-----\n\n\n");*/
		vkQueueSubmit(queue, 1, &submit_info, fence);
		/*printf("\n\n-----3-----\n\n\n");*/
		vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
		/*printf("\n\n-----4-----\n\n\n");*/


		// send into swapchain
		vkResetCommandBuffer(command_buffer, 0);
		vkBeginCommandBuffer(command_buffer, &begin_info);
		vkCmdCopyImage(command_buffer, output_image, VK_IMAGE_LAYOUT_UNDEFINED, swap_images[image_id], VK_IMAGE_LAYOUT_GENERAL, 1, &image_copy);
		vkEndCommandBuffer(command_buffer);

		/*printf("\n\n-----5-----\n\n\n");*/
		vkResetFences(device, 1, &fence);
		/*printf("\n\n-----6-----\n\n\n");*/
		vkQueueSubmit(queue, 1, &submit_info, fence);
		/*printf("\n\n-----7-----\n\n\n");*/
		vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
		/*printf("\n\n-----8-----\n\n\n");*/


		VkPresentInfoKHR present = {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &image_id,
			.waitSemaphoreCount = 0,
		};

		vkQueuePresentKHR(queue, &present);

		/*break;*/
	}
}


void create_instance() {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.apiVersion = VK_API_VERSION_1_3,
	};


	unsigned int glfw_ext_count;
	const char** glfw_ext = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

	unsigned int ext_count = glfw_ext_count;
	const char** ext = malloc(sizeof(char*) * ext_count);
	memcpy(ext, glfw_ext, glfw_ext_count * sizeof(char*));
	/*ext[ext_count - 1] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;*/


	const char* lays[] = {
		"VK_LAYER_KHRONOS_validation"
	};


	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = ext_count,
		.ppEnabledExtensionNames = ext,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = lays,
	};

	vk_assert(vkCreateInstance, &create_info, NULL, &instance);
}


void select_phys_device() {
	unsigned int count;
	vkEnumeratePhysicalDevices(instance, &count, NULL);
	VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * count);
	vkEnumeratePhysicalDevices(instance, &count, devices);

	printf("devices: %d\n", count);
	for (int i = 0; i < count; i++) {
		VkPhysicalDeviceProperties props;

		vkGetPhysicalDeviceProperties(devices[i], &props);

		printf("  %d: %s\n", i, props.deviceName);

		unsigned int queues_count;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queues_count, NULL);
		VkQueueFamilyProperties* queues = malloc(sizeof(VkQueueFamilyProperties) * queues_count);
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queues_count, queues);

		printf("  queues: %d\n", queues_count);
		for (int j = 0; j < queues_count; j++) {
			VkBool32 present;
			vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, surface, &present);
			printf("    %d %d\n", queues[j].queueFlags, present);
		}
	}

	printf("\n");


	unsigned int selected_id = 1;

	queue_fam_ids.compute = 0;
	queue_fam_ids.present = 0;

	phys_device = devices[selected_id];
}


void create_device() {
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(phys_device, &features);


	float priority = 1.0;

	VkDeviceQueueCreateInfo queues[] = {
		(VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = NULL,
			.queueCount = 1,
			.queueFamilyIndex = queue_fam_ids.present,
			.pQueuePriorities = &priority,
		}
	};

	
	const char* ext[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};


	VkDeviceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.pEnabledFeatures = &features,
		.pQueueCreateInfos = queues,
		.queueCreateInfoCount = sizeof(queues) / sizeof(queues[0]),
		.enabledExtensionCount = sizeof(ext) / sizeof(char*),
		.ppEnabledExtensionNames = ext,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
	};

	vk_assert(vkCreateDevice, phys_device, &create_info, NULL, &device);


	vkGetDeviceQueue(device, queue_fam_ids.compute, 0, &queue);
}


void create_shader(char* filepath, VkShaderModule* module) {
	FILE* f = fopen(filepath, "rb");

	if (f == NULL) {
		printf("failed to open file: \"%s\": %s\n", filepath, strerror(errno));
		exit(errno);
	}

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	unsigned int* data = malloc(size);

	fread(data, size, 1, f);

	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = NULL,
		.pCode = data,
		.codeSize = size,
		.flags = 0,
	};

	vk_assert(vkCreateShaderModule, device, &create_info, NULL, module);
}


void create_swapchain() {
	VkSurfaceCapabilitiesKHR cap;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface, &cap);

	printf("capabilites:\n");
	printf("  image count: %d %d\n", cap.minImageCount, cap.maxImageCount);
	printf("  max image size: %dx%d\n", cap.minImageExtent.width, cap.minImageExtent.height);
	printf("  min image size: %dx%d\n", cap.maxImageExtent.width, cap.maxImageExtent.height);
	printf("  transform: %d\n", cap.currentTransform);
	printf("\n");


	unsigned int present_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_count, NULL);
	VkPresentModeKHR* presents = malloc(sizeof(VkPresentModeKHR) * present_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface, &present_count, presents);

	printf("presents: %d\n", present_count);
	for (int i = 0; i < present_count; i++) {
		printf("  %d\n", presents[i]);
	}
	printf("\n");


	unsigned int format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, NULL);
	VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count, formats);

	printf("formats: %d\n", format_count);
	for (int i = 0; i < format_count; i++) {
		printf("  %d %d\n", formats[i].format, formats[i].colorSpace);
	}
	printf("\n");


	swap_image_extent = (VkExtent2D){.width = output_width, .height = output_height};


	VkSwapchainCreateInfoKHR create_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = NULL,
		.flags = 0,
		.surface = surface,
		.minImageCount = 4,
		.imageFormat = VK_FORMAT_R8G8B8A8_UNORM,
		.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = swap_image_extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
		.preTransform = 1,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
		.clipped = VK_TRUE,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	vk_assert(vkCreateSwapchainKHR, device, &create_info, NULL, &swapchain);


	vkGetSwapchainImagesKHR(device, swapchain, &swap_images_count, NULL);
	swap_images = malloc(sizeof(VkImage) * swap_images_count);
	vkGetSwapchainImagesKHR(device, swapchain, &swap_images_count, swap_images);
}


void create_buffers() {
	VkImageCreateInfo output_image_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = (VkExtent3D){.width = output_width, .height = output_height, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_LINEAR,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &queue_fam_ids.compute,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	vk_assert(vkCreateImage, device, &output_image_create_info, NULL, &output_image);

	
	/*VkHostImageLayoutTransitionInfoEXT trans_info = {*/
	/*	.sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,*/
	/*	.pNext = NULL,*/
	/*	.image = output_image,*/
	/*	.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,*/
	/*	.newLayout = VK_IMAGE_LAYOUT_GENERAL,*/
	/*	.subresourceRange = (VkImageSubresourceRange){*/
	/*		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,*/
	/*		.baseMipLevel = 0,*/
	/*		.levelCount = 1,*/
	/*		.baseArrayLayer = 0,*/
	/*		.layerCount = 1,*/
	/*	},*/
	/*};*/
	/**/
	/*vkTransitionImageLayoutEXT(device, 1, &trans_info);*/


	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(phys_device, &mem_props);

	printf("memory info:\n");

	printf("  types: %d\n", mem_props.memoryTypeCount);
	for (int i = 0; i < mem_props.memoryTypeCount; i++) {
		printf("    %d: %d, %d\n", i,
		    mem_props.memoryTypes[i].heapIndex,
		    mem_props.memoryTypes[i].propertyFlags);
	}

	printf("  heap count: %d\n", mem_props.memoryHeapCount);
	for (int i = 0; i < mem_props.memoryHeapCount; i++) {
		printf("    %d: %lu, %d\n", i,
		    mem_props.memoryHeaps[i].size / 1024 / 1024,
		    mem_props.memoryHeaps[i].flags);
	}
	printf("\n");


	VkMemoryRequirements output_image_req;

	vkGetImageMemoryRequirements(device, output_image, &output_image_req);

	printf("%lu %lu %d\n", output_image_req.size, output_image_req.alignment, output_image_req.memoryTypeBits);


	VkMemoryAllocateInfo output_image_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = output_image_req.size,
		.memoryTypeIndex = 1,
	};

	vk_assert(vkAllocateMemory, device, &output_image_alloc_info, NULL, &output_image_mem);

	vkBindImageMemory(device, output_image, output_image_mem, 0);


	VkSamplerCreateInfo output_image_sampler_create_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0,
		.anisotropyEnable = VK_FALSE,
		.compareEnable = VK_FALSE,
		.minLod = 0,
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	vk_assert(vkCreateSampler, device, &output_image_sampler_create_info, NULL, &output_image_sampler);


	VkImageViewCreateInfo output_image_view_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = output_image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.components = (VkComponentMapping){
			.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		},
		.subresourceRange = (VkImageSubresourceRange){
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	vk_assert(vkCreateImageView, device, &output_image_view_create_info, NULL, &output_image_view);


	VkBufferCreateInfo camera_buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = 3 * 4 + 3 * 4,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &queue_fam_ids.compute,
	};

	vk_assert(vkCreateBuffer, device, &camera_buffer_create_info, NULL, &camera_buffer);
	

	VkMemoryRequirements camera_buffer_req;

	vkGetImageMemoryRequirements(device, output_image, &camera_buffer_req);

	printf("%lu %lu %d\n", camera_buffer_req.size, camera_buffer_req.alignment, camera_buffer_req.memoryTypeBits);


	VkMemoryAllocateInfo camera_buffer_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = camera_buffer_req.size,
		.memoryTypeIndex = 1,
	};

	vk_assert(vkAllocateMemory, device, &camera_buffer_alloc_info, NULL, &camera_buffer_mem);

	vkBindBufferMemory(device, camera_buffer, camera_buffer_mem, 0);
}


void create_pipeline_layout() {
	VkDescriptorSetLayoutBinding bindings[] = {
		(VkDescriptorSetLayoutBinding){
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = NULL,
		},
		(VkDescriptorSetLayoutBinding){
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = NULL,
		}
	};

	VkDescriptorSetLayoutCreateInfo descr_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.bindingCount = sizeof(bindings) / sizeof(bindings[0]),
		.pBindings = bindings,
	};

	vk_assert(vkCreateDescriptorSetLayout, device, &descr_create_info, NULL, &descr_layout);


	VkPipelineLayoutCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.pushConstantRangeCount = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &descr_layout,
	};

	vk_assert(vkCreatePipelineLayout, device, &create_info, NULL, &pipeline_layout);
}


void create_pipeline() {
	VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = render_shader,
		.pName = "main",
		.pSpecializationInfo = NULL,
	};


	VkComputePipelineCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.stage = stage_create_info,
		.layout = pipeline_layout,
	};


	vk_assert(vkCreateComputePipelines, device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline);
}


void create_command_pool() {
	VkCommandPoolCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue_fam_ids.compute
	};

	vk_assert(vkCreateCommandPool, device, &create_info, NULL, &command_pool);
}


void create_command_buffer() {
	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = NULL,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = command_pool,
		.commandBufferCount = 1,
	};

	vk_assert(vkAllocateCommandBuffers, device, &alloc_info, &command_buffer);
}


void create_descr_pool() {
	VkDescriptorPoolSize pool_size = {
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
	};

	VkDescriptorPoolCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	vk_assert(vkCreateDescriptorPool, device, &create_info, NULL, &descriptor_pool);
}


void alloc_descr() {
	VkDescriptorSetAllocateInfo desc_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descr_layout,
	};

	vk_assert(vkAllocateDescriptorSets, device, &desc_alloc_info, &set);


	VkDescriptorImageInfo output_image_info = {
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.sampler = output_image_sampler,
		.imageView = output_image_view,
	};

	VkDescriptorBufferInfo camera_buffer_info = {
		.offset = 0,
	};


	VkWriteDescriptorSet desc_writes[] = {
		(VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = NULL,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.dstSet = set,
			.pImageInfo = &output_image_info,
			.dstBinding = 0,
			.dstArrayElement = 0,
		},
		(VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = NULL,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.dstSet = set,
			.pBufferInfo = &camera_buffer_info,
			.dstBinding = 1,
			.dstArrayElement = 0,
		},
	};


	vkUpdateDescriptorSets(device, 1, desc_writes, 0, NULL);
}
