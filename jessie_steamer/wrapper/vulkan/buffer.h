//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// This is the base class of all buffer classes. The user should use it through
// derived classes. Since all buffers need VkDeviceMemory, which is the handle
// to the data stored in the device memory, it will be held and destroyed by
// this base class, and initialized by derived classes.
class Buffer {
 public:
  // Information we need to copy one chunk of memory from host to device.
  // We assume all the data will be copied to one big chunk of device memory,
  // although they might be stored in different places on the host.
  // We will read data of 'size' bytes starting from 'data' pointer on the host,
  // and then copy to the device memory at 'offset'.
  struct CopyInfo {
    const void* data;
    VkDeviceSize size;
    VkDeviceSize offset;
  };

  // Information we need to copy multiple chunks of memory from host to device.
  struct CopyInfos {
    VkDeviceSize total_size;
    std::vector<CopyInfo> copy_infos;
  };

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  virtual ~Buffer() {
    vkFreeMemory(*context_->device(), device_memory_, *context_->allocator());
  }

 protected:
  explicit Buffer(SharedBasicContext context) : context_{std::move(context)} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque device memory object.
  VkDeviceMemory device_memory_;
};

// This is the base class of the buffers that are used to store one dimensional
// data. The user should use it through derived classes. Since all buffers of
// this kind need VkBuffer, which configures the usage of the data, it will be
// held and destroyed by this base class, and initialized by derived classes.
class DataBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  ~DataBuffer() override {
    vkDestroyBuffer(*context_->device(), buffer_, *context_->allocator());
  }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  // Opaque buffer object.
  VkBuffer buffer_;
};

// This is the base class of vertex buffers, and provides shared utility
// functions. The user should use it through derived classes.
class VertexBuffer : public DataBuffer {
 public:
  // Vertex input attribute.
  struct Attribute {
    uint32_t offset;
    VkFormat format;
  };

  // This class is neither copyable nor movable.
  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

  ~VertexBuffer() override = default;

  // Returns attributes of the vertex data stored in this buffer.
  // The 'location' field of attributes will start from 'start_location'.
  // For flexibility, the 'binding' field will not be set.
  std::vector<VkVertexInputAttributeDescription>
  GetAttributes(uint32_t start_location) const;

  // Renders vertices without using per-vertex buffer.
  // This should be called when 'command_buffer' is recording commands.
  static void DrawWithoutBuffer(const VkCommandBuffer& command_buffer,
                                uint32_t vertex_count, uint32_t instance_count);

 protected:
  friend class DynamicBuffer;

  VertexBuffer(SharedBasicContext context, std::vector<Attribute>&& attributes)
      : DataBuffer{std::move(context)}, attributes_{std::move(attributes)} {}

  // Initializes 'device_memory_' and 'buffer_'.
  // For more efficient memory access, indices and vertices data are put in the
  // same buffer, hence only total size is needed.
  // If 'is_dynamic' is true, the buffer will be visible to the host, which can
  // be used for dynamic text rendering. Otherwise, the buffer will be only
  // visible to the device, and we will use staging buffers to transfer data.
  void CreateBufferAndMemory(VkDeviceSize total_size, bool is_dynamic);

  // Attributes of the vertex data stored in this buffer.
  const std::vector<Attribute> attributes_;
};

// This class is a plugin to make a buffer dynamic, i.e., be able to recreate
// the buffer when Reserve() is called with a larger buffer size. The user
// should use it through derived classes.
class DynamicBuffer {
 public:
  // This class is neither copyable nor movable.
  DynamicBuffer(const DynamicBuffer&) = delete;
  DynamicBuffer& operator=(const DynamicBuffer&) = delete;

  ~DynamicBuffer() = default;

 protected:
  DynamicBuffer(size_t initial_size, VertexBuffer* vertex_buffer);

  // Reserves space of the given 'size'. If 'size' is less than the current
  // 'buffer_size_', this will be no-op.
  void Reserve(size_t size);

