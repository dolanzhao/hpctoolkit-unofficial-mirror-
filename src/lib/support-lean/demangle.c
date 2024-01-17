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
// Copyright ((c)) 2002-2023, Rice University
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

// This file provides a wrapper around libiberty's cplus_demangle() to
// provide a uniform interface for the options that we want for
// hpcstruct and hpcprof.  All cases wanting to do demangling should
// use this file.
//
// Libiberty cplus_demangle() does many malloc()s, but does appear to
// be reentrant and thread-safe.  But not signal safe.

//***************************************************************************

#include <string.h>

#include "../../include/gnu_demangle.h"
#include "../prof-lean/spinlock.h"
#include "hpctoolkit_demangle.h"

#define DEMANGLE_FLAGS  (DMGL_PARAMS | DMGL_ANSI | DMGL_VERBOSE | DMGL_RET_DROP)

static spinlock_t demangle_lock = SPINLOCK_UNLOCKED;

// Returns: malloc()ed string for the demangled name, or else NULL if
// 'name' is not a mangled name.
//
// Note: the caller is responsible for calling free() on the result.
//
char *
hpctoolkit_demangle(const char * name)
{
  if (name == NULL) {
    return NULL;
  }

  if (strncmp(name, "_Z", 2) != 0) {
    return NULL;
  }

  // NOTE: comments in GCC demangler indicate that the demangler uses shared state
  // and that locking for multithreading is our responsibility
  spinlock_lock(&demangle_lock);
  char *demangled = cplus_demangle(name, DEMANGLE_FLAGS);
  spinlock_unlock(&demangle_lock);

  return demangled;
}
