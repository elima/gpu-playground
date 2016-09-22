/*
 * Example:
 *
 * Vulkan minimal: An absolute minimal Vulkan demo.
 *
 * This example renders a triangle using the Vulkan API on an X11 window.
 * It does the minimum required to put pixels on the screen, and not much
 * more (e.g, doesn't support resizing the window).
 *
 * Tested on Linux 4.7, Mesa 12.0, Intel GPU (gen7+).
 *
 * Authors:
 *   * Eduardo Lima Mitev <elima@igalia.com>
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
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>

#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>

#define WIDTH  640
#define HEIGHT 480

PFN_vkVoidFunction vk_icdGetInstanceProcAddr (VkInstance instance,
                                              const char* pName);

#define GET_ICD_PROC_ADDR(api, symbol)                                  \
   api.symbol = (PFN_vk ##symbol) vk_icdGetInstanceProcAddr(NULL, "vk" #symbol);

#define GET_PROC_ADDR(api, symbol)                                      \
   api.symbol = (PFN_vk ##symbol) api.GetInstanceProcAddr(NULL, "vk" #symbol);

#define GET_INSTANCE_PROC_ADDR(api, instance, symbol)                   \
   api.symbol = (PFN_vk ##symbol) api.GetInstanceProcAddr(instance, "vk" #symbol);

#define GET_DEVICE_PROC_ADDR(api, device, symbol)                       \
   api.symbol = (PFN_vk ##symbol) api.GetDeviceProcAddr(device, "vk" #symbol);

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

   PFN_vkCreateXcbSurfaceKHR                     CreateXcbSurfaceKHR;
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
};

static uint32_t*
load_file (const char* filename, size_t* file_size)
{
   char* data = NULL;
   size_t size = 0;
   size_t read_size = 0;
   size_t alloc_size = 0;
   int32_t fd = open (filename, O_RDONLY);
   uint8_t buf[1024];

   while ((read_size = read (fd, buf, 1024)) > 0) {
      if (size + read_size > alloc_size) {
         alloc_size = read_size + size;
         data = realloc (data, alloc_size);
      }

      memcpy (data + size, buf, read_size);
      size += read_size;
   }

   if (read_size == 0) {
      if (file_size)
         *file_size = size;

      return (uint32_t*) data;
   }
   else {
      return NULL;
   }
}

int32_t
main (int32_t argc, char* argv[])
{
   VkResult result;
   struct vk_api vk;

   /* XCB setup */
   /* ======================================================================= */

   /* connection to the X server */
   xcb_connection_t* xcb_conn = xcb_connect (NULL, NULL);
   assert (xcb_conn != NULL);

   /* Get the first screen */
   const xcb_setup_t* xcb_setup  = xcb_get_setup (xcb_conn);
   xcb_screen_iterator_t iter   = xcb_setup_roots_iterator (xcb_setup);
   xcb_screen_t* xcb_screen = iter.data;
   assert (xcb_screen != NULL);

   /* Create the window */
   xcb_window_t xcb_win = xcb_generate_id (xcb_conn);

   uint32_t value_mask, value_list[32];
   value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
   value_list[0] = xcb_screen->black_pixel;
   value_list[1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY;

   xcb_create_window (xcb_conn,                      /* Connection          */
                      XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                      xcb_win,                       /* window Id           */
                      xcb_screen->root,              /* parent window       */
                      0, 0,                          /* x, y                */
                      WIDTH, HEIGHT,                 /* width, height       */
                      0,                             /* border_width        */
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class               */
                      xcb_screen->root_visual,       /* visual              */
                      value_mask, value_list);       /* masks, not used yet */

   /* Map the window onto the screen */
   xcb_map_window (xcb_conn, xcb_win);

   /* Make sure commands are sent before we pause so that the window gets
    * shown.
    */
   xcb_flush (xcb_conn);

   /* Vulkan setup */
   /* ======================================================================= */

   /* load inital API entry points */
   GET_ICD_PROC_ADDR (vk, GetInstanceProcAddr);
   GET_PROC_ADDR (vk, EnumerateInstanceLayerProperties);
   GET_PROC_ADDR (vk, EnumerateInstanceExtensionProperties);
   GET_PROC_ADDR (vk, CreateInstance);

   /* enummerate supported instance extensions, where we normally check for
    * support for window systems integration and presence of VK_KHR_surface.
    */
   VkExtensionProperties ext_props[16];
   uint32_t ext_props_count = 16;
   result = vk.EnumerateInstanceExtensionProperties (NULL,
                                                     &ext_props_count,
                                                     ext_props);
   assert (result == VK_SUCCESS);
   printf ("Instance extensions:\n");
   for (unsigned i = 0; i < ext_props_count; i++)
      printf ("   %s(%u)\n",
              ext_props[i].extensionName,
              ext_props[i].specVersion);

   /* the memory allocation callbacks (default by now) */
   const VkAllocationCallbacks* allocator = NULL;

   /* Vulkan application info */
   VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Vulkan minimal example",
      .apiVersion = VK_API_VERSION_1_0,
   };

   /* create the Vulkan instance */
   VkInstance instance = VK_NULL_HANDLE;

   const char* enabled_extensions[2] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_XCB_SURFACE_EXTENSION_NAME
   };

   VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = 2,
      .ppEnabledExtensionNames = enabled_extensions
   };
   result = vk.CreateInstance (&instance_info,
                               allocator,
                               &instance);
   assert (result == VK_SUCCESS);

   /* load instance-dependent API entry points */
   GET_INSTANCE_PROC_ADDR (vk, instance, DestroyInstance);
   GET_INSTANCE_PROC_ADDR (vk, instance, EnumeratePhysicalDevices);
   GET_INSTANCE_PROC_ADDR (vk, instance, GetPhysicalDeviceQueueFamilyProperties);
   GET_INSTANCE_PROC_ADDR (vk, instance, CreateDevice);
   GET_INSTANCE_PROC_ADDR (vk, instance, EnumerateDeviceExtensionProperties);
   GET_INSTANCE_PROC_ADDR (vk, instance, GetPhysicalDeviceProperties);

   GET_INSTANCE_PROC_ADDR (vk, instance, CreateXcbSurfaceKHR);
   GET_INSTANCE_PROC_ADDR (vk, instance, DestroySurfaceKHR);
   GET_INSTANCE_PROC_ADDR (vk, instance, GetPhysicalDeviceSurfaceSupportKHR);
   GET_INSTANCE_PROC_ADDR (vk, instance, GetPhysicalDeviceSurfaceFormatsKHR);
   GET_INSTANCE_PROC_ADDR (vk, instance,
                           GetPhysicalDeviceSurfacePresentModesKHR);
   GET_INSTANCE_PROC_ADDR (vk, instance,
                           GetPhysicalDeviceSurfaceCapabilitiesKHR);

   /* query physical devices */
   uint32_t num_devices = 5;
   VkPhysicalDevice devices[5] = {0};
   result = vk.EnumeratePhysicalDevices (instance,
                                         &num_devices,
                                         devices);
   assert (result == VK_SUCCESS);
   assert (num_devices > 0);

   VkPhysicalDevice physical_device = devices[0];

   /* query physical device's queue families */
   uint32_t num_queue_families = 5;
   VkQueueFamilyProperties queue_families[5] = {0};
   /* get queue families with NULL first, to retrieve count */
   vk.GetPhysicalDeviceQueueFamilyProperties (physical_device,
                                              &num_queue_families,
                                              NULL);
   vk.GetPhysicalDeviceQueueFamilyProperties (devices[0],
                                              &num_queue_families,
                                              queue_families);
   assert (num_queue_families >= 1);

   uint32_t queue_family_index = 0;
   assert (queue_families[queue_family_index].queueFlags &
           VK_QUEUE_GRAPHICS_BIT);

   /* Enummerate supported device extensions, where we would normally check for
    * the presence of VK_KHR_swapchain.
    */
   ext_props_count = 16;
   result = vk.EnumerateDeviceExtensionProperties (physical_device,
                                                   NULL,
                                                   &ext_props_count,
                                                   ext_props);
   assert (result == VK_SUCCESS);
   printf ("Device extensions: \n");
   for (unsigned i = 0; i < ext_props_count; i++)
      printf ("   %s(%u)\n",
              ext_props[i].extensionName,
              ext_props[i].specVersion);

   /* create a vulkan surface, from the XCB window (VkSurfaceKHR) */
   VkSurfaceKHR surface = VK_NULL_HANDLE;
   VkXcbSurfaceCreateInfoKHR surface_info = {
      .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
      .connection = xcb_conn,
      .window = xcb_win
   };
   result = vk.CreateXcbSurfaceKHR (instance,
                                    &surface_info,
                                    allocator,
                                    &surface);
   assert (result == VK_SUCCESS);

   /* check for present support in the selected queue family */
   VkBool32 support_present = VK_FALSE;
   vk.GetPhysicalDeviceSurfaceSupportKHR (physical_device,
                                          queue_family_index,
                                          surface,
                                          &support_present);
   assert (support_present);

   /* physical device capabilities (needed by swapchain for
    * minImageCount and maxImageCount.
    */
   VkSurfaceCapabilitiesKHR surface_caps;
   result = vk.GetPhysicalDeviceSurfaceCapabilitiesKHR (physical_device,
                                                        surface,
                                                        &surface_caps);
   assert (result == VK_SUCCESS);

   /* choose a surface format */
   uint32_t surface_formats_count;
   result = vk.GetPhysicalDeviceSurfaceFormatsKHR (physical_device,
                                                   surface,
                                                   &surface_formats_count,
                                                   NULL);
   assert (surface_formats_count > 0);
   surface_formats_count = 1;
   VkSurfaceFormatKHR surface_format;
   result = vk.GetPhysicalDeviceSurfaceFormatsKHR (physical_device,
                                                   surface,
                                                   &surface_formats_count,
                                                   &surface_format);
   assert (result == VK_SUCCESS || result == VK_INCOMPLETE);

   /* choose a present mode */
   VkPresentModeKHR present_mode;
   uint32_t present_mode_count = 1;
   result = vk.GetPhysicalDeviceSurfacePresentModesKHR (physical_device,
                                                        surface,
                                                        &present_mode_count,
                                                        &present_mode);
   assert (result == VK_SUCCESS || result == VK_INCOMPLETE);
   assert (present_mode_count > 0);

   /* create logical device */
   VkDevice device = VK_NULL_HANDLE;

   const float queue_priorities = 1.0;
   VkDeviceQueueCreateInfo queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .flags = 0,
      .queueCount = 1,
      .pQueuePriorities = &queue_priorities,
      .queueFamilyIndex = queue_family_index
   };

   const char* device_extensions[1] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

   VkDeviceCreateInfo device_info = {
      .sType =  VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pQueueCreateInfos = &queue_info,
      .queueCreateInfoCount = 1,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = device_extensions,
   };
   result = vk.CreateDevice (devices[0],
                             &device_info,
                             allocator,
                             &device);
   assert (result == VK_SUCCESS);

   /* load device-dependent API entry points */
   GET_INSTANCE_PROC_ADDR (vk, instance, GetDeviceProcAddr);
   GET_DEVICE_PROC_ADDR (vk, device, CreateCommandPool);
   GET_DEVICE_PROC_ADDR (vk, device, CmdBeginRenderPass);
   GET_DEVICE_PROC_ADDR (vk, device, CmdDraw);
   GET_DEVICE_PROC_ADDR (vk, device, CmdEndRenderPass);
   GET_DEVICE_PROC_ADDR (vk, device, AllocateCommandBuffers);
   GET_DEVICE_PROC_ADDR (vk, device, FreeCommandBuffers);
   GET_DEVICE_PROC_ADDR (vk, device, GetDeviceQueue);
   GET_DEVICE_PROC_ADDR (vk, device, CreateRenderPass);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyRenderPass);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyCommandPool);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyDevice);
   GET_DEVICE_PROC_ADDR (vk, device, CreateGraphicsPipelines);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyPipeline);
   GET_DEVICE_PROC_ADDR (vk, device, CreateShaderModule);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyShaderModule);
   GET_DEVICE_PROC_ADDR (vk, device, CreatePipelineLayout);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyPipelineLayout);
   GET_DEVICE_PROC_ADDR (vk, device, CreateImageView);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyImageView);
   GET_DEVICE_PROC_ADDR (vk, device, CreateFramebuffer);
   GET_DEVICE_PROC_ADDR (vk, device, DestroyFramebuffer);
   GET_DEVICE_PROC_ADDR (vk, device, BeginCommandBuffer);
   GET_DEVICE_PROC_ADDR (vk, device, EndCommandBuffer);
   GET_DEVICE_PROC_ADDR (vk, device, CmdBindPipeline);
   GET_DEVICE_PROC_ADDR (vk, device, CreateSemaphore);
   GET_DEVICE_PROC_ADDR (vk, device, DestroySemaphore);
   GET_DEVICE_PROC_ADDR (vk, device, QueueSubmit);
   GET_DEVICE_PROC_ADDR (vk, device, DeviceWaitIdle);

   GET_DEVICE_PROC_ADDR (vk, device, CreateSwapchainKHR);
   GET_DEVICE_PROC_ADDR (vk, device, DestroySwapchainKHR);
   GET_DEVICE_PROC_ADDR (vk, device, GetSwapchainImagesKHR);
   GET_DEVICE_PROC_ADDR (vk, device, AcquireNextImageKHR);
   GET_DEVICE_PROC_ADDR (vk, device, QueuePresentKHR);

   /* create the vertex shader module */
   size_t shader_code_size;
   uint32_t* shader_code = load_file (CURRENT_DIR "/vert.spv",
                                      &shader_code_size);
   assert (shader_code != NULL);

   VkShaderModuleCreateInfo shader_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = shader_code_size,
      .pCode = shader_code,
   };
   VkShaderModule vert_shader_module;
   result = vk.CreateShaderModule (device,
                                   &shader_info,
                                   allocator,
                                   &vert_shader_module);
   assert (result == VK_SUCCESS);
   free (shader_code);

   /* create the fragment shader module */
   shader_code = load_file (CURRENT_DIR "/frag.spv", &shader_code_size);
   assert (shader_code != NULL);

   shader_info.codeSize = shader_code_size;
   shader_info.pCode = shader_code;

   VkShaderModule frag_shader_module;
   result = vk.CreateShaderModule (device,
                                   &shader_info,
                                   allocator,
                                   &frag_shader_module);
   assert (result == VK_SUCCESS);
   free (shader_code);

   /* create the shader stages */
   VkPipelineShaderStageCreateInfo vert_stage_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vert_shader_module,
      .pName = "main"
   };

   VkPipelineShaderStageCreateInfo frag_stage_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = frag_shader_module,
      .pName = "main"
   };

   VkPipelineShaderStageCreateInfo shader_stages[2] =
      {vert_stage_info, frag_stage_info};

   /* get first device queue */
   VkQueue queue = VK_NULL_HANDLE;
   vk.GetDeviceQueue (device, queue_family_index, 0, &queue);
   assert (queue != VK_NULL_HANDLE);

   /* create a command pool */
   VkCommandPool cmd_pool = VK_NULL_HANDLE;;
   VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = 0,
      .queueFamilyIndex = queue_family_index,
   };
   result = vk.CreateCommandPool (device,
                                  &cmd_pool_info,
                                  allocator,
                                  &cmd_pool);
   assert (result == VK_SUCCESS);

   /* create rendering semaphores */
   VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
   VkSemaphore render_finished_semaphore = VK_NULL_HANDLE;

   VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
   };

   result = vk.CreateSemaphore (device,
                                &semaphore_info,
                                allocator,
                                &image_available_semaphore);
   assert (result == VK_SUCCESS);

   result = vk.CreateSemaphore (device,
                                &semaphore_info,
                                allocator,
                                &render_finished_semaphore);
   assert (result == VK_SUCCESS);

   /* create the swapchain. This is normally done in a reusable way, because
    * the swapchain needs to be recreated quite often (e.g window resize).
    */
   VkSwapchainKHR swapchain = VK_NULL_HANDLE;

   /* @FIXME: check extent against surface capabilities */
   VkExtent2D swapchain_extent = {WIDTH, HEIGHT};
   VkSwapchainCreateInfoKHR swap_chain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = surface_caps.minImageCount,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .preTransform = surface_caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE
   };

   result = vk.CreateSwapchainKHR (device,
                                   &swap_chain_info,
                                   allocator,
                                   &swapchain);
   assert (result == VK_SUCCESS);

   /* get the images from the swap chain */
   uint32_t swapchain_images_count = 0;
   result = vk.GetSwapchainImagesKHR (device,
                                      swapchain,
                                      &swapchain_images_count,
                                      NULL);
   assert (result == VK_SUCCESS);
   assert (swapchain_images_count > 0 && swapchain_images_count <= 8);

   VkImage swapchain_images[8] = {VK_NULL_HANDLE};
   vk.GetSwapchainImagesKHR (device,
                             swapchain,
                             &swapchain_images_count,
                             swapchain_images);

   /* create an image view for each swap chain image */
   VkImageView image_views[8];
   for (uint32_t i = 0; i < swapchain_images_count; i++) {
      VkImageViewCreateInfo image_view_info = {
         .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
         .image = swapchain_images[i],
         .viewType = VK_IMAGE_VIEW_TYPE_2D,
         .format = surface_format.format,
         .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
         .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
         .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
         .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
         .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
         .subresourceRange.baseMipLevel = 0,
         .subresourceRange.levelCount = 1,
         .subresourceRange.baseArrayLayer = 0,
         .subresourceRange.layerCount = 1,
      };

      result = vk.CreateImageView (device,
                                   &image_view_info,
                                   allocator,
                                   &image_views[i]);
      assert (result == VK_SUCCESS);
   }

   /* config a color attachment */
   VkAttachmentDescription color_attachment = {
      .format = surface_format.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
   };

   /* an attachment reference */
   VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
   };

   /* a render sub-pass */
   VkSubpassDescription render_subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
   };

   VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
   };

   /* create a render pass */
   VkRenderPass renderpass = VK_NULL_HANDLE;
   VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &render_subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency
   };

   result = vk.CreateRenderPass (device,
                                 &render_pass_info,
                                 allocator,
                                 &renderpass);
   assert (result == VK_SUCCESS);

   /* create framebuffers for each image view */
   VkFramebuffer framebuffers[4] = {VK_NULL_HANDLE};
   for (uint32_t i = 0; i < swapchain_images_count; i++) {
         VkImageView attachments[] = {
         image_views[i]
      };

      VkFramebufferCreateInfo framebuffer_info = {
         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
         .renderPass = renderpass,
         .attachmentCount = 1,
         .pAttachments = attachments,
         .width = swapchain_extent.width,
         .height = swapchain_extent.height,
         .layers = 1
      };

      result = vk.CreateFramebuffer (device,
                                     &framebuffer_info,
                                     allocator,
                                     &framebuffers[i]);
      assert (result == VK_SUCCESS);
   }

   /* create the graphics pipeline */

   /* specify the vertex input (all default to zero, since we won't use it here) */
   VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
   };

   /* specify the input assembly (type of primitives) */
   VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
   };

   /* viewport and scissors */
   VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = (float) swapchain_extent.width,
      .height = (float) swapchain_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f
   };

   VkRect2D scissor = {
      .offset.x = 0,
      .offset.y = 0,
      .extent = swapchain_extent
   };

   VkPipelineViewportStateCreateInfo viewport_state_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor
   };

   /* configure rasterizer */
   VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,

      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,

      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f
   };

   /* configure multisampling */
   VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE
   };

   /* color blending */
   VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask =
         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
         | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD
   };

   VkPipelineColorBlendStateCreateInfo color_blending_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment,
      .blendConstants[0] = 0.0f,
      .blendConstants[1] = 0.0f,
      .blendConstants[2] = 0.0f,
      .blendConstants[3] = 0.0f,
   };

   /* pipeline layout */
   VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
   VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = NULL,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = 0
   };
   result = vk.CreatePipelineLayout (device,
                                     &pipeline_layout_info,
                                     NULL,
                                     &pipeline_layout);
   assert (result == VK_SUCCESS);

   /* the pipeline, finally */
   VkPipeline pipeline = VK_NULL_HANDLE;
   VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly_info,
      .pViewportState = &viewport_state_info,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = NULL,
      .pColorBlendState = &color_blending_info,
      .pDynamicState = NULL,
      .layout = pipeline_layout,
      .renderPass = renderpass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1
   };
   result = vk.CreateGraphicsPipelines (device,
                                        VK_NULL_HANDLE,
                                        1,
                                        &pipeline_info,
                                        allocator,
                                        &pipeline);
   assert (result == VK_SUCCESS);

   /* create command buffers */
   VkCommandBuffer cmd_buffers[8] = {VK_NULL_HANDLE,};
   VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = swapchain_images_count,
      .commandPool = cmd_pool
   };
   result = vk.AllocateCommandBuffers (device,
                                       &cmd_buffer_alloc_info,
                                       cmd_buffers);
   assert (result == VK_SUCCESS);

   /* start recording command buffers */
   for (uint32_t i = 0; i < swapchain_images_count; i++) {
      VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
         .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
         .pInheritanceInfo = VK_NULL_HANDLE
      };

      result = vk.BeginCommandBuffer (cmd_buffers[i], &begin_info);
      assert (result == VK_SUCCESS);

      /* start a render pass */
      VkClearValue clear_color = {{{0.01f, 0.01f, 0.01f, 1.0f}}};
      VkOffset2D swapchain_offset = {0, 0};
      VkRenderPassBeginInfo renderpass_begin_info = {
         .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
         .renderPass = renderpass,
         .framebuffer = framebuffers[i],
         .renderArea.offset = swapchain_offset,
         .renderArea.extent = swapchain_extent,
         .clearValueCount = 1,
         .pClearValues = &clear_color
      };
      vk.CmdBeginRenderPass (cmd_buffers[i],
                             &renderpass_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE);

      vk.CmdBindPipeline (cmd_buffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);

      vk.CmdDraw (cmd_buffers[i], 3, 1, 0, 0);

      vk.CmdEndRenderPass (cmd_buffers[i]);

      vk.EndCommandBuffer (cmd_buffers[i]);
   }


   /* start the show (mainloop) */

   while (VK_TRUE) {
      /* acquire next image */
      uint32_t image_index;
      result = vk.AcquireNextImageKHR (device,
                                       swapchain,
                                       1000,
                                       image_available_semaphore,
                                       VK_NULL_HANDLE,
                                       &image_index);
      if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
         /* swapchain needs recreation */
         printf ("EXPOSE event, swapchain needs to be recreated\n");
         continue;
      }

      assert (result == VK_SUCCESS);

      /* submit the command buffer to the graphics queue */
      VkSemaphore wait_semaphores[] = {image_available_semaphore};
      VkSemaphore signal_semaphores[] = {render_finished_semaphore};

      VkPipelineStageFlags wait_stages[] =
         {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

      VkSubmitInfo submit_info = {
         .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
         .waitSemaphoreCount = 1,
         .pWaitSemaphores = wait_semaphores,
         .pWaitDstStageMask = wait_stages,
         .commandBufferCount = 1,
         .pCommandBuffers = &cmd_buffers[image_index],
         .signalSemaphoreCount = 1,
         .pSignalSemaphores = signal_semaphores
      };

      result = vk.QueueSubmit (queue,
                               1,
                               &submit_info,
                               VK_NULL_HANDLE);
      assert (result == VK_SUCCESS);

      /* present the frame */
      VkSwapchainKHR swap_chains[] = {swapchain};
      VkPresentInfoKHR present_info = {
         .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
         .waitSemaphoreCount = 1,
         .pWaitSemaphores = signal_semaphores,
         .swapchainCount = 1,
         .pSwapchains = swap_chains,
         .pImageIndices = &image_index,
         .pResults = NULL
      };

      /* present the render pass onto the surface. or... DRAW! */
      result = vk.QueuePresentKHR (queue, &present_info);
      if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
         /* swapchain needs recreation */
         printf ("EXPOSE event, swapchain needs to be recreated\n");
         continue;
      }

      assert (result == VK_SUCCESS);
   }

   /* free all allocated objects */

   /* wait for all async ops on device */
   if (device != VK_NULL_HANDLE)
      vk.DeviceWaitIdle (device);

   /* destroy state */
   for (uint32_t i = 0; i < swapchain_images_count; i++)
      vk.DestroyFramebuffer (device, framebuffers[i], allocator);

   for (uint32_t i = 0; i < swapchain_images_count; i++)
      vk.DestroyImageView (device, image_views[i], allocator);

   vk.DestroyPipeline (device, pipeline, allocator);
   vk.DestroyPipelineLayout (device, pipeline_layout, allocator);
   vk.DestroyRenderPass (device, renderpass, allocator);
   vk.DestroySwapchainKHR (device, swapchain, allocator);

   /* destroy immutable objects */
   vk.DestroySemaphore (device, image_available_semaphore, allocator);
   vk.DestroySemaphore (device, render_finished_semaphore, allocator);
   vk.DestroyCommandPool (device, cmd_pool, allocator);
   vk.DestroyShaderModule (device, vert_shader_module, allocator);
   vk.DestroyShaderModule (device, frag_shader_module, allocator);
   vk.DestroyDevice (device, allocator);
   vk.DestroySurfaceKHR (instance, surface, allocator);
   vk.DestroyInstance (instance, allocator);

   /* Unmap the window from the screen */
   xcb_unmap_window (xcb_conn, xcb_win);

   /* disconnect from the X server */
   if (xcb_conn != NULL)
      xcb_disconnect (xcb_conn);

   return 0;
}
