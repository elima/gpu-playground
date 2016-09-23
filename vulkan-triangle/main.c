/*
 * Example:
 *
 * Vulkan triangle: (yet another) Vulkan triangle demo.
 *
 * This example shows a triangle rendered by Vulkan API on an X11 window. It
 * supports resizing the window, and toggling fullscreen mode (F-key).
 *
 * Tested on Linux 4.7, Mesa 12.0, Intel Haswell (gen7+).
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
#include <signal.h>
#include <stdbool.h>
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

static const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;

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

static struct vk_api vk;

struct vk_objects {
   VkPhysicalDevice physical_device;
   VkDevice device;

   VkSurfaceKHR surface;
   VkQueue graphics_queue;
   VkCommandPool cmd_pool;
   VkPipelineShaderStageCreateInfo shader_stages[2];

   VkSemaphore image_available_semaphore;
   VkSemaphore render_finished_semaphore;
};

struct vk_config {
   VkSurfaceCapabilitiesKHR surface_caps;
   VkSurfaceFormatKHR surface_format;
   VkPresentModeKHR present_mode;
};

#define MAX_SWAPCHAIN_IMAGES 8

struct vk_state {
   VkExtent2D surface_extent;

   VkSwapchainKHR swapchain;
   VkSwapchainKHR previous_swapchain;
   uint32_t swapchain_images_count;
   VkImageView image_views[MAX_SWAPCHAIN_IMAGES];
   VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES];

   VkRenderPass renderpass;
   VkPipelineLayout pipeline_layout;
   VkPipeline pipeline;
   VkCommandBuffer cmd_buffers[MAX_SWAPCHAIN_IMAGES];
};

static struct vk_objects objs = {VK_NULL_HANDLE,};
static struct vk_config config = {0,};
static struct vk_state state = {0,};

static bool running = false;
static bool damaged = false;
static bool expose = false;

static xcb_connection_t* xcb_conn = NULL;
static xcb_screen_t* xcb_screen = NULL;
static xcb_window_t xcb_win = 0;
static xcb_intern_atom_reply_t* atom_wm_delete_window;

static bool
recreate_swapchain (struct vk_objects* objs,
                    struct vk_config* config,
                    struct vk_state* state);

static uint32_t*
load_file (const char* filename, size_t* file_size)
{
   char *data = NULL;
   size_t size = 0;
   size_t read_size = 0;
   size_t alloc_size = 0;
   int fd = open (filename, O_RDONLY);
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

static void
ctrl_c_handler (int32_t dummy)
{
   running = false;
   signal (SIGINT, NULL);
}

static void
wsi_toggle_fullscreen_xcb (void)
{
   static bool fullscreen_mode = false;

   fullscreen_mode = ! fullscreen_mode;

   xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom (xcb_conn, 0, 13, "_NET_WM_STATE");
   xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply (xcb_conn, cookie, 0);
   xcb_atom_t atom1 = reply->atom;
   free (reply);

   cookie = xcb_intern_atom (xcb_conn, 0, 24, "_NET_WM_STATE_FULLSCREEN");
   reply = xcb_intern_atom_reply (xcb_conn, cookie, 0);
   xcb_atom_t atom2 = reply->atom;
   free (reply);

   xcb_client_message_event_t msg = {0};
   msg.response_type = XCB_CLIENT_MESSAGE;
   msg.window = xcb_win;
   msg.format = 32;
   msg.type = atom1;
   memset (msg.data.data32, 0, 5 * sizeof (uint32_t));
   msg.data.data32[0] = fullscreen_mode ? 1 : 0;
   msg.data.data32[1] = atom2;

   xcb_send_event (xcb_conn,
                   true,
                   xcb_screen->root,
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                   (const char *) &msg);
   xcb_flush (xcb_conn);
}

static bool
wsi_handle_event_xcb (xcb_generic_event_t* event)
{
   while (event != NULL) {
      uint8_t event_code = event->response_type & 0x7f;
      switch (event_code) {
      case XCB_EXPOSE:
         damaged = true;
         expose = true;
         break;

      case XCB_CLIENT_MESSAGE:
         if ((* (xcb_client_message_event_t*) event).data.data32[0] ==
             (* atom_wm_delete_window).atom) {
            running = false;
            damaged = false;
         }
         break;

      case XCB_KEY_RELEASE: {
         const xcb_key_release_event_t* key =
            (const xcb_key_release_event_t*) event;
         switch (key->detail) {
         case 0x9:
            /* ESC key */
            running = false;
            damaged = false;
            break;

         case 0x29:
            /* F key */
            wsi_toggle_fullscreen_xcb ();
            break;

         default:
            printf ("key pressed: %x\n", key->detail);
            break;
         }
         break;
      }

      default:
         break;
      }

      free (event);
      event = xcb_poll_for_event (xcb_conn);
   }

   return running;
}

