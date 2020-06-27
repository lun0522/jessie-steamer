//
//  command.cc
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/command.h"

#include <limits>

#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

constexpr auto kTimeoutForever = std::numeric_limits<uint64_t>::max();

// Creates a command pool on 'queue'. If 'is_transient' is true, the command
// pool is expected to have a short lifetime.
VkCommandPool CreateCommandPool(const BasicContext& context,
                                const Queues::Queue& queue,
                                bool is_transient) {
  const auto flags =
      is_transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
                   : VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  const VkCommandPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      flags,
      queue.family_index,
  };

  VkCommandPool pool;
  ASSERT_SUCCESS(vkCreateCommandPool(*context.device(), &pool_info,
                                     *context.allocator(), &pool),
                 "Failed to create command pool");
  return pool;
}

// Allocates command buffers of 'count' from 'command_pool'.
std::vector<VkCommandBuffer> AllocateCommandBuffers(
    const BasicContext& context,
    const VkCommandPool& command_pool, uint32_t count) {
  const VkCommandBufferAllocateInfo buffer_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      command_pool,
      VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      count,
  };

  std::vector<VkCommandBuffer> buffers(count);
  ASSERT_SUCCESS(vkAllocateCommandBuffers(*context.device(), &buffer_info,
                                          buffers.data()),
                 "Failed to allocate command buffers");
  return buffers;
}

// Uses 'command_buffer' to record commands.
void RecordCommands(
    const VkCommandBuffer& command_buffer,
    VkCommandBufferUsageFlags usage_flags,
    const std::function<void(const VkCommandBuffer&)>& on_record) {
  const VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      /*pNext=*/nullptr,
      usage_flags,
      /*pInheritanceInfo=*/nullptr,
  };
  ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffer, &begin_info),
                 "Failed to begin recording command buffer");
  if (on_record != nullptr) {
    on_record(command_buffer);
  }
  ASSERT_SUCCESS(vkEndCommandBuffer(command_buffer),
                 "Failed to end recording command buffer");
}

// Checks the given 'result'. The return value is the same as
// PerFrameCommand::Run().
absl::optional<VkResult> CheckResult(VkResult result) {
  // VK_ERROR_OUT_OF_DATE_KHR means the swapchain can no longer present image.
  // VK_SUBOPTIMAL_KHR is not ideal, but we would consider it as a good state.
  switch (result) {
    case VK_ERROR_OUT_OF_DATE_KHR:
      return result;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR:
      return absl::nullopt;
    default:
      FATAL(absl::StrFormat("Errno: %d", result));
  }
}

} /* namespace */

OneTimeCommand::OneTimeCommand(SharedBasicContext context,
                               const Queues::Queue* queue)
    : Command{std::move(context)}, queue_{FATAL_IF_NULL(queue)} {
  const auto command_pool = CreateCommandPool(*context_, *queue_,
                                              /*is_transient=*/true);
  set_command_pool(command_pool);
  command_buffer_ =
      AllocateCommandBuffers(*context_, command_pool, /*count=*/1)[0];
}

void OneTimeCommand::Run(const OnRecord& on_record) const {
  RecordCommands(command_buffer_, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                 on_record);
  const VkSubmitInfo submit_info{
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/0,
      /*pWaitSemaphores=*/nullptr,
      /*pWaitDstStageMask=*/nullptr,
      /*commandBufferCount=*/1,
      &command_buffer_,
      /*signalSemaphoreCount=*/0,
      /*pSignalSemaphores=*/nullptr,
  };
  vkQueueSubmit(queue_->queue, /*submitCount=*/1, &submit_info,
                /*fence=*/VK_NULL_HANDLE);
  vkQueueWaitIdle(queue_->queue);
}

PerFrameCommand::PerFrameCommand(const SharedBasicContext& context,
                                 int num_frames_in_flight,
                                 bool has_offscreen_pass)
    : Command{context},
      present_finished_semas_{context, num_frames_in_flight},
      render_finished_semas_{context, num_frames_in_flight},
      in_flight_fences_{context, num_frames_in_flight,
                        /*is_signaled=*/true} {
  const auto command_pool = CreateCommandPool(
      *context_, context_->queues().graphics_queue(), /*is_transient=*/false);
  set_command_pool(command_pool);
  command_buffers_ = AllocateCommandBuffers(
      *context_, command_pool, static_cast<uint32_t>(num_frames_in_flight));

  if (has_offscreen_pass) {
    offscreen_objects_.emplace(
        context_, num_frames_in_flight,
        AllocateCommandBuffers(*context, command_pool,
                               static_cast<uint32_t>(num_frames_in_flight)));
  }
}

