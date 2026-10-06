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

bool SecureBuffer::append(uint32_t codepoint) {
  assertOwnerThread();
  if (!isAllowedSecretCodepoint(codepoint)) {
    throw EskError(ErrorCode::Internal, "SecureBuffer: character out of range");
  }
  // The charset is BMP-only and surrogate-free, so 1..3 bytes covers it.
  const size_t n = codepoint < 0x80 ? 1 : (codepoint < 0x800 ? 2 : 3);
  if (length_ + n > kMaxSecretLength) {
    return false;
  }
  // Encoded straight into the page: no stack copy of the secret to cleanse.
  uint8_t* p = page_ + length_;
  if (n == 1) {
    p[0] = static_cast<uint8_t>(codepoint);
  } else if (n == 2) {
    p[0] = static_cast<uint8_t>(0xC0 | (codepoint >> 6));
    p[1] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
  } else {
    p[0] = static_cast<uint8_t>(0xE0 | (codepoint >> 12));
    p[1] = static_cast<uint8_t>(0x80 | ((codepoint >> 6) & 0x3F));
    p[2] = static_cast<uint8_t>(0x80 | (codepoint & 0x3F));
  }
  length_ += n;
  ++chars_;
  return true;
}

bool SecureBuffer::pop() {
  assertOwnerThread();
  if (length_ == 0) {
    return false;
  }
  // Walk back over UTF-8 continuation bytes (10xxxxxx) to the lead byte.
  size_t start = length_ - 1;
  while (start > 0 && (page_[start] & 0xC0) == 0x80) --start;
  cleanse(page_ + start, length_ - start);
  length_ = start;
  --chars_;
  return true;
}

void SecureBuffer::clear() {
  assertOwnerThread();
  cleanse(page_, kMaxSecretLength);
  length_ = 0;
  chars_ = 0;
}

size_t SecureBuffer::length() const {
  assertOwnerThread();
  return length_;
}

size_t SecureBuffer::charCount() const {
  assertOwnerThread();
  return chars_;
}

void SecureBuffer::withPlaintext(
    const std::function<void(const uint8_t*, size_t)>& fn) const {
  assertOwnerThread();
  fn(page_, length_);
}

}  // namespace esk