  // Accessors.
  VkDeviceSize buffer_size() const { return buffer_size_; }

 private:
  // Pointer to the vertex buffer whose buffer and device memory will be managed
  // bt this class.
  VertexBuffer* vertex_buffer_;

  // Tracks the current size of buffer. This is initialized to be 0 so that we
  // will not try to deallocate an uninitialized buffer in 'Reserve()'.
  VkDeviceSize buffer_size_ = 0;
};

// This is the base class of buffers storing per-vertex data. The user should
// use it through derived classes.
class PerVertexBuffer : public VertexBuffer {
 public:
  // Used to Interpret the vertex data stored in containers.
  struct VertexDataInfo {
    // Assuming the data in 'container' is used for multiple meshes.
    template <typename Container>
    VertexDataInfo(const Container& container, int num_units_per_mesh)
        : data{container.data()},
          num_units_per_mesh{num_units_per_mesh},
          size_per_mesh{sizeof(container[0]) * num_units_per_mesh} {}

    // Assuming all the data in 'container' is used for one mesh.
    template <typename Container>
    explicit VertexDataInfo(const Container& container)
        : VertexDataInfo{
              container,
              /*num_units_per_mesh=*/static_cast<int>(container.size())} {}

    const void* data;
    int num_units_per_mesh;
    size_t size_per_mesh;
  };

  // Holds the number of vertices in each mesh and the data size offset within
  // the vertex buffer.
  struct MeshDataInfosNoIndices {
    struct Info {
      uint32_t vertices_count;
      VkDeviceSize vertices_offset;
    };
    std::vector<Info> infos;
  };

  // Holds the number of indices in each mesh and the data size offset within
  // the vertex buffer.
  struct MeshDataInfosWithIndices {
    struct Info {
      uint32_t indices_count;
      VkDeviceSize indices_offset;
      VkDeviceSize vertices_offset;
    };
    std::vector<Info> infos;
  };

  using MeshDataInfos = absl::variant<MeshDataInfosNoIndices,
                                      MeshDataInfosWithIndices>;

  // Interface of different forms of buffer data info.
  class BufferDataInfo {
   public:
    virtual ~BufferDataInfo() = default;

    // Populates 'mesh_data_infos_' of 'buffer' and returns an instance of
    // CopyInfos that can be used for copying data from the host to device.
    virtual CopyInfos CreateCopyInfos(PerVertexBuffer* buffer) const = 0;
  };

  // Holds data information for multiple meshes that do not have indices.
  // Each mesh may have different number of vertices.
  class NoIndicesDataInfo : public BufferDataInfo {
   public:
    explicit NoIndicesDataInfo(std::vector<VertexDataInfo>&& per_mesh_vertices)
        : per_mesh_vertices_{std::move(per_mesh_vertices)} {}

    // Overrides.
    CopyInfos CreateCopyInfos(PerVertexBuffer* buffer) const override;

   private:
    const std::vector<VertexDataInfo> per_mesh_vertices_;
  };

  // Holds data information for multiple meshes that share indices.
  // Each mesh has the same number of vertices.
  class ShareIndicesDataInfo : public BufferDataInfo {
   public:
    ShareIndicesDataInfo(int num_meshes,
                         const VertexDataInfo& per_mesh_vertices,
                         const VertexDataInfo& shared_indices)
        : num_meshes_{num_meshes},
          per_mesh_vertices_{per_mesh_vertices},
          shared_indices_{shared_indices} {}

    // Overrides.
    CopyInfos CreateCopyInfos(PerVertexBuffer* buffer) const override;

   private:
    const int num_meshes_;
    const VertexDataInfo per_mesh_vertices_;
    const VertexDataInfo shared_indices_;
  };

  // Holds data information for multiple meshes that do not share indices.
  // Each mesh may have different number of indices and vertices.
  class NoShareIndicesDataInfo : public BufferDataInfo {
   public:
    // Holds data information for each mesh.
    struct PerMeshInfo {
      VertexDataInfo indices;
      VertexDataInfo vertices;
    };

