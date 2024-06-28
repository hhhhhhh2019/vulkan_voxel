#ifndef VULKAN_H
#define VULKAN_H


#include <vulkan/vulkan.h>


extern VkInstance instance;
extern VkPhysicalDevice phys_device;
extern VkDevice device;
extern unsigned int queue_family_id;
extern VkQueue queue;


void init_vulkan();
void create_instance();
void select_phys_device();
void create_device();


#endif
