//
//  pipeline_util.h
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace pipeline {

/* Color blend */

// Returns a viewport transform targeting the full frame of 'frame_size'.
GraphicsPipelineBuilder::ViewportInfo GetFullFrameViewport(
    const VkExtent2D& frame_size);

// Returns a viewport transform that keeps the aspect ratio of objects
// unchanged, and fills the frame as much as possible.
GraphicsPipelineBuilder::ViewportInfo GetViewport(const VkExtent2D& frame_size,
                                                  float aspect_ratio);

// Returns the color blend state that gives:
//   C = Cs * As + Cd * (1. - As)
//   A = 1. * As + Ad * (1. - As)
// Where: C - color, A - alpha, s - source, d - destination.
VkPipelineColorBlendAttachmentState GetColorBlendState(bool enable_blend);

/* Vertex input binding */

// Returns how to interpret the vertex data. Note that the 'binding' field of
// the returned value will not be set, since it will be assigned in pipeline.
VkVertexInputBindingDescription GetBindingDescription(uint32_t stride,
                                                      bool instancing);

// Convenient function to return VkVertexInputBindingDescription, assuming
// each vertex will get data of DataType, which is updated per-vertex.
template <typename DataType>
inline VkVertexInputBindingDescription GetPerVertexBindingDescription() {
  return GetBindingDescription(sizeof(DataType), /*instancing=*/false);
}

// Convenient function to return VkVertexInputBindingDescription, assuming
// each vertex will get data of DataType, which is updated per-instance.
template <typename DataType>
inline VkVertexInputBindingDescription GetPerInstanceBindingDescription() {
  return GetBindingDescription(sizeof(DataType), /*instancing=*/true);
}

/* Vertex input attribute */

// Convenient function to return a list of VertexBuffer::Attribute, assuming
// each vertex will get data of DataType. For now this is only implemented for
// Vertex2D, Vertex3DPosOnly, Vertex3DWithColor and Vertex3DWithTex.
template <typename DataType>
std::vector<VertexBuffer::Attribute> GetVertexAttribute();

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_PIPELINE_UTIL_H */
