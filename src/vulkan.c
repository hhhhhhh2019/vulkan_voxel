#include "vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>


VkInstance instance;
VkPhysicalDevice phys_device;
VkDevice device;
VkQueue queue;

unsigned int queue_family_id;


void init_vulkan() {
	create_instance();
	select_phys_device();
	create_device();
}


void create_instance() {
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = NULL,
		.apiVersion = VK_API_VERSION_1_3,
		.applicationVersion = VK_MAKE_VERSION(1,0,0),
		.engineVersion = VK_MAKE_VERSION(1,0,0),
		.pApplicationName = "voxel render",
		.pEngineName = "voxel render",
	};

	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = NULL,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.enabledExtensionCount = 0,
	};

	VkResult result = vkCreateInstance(&create_info, NULL, &instance);

	if (result != VK_SUCCESS) {
		printf("VkCreateInstance failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void select_phys_device() {
	unsigned int devices_count;
	vkEnumeratePhysicalDevices(instance, &devices_count, NULL);
	VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * devices_count);
	vkEnumeratePhysicalDevices(instance, &devices_count, devices);

	for (int i = 0; i < devices_count; i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[i], &props);
		printf("%d: %s\n", i + 1, props.deviceName);
		printf("    vulkan version: %d\n", props.apiVersion);
		printf("    shared mem size: %d kb\n", props.limits.maxComputeSharedMemorySize / 1024);
	
		unsigned int families_count;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &families_count, NULL);
		VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * families_count);
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &families_count, families);
	
		printf("    queues:\n");
		for (int j = 0; j < families_count; j++) {
			printf("        %d\n", families[j].queueFlags);
		}
	}

	int selected_device_id = 1;
	
	phys_device = devices[selected_device_id];
	printf("selected device: %d\n", selected_device_id + 1);

	queue_family_id = 0;
}

void create_device() {
	float priority = 1.0;

	VkDeviceQueueCreateInfo queue_create_infos[] = {
		(VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = NULL,
			.queueCount = 1,
			.queueFamilyIndex = queue_family_id,
			.pQueuePriorities = &priority,
		},
	};

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(phys_device, &features);

	VkDeviceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueCreateInfoCount = sizeof(queue_create_infos) / sizeof(VkDeviceQueueCreateInfo),
		.pQueueCreateInfos = queue_create_infos,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = NULL,
		.enabledExtensionCount = 0,
		.ppEnabledExtensionNames = NULL,
		.pEnabledFeatures = &features,
	};


	VkResult result = vkCreateDevice(phys_device, &create_info, NULL, &device);

	if (result != VK_SUCCESS) {
		printf("vkCreateDevice failed: %s\n", string_VkResult(result));
		exit(1);
	}

	vkGetDeviceQueue(device, queue_family_id, 0, &queue);
}