static bool
create_renderpass (struct vk_objects* objs,
                   struct vk_config* config,
                   struct vk_state* state)
{
   assert (objs->device != VK_NULL_HANDLE);

   state->renderpass = VK_NULL_HANDLE;

   /* config a color attachment */
   VkAttachmentDescription color_attachment = {
      .format = config->surface_format.format,
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
   VkRenderPassCreateInfo render_pass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment,
      .subpassCount = 1,
      .pSubpasses = &render_subpass,
      .dependencyCount = 1,
      .pDependencies = &dependency
   };

   if (vk.CreateRenderPass (objs->device,
                            &render_pass_info,
                            allocator,
                            &state->renderpass) != VK_SUCCESS) {
      printf ("Error: Failed to create render pass\n");
      return false;
   }
   printf ("Render pass created\n");

   return true;
}

static bool
create_pipeline (struct vk_objects* objs,
                 struct vk_config* config,
                 struct vk_state* state)
{
   assert (objs->device != VK_NULL_HANDLE);
   assert (state->renderpass != VK_NULL_HANDLE);

   /* specify the vertex input */
   VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = NULL,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = NULL
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
      .width = (float) state->surface_extent.width,
      .height = (float) state->surface_extent.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f
   };

   VkRect2D scissor = {
      .offset.x = 0,
      .offset.y = 0,
      .extent = state->surface_extent
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

   vk.DestroyPipelineLayout (objs->device,
                             state->pipeline_layout,
                             allocator);

   /* pipeline layout */
   VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
   VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = NULL,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = 0
   };
   if (vk.CreatePipelineLayout (objs->device,
                                &pipeline_layout_info,
                                NULL,
                                &pipeline_layout) != VK_SUCCESS) {
      printf ("Error: Failed to create a pipeline layout\n");
      return false;
   }
   state->pipeline_layout = pipeline_layout;
   printf ("Pipeline layout created\n");

   /* the graphics pipeline */
   VkPipeline pipeline = VK_NULL_HANDLE;
   VkGraphicsPipelineCreateInfo pipeline_info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = objs->shader_stages,
      .pVertexInputState = &vertex_input_info,
      .pInputAssemblyState = &input_assembly_info,
      .pViewportState = &viewport_state_info,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = NULL,
      .pColorBlendState = &color_blending_info,
      .pDynamicState = NULL,
      .layout = pipeline_layout,
      .renderPass = state->renderpass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1
   };
   if (vk.CreateGraphicsPipelines (objs->device,
                                   VK_NULL_HANDLE,
                                   1,
                                   &pipeline_info,
                                   allocator,
                                   &pipeline) != VK_SUCCESS) {
      printf ("Error: Failed to create the graphics pipeline\n");
      return false;
   }
   state->pipeline = pipeline;
   printf ("Graphics pipeline created\n");

   return true;
}

static bool
create_command_buffers (struct vk_objects* objs,
                        struct vk_config* config,
                        struct vk_state* state)
{
   assert (objs->device != VK_NULL_HANDLE);
   assert (objs->cmd_pool != VK_NULL_HANDLE);
   assert (state->renderpass != VK_NULL_HANDLE);

