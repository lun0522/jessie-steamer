//
//  button_util.h
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_UTIL_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_UTIL_H

#include "jessie_steamer/wrapper/vulkan/align.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace button {

enum State { kSelectedState = 0, kUnselectedState, kNumStates };

/* BEGIN: Consistent with uniform blocks defined in shaders. */

constexpr int kNumVerticesPerButton = 6;

struct VerticesInfo {
  ALIGN_VEC4 glm::vec4 pos_tex_coords[kNumVerticesPerButton];
};

/* END: Consistent with uniform blocks defined in shaders. */

// Helps to set the position part of VerticesInfo.
void SetVerticesPositions(const glm::vec2& size_ndc, const glm::vec2& scale,
                          VerticesInfo* info);

// Helps to set the texture coordinate part of VerticesInfo.
void SetVerticesTexCoords(const glm::vec2& center_uv, const glm::vec2& size_uv,
                          VerticesInfo* info);

} /* namespace button */
} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_UTIL_H */
