// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef HPCTOOLKIT_STDSHIM_FILESYSTEM_H
#define HPCTOOLKIT_STDSHIM_FILESYSTEM_H

// This file is one of multiple "stdshim" headers, which act as a seamless
// transition library across versions of the STL. Mostly all this does is
// backport features from C++17 into C++11, sometimes by using class inheritance
// tricks, and sometimes by importing implementations from Boost or ourselves.
// Also see Google's Abseil project.

// This is the shim for <filesystem>.

#include "version.hpp"

#if defined(HPCTOOLKIT_STDSHIM_STD_HAS_filesystem)
#include <filesystem>

namespace hpctoolkit::stdshim {
namespace filesystem = std::filesystem;
}

#elif defined(HPCTOOLKIT_STDSHIM_STD_HAS_experimental_filesystem)
#include <experimental/filesystem>

namespace hpctoolkit::stdshim {
namespace filesystem {
  class path : public std::experimental::filesystem::path {
  private:
    using base = std::experimental::filesystem::path;

  public:
    template<class... Args>
    path(Args&&... a) : base(std::forward<Args>(a)...) {};

    path relative_path() const { return base::relative_path(); }

    path lexically_normal() const { return (std::string)*this; }
  };

  using std::experimental::filesystem::filesystem_error;
  using std::experimental::filesystem::directory_entry;
  using std::experimental::filesystem::directory_iterator;
  using std::experimental::filesystem::recursive_directory_iterator;
  using std::experimental::filesystem::file_status;
  using std::experimental::filesystem::space_info;
  using std::experimental::filesystem::file_type;
  using std::experimental::filesystem::perms;
  using std::experimental::filesystem::copy_options;
  using std::experimental::filesystem::directory_options;
  using std::experimental::filesystem::file_time_type;

  using std::experimental::filesystem::absolute;
  using std::experimental::filesystem::canonical;
  using std::experimental::filesystem::copy;
  using std::experimental::filesystem::copy_file;
  using std::experimental::filesystem::create_directory;
  using std::experimental::filesystem::create_directories;
  using std::experimental::filesystem::create_hard_link;
  using std::experimental::filesystem::create_symlink;
  using std::experimental::filesystem::create_directory_symlink;
  using std::experimental::filesystem::current_path;
  using std::experimental::filesystem::exists;
  using std::experimental::filesystem::equivalent;
  using std::experimental::filesystem::file_size;
  using std::experimental::filesystem::hard_link_count;
  using std::experimental::filesystem::last_write_time;
  using std::experimental::filesystem::permissions;
  using std::experimental::filesystem::read_symlink;
  using std::experimental::filesystem::remove;
  using std::experimental::filesystem::remove_all;
  using std::experimental::filesystem::rename;
  using std::experimental::filesystem::resize_file;
  using std::experimental::filesystem::space;
  using std::experimental::filesystem::status;
  using std::experimental::filesystem::symlink_status;
  using std::experimental::filesystem::temp_directory_path;

  using std::experimental::filesystem::hash_value;

  using std::experimental::filesystem::is_block_file;
  using std::experimental::filesystem::is_character_file;
  using std::experimental::filesystem::is_directory;
  using std::experimental::filesystem::is_empty;
  using std::experimental::filesystem::is_fifo;
  using std::experimental::filesystem::is_other;
  using std::experimental::filesystem::is_regular_file;
  using std::experimental::filesystem::is_socket;
  using std::experimental::filesystem::is_symlink;
  using std::experimental::filesystem::status_known;
}
}

#else  // HPCTOOLKIT_STDSHIM_STD_HAS_filesystem
#error HPCToolkit requires C++17 std::filesystem support!
#endif  // HPCTOOLKIT_STDSHIM_STD_HAS_filesystem

// C++17 was released without a specialization for std::hash<filesystem::path>.
// This was added back later (GCC 12), so we can't define the specialization
// ourselves. So instead we define the common hash callable.
namespace hpctoolkit::stdshim {
struct hash_path {
  std::size_t operator()(const filesystem::path& p) const noexcept {
    return filesystem::hash_value(p);
  }
};
}  // namespace hpctoolkit::stdshim

#endif  // HPCTOOLKIT_STDSHIM_FILESYSTEM_H
