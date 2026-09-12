// Copyright (c) 2026 expo-secure-keypad-jsi contributors. MIT License.
#include "esk/SecureBuffer.h"

#include <cassert>

#include <sys/mman.h>
#include <unistd.h>

#include "esk/Cleanse.h"
#include "esk/Error.h"

#if defined(__linux__)
#include <sys/prctl.h>
#endif

namespace esk {

namespace {
size_t systemPageSize() {
  long ps = sysconf(_SC_PAGESIZE);
  return ps > 0 ? static_cast<size_t>(ps) : 4096;
}
}  // namespace

SecureBuffer::SecureBuffer() : owner_(std::this_thread::get_id()) {
  pageSize_ = systemPageSize();

  void* p = mmap(nullptr, pageSize_, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) {
    throw EskError(ErrorCode::Internal, "SecureBuffer: mmap failed");
  }
  page_ = static_cast<uint8_t*>(p);

  // Non-fatal on failure (small RLIMIT_MEMLOCK): cleanse still runs.
  locked_ = (mlock(page_, pageSize_) == 0);

#if defined(__linux__)
  madvise(page_, pageSize_, MADV_DONTDUMP);
#if defined(MADV_WIPEONFORK)
  madvise(page_, pageSize_, MADV_WIPEONFORK);
#endif
#endif

  cleanse(page_, pageSize_);
}

SecureBuffer::~SecureBuffer() {
  if (page_ != nullptr) {
    cleanse(page_, pageSize_);
    if (locked_) {
      munlock(page_, pageSize_);
    }
    munmap(page_, pageSize_);
    page_ = nullptr;
  }
}

void SecureBuffer::assertOwnerThread() const {
  // Throws as well as asserts: release builds must refuse too, not corrupt.
  assert(owner_ == std::this_thread::get_id() &&
         "SecureBuffer accessed from a non-owning thread");
  if (owner_ != std::this_thread::get_id()) {
    throw EskError(ErrorCode::Internal, "SecureBuffer cross-thread access");
  }
}

bool SecureBuffer::append(uint8_t asciiChar) {
  assertOwnerThread();
  if (!isAllowedSecretByte(asciiChar)) {
    throw EskError(ErrorCode::Internal, "SecureBuffer: byte out of range");
  }
  if (length_ >= kMaxSecretLength) {
    return false;
  }
  page_[length_] = asciiChar;
  ++length_;
  return true;
}

bool SecureBuffer::pop() {
  assertOwnerThread();
  if (length_ == 0) {
    return false;
  }
  --length_;
  cleanse(page_ + length_, 1);
  return true;
}

void SecureBuffer::clear() {
  assertOwnerThread();
  cleanse(page_, kMaxSecretLength);
  length_ = 0;
}

size_t SecureBuffer::length() const {
  assertOwnerThread();
  return length_;
}

void SecureBuffer::withPlaintext(
    const std::function<void(const uint8_t*, size_t)>& fn) const {
  assertOwnerThread();
  fn(page_, length_);
}

}  // namespace esk
