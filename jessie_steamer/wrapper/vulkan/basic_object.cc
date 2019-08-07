//
//  basic_object.cc
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/basic_object.h"

#include <iostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#ifndef NDEBUG
#include "jessie_steamer/wrapper/vulkan/validation.h"
#endif /* !NDEBUG */

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

using std::vector;

// Returns whether swapchain is supported.
bool HasSwapchainSupport(const VkPhysicalDevice& physical_device,
                         const WindowSupport& window_support) {
  std::cout << "Checking extension support required for swapchain..."
            << std::endl << std::endl;

  // Query support for device extensions.
  const vector<std::string> required{
      window_support.swapchain_extensions.begin(),
      window_support.swapchain_extensions.end(),
  };
  const auto extensions{QueryAttribute<VkExtensionProperties>(
      [&physical_device](uint32_t* count, VkExtensionProperties* properties) {
        return vkEnumerateDeviceExtensionProperties(
            physical_device, nullptr, count, properties);
      }
  )};
  const auto get_name = [](const VkExtensionProperties& property) {
    return property.extensionName;
  };
  const auto unsupported = FindUnsupported<VkExtensionProperties>(
      required, extensions, get_name);

  if (unsupported.has_value()) {
    std::cout << "Unsupported: " << unsupported.value() << std::endl;
    return false;
  }

  // Physical device may support swapchain but maybe not compatible with the
  // window system, so we need to query details.
  uint32_t format_count, mode_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, *window_support.surface, &format_count, nullptr);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, *window_support.surface, &mode_count, nullptr);
  return format_count && mode_count;
}

// Finds family indices of queues we need. If any queue is not found in the
// given 'physical_device', returns absl::nullopt.
// The graphics queue will also be used as transfer queue.
absl::optional<Queues::FamilyIndices> FindDeviceQueues(
    const VkPhysicalDevice& physical_device,
    const absl::optional<WindowSupport>& window_support) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  std::cout << "Found device: " << properties.deviceName
            << std::endl << std::endl;

  // Request swapchain support if use window.
  if (window_support.has_value() &&
      !HasSwapchainSupport(physical_device, window_support.value())) {
    return absl::nullopt;
  }

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return absl::nullopt;
  }

  // Find queue family that holds graphics queue.
  Queues::FamilyIndices candidate{};
  const auto families{QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  )};
  const auto has_graphics_support = [](const VkQueueFamilyProperties& family) {
    return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  };
  const auto graphics_queue_index =
      util::FindIndexOfFirst<VkQueueFamilyProperties>(
          families, has_graphics_support);
  if (graphics_queue_index == absl::nullopt) {
    return absl::nullopt;
  } else {
    candidate.graphics = candidate.transfer =
        static_cast<uint32_t>(graphics_queue_index.value());
  }

  // Find queue family that holds presentation queue if use window.
  if (window_support.has_value()) {
    uint32_t index = 0;
    const auto has_present_support = [&physical_device, &window_support, index]
        (const VkQueueFamilyProperties& family) mutable {
      VkBool32 support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, index++, *window_support.value().surface, &support);
      return support;
    };
    const auto present_queue_index =
        util::FindIndexOfFirst<VkQueueFamilyProperties>(
            families, has_present_support);
    if (present_queue_index == absl::nullopt) {
      return absl::nullopt;
    } else {
      candidate.present = static_cast<uint32_t>(present_queue_index.value());
    }
  }

  return candidate;
}

// Returns unique queue family indices given 'family_indices'.
// We might be using the same queue for different purposes, so we use a hash set
// to remove duplicates.
absl::flat_hash_set<uint32_t> GetUniqueFamilyIndices(
    const Queues::FamilyIndices& family_indices) {
  absl::flat_hash_set<uint32_t> unique_indices{family_indices.graphics,
                                               family_indices.transfer};
  if (family_indices.present.has_value()) {
    unique_indices.emplace(family_indices.present.value());
  }
  return unique_indices;
}

} /* namespace */