    explicit NoShareIndicesDataInfo(std::vector<PerMeshInfo>&& per_mesh_infos)
        : per_mesh_infos_{std::move(per_mesh_infos)} {}

    // Overrides.
    CopyInfos CreateCopyInfos(PerVertexBuffer* buffer) const override;

   private:
    const std::vector<PerMeshInfo> per_mesh_infos_;
  };

  // This class is neither copyable nor movable.
  PerVertexBuffer(const PerVertexBuffer&) = delete;
  PerVertexBuffer& operator=(const PerVertexBuffer&) = delete;

  // Renders one mesh with 'mesh_index' for 'instance_count' times.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, uint32_t binding_point,
            int mesh_index, uint32_t instance_count) const;

 protected:
  // Inherits constructor.
  using VertexBuffer::VertexBuffer;

  // Holds data information for all meshes stored in the vertex buffer.
  MeshDataInfos mesh_data_infos_;
};

// This class creates a vertex buffer that stores static data, which will be
// transferred to the device via the staging buffer.
class StaticPerVertexBuffer : public PerVertexBuffer {
 public:
  StaticPerVertexBuffer(SharedBasicContext context, const BufferDataInfo& info,
                        std::vector<Attribute>&& attributes);

  // This class is neither copyable nor movable.
  StaticPerVertexBuffer(const StaticPerVertexBuffer&) = delete;
  StaticPerVertexBuffer& operator=(const StaticPerVertexBuffer&) = delete;
};

// This class creates a vertex buffer that stores dynamic data, and is able to
// re-allocate a larger buffer internally if necessary.
// If possible, the user should compute the maximum required space of this
// buffer, and pass it as 'initial_size' to the constructor, so that we can
// pre-allocate enough space and always reuse the same chunk of device memory.
// Since the data is dynamic, this buffer will be visible to the host, and we
// don't use the staging buffer.
class DynamicPerVertexBuffer : public PerVertexBuffer, public DynamicBuffer {
 public:
  // 'initial_size' should be greater than 0.
  DynamicPerVertexBuffer(SharedBasicContext context, size_t initial_size,
                         std::vector<Attribute>&& attributes)
      : PerVertexBuffer{std::move(context), std::move(attributes)},
        DynamicBuffer{initial_size, this} {}

  // This class is neither copyable nor movable.
  DynamicPerVertexBuffer(const DynamicPerVertexBuffer&) = delete;
  DynamicPerVertexBuffer& operator=(const DynamicPerVertexBuffer&) = delete;

  // Copies host data to device. If the device buffer allocated previously is
  // not large enough to hold the new data, it will be recreated internally.
  void CopyHostData(const BufferDataInfo& info);
};

// This is the base class of buffers storing per-instance data. The user should
// use it through derived classes.
class PerInstanceBuffer : public VertexBuffer {
 public:
  // This class is neither copyable nor movable.
  PerInstanceBuffer(const PerInstanceBuffer&) = delete;
  PerInstanceBuffer& operator=(const PerInstanceBuffer&) = delete;

  // Binds vertex data to the given 'binding_point'.
  void Bind(const VkCommandBuffer& command_buffer,
            uint32_t binding_point) const;

  // Accessors.
  uint32_t per_instance_data_size() const { return per_instance_data_size_; }

 protected:
  PerInstanceBuffer(SharedBasicContext context,
                    uint32_t per_instance_data_size,
                    std::vector<Attribute>&& attributes)
      : VertexBuffer{std::move(context), std::move(attributes)},
        per_instance_data_size_{per_instance_data_size} {}

 private:
  // Per-instance data size.
  const uint32_t per_instance_data_size_;
};

// This class creates a vertex buffer that stores static data, which will be
// transferred to the device via the staging buffer.
class StaticPerInstanceBuffer : public PerInstanceBuffer {
 public:
  StaticPerInstanceBuffer(
      SharedBasicContext context, uint32_t per_instance_data_size,
      const void* data, uint32_t num_instances,
      std::vector<Attribute>&& attributes);

