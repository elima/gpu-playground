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
#include "common/wsi.h"
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 'VK_USE_PLATFORM_X_KHR' currently defined as flag in Makefile */
#include <vulkan/vulkan.h>
#include "common/vk-api.h"

#define WIDTH  640
#define HEIGHT 480

static struct vk_api vk = { NULL, };
static const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;

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

static void
wsi_on_expose (void)
{
   expose = true;
   damaged = true;
}

int32_t
main (int32_t argc, char* argv[])
{
   /* XCB setup */
   /* ======================================================================= */
   wsi_init (NULL, WIDTH, HEIGHT, wsi_on_expose);

   /* Vulkan setup */
   /* ======================================================================= */

   /* load API entry points from ICD */
   vk_api_load_from_icd (&vk);

   /* enummerate available layers */
   uint32_t layers_count;
   printf ("%p\n", &vk);
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
   vk_api_load_from_instance (&vk, &instance);

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
   xcb_window_t* xcb_win = 0;
   xcb_connection_t* xcb_conn = NULL;
   wsi_get_connection_and_window ((const void**) &xcb_conn,
                                  (const void**) &xcb_win);

   VkSurfaceKHR surface = VK_NULL_HANDLE;
   VkXcbSurfaceCreateInfoKHR surface_info = {
      .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
      .connection = xcb_conn,
      .window = *xcb_win,
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
   vk_api_load_from_device (&vk, &device);

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
   expose = true;

   /* Map the window onto the screen */
   wsi_window_show ();

   while (running) {
      if (! damaged && ! expose) {
         if (! wsi_wait_for_events ())
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
   wsi_finish ();

   printf ("Clean exit\n");

   return 0;
}
