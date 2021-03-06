//
//  file.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_FILE_H
#define LIGHTER_COMMON_FILE_H

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/common/graphics_api.h"
#include "third_party/absl/types/span.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/glm/glm.hpp"

namespace lighter::common {
namespace file {

// Enables looking up the runtime path of runfiles (i.e. data dependencies of
// Bazel-built binaries and tests). This should be called once with argv[0] in
// main() before accessing any runfiles.
void EnableRunfileLookup(std::string_view arg0);

// Returns the full path to a file or directory in the resource folder.
// Since Bazel only maintains a manifest for file path, in order to get a
// directory path, we should pass in the path to any file in that directory, and
// set 'want_directory_path' to true.
std::string GetResourcePath(std::string_view relative_file_path,
                            bool want_directory_path = false);

// Returns the full path to the shader binary.
std::string GetShaderBinaryPath(std::string_view relative_shader_path,
                                api::GraphicsApi graphics_api);

// Returns the full path to files in the Vulkan SDK folder.
std::string GetVulkanSdkPath(std::string_view relative_path);

}  // namespace file

// Reads raw data from file.
struct RawData {
  explicit RawData(std::string_view path);

  // This class is neither copyable nor movable.
  RawData(const RawData&) = delete;
  RawData& operator=(const RawData&) = delete;

  ~RawData() { delete[] data; }

  // Returns the whole data span, which lives as long as this `RawData` object.
  absl::Span<const char> GetSpan() const { return {data, size}; }

  // Pointer to data.
  const char* data;

  // Data size.
  size_t size;
};

// TODO: Remove this struct and related methods.
// Describes a vertex input attribute.
struct VertexAttribute {
  enum class DataType { kFloat };

  int offset;
  DataType data_type;
  int length;
};

// 2D vertex data, including only position.
struct Vertex2DPosOnly {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2DPosOnly, 6> GetFullScreenSquadVertices();

  // Vertex data.
  glm::vec2 pos;
};

// 2D vertex data, consisting of position and texture coordinates.
struct Vertex2D {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2D, 6> GetFullScreenSquadVertices(bool flip_y);

  // Vertex data.
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

// 3D vertex data, including only position.
struct Vertex3DPosOnly {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
};

// 3D vertex data, consisting of position and color.
struct Vertex3DWithColor {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
  glm::vec3 color;
};

// 3D vertex data, consisting of position, normal and texture coordinates.
struct Vertex3DWithTex {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;
};

namespace file {

// Appends vertex input attributes of DataType to 'attributes'. This is used for
// vector types with floating point values, such as glm::vec3 and glm::vec4.
template <typename DataType>
void AppendVertexAttributes(std::vector<VertexAttribute>& attributes,
                            int offset_bytes) {
  attributes.push_back(
      {offset_bytes, VertexAttribute::DataType::kFloat, DataType::length()});
}

// Appends vertex input attributes of glm::mat4 to 'attributes'.
template <>
void AppendVertexAttributes<glm::mat4>(std::vector<VertexAttribute>& attributes,
                                       int offset_bytes);

// Convenience function to return vertex input attributes for the data that has
// only one attribute of DataType.
template <typename DataType>
std::vector<VertexAttribute> CreateVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  AppendVertexAttributes<DataType>(attributes, /*offset_bytes=*/0);
  return attributes;
}

}  // namespace file

// Loads Wavefront .obj file.
struct ObjFile {
  ObjFile(std::string_view path, int index_base);

  // This class is neither copyable nor movable.
  ObjFile(const ObjFile&) = delete;
  ObjFile& operator=(const ObjFile&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DWithTex> vertices;
};

// Loads Wavefront .obj file but only preserves vertex positions.
struct ObjFilePosOnly {
  ObjFilePosOnly(std::string_view path, int index_base);

  // This class is neither copyable nor movable.
  ObjFilePosOnly(const ObjFilePosOnly&) = delete;
  ObjFilePosOnly& operator=(const ObjFilePosOnly&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DPosOnly> vertices;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_FILE_H