  template <typename Container>
  StaticPerInstanceBuffer(
      SharedBasicContext context, const Container& container,
      std::vector<Attribute>&& attributes)
      : StaticPerInstanceBuffer{std::move(context), sizeof(container[0]),
                                container.data(), CONTAINER_SIZE(container),
                                std::move(attributes)} {}

  // This class is neither copyable nor movable.
  StaticPerInstanceBuffer(const StaticPerInstanceBuffer&) = delete;
  StaticPerInstanceBuffer& operator=(const StaticPerInstanceBuffer&) = delete;
};

// This class creates a vertex buffer that stores dynamic data, and is able to
// re-allocate a larger buffer internally if necessary.
// If possible, the user should specify 'max_num_instances', so that we can
// pre-allocate enough space and always reuse the same chunk of device memory.
// Since the data is dynamic, this buffer will be visible to the host, and we
// don't use the staging buffer.
class DynamicPerInstanceBuffer : public PerInstanceBuffer,
                                 public DynamicBuffer {
 public:
  DynamicPerInstanceBuffer(SharedBasicContext context,
                           uint32_t per_instance_data_size,
                           size_t max_num_instances,
                           std::vector<Attribute>&& attributes)
      : PerInstanceBuffer{std::move(context), per_instance_data_size,
                          std::move(attributes)},
        DynamicBuffer{/*initial_size=*/
                      per_instance_data_size * max_num_instances, this} {}

  // This class is neither copyable nor movable.
  DynamicPerInstanceBuffer(const DynamicPerInstanceBuffer&) = delete;
  DynamicPerInstanceBuffer& operator=(const DynamicPerInstanceBuffer&) = delete;

  // Copies host data to device. If the device buffer allocated previously is
  // not large enough to hold the new data, it will be recreated internally.
  void CopyHostData(const void* data, uint32_t num_instances);

  // Convenient method to copy all elements of 'container' to the device.
  template <typename Container>
  void CopyHostData(const Container& container) {
    CopyHostData(container.data(), CONTAINER_SIZE(container));
  }
};

// Holds uniform buffer data on both the host and device. To make it more
// flexible, the user may allocate several chunks of memory in this buffer.
// For rendering a single frame (for example, when we render single characters
// onto a large text texture), these chunks may store data for different meshes.
// For rendering multiple frames (which is more general), all chunks can be used
// for one mesh, and each chunk used for one frame.
// The user should use 'HostData()' to update the data on the host, and then
// call 'Flush()' to send it to the device. This buffer will be visible to the
// host, and we don't use the staging buffer.
class UniformBuffer : public DataBuffer {
 public:
  UniformBuffer(SharedBasicContext context, size_t chunk_size, int num_chunks);

  // This class is neither copyable nor movable.
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  ~UniformBuffer() override { delete data_; }

  // Returns a pointer to the data on the host, casted to 'DataType'.
  template <typename DataType>
  DataType* HostData(int chunk_index) const {
    return reinterpret_cast<DataType*>(data_ + chunk_data_size_ * chunk_index);
  }

  // Flushes the data from host to device.
  void Flush(int chunk_index) const;

  // Returns the description of the data chunk at 'chunk_index'.
  VkDescriptorBufferInfo GetDescriptorInfo(int chunk_index) const;

 private:
  // Pointer to data on the host.
  char* data_;

  // Size of each chunk of data in bytes on the host.
  size_t chunk_data_size_;

  // Since we have to align the memory offset with
  // 'minUniformBufferOffsetAlignment' on the device, this stores the aligned
  // value of 'chunk_data_size_'.
  size_t chunk_memory_size_;
};

