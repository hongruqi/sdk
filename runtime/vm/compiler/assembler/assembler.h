// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef RUNTIME_VM_COMPILER_ASSEMBLER_ASSEMBLER_H_
#define RUNTIME_VM_COMPILER_ASSEMBLER_ASSEMBLER_H_

#include "platform/assert.h"
#include "vm/allocation.h"
#include "vm/globals.h"
#include "vm/growable_array.h"
#include "vm/hash_map.h"
#include "vm/object.h"

namespace dart {

#if defined(TARGET_ARCH_ARM) || defined(TARGET_ARCH_ARM64)
DECLARE_FLAG(bool, use_far_branches);
#endif

// Forward declarations.
class Assembler;
class AssemblerFixup;
class AssemblerBuffer;
class MemoryRegion;

class Label : public ZoneAllocated {
 public:
  Label() : position_(0), unresolved_(0) {
#ifdef DEBUG
    for (int i = 0; i < kMaxUnresolvedBranches; i++) {
      unresolved_near_positions_[i] = -1;
    }
#endif  // DEBUG
  }

  ~Label() {
    // Assert if label is being destroyed with unresolved branches pending.
    ASSERT(!IsLinked());
    ASSERT(!HasNear());
  }

  // Returns the position for bound and linked labels. Cannot be used
  // for unused labels.
  intptr_t Position() const {
    ASSERT(!IsUnused());
    return IsBound() ? -position_ - kWordSize : position_ - kWordSize;
  }

  intptr_t LinkPosition() const {
    ASSERT(IsLinked());
    return position_ - kWordSize;
  }

  intptr_t NearPosition() {
    ASSERT(HasNear());
    return unresolved_near_positions_[--unresolved_];
  }

  bool IsBound() const { return position_ < 0; }
  bool IsUnused() const { return position_ == 0 && unresolved_ == 0; }
  bool IsLinked() const { return position_ > 0; }
  bool HasNear() const { return unresolved_ != 0; }

 private:
#if defined(TARGET_ARCH_X64) || defined(TARGET_ARCH_IA32)
  static const int kMaxUnresolvedBranches = 20;
#else
  static const int kMaxUnresolvedBranches = 1;  // Unused on non-Intel.
#endif

  intptr_t position_;
  intptr_t unresolved_;
  intptr_t unresolved_near_positions_[kMaxUnresolvedBranches];

  void Reinitialize() { position_ = 0; }

  void BindTo(intptr_t position) {
    ASSERT(!IsBound());
    ASSERT(!HasNear());
    position_ = -position - kWordSize;
    ASSERT(IsBound());
  }

  void LinkTo(intptr_t position) {
    ASSERT(!IsBound());
    position_ = position + kWordSize;
    ASSERT(IsLinked());
  }

  void NearLinkTo(intptr_t position) {
    ASSERT(!IsBound());
    ASSERT(unresolved_ < kMaxUnresolvedBranches);
    unresolved_near_positions_[unresolved_++] = position;
  }

  friend class Assembler;
  DISALLOW_COPY_AND_ASSIGN(Label);
};

// External labels keep a function pointer to allow them
// to be called from code generated by the assembler.
class ExternalLabel : public ValueObject {
 public:
  explicit ExternalLabel(uword address) : address_(address) {}

  bool is_resolved() const { return address_ != 0; }
  uword address() const {
    ASSERT(is_resolved());
    return address_;
  }

 private:
  const uword address_;
};

// Assembler fixups are positions in generated code that hold relocation
// information that needs to be processed before finalizing the code
// into executable memory.
class AssemblerFixup : public ZoneAllocated {
 public:
  virtual void Process(const MemoryRegion& region, intptr_t position) = 0;

  virtual bool IsPointerOffset() const = 0;

  // It would be ideal if the destructor method could be made private,
  // but the g++ compiler complains when this is subclassed.
  virtual ~AssemblerFixup() { UNREACHABLE(); }

 private:
  AssemblerFixup* previous_;
  intptr_t position_;

  AssemblerFixup* previous() const { return previous_; }
  void set_previous(AssemblerFixup* previous) { previous_ = previous; }

