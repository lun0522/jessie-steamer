//
//  buffer_usage.h
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_BUFFER_USAGE_H
#define LIGHTER_RENDERER_BUFFER_USAGE_H

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"

namespace lighter {
namespace renderer {

// Describes how we would use a buffer.
class BufferUsage {
 public:
  // Usage types of buffers that we can handle.
  enum class UsageType {
    // Don't care about the content stored in the buffer.
    kDontCare,
    // Vertex buffer without index data.
    kVertexWithoutIndex,
    // Vertex buffer with index data.
    kVertexWithIndex,
    // Uniform buffer.
    kUniform,
    // Used for transferring data within the device.
    kTransfer,
  };

  // Convenience function to return usage for vertex buffers.
  static BufferUsage GetVertexBufferUsage(bool has_index) {
    return BufferUsage{has_index ? UsageType::kVertexWithIndex
                                 : UsageType::kVertexWithoutIndex,
                       AccessType::kReadOnly, AccessLocation::kVertexShader};
  }

  // Convenience function to return usage for uniform buffers.
  static BufferUsage GetUniformBufferUsage(AccessLocation access_location) {
    ASSERT_TRUE(access_location == AccessLocation::kVertexShader
                    || access_location == AccessLocation::kFragmentShader
                    || access_location == AccessLocation::kComputeShader,
                 "Unexpected access location");
    return BufferUsage{UsageType::kUniform, AccessType::kReadOnly,
                       access_location};
  }

  // Accessors.
  UsageType usage_type() const { return usage_type_; }
  AccessType access_type() const { return access_type_; }
  AccessLocation access_location() const { return access_location_; }

 private:
  // We make this constructor private so that the user can only construct the
  // default usage or use static methods to construct usages that are guaranteed
  // to be valid.
  BufferUsage(UsageType usage_type, AccessType access_type,
              AccessLocation access_location)
  : usage_type_{usage_type}, access_type_{access_type},
    access_location_{access_location} {}

  UsageType usage_type_;
  AccessType access_type_;
  AccessLocation access_location_;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_BUFFER_USAGE_H */
