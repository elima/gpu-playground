/*
 * Vulkan API loader helper
 *
 * This code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 3, or (at your option) any later version as published by
 * the Free Software Foundation.
 *
 * THIS CODE IS PROVIDED AS-IS, WITHOUT WARRANTY OF ANY KIND, OR POSSIBLE
 * LIABILITY TO THE AUTHORS FOR ANY CLAIM OR DAMAGE.
 */

#pragma once

#include <vulkan/vulkan.h>

/* This is only necessary if program is linked to a Vulkan vendor driver\
 * directly
 */
PFN_vkVoidFunction vk_icdGetInstanceProcAddr (VkInstance instance,
                                              const char* pName);
#define GET_ICD_PROC_ADDR(api, symbol)                                  \
   api.symbol = (PFN_vk ##symbol) vk_icdGetInstanceProcAddr(NULL, "vk" #symbol);


#define GET_PROC_ADDR(api, symbol)                                      \
   (api).symbol = (PFN_vk ##symbol) (api).GetInstanceProcAddr(NULL, "vk" #symbol);

#define GET_INSTANCE_PROC_ADDR(api, instance, symbol)                   \
   (api).symbol = (PFN_vk ##symbol) (api).GetInstanceProcAddr(instance, "vk" #symbol);

#define GET_DEVICE_PROC_ADDR(api, device, symbol)                       \
   (api).symbol = (PFN_vk ##symbol) (api).GetDeviceProcAddr(device, "vk" #symbol);

struct vk_api {
   PFN_vkGetInstanceProcAddr                     GetInstanceProcAddr;
   PFN_vkGetDeviceProcAddr                       GetDeviceProcAddr;
   PFN_vkEnumerateInstanceLayerProperties        EnumerateInstanceLayerProperties;
   PFN_vkEnumerateInstanceExtensionProperties    EnumerateInstanceExtensionProperties;
   PFN_vkCreateInstance                          CreateInstance;
   PFN_vkEnumeratePhysicalDevices                EnumeratePhysicalDevices;
   PFN_vkGetPhysicalDeviceProperties             GetPhysicalDeviceProperties;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties  GetPhysicalDeviceQueueFamilyProperties;
   PFN_vkCreateDevice                            CreateDevice;
   PFN_vkEnumerateDeviceExtensionProperties      EnumerateDeviceExtensionProperties;
   PFN_vkGetDeviceQueue                          GetDeviceQueue;
   PFN_vkCreateCommandPool                       CreateCommandPool;
   PFN_vkAllocateCommandBuffers                  AllocateCommandBuffers;
   PFN_vkFreeCommandBuffers                      FreeCommandBuffers;
   PFN_vkCreateRenderPass                        CreateRenderPass;
   PFN_vkDestroyRenderPass                       DestroyRenderPass;
   PFN_vkDestroyCommandPool                      DestroyCommandPool;
   PFN_vkDestroyDevice                           DestroyDevice;
   PFN_vkDestroyInstance                         DestroyInstance;
   PFN_vkCreateGraphicsPipelines                 CreateGraphicsPipelines;
   PFN_vkDestroyPipeline                         DestroyPipeline;
   PFN_vkCreateShaderModule                      CreateShaderModule;
   PFN_vkDestroyShaderModule                     DestroyShaderModule;
   PFN_vkCreatePipelineLayout                    CreatePipelineLayout;
   PFN_vkDestroyPipelineLayout                   DestroyPipelineLayout;
   PFN_vkCreateImageView                         CreateImageView;
   PFN_vkDestroyImageView                        DestroyImageView;
   PFN_vkCreateFramebuffer                       CreateFramebuffer;
   PFN_vkDestroyFramebuffer                      DestroyFramebuffer;
   PFN_vkBeginCommandBuffer                      BeginCommandBuffer;
   PFN_vkEndCommandBuffer                        EndCommandBuffer;
   PFN_vkCmdBeginRenderPass                      CmdBeginRenderPass;
   PFN_vkCmdBindPipeline                         CmdBindPipeline;
   PFN_vkCmdDraw                                 CmdDraw;
   PFN_vkCmdEndRenderPass                        CmdEndRenderPass;
   PFN_vkCreateSemaphore                         CreateSemaphore;
   PFN_vkDestroySemaphore                        DestroySemaphore;
   PFN_vkQueueSubmit                             QueueSubmit;
   PFN_vkDeviceWaitIdle                          DeviceWaitIdle;

   PFN_vkDestroySurfaceKHR                       DestroySurfaceKHR;
   PFN_vkGetPhysicalDeviceSurfaceSupportKHR      GetPhysicalDeviceSurfaceSupportKHR;
   PFN_vkGetPhysicalDeviceSurfaceFormatsKHR      GetPhysicalDeviceSurfaceFormatsKHR;
   PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
   PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
   PFN_vkCreateSwapchainKHR                      CreateSwapchainKHR;
   PFN_vkDestroySwapchainKHR                     DestroySwapchainKHR;
   PFN_vkGetSwapchainImagesKHR                   GetSwapchainImagesKHR;
   PFN_vkAcquireNextImageKHR                     AcquireNextImageKHR;
   PFN_vkQueuePresentKHR                         QueuePresentKHR;

#ifdef VK_USE_PLATFORM_XCB_KHR
   PFN_vkCreateXcbSurfaceKHR                     CreateXcbSurfaceKHR;
#endif
};

void vk_api_load_from_icd      (struct vk_api* vk);

void vk_api_load_from_instance (struct vk_api* vk, VkInstance* instance);

void vk_api_load_from_device   (struct vk_api* vk, VkDevice* device);
