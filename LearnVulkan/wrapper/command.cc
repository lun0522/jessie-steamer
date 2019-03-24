//
//  command.cc
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "command.h"

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkCommandPool CreateCommandPool(SharedContext context,
                                const Queues::Queue& queue,
                                bool is_transient) {
  // create pool to hold command buffers
  VkCommandPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      queue.family_index,
  };
  if (is_transient) {
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  }

  VkCommandPool pool;
  ASSERT_SUCCESS(vkCreateCommandPool(*context->device(), &pool_info,
                                     context->allocator(), &pool),
                 "Failed to create command pool");
  return pool;
}

VkCommandBuffer CreateCommandBuffer(SharedContext context,
                                    const VkCommandPool& command_pool) {
  // allocate command buffer
  VkCommandBufferAllocateInfo buffer_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      command_pool,
      // secondary level command buffer can be called from primary level
      /*level=*/VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      /*commandBufferCount=*/1,
  };

  VkCommandBuffer buffer;
  ASSERT_SUCCESS(
      vkAllocateCommandBuffers(*context->device(), &buffer_info, &buffer),
      "Failed to allocate command buffer");
  return buffer;
}

vector<VkCommandBuffer> CreateCommandBuffers(SharedContext context,
                                             const VkCommandPool& command_pool,
                                             size_t count) {
  // allocate command buffers
  VkCommandBufferAllocateInfo buffer_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      command_pool,
      // secondary level command buffers can be called from primary level
      /*level=*/VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      /*commandBufferCount=*/static_cast<uint32_t>(count),
  };

  vector<VkCommandBuffer> buffers(count);
  ASSERT_SUCCESS(vkAllocateCommandBuffers(*context->device(), &buffer_info,
                                          buffers.data()),
                 "Failed to allocate command buffers");
  return buffers;
}

} /* namespace */

void command::OneTimeCommand(SharedContext context,
                             const Queues::Queue& queue,
                             const OneTimeRecordCommand& on_record) {
  // construct command pool and buffer
  VkCommandPool command_pool = CreateCommandPool(context, queue, true);
  VkCommandBuffer command_buffer = CreateCommandBuffer(context, command_pool);

  // record command
  VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      /*pNext=*/nullptr,
      /*flags=*/VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      /*pInheritanceInfo=*/nullptr,
  };
  ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffer, &begin_info),
                 "Failed to begin recording command buffer");
  on_record(command_buffer);
  ASSERT_SUCCESS(vkEndCommandBuffer(command_buffer),
                 "Failed to end recording command buffer");

  // submit command buffers, wait until finish and cleanup
  VkSubmitInfo submit_info{
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/0,
      /*pWaitSemaphores=*/nullptr,
      /*pWaitDstStageMask=*/nullptr,
      /*commandBufferCount=*/1,
      &command_buffer,
      /*signalSemaphoreCount=*/0,
      /*pSignalSemaphores=*/nullptr,
  };
  vkQueueSubmit(queue.queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue.queue);
  vkDestroyCommandPool(*context->device(), command_pool, context->allocator());
}

void Command::RecordCommand(const command::MultiTimeRecordCommand& on_record) {
  for (size_t i = 0; i < command_buffers_.size(); ++i) {
    // start command buffer recording
    VkCommandBufferBeginInfo begin_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        /*pNext=*/nullptr,
        /*flags=*/VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        /*pInheritanceInfo=*/nullptr,
        // .pInheritanceInfo sets what to inherit from primary buffers
        // to secondary buffers
    };

    ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffers_[i], &begin_info),
                   "Failed to begin recording command buffer");
    on_record(command_buffers_[i], i);
    ASSERT_SUCCESS(vkEndCommandBuffer(command_buffers_[i]),
                   "Failed to end recording command buffer");
  }
}

VkResult Command::DrawFrame(size_t current_frame,
                            const command::UpdateDataFunc& update_func) {
  // Action  |  Acquire image  | Submit commands |  Present image  |
  // Wait on |        -        | Image available | Render finished |
  // Signal  | Image available | Render finished |        -        |
  //         ^                                   ^
  //   Wait for fence                       Signal fence

  // fence was initialized to signaled state
  // so waiting for it at the beginning is fine
  vkWaitForFences(*context_->device(), 1, &in_flight_fences_[current_frame],
                  VK_TRUE, std::numeric_limits<uint64_t>::max());

  // acquire swapchain image
  uint32_t image_index;
  VkResult acquire_result = vkAcquireNextImageKHR(
      *context_->device(), *context_->swapchain(),
      std::numeric_limits<uint64_t>::max(),
      image_available_semas_[current_frame], VK_NULL_HANDLE, &image_index);
  switch (acquire_result) {
    case VK_ERROR_OUT_OF_DATE_KHR:  // swapchain can no longer present image
      return acquire_result;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:  // may be considered as good state as well
      break;
    default:
      throw std::runtime_error{"Failed to acquire swapchain image"};
  }

  // update per-frame data
  update_func(image_index);

  // we have to wait only if we want to write to color attachment
  // so we actually can start running pipeline long before that image is ready
  VkPipelineStageFlags wait_stages[]{
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };

  VkSubmitInfo submit_info{
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/1,
      &image_available_semas_[current_frame],
      // we specify one stage for each semaphore, so no need to pass count
      wait_stages,
      /*commandBufferCount=*/1,
      &command_buffers_[image_index],
      /*signalSemaphoreCount=*/1,
      &render_finished_semas_[current_frame],
  };

  // reset to fences unsignaled state
  vkResetFences(*context_->device(), 1, &in_flight_fences_[current_frame]);
  ASSERT_SUCCESS(
      vkQueueSubmit(context_->queues().graphics.queue, 1, &submit_info,
                    in_flight_fences_[current_frame]),
      "Failed to submit draw command buffer");

  // present image to screen
  VkPresentInfoKHR present_info{
      VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/1,
      &render_finished_semas_[current_frame],
      /*swapchainCount=*/1,
      &*context_->swapchain(),
      &image_index,  // image for each swapchain
      /*pResults=*/nullptr,
      // may use .pResults to check wether each swapchain rendered successfully
  };

  VkResult present_result = vkQueuePresentKHR(
      context_->queues().present.queue, &present_info);
  switch (present_result) {
    case VK_ERROR_OUT_OF_DATE_KHR:  // swapchain can no longer present image
      return present_result;
      break;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:  // may be considered as good state as well
      break;
    default:
      throw std::runtime_error{"Failed to present swapchain image"};
  }

  return VK_SUCCESS;
}

void Command::Init(SharedContext context,
                   size_t num_frame,
                   const command::MultiTimeRecordCommand& on_record) {
  context_ = context;

  if (is_first_time_) {
    command_pool_ = CreateCommandPool(
        context_, context_->queues().graphics, false);
    image_available_semas_.Init(context_, num_frame);
    render_finished_semas_.Init(context_, num_frame);
    in_flight_fences_.Init(context_, num_frame, true);
    is_first_time_ = false;
  }
  command_buffers_ = CreateCommandBuffers(
      context_, command_pool_, context_->swapchain().size());
  RecordCommand(on_record);
}

void Command::Cleanup() {
  vkFreeCommandBuffers(*context_->device(), command_pool_,
                       CONTAINER_SIZE(command_buffers_),
                       command_buffers_.data());
}

Command::~Command() {
  vkDestroyCommandPool(*context_->device(), command_pool_,
                       context_->allocator());
  // command buffers are implicitly cleaned up with command pool
}

} /* namespace vulkan */
} /* namespace wrapper */