Queues::Queues(const VkDevice& device, const FamilyIndices& family_indices)
    : unique_family_indices_{GetUniqueFamilyIndices(family_indices)} {
  SetQueue(device, family_indices.graphics, &graphics_queue_);
  SetQueue(device, family_indices.transfer, &transfer_queue_);
  if (family_indices.present.has_value()) {
    present_queue_.emplace();
    SetQueue(device, family_indices.present.value(), &present_queue_.value());
  }
}

void Queues::SetQueue(const VkDevice& device, uint32_t family_index,
                      Queue* queue) const {
  constexpr int kQueueIndex = 0;
  queue->family_index = family_index;
  vkGetDeviceQueue(device, family_index, kQueueIndex, &queue->queue);
}

void Instance::Init(SharedBasicContext context,
                    const absl::optional<WindowSupport>& window_support) {
  context_ = std::move(context);

  // Request support for pushing descriptors.
  vector<const char*> instance_extensions{
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };
  // Request support for window if necessary.
  if (window_support.has_value()) {
    instance_extensions.insert(
        instance_extensions.end(),
        window_support.value().window_extensions.begin(),
        window_support.value().window_extensions.end()
    );
  }
#ifndef NDEBUG
  // Request support for debug reports.
  instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  // Make sure we have support for extensions and layers for validation.
  validation::EnsureInstanceExtensionSupport({
      instance_extensions.begin(),
      instance_extensions.end()
  });
  validation::EnsureValidationLayerSupport({
      validation::layers().begin(),
      validation::layers().end()
  });
#endif /* !NDEBUG */

  // [optional]
  // Might be useful for the driver to optimize for some graphics engine.
  const VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      /*pNext=*/nullptr,
      /*pApplicationName=*/"Vulkan Application",
      /*applicationVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*pEngineName=*/"No Engine",
      /*engineVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*apiVersion=*/VK_API_VERSION_1_0,
  };

  // [required]
  // Specify which global extensions and validation layers to use.
  const VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      &app_info,
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(instance_extensions),
      instance_extensions.data(),
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, context_->allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, context_->allocator());
}

Queues::FamilyIndices PhysicalDevice::Init(
    SharedBasicContext context,
    const absl::optional<WindowSupport>& window_support) {
  context_ = std::move(context);

  // Find all physical devices.
  const auto physical_devices{QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_->instance(), count, physical_device);
      }
  )};

  // Find a suitable device. If window support is requested, also request for
  // the swapchain and presentation queue here.
  for (const auto& candidate : physical_devices) {
    const auto indices = FindDeviceQueues(candidate, window_support);
    if (indices.has_value()) {
      physical_device_ = candidate;

      // Query device limits.
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      limits_ = properties.limits;

      return indices.value();
    }
  }
  FATAL("Failed to find suitable graphics device");
}

std::unique_ptr<Queues> Device::Init(
    SharedBasicContext context,
    const Queues::FamilyIndices& queue_family_indices,
    const absl::optional<WindowSupport>& window_support) {
  if (window_support != absl::nullopt) {
    ASSERT_HAS_VALUE(queue_family_indices.present,
                     "Presentation queue is not properly set up");
  }

  context_ = std::move(context);

  // Request support for anisotropy filtering.
  VkPhysicalDeviceFeatures required_features{};
  required_features.samplerAnisotropy = VK_TRUE;

  // Request support for negative-height viewport.
  vector<const char*> device_extensions{
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
      VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
  };
  // Request support for window if necessary.
  if (window_support.has_value()) {
    device_extensions.insert(
        device_extensions.end(),
        window_support.value().swapchain_extensions.begin(),
        window_support.value().swapchain_extensions.end());
  }

  // Specify which queue we want to use.
  vector<VkDeviceQueueCreateInfo> queue_infos;
  // 'priority' is always required even if there is only one queue.
  constexpr float kPriority = 1.0f;
  for (uint32_t family_index : GetUniqueFamilyIndices(queue_family_indices)) {
    queue_infos.emplace_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        family_index,
        /*queueCount=*/1,
        &kPriority,
    });
  }

  const VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(device_extensions),
      device_extensions.data(),
      &required_features,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context_->physical_device(), &device_info,
                                context_->allocator(), &device_),
                 "Failed to create logical device");

  return absl::make_unique<Queues>(device_, queue_family_indices);
}

Device::~Device() {
  vkDestroyDevice(device_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
