//
// Copyright (c) 2017 XiaoMi All rights reserved.
//

#ifndef MACE_CORE_TENSOR_H_
#define MACE_CORE_TENSOR_H_

#include "mace/core/common.h"
#include "mace/proto/mace.pb.h"
#include "mace/core/allocator.h"
#include "mace/core/types.h"
#include "mace/core/logging.h"

namespace mace {

#define SINGLE_ARG(...) __VA_ARGS__
#define CASE(TYPE, STMTS)             \
  case DataTypeToEnum<TYPE>::value: { \
    typedef TYPE T;                   \
    STMTS;                            \
    break;                            \
  }

#define CASES_WITH_DEFAULT(TYPE_ENUM, STMTS, INVALID, DEFAULT) \
  switch (TYPE_ENUM) {                                         \
    CASE(float, SINGLE_ARG(STMTS))                             \
    CASE(double, SINGLE_ARG(STMTS))                            \
    CASE(int32_t, SINGLE_ARG(STMTS))                             \
    CASE(uint8_t, SINGLE_ARG(STMTS))                             \
    CASE(uint16_t, SINGLE_ARG(STMTS))                            \
    CASE(int16_t, SINGLE_ARG(STMTS))                             \
    CASE(int8_t, SINGLE_ARG(STMTS))                              \
    CASE(string, SINGLE_ARG(STMTS))                            \
    CASE(int64_t, SINGLE_ARG(STMTS))                             \
    CASE(bool, SINGLE_ARG(STMTS))                              \
    case DT_INVALID:                                           \
      INVALID;                                                 \
      break;                                                   \
    default:                                                   \
      DEFAULT;                                                 \
      break;                                                   \
  }


#define CASES(TYPE_ENUM, STMTS)                                      \
  CASES_WITH_DEFAULT(TYPE_ENUM, STMTS, LOG(FATAL) << "Type not set"; \
                     , LOG(FATAL) << "Unexpected type: " << TYPE_ENUM;)


class Tensor {
 public:
  Tensor()
      : alloc_(cpu_allocator()),
        size_(0), dtype_(DT_FLOAT), data_(nullptr) {};

  Tensor(Allocator* a, DataType type)
      : alloc_(a), size_(0), dtype_(type), data_(nullptr) {};

  ~Tensor() {
    if (alloc_ && data_.get()) {
      data_.reset();
    }
  }

  inline DataType dtype() const { return dtype_; }

  inline const vector<index_t>& shape() const { return shape_; }

  inline index_t dim_size() const { return shape_.size(); }

  inline index_t dim(unsigned int index) const {
    MACE_CHECK(index < shape_.size(), "Exceeding ndim limit");
    return shape_[index];
  }

  inline index_t size() const { return size_; }

  inline const void* raw_data() const {
    MACE_CHECK(data_.get() || size_ == 0);
    return data_.get();
  }

  template <typename T>
  inline const T* data() const {
    MACE_CHECK(
        data_.get() || size_ == 0,
        "The tensor is of non-zero shape, but its data is not allocated yet. ");
    return static_cast<T*>(data_.get());
  }

  inline void* raw_mutable_data() {
    if (data_.get() || size_ == 0) {
      return data_.get();
    } else {
      CASES(dtype_, data_.reset(alloc_->New(size_ * sizeof(T)), [this](void* ptr) {
        alloc_->Delete(ptr);
      }));
      return data_.get();
    }
  }

  template <typename T>
  inline T* mutable_data() {
    if (size_ == 0 || data_.get()) {
      return static_cast<T*>(data_.get());
    }
    return static_cast<T*>(raw_mutable_data());
  }

  inline void Resize(const vector<index_t>& shape) {
    shape_ = shape;
    index_t size = NumElements();
    if (size_ != size) {
      size_ = size;
      data_.reset();
    }
  }

  inline void ResizeLike(const Tensor& other) {
    Resize(other.shape());
  }

  inline void ResizeLike(const Tensor* other) {
    Resize(other->shape());
  }

  template <typename T>
  inline void Copy(const T* src, index_t size) {
    MACE_CHECK(size == size_, "copy src and dst with different size.");
    CopyBytes(static_cast<const void*>(src), sizeof(T) * size);
  }

  template <typename SrcType, typename DstType>
  inline void CopyWithCast(const SrcType* src, size_t size) {
    MACE_CHECK(static_cast<index_t>(size) == size_, "copy src and dst with different size.");
    unique_ptr<DstType[]> buffer(new DstType[size]);
    for (size_t i = 0; i < size; ++i) {
      buffer[i] = static_cast<DstType>(src[i]);
    }
    CopyBytes(static_cast<const void*>(buffer.get()), sizeof(DstType) * size);
  }

  inline void CopyBytes(const void* src, size_t size) {
    alloc_->CopyBytes(raw_mutable_data(), src, size);
  }

  inline void DebugPrint() {
    std::stringstream os;
    for (int i: shape_) {
      os << i << ", ";
    }
    LOG(INFO) << "Tensor shape: " << os.str() << " type: " << DataType_Name(dtype_);

    os.str("");
    os.clear();
    for (int i = 0; i < size_; ++i) {
      CASES(dtype_, (os << this->data<T>()[i]) << ", ");
    }
    LOG(INFO) << os.str();
  }

  inline size_t SizeOfType() {
    size_t type_size = 0;
    CASES(dtype_, type_size = sizeof(T));
    return type_size;
  }

  inline void Copy(const Tensor& other) {
    alloc_ = other.alloc_;
    dtype_ = other.dtype_;
    ResizeLike(other);
    const void* other_data = other.raw_data();
    memcpy(raw_mutable_data(), other_data, size_ * SizeOfType());
  }

 private:
  inline int64_t NumElements() const {
    return std::accumulate(shape_.begin(), shape_.end(), 1, std::multiplies<int64_t>());
  }

  Allocator* alloc_;
  index_t size_;
  DataType dtype_;
  std::shared_ptr<void> data_;
  vector<index_t> shape_;

 DISABLE_COPY_AND_ASSIGN(Tensor);
};

} // namespace tensor

#endif //MACE_CORE_TENSOR_H_