   /* create command buffers */
   VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = state->swapchain_images_count,
      .commandPool = objs->cmd_pool
   };
   if (vk.AllocateCommandBuffers (objs->device,
                                  &cmd_buffer_alloc_info,
                                  state->cmd_buffers) != VK_SUCCESS) {
      printf ("Error: Failed to allocate command buffers\n");
      return false;
   }
   printf ("Command buffers allocated\n");

   /* start recording to command buffers */
   for (uint32_t i = 0; i < state->swapchain_images_count; i++) {
      assert (state->framebuffers[i] != VK_NULL_HANDLE);

      VkCommandBufferBeginInfo begin_info = {
         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
         .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
         .pInheritanceInfo = NULL
      };

      if (vk.BeginCommandBuffer (state->cmd_buffers[i],
                                 &begin_info) != VK_SUCCESS) {
         printf ("Error: Failed to begin recording of command buffer\n");
         return false;
      }

      /* start a render pass */
      VkClearValue clear_color = {{{0.01f, 0.01f, 0.01f, 1.0f}}};
      VkOffset2D swapchain_offset = {0, 0};
      VkRenderPassBeginInfo renderpass_begin_info = {
         .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
         .renderPass = state->renderpass,
         .framebuffer = state->framebuffers[i],
         .renderArea.offset = swapchain_offset,
         .renderArea.extent = state->surface_extent,
         .clearValueCount = 1,
         .pClearValues = &clear_color
      };
      vk.CmdBeginRenderPass (state->cmd_buffers[i],
                             &renderpass_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE);

      vk.CmdBindPipeline (state->cmd_buffers[i],
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          state->pipeline);

      vk.CmdDraw (state->cmd_buffers[i], 3, 1, 0, 0);

      vk.CmdEndRenderPass (state->cmd_buffers[i]);

      vk.EndCommandBuffer (state->cmd_buffers[i]);
   }
   printf ("Render pass commands recorded in buffer\n");

   return true;
}

static bool
recreate_swapchain (struct vk_objects* objs,
                    struct vk_config* config,
                    struct vk_state* state)
{
   uint32_t width, height;

   assert (objs->physical_device != VK_NULL_HANDLE);
   assert (objs->device != VK_NULL_HANDLE);
   assert (objs->surface != VK_NULL_HANDLE);

   /* wait for all async ops on device */
   vk.DeviceWaitIdle (objs->device);

   /* resolve swap image size */
   VkSurfaceCapabilitiesKHR surface_caps;
   vk.GetPhysicalDeviceSurfaceCapabilitiesKHR (objs->physical_device,
                                               objs->surface,
                                               &surface_caps);
   config->surface_caps = surface_caps;
   printf ("Surface's image count (min, max): (%u, %u)\n",
           surface_caps.minImageCount,
           surface_caps.maxImageCount);
   printf ("Surface's current extent (width, height): (%u, %u)\n",
           surface_caps.currentExtent.width,
           surface_caps.currentExtent.height);

   width = surface_caps.currentExtent.width;
   height = surface_caps.currentExtent.height;

   state->surface_extent.width = width;
   state->surface_extent.height = height;

   /* destroy previous swapchain */
   if (state->swapchain != VK_NULL_HANDLE) {
      /* keep record of the previous swapchain to link it to the new one */
      if (state->previous_swapchain != VK_NULL_HANDLE)
         vk.DestroySwapchainKHR (objs->device,
                                 state->previous_swapchain,
                                 allocator);
      state->previous_swapchain = state->swapchain;

      state->swapchain = VK_NULL_HANDLE;
   }

   /* create a swapchain */
   VkSwapchainKHR swapchain = VK_NULL_HANDLE;
   VkExtent2D swapchain_extent = {width, height};

   VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = objs->surface,
      .minImageCount = config->surface_caps.minImageCount,
      .imageFormat = config->surface_format.format,
      .imageColorSpace = config->surface_format.colorSpace,
      .imageExtent = swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .preTransform = config->surface_caps.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = config->present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = state->previous_swapchain
   };

   if (vk.CreateSwapchainKHR (objs->device,
                              &swapchain_info,
                              allocator,
                              &swapchain) != VK_SUCCESS) {
      printf ("Error: Failed to create a swap chain\n");
      return false;
   }
   state->swapchain = swapchain;
   printf ("Swap chain created\n");

   /* get the images from the swap chain */
   uint32_t swapchain_images_count = 0;
   if (vk.GetSwapchainImagesKHR (objs->device,
                                 state->swapchain,
                                 &swapchain_images_count,
                                 NULL) != VK_SUCCESS) {
      printf ("Error: Failed to get the images from the swap chain\n");
      return false;
   }
   if (swapchain_images_count > MAX_SWAPCHAIN_IMAGES) {
      printf ("Too many images in the swapchain. I can handle only %u\n",
              swapchain_images_count);
      return false;
   }
   uint32_t old_swapchain_images_count = state->swapchain_images_count;
   state->swapchain_images_count = swapchain_images_count;
   printf ("%u images in the swap chain\n", swapchain_images_count);

   VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES] = {VK_NULL_HANDLE,};
   vk.GetSwapchainImagesKHR (objs->device,
                             state->swapchain,
                             &swapchain_images_count,
                             swapchain_images);

   /* destroy previous image views */
   for (uint32_t i = 0; i < old_swapchain_images_count; i++) {
      if (state->image_views[i] != VK_NULL_HANDLE) {
         vk.DestroyImageView (objs->device,
                              state->image_views[i],
                              allocator);
         state->image_views[i] = VK_NULL_HANDLE;
      }
   }

   /* create an image view for each swapchain image */
   VkImageView image_views[8] = {VK_NULL_HANDLE,};
   for (uint32_t i = 0; i < swapchain_images_count; i++) {
      VkImageViewCreateInfo image_view_info = {
         .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
         .image = swapchain_images[i],
         .viewType = VK_IMAGE_VIEW_TYPE_2D,
         .format = config->surface_format.format,
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

      image_views[i] = VK_NULL_HANDLE;
      if (vk.CreateImageView (objs->device,
                              &image_view_info,
                              allocator,
                              &image_views[i]) != VK_SUCCESS) {
         printf ("Error: Failed to create image view\n");
         return false;
      }
      state->image_views[i] = image_views[i];
   }
   printf ("Image views created\n");

   /* create a new renderpass */
   if (state->renderpass != VK_NULL_HANDLE)
      vk.DestroyRenderPass (objs->device, state->renderpass, allocator);
   if (! create_renderpass (objs, config, state))
      return false;

   /* destroy any previous framebuffers */
   for (uint32_t i = 0; i < old_swapchain_images_count; i++) {
      if (state->framebuffers[i] != VK_NULL_HANDLE) {
         vk.DestroyFramebuffer (objs->device,
                                state->framebuffers[i],
                                allocator);
         state->framebuffers[i] = VK_NULL_HANDLE;
      }
   }

   /* create framebuffers for each image view */
   VkFramebuffer framebuffers[MAX_SWAPCHAIN_IMAGES] = {VK_NULL_HANDLE,};
   for (uint32_t i = 0; i < swapchain_images_count; i++) {
      VkImageView attachments[] = {
         state->image_views[i]
      };

      VkFramebufferCreateInfo framebuffer_info = {
         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
         .renderPass = state->renderpass,
         .attachmentCount = 1,
         .pAttachments = attachments,
         .width = swapchain_extent.width,
         .height = swapchain_extent.height,
         .layers = 1
      };

      if (vk.CreateFramebuffer (objs->device,
                                &framebuffer_info,
                                allocator,
                                &framebuffers[i]) != VK_SUCCESS) {
         printf ("Error: Failed to create a framebuffer\n");
         return false;
      }

      state->framebuffers[i] = framebuffers[i];
   }
   printf ("Framebuffers created\n");

   /* destroy any previous pipeline */
   if (state->pipeline != VK_NULL_HANDLE)
      vk.DestroyPipeline (objs->device, state->pipeline, allocator);

   /* create a new pipeline */
   if (! create_pipeline (objs, config, state))
      return false;

   /* free any previous command buffers */
   vk.FreeCommandBuffers (objs->device,
                          objs->cmd_pool,
                          old_swapchain_images_count,
                          state->cmd_buffers);

   /* create new command buffers */
   if (! create_command_buffers (objs, config, state))
      return false;

   return true;
}