  intptr_t position() const { return position_; }
  void set_position(intptr_t position) { position_ = position; }

  friend class AssemblerBuffer;
};

// Assembler buffers are used to emit binary code. They grow on demand.
class AssemblerBuffer : public ValueObject {
 public:
  AssemblerBuffer();
  ~AssemblerBuffer();

  // Basic support for emitting, loading, and storing.
  template <typename T>
  void Emit(T value) {
    ASSERT(HasEnsuredCapacity());
    *reinterpret_cast<T*>(cursor_) = value;
    cursor_ += sizeof(T);
  }

  template <typename T>
  void Remit() {
    ASSERT(Size() >= static_cast<intptr_t>(sizeof(T)));
    cursor_ -= sizeof(T);
  }

  // Return address to code at |position| bytes.
  uword Address(intptr_t position) { return contents_ + position; }

  template <typename T>
  T Load(intptr_t position) {
    ASSERT(position >= 0 &&
           position <= (Size() - static_cast<intptr_t>(sizeof(T))));
    return *reinterpret_cast<T*>(contents_ + position);
  }

  template <typename T>
  void Store(intptr_t position, T value) {
    ASSERT(position >= 0 &&
           position <= (Size() - static_cast<intptr_t>(sizeof(T))));
    *reinterpret_cast<T*>(contents_ + position) = value;
  }

  const ZoneGrowableArray<intptr_t>& pointer_offsets() const {
#if defined(DEBUG)
    ASSERT(fixups_processed_);
#endif
    return *pointer_offsets_;
  }

  // Emit an object pointer directly in the code.
  void EmitObject(const Object& object);

  // Emit a fixup at the current location.
  void EmitFixup(AssemblerFixup* fixup) {
    fixup->set_previous(fixup_);
    fixup->set_position(Size());
    fixup_ = fixup;
  }

  // Count the fixups that produce a pointer offset, without processing
  // the fixups.
  intptr_t CountPointerOffsets() const;

  // Get the size of the emitted code.
  intptr_t Size() const { return cursor_ - contents_; }
  uword contents() const { return contents_; }

  // Copy the assembled instructions into the specified memory block
  // and apply all fixups.
  void FinalizeInstructions(const MemoryRegion& region);

// To emit an instruction to the assembler buffer, the EnsureCapacity helper
// must be used to guarantee that the underlying data area is big enough to
// hold the emitted instruction. Usage:
//
//     AssemblerBuffer buffer;
//     AssemblerBuffer::EnsureCapacity ensured(&buffer);
//     ... emit bytes for single instruction ...

#if defined(DEBUG)
  class EnsureCapacity : public ValueObject {
   public:
    explicit EnsureCapacity(AssemblerBuffer* buffer);
    ~EnsureCapacity();

   private:
    AssemblerBuffer* buffer_;
    intptr_t gap_;

    intptr_t ComputeGap() { return buffer_->Capacity() - buffer_->Size(); }
  };

  bool has_ensured_capacity_;
  bool HasEnsuredCapacity() const { return has_ensured_capacity_; }
#else
  class EnsureCapacity : public ValueObject {
   public:
    explicit EnsureCapacity(AssemblerBuffer* buffer) {
      if (buffer->cursor() >= buffer->limit()) buffer->ExtendCapacity();
    }
  };

  // When building the C++ tests, assertion code is enabled. To allow
  // asserting that the user of the assembler buffer has ensured the
  // capacity needed for emitting, we add a dummy method in non-debug mode.
  bool HasEnsuredCapacity() const { return true; }
#endif

  // Returns the position in the instruction stream.
  intptr_t GetPosition() const { return cursor_ - contents_; }

  void Reset() { cursor_ = contents_; }

 private:
  // The limit is set to kMinimumGap bytes before the end of the data area.
  // This leaves enough space for the longest possible instruction and allows
  // for a single, fast space check per instruction.
  static const intptr_t kMinimumGap = 32;

  uword contents_;
  uword cursor_;
  uword limit_;
  AssemblerFixup* fixup_;
  ZoneGrowableArray<intptr_t>* pointer_offsets_;
#if defined(DEBUG)
  bool fixups_processed_;
#endif

