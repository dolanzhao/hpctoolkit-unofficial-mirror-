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
// Copyright ((c)) 2002-2021, Rice University
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

#include "frame.h"
#include "hpctoolkit-config.h"
#include "utilities/ip-normalized.h"
#include "unwind/common/unwind.h"

#ifdef ENABLE_LOGICAL_PYTHON
#include "logical/python.h"
#endif

#include "lib/prof-lean/spinlock.h"

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "lib/prof-lean/stdatomic.h"

#ifndef LOGICAL_COMMON_H
#define LOGICAL_COMMON_H

// --------------------------------------
// Logical stack and mutator functions
// --------------------------------------

typedef union logical_frame_t {
  uintptr_t generic;
#ifdef ENABLE_LOGICAL_PYTHON
  logical_python_frame_t python;
#endif
} logical_frame_t;

struct logical_frame_segment_t {
  // Mini-stack of logical frames. Grows upward (increasing index).
  logical_frame_t frames[16];
  // Previous segment, older frames
  struct logical_frame_segment_t* prev;
};

typedef struct logical_region_t {
  // Generator for the logical stack frames contained within this region.
  // Replaces `frame` with the appropriate data for the `index`'th logical frame.
  // Repeatedly called with increasing `index` until `false` is returned.
  // `store` can be used to save state during a single generation pass.
  // `logical_frame` may be NULL if there are not enough logical frames in the region.
  bool (*generator)(struct logical_region_t* arg, void** store,
                    unsigned int index, logical_frame_t* logical_frame,
                    frame_t* frame);
  union {
    uintptr_t generic;
#ifdef ENABLE_LOGICAL_PYTHON
    logical_python_region_t python;
#endif
  } specific;

  // Number of logical stack frames that will be generated by this region.
  // Can be an overestimate, but should never be an underestimate.
  size_t expected;

  // First (earliest) frame NOT part of the logical region. In other words the
  // physical caller of the logical bits.
  hpcrun_unw_cursor_t beforeenter;

  // Optional finalizer called on beforeenter during the first unwind with this
  // region. Set to NULL after use.
  void (*beforeenter_fixup)(struct logical_region_t*, hpcrun_unw_cursor_t*);

  // If not NULL, the last (latest) frame part of the logical region. In other
  // words the caller of the physical code called from the logical scope.
  // Stored as the cursor's sp field.
  // If NULL, indicates that we are still within the logical region.
  void* exit;

  // Optional finalizer for exit, returns the last (latest) frame part of the
  // logical region. Frames at lower offsets are newer. Must not return a frame
  // later (less) than top.
  const frame_t* (*afterexit)(struct logical_region_t*, const frame_t* cur,
                              const frame_t* top);

  // Logical frames residing in this region. Can be used by logical unwinders to
  // store additional per-frame state.
  size_t subdepth;
  struct logical_frame_segment_t* subhead;
} logical_region_t;

struct logical_region_segment_t {
  // Mini-stack of logical regions. Grows upward (increasing index).
  logical_region_t regions[4];
  // Previous segment, older regions
  struct logical_region_segment_t* prev;
};

typedef struct logical_region_stack_t {
  // Current depth of the logical region stack
  size_t depth;
  // Linked list of segments for storing logical stack frames
  struct logical_region_segment_t* head;
  // Linked list of "spare" segments to reuse as nessesary
  struct logical_region_segment_t* spare;
  // Linked list of "spare" substack segments to reuse as nessesary
  struct logical_frame_segment_t* subspare;
} logical_region_stack_t;

// Initialize an empty logical region stack
extern void hpcrun_logical_stack_init(logical_region_stack_t*);

// Get the current top of a logical stack. Returns NULL if empty.
extern logical_region_t* hpcrun_logical_stack_top(const logical_region_stack_t*);

// Push a new region onto a logical stack. Returns the new region entry.
extern logical_region_t* hpcrun_logical_stack_push(logical_region_stack_t*, const logical_region_t*);

// Truncate a logical stack to a particular depth. If the stack is shallower
// than `n`, returns the difference between the current depth and `n`.
// The memory associated with the removed elements is still available after this.
extern size_t hpcrun_logical_stack_settop(logical_region_stack_t*, size_t n);

// Pop up to `n` regions off the logical stack.
static inline void hpcrun_logical_stack_pop(logical_region_stack_t* s, size_t n) {
  hpcrun_logical_stack_settop(s, s->depth > n ? s->depth - n : 0);
}

// Get the top frame of a logical region. Returns NULL if empty.
extern logical_frame_t* hpcrun_logical_substack_top(const logical_region_t*);

// Push a new frame onto a logical region. Returns the new region entry.
extern logical_frame_t* hpcrun_logical_substack_push(logical_region_stack_t*, logical_region_t*, const logical_frame_t*);

// Truncate the logical substack for a region to a particular depth.
// Same as hpcrun_logical_stack_settop otherwise.
extern size_t hpcrun_logical_substack_settop(logical_region_stack_t*, logical_region_t*, size_t n);