// This is the base class of buffers storing images. The user should use it
// through derived classes. Since all buffers of this kind need VkImage,
// which configures how do we use the device memory to store multidimensional
// data, it will be held and destroyed by this base class, and initialized by
// derived classes.
class ImageBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  ~ImageBuffer() override {
    vkDestroyImage(*context_->device(), image_, *context_->allocator());
  }

  // Accessors.
  const VkImage& image() const { return image_; }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  // Opaque image object.
  VkImage image_;
};

// This class copies an image on the host to device via the staging buffer,
// and generates mipmaps if requested.
class TextureBuffer : public ImageBuffer {
 public:
  // Description of the image data. The size of 'datas' can only be either 1
  // or 6 (for cubemaps), otherwise, the constructor will throw an exception.
  struct Info{
    VkExtent2D GetExtent2D() const { return {width, height}; }
    VkExtent3D GetExtent3D() const { return {width, height, /*depth=*/1}; }
    VkDeviceSize GetDataSize() const {
      return datas.size() * width * height * channel;
    }

    std::vector<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
  };

  TextureBuffer(SharedBasicContext context,
                bool generate_mipmaps, const Info& info);

  // This class is neither copyable nor movable.
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  // Accessors.
  uint32_t mip_levels() const { return mip_levels_; }

 private:
  // Level of mipmaps.
  uint32_t mip_levels_;
};

// This class creates an image buffer that can be used as offscreen rendering
// target. No data transfer is required at construction.
class OffscreenBuffer : public ImageBuffer {
 public:
  OffscreenBuffer(SharedBasicContext context,
                  const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  OffscreenBuffer(const OffscreenBuffer&) = delete;
  OffscreenBuffer& operator=(const OffscreenBuffer&) = delete;
};

// This class creates an image buffer that can be used as depth stencil image
// buffer. No data transfer is required at construction.
class DepthStencilBuffer : public ImageBuffer {
 public:
  DepthStencilBuffer(SharedBasicContext context,
                     const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;
};

// This class creates an image buffer for multisampling. No data transfer is
// required at construction.
class MultisampleBuffer : public ImageBuffer {
 public:
  enum class Type { kColor, kDepthStencil };

  MultisampleBuffer(SharedBasicContext context,
                    Type type, const VkExtent2D& extent, VkFormat format,
                    VkSampleCountFlagBits sample_count);

  // This class is neither copyable nor movable.
  MultisampleBuffer(const MultisampleBuffer&) = delete;
  MultisampleBuffer& operator=(const MultisampleBuffer&) = delete;
};

// Holds a small amount of data that can be modified per-frame efficiently.
// To make it flexible, the user may use one chunk of memory for each frame,
// just like the uniform buffer. What is different is that this data does not
// need alignment, and the total size is very limited. According to the Vulkan
// specification, to make it compatible with all devices, we only allow the user
// to push at most 128 bytes per-frame.
// The user should use 'HostData()' to update the data on the host, and then
// call 'Flush()' to send it to the device.
class PushConstant {
 public:
  // 'size_per_frame' must be less than 128.
  PushConstant(const SharedBasicContext& context,
               size_t size_per_frame, int num_frames_in_flight);

  // This class is neither copyable nor movable.
  PushConstant(const PushConstant&) = delete;
  PushConstant& operator=(const PushConstant&) = delete;

  ~PushConstant() { delete[] data_; }

  // Returns a pointer to the data on the host, casted to 'DataType'.
  template <typename DataType>
  DataType* HostData(int frame) const {
    return reinterpret_cast<DataType*>(data_ + size_per_frame_ * frame);
  }

  // Flushes the data from host to device. Data of 'size_per_frame_' will be
  // sent, and written to the target with 'target_offset' bytes.
  void Flush(const VkCommandBuffer& command_buffer,
             const VkPipelineLayout& pipeline_layout,
             int frame, uint32_t target_offset,
             VkShaderStageFlags shader_stage) const;

  // Accessors.
  uint32_t size_per_frame() const { return size_per_frame_; }

 private:
  // Pointer to data on the host.
  char* data_;

  // Size of data for one frame in bytes.
  uint32_t size_per_frame_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H */
