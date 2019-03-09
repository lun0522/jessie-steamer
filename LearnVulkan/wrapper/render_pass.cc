//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "render_pass.h"

#include "context.h"
#include "util.h"

using std::vector;

namespace vulkan {
namespace wrapper {
namespace {

vector<VkFramebuffer> CreateFramebuffers(
    VkExtent2D image_extent,
    const vector<VkImageView>& image_views,
    const VkDevice& device,
    const VkRenderPass& render_pass,
    const VkAllocationCallbacks* allocator) {
  vector<VkFramebuffer> framebuffers(image_views.size());
  for (size_t i = 0; i < image_views.size(); ++i) {
    VkFramebufferCreateInfo framebuffer_info{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &image_views[i],
        .width = image_extent.width,
        .height = image_extent.height,
        .layers = 1,
    };

    ASSERT_SUCCESS(vkCreateFramebuffer(
                       device, &framebuffer_info, allocator, &framebuffers[i]),
                   "Failed to create framebuffer");
  }
  return framebuffers;
}

} /* namespace */

void RenderPass::Init(std::shared_ptr<Context> context) {
  context_ = context;

  VkAttachmentDescription color_att_desc{
      .format = context_->swapchain().format(),
      .samples = VK_SAMPLE_COUNT_1_BIT, // no multisampling
      // .loadOp and .storeOp affect color and depth buffers
      // .loadOp options: LOAD / CLEAR / DONT_CARE
      // .storeOp options: STORE / DONT_STORE
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      // layout of pixels in memory. commonly used options:
      //   - VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: for color attachment
      //   - VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: for images in swap chain
      //   - VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: for images as dstination
      //                                           for memory copy
      //   - VK_IMAGE_LAYOUT_UNDEFINED: don't care about layout before this
      //                                render pass
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_att_ref{
      .attachment = 0, // index of attachment to reference to
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass_desc{
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      // layout (location = 0) will be rendered to the first attachement
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_att_ref,
  };

  // render pass takes care of layout transition, so it has to wait until
  // image is ready. VK_SUBPASS_EXTERNAL means subpass before (if .srcSubpass)
  // or after (if .dstSubpass) render pass
  VkSubpassDependency subpass_dep{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0, // refer to our subpass
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                           | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo render_pass_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_att_desc,
      .subpassCount = 1,
      .pSubpasses = &subpass_desc,
      .dependencyCount = 1,
      .pDependencies = &subpass_dep,
  };

  ASSERT_SUCCESS(vkCreateRenderPass(*context_->device(), &render_pass_info,
                                    context_->allocator(), &render_pass_),
                 "Failed to create render pass");

  framebuffers_ = CreateFramebuffers(
      context_->swapchain().extent(), context_->swapchain().image_views(),
      *context_->device(), render_pass_, context_->allocator());
}

void RenderPass::Cleanup() {
  for (const auto& framebuffer : framebuffers_)
    vkDestroyFramebuffer(*context_->device(), framebuffer,
                         context_->allocator());
  vkDestroyRenderPass(*context_->device(), render_pass_, context_->allocator());
}

} /* namespace wrapper */
} /* namespace vulkan */