// Copyright 2016 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef DRACO_POINT_CLOUD_POINT_ATTRIBUTE_H_
#define DRACO_POINT_CLOUD_POINT_ATTRIBUTE_H_

#include <memory>

#include "core/draco_index_type_vector.h"
#include "core/hash_utils.h"
#include "core/macros.h"
#include "point_cloud/geometry_attribute.h"

namespace draco {

// Class for storing point specific data about each attribute. In general,
// multiple points stored in a point cloud can share the same attribute value
// and this class provides the necessary mapping between point ids and attribute
// value ids.
class PointAttribute : public GeometryAttribute {
 public:
  PointAttribute();
  explicit PointAttribute(const GeometryAttribute &att);

  // Make sure the move constructor is defined (needed for better performance
  // when new attributes are added to PointCloud).
  PointAttribute(PointAttribute &&attribute) = default;
  PointAttribute &operator=(PointAttribute &&attribute) = default;

  // Prepares the attribute storage for the specified number of entries.
  void Reset(size_t num_attribute_values);

  size_t size() const { return num_unique_entries_; }
  AttributeValueIndex mapped_index(PointIndex point_index) const {
    if (identity_mapping_)
      return AttributeValueIndex(point_index.value());
    return indices_map_[point_index];
  }
  DataBuffer *buffer() const { return attribute_buffer_.get(); }
  bool is_mapping_identity() const { return identity_mapping_; }

  const uint8_t *GetAddressOfMappedIndex(PointIndex point_index) const {
    return GetAddress(mapped_index(point_index));
  }

  // Sets the new number of unique attribute entries for the attribute.
  void Resize(size_t new_num_unique_entries) {
    num_unique_entries_ = new_num_unique_entries;
  }

  // Functions for setting the type of mapping between point indices and
  // attribute entry ids.
  // This function sets the mapping to implicit, where point indices are equal
  // to attribute entry indices.
  void SetIdentityMapping() {
    identity_mapping_ = true;
    indices_map_.clear();
  }
  // This function sets the mapping to be explicitly using the indices_map_
  // array that needs to be initialized by the caller.
  void SetExplicitMapping(int64_t num_points) {
    identity_mapping_ = false;
    indices_map_.resize(num_points, kInvalidAttributeValueIndex);
  }

  // Set an explicit map entry for a specific point index.
  void SetPointMapEntry(PointIndex point_index,
                        AttributeValueIndex entry_index) {
    DCHECK(!identity_mapping_);
    indices_map_[point_index] = entry_index;
  }

  // Sets a value of an attribute entry. The input value must be allocated to
  // cover all components of a single attribute entry.
  void SetAttributeValue(AttributeValueIndex entry_index, const void *value) {
    const int64_t byte_pos = entry_index.value() * byte_stride();
    buffer()->Write(byte_pos, value, byte_stride());
  }

  // Same as GeometryAttribute::GetValue(), but using point id as the input.
  // Mapping to attribute value index is performed automatically.
  void GetMappedValue(PointIndex point_index, void *out_data) const {
    return GetValue(mapped_index(point_index), out_data);
  }

  // Deduplicate |in_att| values into |this| attribute. |in_att| can be equal
  // to |this|.
  // Returns -1 if the deduplication failed.
  AttributeValueIndex::ValueType DeduplicateValues(
      const GeometryAttribute &in_att);

  // Same as above but the values read from |in_att| are sampled with the
  // provided offset |in_att_offset|.
  AttributeValueIndex::ValueType DeduplicateValues(
      const GeometryAttribute &in_att, AttributeValueIndex in_att_offset);

 private:
  template <typename T>
  AttributeValueIndex::ValueType DeduplicateTypedValues(
      const GeometryAttribute &in_att, AttributeValueIndex in_att_offset);
  template <typename T, int COMPONENTS_COUNT>
  AttributeValueIndex::ValueType DeduplicateFormattedValues(
      const GeometryAttribute &in_att, AttributeValueIndex in_att_offset);

  // Data storage for attribute values. GeometryAttribute itself doesn't own its
  // buffer so we need to allocate it here.
  std::unique_ptr<DataBuffer> attribute_buffer_;

  // Mapping between point ids and attribute value ids.
  IndexTypeVector<PointIndex, AttributeValueIndex> indices_map_;
  AttributeValueIndex::ValueType num_unique_entries_;
  // Flag when the mapping between point ids and attribute values is identity.
  bool identity_mapping_;

  friend struct PointAttributeHasher;
};

// Hasher functor for the PointAttribute class.
struct PointAttributeHasher {
  size_t operator()(const PointAttribute &attribute) const {
    GeometryAttributeHasher base_hasher;
    size_t hash = base_hasher(attribute);
    hash = HashCombine(attribute.identity_mapping_, hash);
    hash = HashCombine(attribute.num_unique_entries_, hash);
    hash = HashCombine(attribute.indices_map_.size(), hash);
    if (attribute.indices_map_.size() > 0) {
      const uint64_t indices_hash = FingerprintString(
          reinterpret_cast<const char *>(attribute.indices_map_.data()),
          attribute.indices_map_.size());
      hash = HashCombine(indices_hash, hash);
    }
    if (attribute.attribute_buffer_ != nullptr) {
      const uint64_t buffer_hash = FingerprintString(
          reinterpret_cast<const char *>(attribute.attribute_buffer_->data()),
          attribute.attribute_buffer_->data_size());
      hash = HashCombine(buffer_hash, hash);
    }
    return hash;
  }
};

}  // namespace draco

#endif  // DRACO_POINT_CLOUD_POINT_ATTRIBUTE_H_