absl::optional<VkResult> PerFrameCommand::Run(int current_frame,
                                              const VkSwapchainKHR& swapchain,
                                              const UpdateData& update_data,
                                              const OnscreenOp& onscreen_op,
                                              const OffscreenOp& offscreen_op) {
  // Each "action" may firstly "wait on" a semaphore, then perform the action
  // itself, and finally "signal" another semaphore:
  //   |-----------------------------------------------------------------------------------------|
  //   |  Action  |   Acquire image  | Offscreen commands |  Onscreen commands |  Present image  |
  //   |-----------------------------------------------------------------------------------------|
  //   |  Wait on |         -        |  Present finished  | Offscreen finished | Render finished |
  //   |-----------------------------------------------------------------------------------------|
  //   |  Signal  | Present finished | Offscreen finished |   Render finished  |        -        |
  //   |-----------------------------------------------------------------------------------------|
  //              ^                                                            ^
  //        Wait for fence                                                Signal fence
  // If we don't have the offscreen pass, "onscreen commands" will wait on
  // "present finished" instead.

  // Fences are initialized to the signaled state, hence waiting for them at the
  // beginning is fine.
  const VkDevice& device = *context_->device();
  vkWaitForFences(device, /*fenceCount=*/1, &in_flight_fences_[current_frame],
                  /*waitAll=*/VK_TRUE, kTimeoutForever);

  // Update per-frame data.
  if (update_data != nullptr) {
    update_data(current_frame);
  }

  // Acquire the next available swapchain image.
  uint32_t image_index;
  const auto acquire_result = CheckResult(vkAcquireNextImageKHR(
      device, swapchain, kTimeoutForever,
      present_finished_semas_[current_frame], /*fence=*/VK_NULL_HANDLE,
      &image_index));
  if (acquire_result.has_value()) {
    return acquire_result;
  }

  // Record operations.
  const bool has_offscreen_pass = offscreen_objects_.has_value();
  constexpr VkCommandBufferUsageFlags kUsageFlags =
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  if (has_offscreen_pass) {
    RecordCommands(offscreen_objects_.value().command_buffers[current_frame],
                   kUsageFlags, offscreen_op);
  }
  RecordCommands(
      command_buffers_[current_frame], kUsageFlags,
      [&onscreen_op, image_index](const VkCommandBuffer& command_buffer) {
        onscreen_op(command_buffer, image_index);
      });

  // We can start the pipeline without waiting, until we need to write to the
  // swapchain image, since that image may still being presented on the screen.
  constexpr VkPipelineStageFlags kWaitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  static const auto get_submit_info =
      [kWaitStage](const VkCommandBuffer* command_buffer,
                   const VkSemaphore* wait_semaphore,
                   const VkSemaphore* signal_semaphore) {
        return VkSubmitInfo{
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            /*pNext=*/nullptr,
            /*waitSemaphoreCount=*/1,
            wait_semaphore,
            &kWaitStage,
            /*commandBufferCount=*/1,
            command_buffer,
            /*signalSemaphoreCount=*/1,
            signal_semaphore,
        };
      };

  std::vector<VkSubmitInfo> submit_infos;
  if (has_offscreen_pass) {
    const auto& offscreen_objects = offscreen_objects_.value();
    submit_infos.push_back(get_submit_info(
        &offscreen_objects.command_buffers[current_frame],
        /*wait_semaphore=*/&present_finished_semas_[current_frame],
        /*signal_semaphore=*/&offscreen_objects.semaphores[current_frame]));
    submit_infos.push_back(get_submit_info(
        &command_buffers_[current_frame],
        /*wait_semaphore=*/&offscreen_objects.semaphores[current_frame],
        /*signal_semaphore=*/&render_finished_semas_[current_frame]));
  } else {
    submit_infos.push_back(get_submit_info(
        &command_buffers_[current_frame],
        /*wait_semaphore=*/&present_finished_semas_[current_frame],
        /*signal_semaphore=*/&render_finished_semas_[current_frame]));
  }

  // Reset the fence to the unsignaled state. Note that we don't need to do this
  // for semaphores.
  vkResetFences(device, /*fenceCount=*/1, &in_flight_fences_[current_frame]);
  ASSERT_SUCCESS(vkQueueSubmit(context_->queues().graphics_queue().queue,
                               CONTAINER_SIZE(submit_infos),
                               submit_infos.data(),
                               in_flight_fences_[current_frame]),
                 "Failed to submit command buffer");

  // Present the swapchain image to screen.
  const VkPresentInfoKHR present_info{
      VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/1,
      /*pWaitSemaphores=*/&render_finished_semas_[current_frame],
      /*swapchainCount=*/1,
      &swapchain,
      &image_index,
      // May use 'pResults' to check if each swapchain rendered successfully.
      /*pResults=*/nullptr,
  };
  return CheckResult(vkQueuePresentKHR(
      context_->queues().present_queue().queue, &present_info));
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
