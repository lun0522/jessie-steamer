//
//  type_mapping.h
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_TYPE_MAPPING_H
#define LIGHTER_RENDERER_VK_TYPE_MAPPING_H

#include "lighter/renderer/type.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace type {

VkVertexInputRate ConvertVertexInputRate(VertexInputRate rate);

VkFormat ConvertDataFormat(DataFormat format);

VkPrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology);

VkShaderStageFlagBits ConvertShaderStage(shader_stage::ShaderStage stage);

} /* namespace type */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_TYPE_MAPPING_H */