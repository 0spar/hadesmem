// Copyright (C) 2010-2015 Joshua Boyce
// See the file COPYING for copying permission.

#pragma once

#include <memory>
#include <utility>

#include <windows.h>

#include <hadesmem/config.hpp>
#include <hadesmem/detail/assert.hpp>
#include <hadesmem/detail/protect_region.hpp>
#include <hadesmem/detail/query_region.hpp>
#include <hadesmem/detail/trace.hpp>
#include <hadesmem/error.hpp>
#include <hadesmem/process.hpp>
#include <hadesmem/protect.hpp>

namespace hadesmem
{
namespace detail
{
enum class ProtectGuardType
{
  kRead,
  kWrite
};

class ProtectGuard
{
public:
  explicit ProtectGuard(Process const& process,
                        PVOID address,
                        ProtectGuardType type)
    : ProtectGuard{process, Query(process, address), type}
  {
  }

  explicit ProtectGuard(Process const& process,
                        MEMORY_BASIC_INFORMATION const& mbi,
                        ProtectGuardType type)
    : process_{&process},
      type_{type},
      can_read_or_write_{false},
      old_protect_{0},
      mbi_(mbi)
  {
    // TODO: Add a 'forced' mode to allow bypassing 'bad' protection detection
    // for the rare cases where it may be necessary.
    if (IsBadProtect(mbi_))
    {
      HADESMEM_DETAIL_THROW_EXCEPTION(
        Error{} << ErrorString{
          "Attempt to access page with a 'bad' protection mask."});
    }

    can_read_or_write_ =
      (type_ == ProtectGuardType::kRead) ? CanRead(mbi_) : CanWrite(mbi_);

    if (!can_read_or_write_)
    {
      try
      {
        old_protect_ = Protect(process, mbi_, PAGE_EXECUTE_READWRITE);
      }
      catch (...)
      {
        // Try and fall back to PAGE_READWRITE because we might not be allowed
        // to set EXECUTE.
        old_protect_ = Protect(process, mbi_, PAGE_READWRITE);
      }
    }
  }

  explicit ProtectGuard(Process const&& process,
                        PVOID address,
                        ProtectGuardType type) = delete;

  explicit ProtectGuard(Process const&& process,
                        MEMORY_BASIC_INFORMATION const& mbi,
                        ProtectGuardType type) = delete;

  ProtectGuard(ProtectGuard const& other) = delete;

  ProtectGuard& operator=(ProtectGuard const& other) = delete;

  ProtectGuard(ProtectGuard&& other) noexcept
    : process_{other.process_},
      type_{other.type_},
      can_read_or_write_{other.can_read_or_write_},
      old_protect_{other.old_protect_},
      mbi_(other.mbi_)
  {
    other.old_protect_ = 0;
  }

  ProtectGuard& operator=(ProtectGuard&& other) noexcept
  {
    process_ = other.process_;
    type_ = other.type_;
    can_read_or_write_ = other.can_read_or_write_;
    old_protect_ = other.old_protect_;
    mbi_ = other.mbi_;

    other.old_protect_ = 0;

    return *this;
  }

  ~ProtectGuard()
  {
    RestoreUnchecked();
  }

  void Restore()
  {
    if (!old_protect_)
    {
      return;
    }

    if (!can_read_or_write_)
    {
      Protect(*process_, mbi_, old_protect_);
    }

    old_protect_ = 0;
  }

  void RestoreUnchecked() noexcept
  {
    try
    {
      Restore();
    }
    catch (...)
    {
      // WARNING: Protection is not restored if 'Restore' fails.
      HADESMEM_DETAIL_TRACE_A(
        boost::current_exception_diagnostic_information().c_str());
      HADESMEM_DETAIL_ASSERT(false);
    }
  }

private:
  Process const* process_;
  ProtectGuardType type_;
  bool can_read_or_write_;
  DWORD old_protect_;
  MEMORY_BASIC_INFORMATION mbi_;
};
}
}
