//
//  spline.cc
//
//  Created by Pujun Lun on 11/30/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/spline.h"

#include <algorithm>

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/gtx/vector_angle.hpp"

namespace lighter::common {

BezierSpline::BezierSpline(int max_recursion_depth,
                           GetMiddlePoint&& get_middle_point,
                           IsSmooth&& is_smooth)
    : max_recursion_depth_{max_recursion_depth},
      get_middle_point_{std::move(FATAL_IF_NULL(get_middle_point))},
      is_smooth_{std::move(FATAL_IF_NULL(is_smooth))} {}

void BezierSpline::Tessellate(const glm::vec3& p0,
                              const glm::vec3& p1,
                              const glm::vec3& p2,
                              const glm::vec3& p3,
                              int recursion_depth) {
  constexpr float kMinDistBetweenPoints = 1E-2;
  if (++recursion_depth == max_recursion_depth_ ||
      glm::distance(p0, p3) < kMinDistBetweenPoints ||
      is_smooth_(p0, p1, p2, p3)) {
    mutable_splines()->push_back(p0);
    return;
  }

  const glm::vec3 p10 = get_middle_point_(p0, p1);
  const glm::vec3 p11 = get_middle_point_(p1, p2);
  const glm::vec3 p12 = get_middle_point_(p2, p3);
  const glm::vec3 p20 = get_middle_point_(p10, p11);
  const glm::vec3 p21 = get_middle_point_(p11, p12);
  const glm::vec3 p30 = get_middle_point_(p20, p21);
  Tessellate(p0, p10, p20, p30, recursion_depth);
  Tessellate(p30, p21, p12, p3, recursion_depth);
}

const int CatmullRomSpline::kMinNumControlPoints = 3;

std::unique_ptr<Spline> CatmullRomSpline::GetOnSphereSpline(
    int max_recursion_depth, float roughness) {
  BezierSpline::GetMiddlePoint get_middle_point =
      [](const glm::vec3& p0, const glm::vec3& p1) {
        return glm::normalize(p0 + p1) * glm::length(p0);
      };

  BezierSpline::IsSmooth is_smooth = [roughness](const glm::vec3& p0,
                                                 const glm::vec3& p1,
                                                 const glm::vec3& p2,
                                                 const glm::vec3& p3) {
    const glm::vec3 p0p1 = glm::normalize(p0 - p1);
    const glm::vec3 p1p2 = glm::normalize(p1 - p2);
    const glm::vec3 p2p3 = glm::normalize(p2 - p3);
    return glm::angle(p0p1, p1p2) <= roughness &&
           glm::angle(p1p2, p2p3) <= roughness;
  };

  return std::make_unique<CatmullRomSpline>(
      max_recursion_depth, std::move(get_middle_point), std::move(is_smooth));
}

void CatmullRomSpline::Tessellate(const glm::vec3& p0,
                                  const glm::vec3& p1,
                                  const glm::vec3& p2,
                                  const glm::vec3& p3) {
  static const glm::mat4* catmull_rom_to_bezier = nullptr;
  if (catmull_rom_to_bezier == nullptr) {
    const glm::mat4 catmull_rom_coeff{
        -0.5f,  1.5f, -1.5f,  0.5f,
         1.0f, -2.5f,  2.0f, -0.5f,
        -0.5f,  0.0f,  0.5f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f
    };
    const glm::mat4 bezier_coeff{
        -1.0f,  3.0f, -3.0f,  1.0f,
         3.0f, -6.0f,  3.0f,  0.0f,
        -3.0f,  3.0f,  0.0f,  0.0f,
         1.0f,  0.0f,  0.0f,  0.0f,
    };
    catmull_rom_to_bezier =
        new glm::mat4{catmull_rom_coeff * glm::inverse(bezier_coeff)};
  }
  const glm::mat4 catmull_rom_points{
      glm::vec4{p0, 0.0f},
      glm::vec4{p1, 0.0f},
      glm::vec4{p2, 0.0f},
      glm::vec4{p3, 0.0f},
  };
  const glm::mat4 bezier_points = catmull_rom_points * (*catmull_rom_to_bezier);
  BezierSpline::Tessellate(bezier_points[0],
                           bezier_points[1],
                           bezier_points[2],
                           bezier_points[3],
                           /*recursion_depth=*/0);
}

void CatmullRomSpline::BuildSpline(absl::Span<const glm::vec3> control_points) {
  const auto num_control_points = static_cast<int>(control_points.size());
  ASSERT_TRUE(num_control_points >= kMinNumControlPoints,
              absl::StrFormat(
                  "Must have at least %d control points, while %d provided",
                  kMinNumControlPoints, num_control_points));

  mutable_splines()->clear();
  for (int i = 0; i < num_control_points; ++i) {
    Tessellate(control_points[(i + 0) % num_control_points],
               control_points[(i + 1) % num_control_points],
               control_points[(i + 2) % num_control_points],
               control_points[(i + 3) % num_control_points]);
  }
  // Close the spline.
  mutable_splines()->push_back(spline_points()[0]);
}

SplineEditor::SplineEditor(int min_num_control_points,
                           int max_num_control_points,
                           std::vector<glm::vec3>&& initial_control_points,
                           std::unique_ptr<Spline>&& spline)
    : min_num_control_points_{min_num_control_points},
      max_num_control_points_{max_num_control_points},
      control_points_{std::move(initial_control_points)},
      spline_{std::move(spline)} {
  RebuildSpline();
}

bool SplineEditor::CanInsertControlPoint() const {
  return control_points_.size() != max_num_control_points_;
}

bool SplineEditor::InsertControlPoint(int index, const glm::vec3& position) {
  if (!CanInsertControlPoint()) {
    return false;
  }

  control_points_.insert(control_points_.begin() + index, position);
  RebuildSpline();
  return true;
}

void SplineEditor::UpdateControlPoint(int index, const glm::vec3& position) {
  control_points_.at(index) = position;
  RebuildSpline();
}

bool SplineEditor::RemoveControlPoint(int index) {
  if (control_points_.size() == min_num_control_points_) {
    return false;
  }

  control_points_.erase(control_points_.begin() + index);
  RebuildSpline();
  return true;
}

void SplineEditor::RebuildSpline() {
  spline_->BuildSpline(control_points_);
}

}  // namespace lighter::common