static bool
draw_frame (struct vk_objects* objs, struct vk_state* state)
{
   VkResult result;

   /* acquire swapchain's next image */
   uint32_t image_index;
   result = vk.AcquireNextImageKHR (objs->device,
                                    state->swapchain,
                                    1000000,
                                    objs->image_available_semaphore,
                                    VK_NULL_HANDLE,
                                    &image_index);
   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      expose = true;
      return true;
   } else if (result != VK_SUCCESS) {
      printf ("Error: Failed to acquire next image from swap chain\n");
      return false;
   }

   /* submit graphics queue */
   VkSemaphore wait_semaphores[] = {objs->image_available_semaphore};
   VkSemaphore signal_semaphores[] = {objs->render_finished_semaphore};

   VkPipelineStageFlags wait_stages[] =
      {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

   VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = wait_semaphores,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &state->cmd_buffers[image_index],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphores
   };

   if (vk.QueueSubmit (objs->graphics_queue,
                       1,
                       &submit_info,
                       VK_NULL_HANDLE) != VK_SUCCESS) {
      printf ("Error: Failed to submit queue\n");
      return false;
   }

   /* present the frame */
   VkSwapchainKHR swapchains[] = {state->swapchain};
   VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphores,
      .swapchainCount = 1,
      .pSwapchains = swapchains,
      .pImageIndices = &image_index,
      .pResults = NULL
   };

   result =  vk.QueuePresentKHR (objs->graphics_queue, &present_info);
   if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      expose = true;
      return true;
   } else if (result != VK_SUCCESS) {
      printf ("Error: Failed to present the queue\n");
      return false;
   }

   printf ("Frame!\n");

   return true;
}

