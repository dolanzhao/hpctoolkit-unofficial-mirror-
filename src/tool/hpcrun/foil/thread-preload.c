// -*-Mode: C++;-*- // technically C99

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

#define _GNU_SOURCE

#include "foil.h"
#include "../libmonitor/monitor.h"
#include <pthread.h>

HPCRUN_EXPOSED int pthread_create(pthread_t* t, const pthread_attr_t* a, pthread_start_fcn_t* s, void* d) {
  LOOKUP_FOIL_BASE(base, pthread_create);
  return base(__builtin_return_address(0), t, a, s, d);
}

HPCRUN_EXPOSED void pthread_exit(void* data) {
  LOOKUP_FOIL_BASE(base, pthread_exit);
  base(data);
  __builtin_unreachable();
}

HPCRUN_EXPOSED int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
  LOOKUP_FOIL_BASE(base, pthread_sigmask);
  return base(how, set, oldset);
}
