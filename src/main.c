#include "vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vk_enum_string_helper.h>


#define NUMS_COUNT 160
#define ROW_SIZE 10


VkBuffer buffer;
VkDeviceMemory buffer_mem;

static VkResult result;

VkShaderModule shader_module;
VkDescriptorSetLayout descr_layout;
VkPipelineLayout pipeline_layout;
VkPipelineCache pipeline_cache;
VkPipeline pipeline;
VkDescriptorSet set;
VkDescriptorPool descriptor_pool;
VkCommandPool command_pool;
VkCommandBuffer command_buffer;


void create_buffers();
void load_shaders();
void create_pipeline_layout();
void create_pipeline();
void create_descriptor_pool();
void alloc_descr();
void create_command_pool();
void create_command_buffer();
void dispatch_cmd_buffer();


int main() {
	init_vulkan();
	create_buffers();
	load_shaders();
	create_pipeline_layout();
	create_pipeline();
	create_descriptor_pool();
	alloc_descr();
	create_command_pool();
	create_command_buffer();
	dispatch_cmd_buffer();

	VkFence fence;
	VkFenceCreateInfo fence_create_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0
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

	vkQueueSubmit(queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, -1);




	int* data;
	vkMapMemory(device, buffer_mem, 0, sizeof(int) * NUMS_COUNT, 0, (void**)&data);

	for (int i = 0; i < NUMS_COUNT / ROW_SIZE; i++) {
		for (int j = 0; j < ROW_SIZE; j++)
			printf("%02x ", data[i * ROW_SIZE + j]);
		printf("\n");
	}

	vkUnmapMemory(device, buffer_mem);
}


void create_buffers() {
	VkBufferCreateInfo buffer_create_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.size = sizeof(int) * NUMS_COUNT,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &queue_family_id,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.flags = 0,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	};

	result = vkCreateBuffer(device, &buffer_create_info, NULL, &buffer);

	if (result != VK_SUCCESS) {
		printf("vkCreateBuffer failed: %s\n", string_VkResult(result));
		exit(1);
	}


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


	VkMemoryRequirements req;

	vkGetBufferMemoryRequirements(device, buffer, &req);

	printf("%lu %lu %d\n", req.size, req.alignment, req.memoryTypeBits);


	int mem_type_id = 1; // hardcoded
	int mem_head_id = 0; // hardcoded


	VkMemoryAllocateInfo buffer_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = NULL,
		.allocationSize = req.size,
		.memoryTypeIndex = mem_type_id,
	};

	result = vkAllocateMemory(device, &buffer_alloc_info, NULL, &buffer_mem);

	if (result != VK_SUCCESS) {
		printf("vkAllocateMemory failed: %s\n", string_VkResult(result));
		exit(1);
	}


	vkBindBufferMemory(device, buffer, buffer_mem, 0);
}


void load_shaders() {
	FILE* f = fopen("build/shader.spv", "rb");

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	unsigned int* shader_data = malloc(size);
	fread(shader_data, size, 1, f);

	fclose(f);


	VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = NULL,
		.pCode = shader_data,
		.codeSize = size,
	};

	result = vkCreateShaderModule(device, &create_info, NULL, &shader_module);

	if (result != VK_SUCCESS) {
		printf("vkCreateShaderModule failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void create_pipeline_layout() {
	VkDescriptorSetLayoutBinding bindings[] = {
		(VkDescriptorSetLayoutBinding){
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = NULL,
		}
	};

	VkDescriptorSetLayoutCreateInfo deskr_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.bindingCount = 1,
		.pBindings = bindings,
	};

	result = vkCreateDescriptorSetLayout(device, &deskr_create_info, NULL, &descr_layout);

	if (result != VK_SUCCESS) {
		printf("vkCreateDescriptorSetLayout failed: %s\n", string_VkResult(result));
		exit(1);
	}


	VkPipelineLayoutCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = NULL,
		.pSetLayouts = &descr_layout,
		.setLayoutCount = 1,
		.pushConstantRangeCount = 0,
	};

	result = vkCreatePipelineLayout(device, &create_info, NULL, &pipeline_layout);

	if (result != VK_SUCCESS) {
		printf("vkCreatePipelineLayout failed: %s\n", string_VkResult(result));
		exit(1);
	dispatch_cmd_buffer();
	}

	
	VkPipelineCacheCreateInfo cache_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.initialDataSize = 0,
	};

	result = vkCreatePipelineCache(device, &cache_create_info, NULL, &pipeline_cache);

	if (result != VK_SUCCESS) {
		printf("vkCreatePipelineCache failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void create_pipeline() {
	VkPipelineShaderStageCreateInfo shader_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
		.pSpecializationInfo = NULL
	};

	VkComputePipelineCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = NULL,
		.stage = shader_create_info,
		.flags = 0,
		.layout = pipeline_layout,
	};

	result = vkCreateComputePipelines(device, pipeline_cache, 1, &create_info, NULL, &pipeline);

	if (result != VK_SUCCESS) {
		printf("vkCreateComputePipelines failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void create_descriptor_pool() {
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

	result = vkCreateDescriptorPool(device, &create_info, NULL, &descriptor_pool);

	if (result != VK_SUCCESS) {
		printf("vkCreateDescriptorPool failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void alloc_descr() {
	VkDescriptorSetAllocateInfo desc_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descr_layout,
	};

	result = vkAllocateDescriptorSets(device, &desc_alloc_info, &set);

	if (result != VK_SUCCESS) {
		printf("vkAllocateDescriptorSets failed: %s\n", string_VkResult(result));
		exit(1);
	}


	VkDescriptorBufferInfo buffer_info = {
		.buffer = buffer,
		.offset = 0,
		.range = sizeof(int) * NUMS_COUNT
	};


	VkWriteDescriptorSet desc_writes[] = {
		(VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = NULL,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.dstSet = set,
			.pBufferInfo = &buffer_info,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.pImageInfo = NULL,
		},
	};


	vkUpdateDescriptorSets(device, 1, desc_writes, 0, NULL);
}


void create_command_pool() {
	VkCommandPoolCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueFamilyIndex = queue_family_id
	};

	result = vkCreateCommandPool(device, &create_info, NULL, &command_pool);

	if (result != VK_SUCCESS) {
		printf("vkCreateCommandPool failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void create_command_buffer() {
	VkCommandBufferAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = NULL,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = command_pool,
		.commandBufferCount = 1,
	};

	result = vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

	if (result != VK_SUCCESS) {
		printf("vkAllocateCommandBuffers failed: %s\n", string_VkResult(result));
		exit(1);
	}
}


void dispatch_cmd_buffer() {
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = NULL,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = NULL,
	};

	vkBeginCommandBuffer(command_buffer, &begin_info);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, NULL);
	vkCmdDispatch(command_buffer, 1, 1, 1);
	vkEndCommandBuffer(command_buffer);
}