int
main (int argc, char* argv[])
{
   /* XCB setup */
   /* ======================================================================= */

   /* connection to the X server */
   xcb_conn = xcb_connect (NULL, NULL);
   if (xcb_conn == NULL) {
      printf ("XCB: Error: Failed to connect to X server\n");
      goto free_stuff;
   }
   printf ("XCB: Connected to the X server\n");

   /* Get the first screen */
   const xcb_setup_t* xcb_setup  = xcb_get_setup (xcb_conn);
   xcb_screen_iterator_t iter   = xcb_setup_roots_iterator (xcb_setup);
   xcb_screen = iter.data;

   /* Create the window */
   xcb_win = xcb_generate_id (xcb_conn);

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

   xcb_intern_atom_cookie_t cookie =
      xcb_intern_atom (xcb_conn, 1, 12, "WM_PROTOCOLS");
   xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply (xcb_conn, cookie, 0);

   xcb_intern_atom_cookie_t cookie1 =
      xcb_intern_atom (xcb_conn, 0, 16, "WM_DELETE_WINDOW");
   atom_wm_delete_window = xcb_intern_atom_reply (xcb_conn, cookie1, 0);

   xcb_change_property (xcb_conn,
                        XCB_PROP_MODE_REPLACE,
                        xcb_win,
                        (*reply).atom,
                        4, 32, 1,
                        &(*atom_wm_delete_window).atom);

   free (reply);

   /* Make sure commands are sent before we pause so that the window gets
    * shown.
    */
   xcb_flush (xcb_conn);

   /* Vulkan setup */
   /* ======================================================================= */

   /* load API entry points from ICD */
   GET_ICD_PROC_ADDR (vk, GetInstanceProcAddr);
   GET_PROC_ADDR (vk, EnumerateInstanceLayerProperties);
   GET_PROC_ADDR (vk, EnumerateInstanceExtensionProperties);
   GET_PROC_ADDR (vk, CreateInstance);

   /* enummerate available layers */
   uint32_t layers_count;
   vk.EnumerateInstanceLayerProperties (&layers_count, NULL);
   printf ("Found %u instance layers\n", layers_count);

   if (layers_count > 0) {
      VkLayerProperties available_layers[16];
      vk.EnumerateInstanceLayerProperties (&layers_count, available_layers);
   }

   /* enummerate supported instance extensions */
   VkExtensionProperties ext_props[16];
   uint32_t ext_props_count = 16;
   if (vk.EnumerateInstanceExtensionProperties (NULL,
                                                &ext_props_count,
                                                ext_props) != VK_SUCCESS) {
      printf ("Error: Failed to enummerate instance extension properties\n");
      return -1;
   }
   printf ("Instance extensions: \n");
   for (unsigned i = 0; i < ext_props_count; i++)
      printf ("   %s(%u)\n",
              ext_props[i].extensionName,
              ext_props[i].specVersion);

   /* the memory allocation callbacks (default by now) */
   allocator = VK_NULL_HANDLE;

   /* Vulkan application info */
   VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Vulkan triangle example",
      .applicationVersion = 0,
      .apiVersion = VK_API_VERSION_1_0
   };

   /* create Vulkan instance */
   VkInstance instance = VK_NULL_HANDLE;

   const char* enabled_extensions[2] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_XCB_SURFACE_EXTENSION_NAME
   };

   VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = 2,
      .ppEnabledExtensionNames = enabled_extensions,
   };
   if (vk.CreateInstance (&instance_info,
                          allocator,
                          &instance) != VK_SUCCESS) {
      printf ("Error: Failed to create Vulkan instance\n");
      return -1;
   }
   printf ("Vulkan instance created\n");

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
   if (vk.EnumeratePhysicalDevices (instance,
                                    &num_devices,
                                    devices) != VK_SUCCESS) {
      printf ("Error: Failed to enummerate Vulkan physical devices\n");
      return -1;
   }
   printf ("Found %u physical devices\n", num_devices);
   if (num_devices == 0)
      goto free_stuff;
   VkPhysicalDevice physical_device = devices[0];
   objs.physical_device = physical_device;

   /* physical device properties (informative only) */
   VkPhysicalDeviceProperties physical_device_props;
   vk.GetPhysicalDeviceProperties (physical_device, &physical_device_props);
   printf ("Physical device: %s\n", physical_device_props.deviceName);

   /* query physical device's queue families */
