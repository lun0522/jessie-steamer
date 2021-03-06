//
//  validation.cc
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/validation.h"

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Returns a callback that simply prints the error reason.
VKAPI_ATTR VkBool32 VKAPI_CALL UserCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data) {
  LOG_INFO << callback_data->pMessage;
  return VK_FALSE;
}

} /* namespace */

namespace validation {

const std::vector<const char*>& GetRequiredLayers() {
  static const auto* validation_layers = new std::vector<const char*>{
      "VK_LAYER_KHRONOS_validation",
  };
  return *validation_layers;
}

} /* namespace validation */

DebugCallback::DebugCallback(const BasicContext* context,
                             const TriggerCondition& trigger_condition)
    : context_{FATAL_IF_NULL(context)} {
  // We may pass data to 'pUserData' which can be retrieved from the callback.
  const VkDebugUtilsMessengerCreateInfoEXT create_info{
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      trigger_condition.severity,
      trigger_condition.type,
      UserCallback,
      /*pUserData=*/nullptr,
  };
  const auto vkCreateDebugUtilsMessengerEXT =
      util::LoadInstanceFunction<PFN_vkCreateDebugUtilsMessengerEXT>(
          *context_->instance(), "vkCreateDebugUtilsMessengerEXT");
  vkCreateDebugUtilsMessengerEXT(*context_->instance(), &create_info,
                                 *context_->allocator(), &callback_);
}

DebugCallback::~DebugCallback() {
  const auto vkDestroyDebugUtilsMessengerEXT =
      util::LoadInstanceFunction<PFN_vkDestroyDebugUtilsMessengerEXT>(
          *context_->instance(), "vkDestroyDebugUtilsMessengerEXT");
  vkDestroyDebugUtilsMessengerEXT(*context_->instance(), callback_,
                                  *context_->allocator());
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
