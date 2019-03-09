//
//  validation.h
//
//  Created by Pujun Lun on 11/30/18.
//  Copyright © 2018 Pujun Lun. All rights reserved.
//

#ifdef DEBUG
#ifndef VULKAN_WRAPPER_VALIDATION_H
#define VULKAN_WRAPPER_VALIDATION_H

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {
namespace wrapper {

class Context;

namespace MessageSeverity {

enum Severity {
  kVerbose  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
  kInfo     = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
  kWarning  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
  kError    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
};

} /* namespace MessageSeverity */

namespace MessageType {

enum Type {
  kGeneral      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
  kValidation   = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
  kPerformance  = VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
};

} /* namespace MessageType */

class DebugCallback {
 public:
  void Init(std::shared_ptr<Context> context,
            VkDebugUtilsMessageSeverityFlagsEXT message_severity,
            VkDebugUtilsMessageTypeFlagsEXT message_type);
  ~DebugCallback();

  // This class is neither copyable nor movable
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

 private:
  std::shared_ptr<Context> context_;
  VkDebugUtilsMessengerEXT callback_;
};

extern const std::vector<const char*> kValidationLayers;
void CheckInstanceExtensionSupport(const std::vector<std::string>& required);
void CheckValidationLayerSupport(const std::vector<std::string>& required);

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_VALIDATION_H */
#endif /* DEBUG */
