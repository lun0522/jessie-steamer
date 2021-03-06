//
//  basic_object.h
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_BASIC_OBJECT_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_BASIC_OBJECT_H

#include <optional>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class BasicContext;
struct WindowSupport;

// VkAllocationCallbacks is used for allocating space on the host device for
// Vulkan objects. For now this wrapper class simply does nothing.
class HostMemoryAllocator {
 public:
  HostMemoryAllocator() = default;

  // This class is neither copyable nor movable.
  HostMemoryAllocator(const HostMemoryAllocator&) = delete;
  HostMemoryAllocator& operator=(const HostMemoryAllocator&) = delete;

  // Overloads.
  const VkAllocationCallbacks* operator*() const {
    return allocation_callback_;
  }

 private:
  // Used to allocate memory on the host device.
  const VkAllocationCallbacks* allocation_callback_ = nullptr;
};

// Holds queue family indices for the queues we need.
// All queues in one family share the same property.
struct QueueFamilyIndices {
  uint32_t graphics;
  uint32_t compute;
  uint32_t transfer;
  std::optional<uint32_t> present;

  // Returns unique queue family indices. Note that we might be using the same
  // queue for different purposes.
  std::vector<uint32_t> GetUniqueFamilyIndices() const;
};

// VkInstance is used to establish connection with Vulkan library and maintain
// per-application states.
class Instance {
 public:
  // If the window support is requested, 'WindowSupport::create_surface' will be
  // called internally.
  Instance(const BasicContext* context,
           const std::optional<WindowSupport>& window_support);

  // This class is neither copyable nor movable.
  Instance(const Instance&) = delete;
  Instance& operator=(const Instance&) = delete;

  ~Instance();

  // Overloads.
  const VkInstance& operator*() const { return instance_; }

 private:
  // Pointer to context.
  const BasicContext* context_;

  // Opaque instance object.
  VkInstance instance_;
};

// VkPhysicalDevice is a handle to a physical graphics card.
struct PhysicalDevice {
 public:
  // If there is no physical device that satisfies our need, a runtime exception
  // will be thrown.
  PhysicalDevice(const BasicContext* context,
                 const std::optional<WindowSupport>& window_support);

  // This class is neither copyable nor movable.
  PhysicalDevice(const PhysicalDevice&) = delete;
  PhysicalDevice& operator=(const PhysicalDevice&) = delete;

  // Implicitly cleaned up.
  ~PhysicalDevice() = default;

  // Overloads.
  const VkPhysicalDevice& operator*() const { return physical_device_; }

  // Accessors.
  const QueueFamilyIndices& queue_family_indices() const {
    return queue_family_indices_;
  }
  const VkPhysicalDeviceLimits& physical_device_limits() const {
    return physical_device_limits_;
  }

 private:
  // Pointer to context.
  const BasicContext* context_;

  // Opaque physical device object.
  VkPhysicalDevice physical_device_;

  // Family indices for the queues we need.
  QueueFamilyIndices queue_family_indices_;

  // Limits of the physical device.
  VkPhysicalDeviceLimits physical_device_limits_;
};

// VkDevice interfaces with the physical device.
struct Device {
 public:
  Device(const BasicContext* context,
         const std::optional<WindowSupport>& window_support);

  // This class is neither copyable nor movable.
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device();

  // Blocks host until 'device_' becomes idle.
  void WaitIdle() const { vkDeviceWaitIdle(device_); }

  // Overloads.
  const VkDevice& operator*() const { return device_; }

 private:
  // Pointer to context.
  const BasicContext* context_;

  // Opaque device object.
  VkDevice device_;
};

// VkQueue is the queue associated with the logical device.
class Queues {
 public:
  // Holds an opaque queue object and its family index.
  struct Queue {
    VkQueue queue;
    uint32_t family_index;
  };

  Queues(const BasicContext& context,
         const QueueFamilyIndices& family_indices);

  // This class is neither copyable nor movable.
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  // Implicitly cleaned up with physical device.
  ~Queues() = default;

  // Returns an instance of util::QueueUsage that only involves
  // 'graphics_queue_'.
  util::QueueUsage GetGraphicsQueueUsage() const {
    return util::QueueUsage{{graphics_queue_.family_index}};
  }

  // Returns an instance of util::QueueUsage that only involves
  // 'transfer_queue_'.
  util::QueueUsage GetTransferQueueUsage() const {
    return util::QueueUsage{{transfer_queue_.family_index}};
  }

  // Accessors.
  const Queue& graphics_queue() const { return graphics_queue_; }
  const Queue& compute_queue() const { return compute_queue_; }
  const Queue& transfer_queue() const { return transfer_queue_; }
  const Queue& present_queue() const {
    ASSERT_HAS_VALUE(present_queue_, "No presentation queue");
    return present_queue_.value();
  }

 private:
  // Populates 'queue' with the first queue in the family with 'family_index'.
  void SetQueue(const VkDevice& device, uint32_t family_index,
                Queue* queue) const;

  // Graphics queue.
  Queue graphics_queue_;

  // Compute queue.
  Queue compute_queue_;

  // TODO: Both graphics queue and compute queue can do transfer.
  // Transfer queue.
  Queue transfer_queue_;

  // Presentation queue.
  std::optional<Queue> present_queue_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_BASIC_OBJECT_H */