#define NUM_QUEUE_FAMILIES 5
   uint32_t num_queue_families = NUM_QUEUE_FAMILIES;
   VkQueueFamilyProperties queue_families[NUM_QUEUE_FAMILIES] = {0};
   /* get queue families with NULL first, to retrieve count */
   vk.GetPhysicalDeviceQueueFamilyProperties (physical_device,
                                              &num_queue_families,
                                              NULL);
   vk.GetPhysicalDeviceQueueFamilyProperties (devices[0],
                                              &num_queue_families,
                                              queue_families);
   assert (num_queue_families >= 1);
   printf ("Physical device has %u queue families\n", num_queue_families);
   for (unsigned i = 0; i < num_queue_families; i++) {
      printf ("Queue family index: %u, flags: %u, count: %u\n",
              i,
              queue_families[i].queueFlags,
              queue_families[i].queueCount);
   }
   uint32_t queue_family_index = 0;

   /* Enummerate supported device extensions */
   ext_props_count = 16;
   if (vk.EnumerateDeviceExtensionProperties (physical_device,
                                              NULL,
                                              &ext_props_count,
                                              ext_props) != VK_SUCCESS) {
      printf ("Error: Failed to enummerate device extension properties\n");
      return -1;
   }
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
      .window = xcb_win,
   };
   if (vk.CreateXcbSurfaceKHR (instance,
                               &surface_info,
                               allocator,
                               &surface) != VK_SUCCESS) {
      printf ("Error: Failed to create a vulkan surface from an XCB window\n ");
      goto free_stuff;
   }
   objs.surface = surface;
   printf ("Vulkan surface created from the XCB window\n");

   /* check for present support in the selected queue family */
   VkBool32 support_present = VK_FALSE;
   vk.GetPhysicalDeviceSurfaceSupportKHR (physical_device,
                                          queue_family_index,
                                          surface,
                                          &support_present);
   if (support_present) {
      printf ("Queue family supports presentation\n");
   } else {
      printf ("Queue family doesn't support presentation\n");
      goto free_stuff;
   }

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
   if (vk.CreateDevice (devices[0],
                        &device_info,
                        allocator,
                        &device) != VK_SUCCESS) {
      printf ("Error: Failed to create Vulkan device\n");
      goto free_stuff;
   }
   objs.device = device;
   printf ("Logical device created\n");

   /* choose a surface format */
   uint32_t surface_formats_count = 0;
   vk.GetPhysicalDeviceSurfaceFormatsKHR (physical_device,
                                          surface,
                                          &surface_formats_count,
                                          NULL);
   printf ("Found %u surface format(s). Choosing first.\n", surface_formats_count);
   if (surface_formats_count == 0) {
      printf ("Error: No suitable surface format found\n");
      goto free_stuff;
   }
   surface_formats_count = 1;
   VkSurfaceFormatKHR surface_format;
   vk.GetPhysicalDeviceSurfaceFormatsKHR (physical_device,
                                          surface,
                                          &surface_formats_count,
                                          &surface_format);
   config.surface_format = surface_format;

   /* choose a present mode */
   uint32_t present_mode_count = 0;
   vk.GetPhysicalDeviceSurfacePresentModesKHR (physical_device,
                                               surface,
                                               &present_mode_count,
                                               NULL);
   printf ("Found %u present mode(s). Choosing first.\n", present_mode_count);
   if (present_mode_count == 0) {
      printf ("Error: No suitable present modes found\n");
      goto free_stuff;
   }
   present_mode_count = 1;
   VkPresentModeKHR present_mode;
   vk.GetPhysicalDeviceSurfacePresentModesKHR (physical_device,
                                               surface,
                                               &present_mode_count,
                                               &present_mode);
   config.present_mode = present_mode;

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
   if (shader_code == NULL) {
      printf ("Error: Failed to load vertex shader code from 'vert.spv'\n");
      goto free_stuff;
   }

   VkShaderModuleCreateInfo shader_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = shader_code_size,
      .pCode = shader_code,
   };
   VkShaderModule vert_shader_module;
   if (vk.CreateShaderModule (device,
                              &shader_info,
                              allocator,
                              &vert_shader_module) != VK_SUCCESS) {
      printf ("Error: Failed to create vertex shader module\n");
      goto free_stuff;
   }
   free (shader_code);
   printf ("Vertex shader created\n");

   /* create the fragment shader module */
   shader_code = load_file (CURRENT_DIR "/frag.spv",
                            &shader_code_size);
   if (shader_code == NULL) {
      printf ("Error: Failed to load fragment shader code from 'frag.spv'\n");
      goto free_stuff;
   }

   shader_info.codeSize = shader_code_size;
   shader_info.pCode = shader_code;
   VkShaderModule frag_shader_module;
   if (vk.CreateShaderModule (device,
                              &shader_info,
                              allocator,
                              &frag_shader_module) != VK_SUCCESS) {
      printf ("Error: Failed to create fragment shader module\n");
      goto free_stuff;
   }
   free (shader_code);
   printf ("Fragment shader created\n");

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

   objs.shader_stages[0] = vert_stage_info;
   objs.shader_stages[1] = frag_stage_info;

   /* get first device queue */
   VkQueue queue = VK_NULL_HANDLE;
   vk.GetDeviceQueue (device, queue_family_index, 0, &queue);
   if (queue == NULL) {
      printf ("Error: Failed to get a device queue\n");
      goto free_stuff;
   }
   objs.graphics_queue = queue;
   printf ("Got a device queue\n");

   /* create command pool */
   VkCommandPool cmd_pool = VK_NULL_HANDLE;;
   VkCommandPoolCreateInfo cmd_pool_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = 0,
      .queueFamilyIndex = queue_family_index,
   };
   if (vk.CreateCommandPool (device,
                             &cmd_pool_info,
                             allocator,
                             &cmd_pool) != VK_SUCCESS) {
      printf ("Error: Failed to create Vulkan device\n");
      goto free_stuff;
   }
   objs.cmd_pool = cmd_pool;
   printf ("Command pool created\n");

   /* create semaphores */
   VkSemaphore image_available_semaphore = VK_NULL_HANDLE;
   VkSemaphore render_finished_semaphore = VK_NULL_HANDLE;

   VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO  \
   };
   if (vk.CreateSemaphore (device,
                           &semaphore_info,
                           allocator,
                           &image_available_semaphore) != VK_SUCCESS ||
       vk.CreateSemaphore (device,
                           &semaphore_info,
                           allocator,
                           &render_finished_semaphore) != VK_SUCCESS) {
      printf ("Error: Failed to create semaphores\n");
      goto free_stuff;
   }
   objs.image_available_semaphore = image_available_semaphore;
   objs.image_available_semaphore = image_available_semaphore;
   printf ("Semaphores create\n");

   /* create the first swapchain */
   if (! recreate_swapchain (&objs, &config, &state)) {
      printf ("Error: Failed to create a swap chain\n");
      goto free_stuff;
   }

   /* start the show */
   signal (SIGINT, ctrl_c_handler);

   running = true;
   damaged = true;

   /* Map the window onto the screen */
   xcb_map_window (xcb_conn, xcb_win);

   while (running) {
      xcb_generic_event_t* event;

      if (! damaged && ! expose) {
         event = xcb_wait_for_event (xcb_conn);
         if (! wsi_handle_event_xcb (event))
            break;
      }

      if (expose) {
         if (! recreate_swapchain (&objs, &config, &state)) {
            printf ("Error: Failed to create a swap chain\n");
            break;
         }
         expose = false;
         damaged = true;
      }

      if (damaged) {
         if (! draw_frame (&objs, &state))
            break;
         damaged = false;
      }
   }
   printf ("Main-loop ended\n");

 free_stuff:
   /* free all allocated objects, in the right order */

   /* wait for all async ops on device */
   if (device != VK_NULL_HANDLE)
      vk.DeviceWaitIdle (device);

   /* destroy state */

   /* command buffers are implicitly freed when the command pool is
    * destroyed.
    */

   vk.DestroyPipeline (device, state.pipeline, allocator);
   vk.DestroyPipelineLayout (device, state.pipeline_layout, allocator);

   for (uint32_t i = 0; i < state.swapchain_images_count; i++)
      vk.DestroyFramebuffer (device, state.framebuffers[i], allocator);

   for (uint32_t i = 0; i < state.swapchain_images_count; i++)
      vk.DestroyImageView (device, state.image_views[i], allocator);

   vk.DestroyRenderPass (device, state.renderpass, allocator);
   vk.DestroySwapchainKHR (device, state.previous_swapchain, allocator);
   vk.DestroySwapchainKHR (device, state.swapchain, allocator);

   /* destroy immutable objects */
   vk.DestroySemaphore (device, image_available_semaphore, allocator);
   vk.DestroySemaphore (device, render_finished_semaphore, allocator);
   vk.DestroyCommandPool (device, cmd_pool, allocator);
   vk.DestroyShaderModule (device, vert_shader_module, allocator);
   vk.DestroyShaderModule (device, frag_shader_module, allocator);
   vk.DestroyDevice (device, allocator);
   vk.DestroySurfaceKHR (instance, surface, allocator);
   vk.DestroyInstance (instance, allocator);

   /* teardown WSI */

   /* Unmap the window from the screen */
   xcb_unmap_window (xcb_conn, xcb_win);

   free (atom_wm_delete_window);

   /* disconnect from the X server */
   if (xcb_conn != NULL)
      xcb_disconnect (xcb_conn);

   printf ("Clean exit\n");

   return 0;
}