  uword cursor() const { return cursor_; }
  uword limit() const { return limit_; }
  intptr_t Capacity() const {
    ASSERT(limit_ >= contents_);
    return (limit_ - contents_) + kMinimumGap;
  }

  // Process the fixup chain.
  void ProcessFixups(const MemoryRegion& region);

  // Compute the limit based on the data area and the capacity. See
  // description of kMinimumGap for the reasoning behind the value.
  static uword ComputeLimit(uword data, intptr_t capacity) {
    return data + capacity - kMinimumGap;
  }

  void ExtendCapacity();

  friend class AssemblerFixup;
};

struct ObjectPoolWrapperEntry {
  ObjectPoolWrapperEntry() : raw_value_(), entry_bits_(0), equivalence_() {}
  ObjectPoolWrapperEntry(const Object* obj, ObjectPool::Patchability patchable)
      : obj_(obj),
        entry_bits_(ObjectPool::TypeBits::encode(ObjectPool::kTaggedObject) |
                    ObjectPool::PatchableBit::encode(patchable)),
        equivalence_(obj) {}
  ObjectPoolWrapperEntry(const Object* obj,
                         const Object* eqv,
                         ObjectPool::Patchability patchable)
      : obj_(obj),
        entry_bits_(ObjectPool::TypeBits::encode(ObjectPool::kTaggedObject) |
                    ObjectPool::PatchableBit::encode(patchable)),
        equivalence_(eqv) {}
  ObjectPoolWrapperEntry(uword value,
                         ObjectPool::EntryType info,
                         ObjectPool::Patchability patchable)
      : raw_value_(value),
        entry_bits_(ObjectPool::TypeBits::encode(info) |
                    ObjectPool::PatchableBit::encode(patchable)),
        equivalence_() {}

  ObjectPool::EntryType type() const {
    return ObjectPool::TypeBits::decode(entry_bits_);
  }

  ObjectPool::Patchability patchable() const {
    return ObjectPool::PatchableBit::decode(entry_bits_);
  }

  union {
    const Object* obj_;
    uword raw_value_;
  };
  uint8_t entry_bits_;
  const Object* equivalence_;
};

// Pair type parameter for DirectChainedHashMap used for the constant pool.
class ObjIndexPair {
 public:
  // Typedefs needed for the DirectChainedHashMap template.
  typedef ObjectPoolWrapperEntry Key;
  typedef intptr_t Value;
  typedef ObjIndexPair Pair;

  static const intptr_t kNoIndex = -1;

  ObjIndexPair()
      : key_(static_cast<uword>(NULL),
             ObjectPool::kTaggedObject,
             ObjectPool::kPatchable),
        value_(kNoIndex) {}

  ObjIndexPair(Key key, Value value) : value_(value) {
    key_.entry_bits_ = key.entry_bits_;
    if (key.type() == ObjectPool::kTaggedObject) {
      key_.obj_ = key.obj_;
      key_.equivalence_ = key.equivalence_;
    } else {
      key_.raw_value_ = key.raw_value_;
    }
  }

  static Key KeyOf(Pair kv) { return kv.key_; }

  static Value ValueOf(Pair kv) { return kv.value_; }

  static intptr_t Hashcode(Key key);

  static inline bool IsKeyEqual(Pair kv, Key key) {
    if (kv.key_.entry_bits_ != key.entry_bits_) return false;
    if (kv.key_.type() == ObjectPool::kTaggedObject) {
      return (kv.key_.obj_->raw() == key.obj_->raw()) &&
             (kv.key_.equivalence_->raw() == key.equivalence_->raw());
    }
    return kv.key_.raw_value_ == key.raw_value_;
  }