// Pop up to `n` frames off a region's substack.
static inline void hpcrun_logical_substack_pop(logical_region_stack_t* s, logical_region_t* r, size_t n) {
  hpcrun_logical_substack_settop(s, r, r->subdepth > n ? r->subdepth - n : 0);
}

// Get the unwind cursor for the Nth caller frame. Helper for filling beforeenter.
#define hpcrun_logical_frame_cursor(CUR, N) ({    \
  ucontext_t ctx;                                 \
  assert(getcontext(&ctx) == 0);                  \
  hpcrun_logical_frame_cursor_real(&ctx, CUR, N); \
})
__attribute__((always_inline))
static inline void hpcrun_logical_frame_cursor_real(ucontext_t* ctx, hpcrun_unw_cursor_t* cur, size_t n) {
  int steps_taken = 0;
  hpcrun_unw_init_cursor(cur, ctx);
  for(size_t i = 0; i < n; i++) {
    if(hpcrun_unw_step(cur, &steps_taken) <= STEP_STOP)
      break;
  }
}

// Get the stack pointer for the Nth caller frame. Helper for filling exit.
#define hpcrun_logical_frame_sp(N) ({             \
  hpcrun_unw_cursor_t cur;                        \
  hpcrun_logical_frame_cursor(&cur, N);           \
  cur.sp;                                         \
})

// --------------------------------------
// Logical load module management
// --------------------------------------

struct logical_metadata_store_hashentry_t {
  char* funcname;  // funcname used for this entry
  char* filename;  // filename used for this entry
  uint32_t lineno; // lineno used for this entry
  size_t hash;  // Full hash value for this entry
  uint32_t id;  // value: Identifier for this key
};

// There will generally be one of these per logical context generator
// The actual initialization is handled in hpcrun_logical_metadata_register
typedef struct logical_metadata_store_t {
  // Lock used to protect everything in here
  spinlock_t lock;

  // Next identifier to be allocated somewhere
  uint32_t nextid;
  // Hash table for function/file name identifiers
  struct logical_metadata_store_hashentry_t* idtable;
  // Current size of the hash table, must always be a power of 2
  size_t tablesize;

  // Generator identifier for this store
  const char* generator;
  // Load module identifier used to refer to this particular metadata store
  // Use hpcrun_logical_metadata_lmid to get the correct value for this
  _Atomic(uint16_t) lm_id;
  // Path to the metadata storage file
  char* path;

  // Pointer to the next metadata storage (for cleanup by fini())
  struct logical_metadata_store_t* next;
} logical_metadata_store_t;

// Register a logical metadata store with the given identifier
// NOTE: The given string must be a data-constant, its referred to after this call
extern void hpcrun_logical_metadata_register(logical_metadata_store_t*, const char* generator);

// Generate a new load module id for a metadata store. Expensive
// Requires that the lock is held, in practice just use the wrapper below
extern void hpcrun_logical_metadata_generate_lmid(logical_metadata_store_t*);

// Get the load module id for a metadata store. Caches properly
static inline uint16_t hpcrun_logical_metadata_lmid(logical_metadata_store_t* store) {
  uint16_t ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
  if(ret == 0) {
    spinlock_lock(&store->lock);
    ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
    if(ret == 0) {
      hpcrun_logical_metadata_generate_lmid(store);
      ret = atomic_load_explicit(&store->lm_id, memory_order_relaxed);
    }
    spinlock_unlock(&store->lock);
  }
  return ret;
}

// Get a unique identifier for:
//  - (NULL, NULL, <ignored>): Unknown logical region (always 0)
//  - (NULL, "file", <ignored>): A source file "file" with no known function information
//  - ("func", NULL, <ignored>): A function "func" with no known source file
//  - ("func", "file", lineno): A function "func" defined in "file" at line `lineno`
// This is nearly useless alone, pass to hpcrun_logical_metadata_ipnorm
extern uint32_t hpcrun_logical_metadata_fid(logical_metadata_store_t*,
    const char* func, const char* file, uint32_t lineno);

// Compose a full normalized ip for a particular line number within a logical function/file.
static inline ip_normalized_t hpcrun_logical_metadata_ipnorm(
    logical_metadata_store_t* store, uint32_t fid, uint32_t lineno) {
  ip_normalized_t ip = {
    .lm_id = hpcrun_logical_metadata_lmid(store), .lm_ip = fid,
  };
  ip.lm_ip = (ip.lm_ip << 32) + lineno;
  return ip;
}

// --------------------------------------
// Hook registration functions
// --------------------------------------

// Initialize all available logical context generators
extern void hpcrun_logical_init();

// Register the nessesary handler for modifying backtraces based on the logical stack
extern void hpcrun_logical_register();

// Finalize any of the logical bits that need to be finalized for this process
extern void hpcrun_logical_fini();

#endif  // LOGICAL_COMMON_H
