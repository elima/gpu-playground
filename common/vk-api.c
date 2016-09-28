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

#include <assert.h>
#include "vk-api.h"

void
vk_api_load_from_icd (struct vk_api* vk)
{
   /* load API entry points from ICD */
   GET_ICD_PROC_ADDR (*vk, GetInstanceProcAddr);
   GET_PROC_ADDR (*vk, EnumerateInstanceLayerProperties);
   GET_PROC_ADDR (*vk, EnumerateInstanceExtensionProperties);
   GET_PROC_ADDR (*vk, CreateInstance);
}

void
vk_api_load_from_instance (struct vk_api* vk, VkInstance* instance)
{
   assert (instance != VK_NULL_HANDLE);
   if (vk->GetInstanceProcAddr == NULL)
      vk_api_load_from_icd (vk);

   /* load instance-dependent API entry points */
   GET_INSTANCE_PROC_ADDR (*vk, *instance, GetDeviceProcAddr);

   GET_INSTANCE_PROC_ADDR (*vk, *instance, DestroyInstance);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, EnumeratePhysicalDevices);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, GetPhysicalDeviceQueueFamilyProperties);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, CreateDevice);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, EnumerateDeviceExtensionProperties);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, GetPhysicalDeviceProperties);

   GET_INSTANCE_PROC_ADDR (*vk, *instance, DestroySurfaceKHR);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, GetPhysicalDeviceSurfaceSupportKHR);
   GET_INSTANCE_PROC_ADDR (*vk, *instance, GetPhysicalDeviceSurfaceFormatsKHR);
   GET_INSTANCE_PROC_ADDR (*vk, *instance,
                           GetPhysicalDeviceSurfacePresentModesKHR);
   GET_INSTANCE_PROC_ADDR (*vk, (*instance),
                           GetPhysicalDeviceSurfaceCapabilitiesKHR);

#ifdef VK_USE_PLATFORM_XCB_KHR
   GET_INSTANCE_PROC_ADDR (*vk, *instance, CreateXcbSurfaceKHR);
#endif
}

void
vk_api_load_from_device (struct vk_api* vk, VkDevice* device)
{
   assert (device != VK_NULL_HANDLE);
   assert (vk->GetDeviceProcAddr != NULL);

   /* load device-dependent API entry points */
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateCommandPool);
   GET_DEVICE_PROC_ADDR (*vk, *device, CmdBeginRenderPass);
   GET_DEVICE_PROC_ADDR (*vk, *device, CmdDraw);
   GET_DEVICE_PROC_ADDR (*vk, *device, CmdEndRenderPass);
   GET_DEVICE_PROC_ADDR (*vk, *device, AllocateCommandBuffers);
   GET_DEVICE_PROC_ADDR (*vk, *device, FreeCommandBuffers);
   GET_DEVICE_PROC_ADDR (*vk, *device, GetDeviceQueue);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateRenderPass);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyRenderPass);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyCommandPool);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyDevice);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateGraphicsPipelines);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyPipeline);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateShaderModule);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyShaderModule);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreatePipelineLayout);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyPipelineLayout);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateImageView);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyImageView);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateFramebuffer);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroyFramebuffer);
   GET_DEVICE_PROC_ADDR (*vk, *device, BeginCommandBuffer);
   GET_DEVICE_PROC_ADDR (*vk, *device, EndCommandBuffer);
   GET_DEVICE_PROC_ADDR (*vk, *device, CmdBindPipeline);
   GET_DEVICE_PROC_ADDR (*vk, *device, CreateSemaphore);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroySemaphore);
   GET_DEVICE_PROC_ADDR (*vk, *device, QueueSubmit);
   GET_DEVICE_PROC_ADDR (*vk, *device, DeviceWaitIdle);

   GET_DEVICE_PROC_ADDR (*vk, *device, CreateSwapchainKHR);
   GET_DEVICE_PROC_ADDR (*vk, *device, DestroySwapchainKHR);
   GET_DEVICE_PROC_ADDR (*vk, *device, GetSwapchainImagesKHR);
   GET_DEVICE_PROC_ADDR (*vk, *device, AcquireNextImageKHR);
   GET_DEVICE_PROC_ADDR (*vk, *device, QueuePresentKHR);
}