 private:
  Key key_;
  Value value_;
};

class ObjectPoolWrapper : public ValueObject {
 public:
  intptr_t AddObject(
      const Object& obj,
      ObjectPool::Patchability patchable = ObjectPool::kNotPatchable);
  intptr_t AddImmediate(uword imm);
  intptr_t FindObject(
      const Object& obj,
      ObjectPool::Patchability patchable = ObjectPool::kNotPatchable);
  intptr_t FindObject(const Object& obj, const Object& equivalence);
  intptr_t FindImmediate(uword imm);
  intptr_t FindNativeFunction(const ExternalLabel* label,
                              ObjectPool::Patchability patchable);
  intptr_t FindNativeFunctionWrapper(const ExternalLabel* label,
                                     ObjectPool::Patchability patchable);

  RawObjectPool* MakeObjectPool();

 private:
  intptr_t AddObject(ObjectPoolWrapperEntry entry);
  intptr_t FindObject(ObjectPoolWrapperEntry entry);

  // Objects and jump targets.
  GrowableArray<ObjectPoolWrapperEntry> object_pool_;

  // Hashmap for fast lookup in object pool.
  DirectChainedHashMap<ObjIndexPair> object_pool_index_table_;
};

enum RestorePP { kRestoreCallerPP, kKeepCalleePP };

class AssemblerBase : public ValueObject {
 public:
  explicit AssemblerBase(ObjectPoolWrapper* object_pool_wrapper)
      : prologue_offset_(-1),
        has_single_entry_point_(true),
        object_pool_wrapper_(object_pool_wrapper) {}
  virtual ~AssemblerBase() {}

  intptr_t CodeSize() const { return buffer_.Size(); }

  uword CodeAddress(intptr_t offset) { return buffer_.Address(offset); }

  ObjectPoolWrapper& object_pool_wrapper() { return *object_pool_wrapper_; }

  intptr_t prologue_offset() const { return prologue_offset_; }
  bool has_single_entry_point() const { return has_single_entry_point_; }

  void Comment(const char* format, ...) PRINTF_ATTRIBUTE(2, 3);
  static bool EmittingComments();

  const Code::Comments& GetCodeComments() const;

  void Unimplemented(const char* message);
  void Untested(const char* message);
  void Unreachable(const char* message);
  virtual void Stop(const char* message) = 0;

  void FinalizeInstructions(const MemoryRegion& region) {
    buffer_.FinalizeInstructions(region);
  }

  // Count the fixups that produce a pointer offset, without processing
  // the fixups.
  intptr_t CountPointerOffsets() const { return buffer_.CountPointerOffsets(); }

  const ZoneGrowableArray<intptr_t>& GetPointerOffsets() const {
    return buffer_.pointer_offsets();
  }

  RawObjectPool* MakeObjectPool() {
    if (object_pool_wrapper_ != nullptr) {
      return object_pool_wrapper_->MakeObjectPool();
    }
    return ObjectPool::null();
  }

 protected:
  AssemblerBuffer buffer_;  // Contains position independent code.
  int32_t prologue_offset_;
  bool has_single_entry_point_;

 private:
  class CodeComment : public ZoneAllocated {
   public:
    CodeComment(intptr_t pc_offset, const String& comment)
        : pc_offset_(pc_offset), comment_(comment) {}

    intptr_t pc_offset() const { return pc_offset_; }
    const String& comment() const { return comment_; }

   private:
    intptr_t pc_offset_;
    const String& comment_;

    DISALLOW_COPY_AND_ASSIGN(CodeComment);
  };

  GrowableArray<CodeComment*> comments_;
  ObjectPoolWrapper* object_pool_wrapper_;
};

}  // namespace dart

#if defined(TARGET_ARCH_IA32)
#include "vm/compiler/assembler/assembler_ia32.h"
#elif defined(TARGET_ARCH_X64)
#include "vm/compiler/assembler/assembler_x64.h"
#elif defined(TARGET_ARCH_ARM)
#include "vm/compiler/assembler/assembler_arm.h"
#elif defined(TARGET_ARCH_ARM64)
#include "vm/compiler/assembler/assembler_arm64.h"
#elif defined(TARGET_ARCH_DBC)
#include "vm/compiler/assembler/assembler_dbc.h"
#else
#error Unknown architecture.
#endif

#endif  // RUNTIME_VM_COMPILER_ASSEMBLER_ASSEMBLER_H_
