// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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


/******************************************************************************
 * local includes
 *****************************************************************************/

/**
 * WARNING : THIS IS AN EXPERIMENTAL FEATURE
 *
 * Kernel blocking event is not validated yet, and only works for Kernel 4.3
 * (at least). This file will be updated once we find a way to make it work
 * properly.
 */
#define _GNU_SOURCE

#include "../../../../include/linux_info.h"

#include "../../metrics.h"

#include "kernel_blocking.h"

#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "perf_mmap.h"
#include "event_custom.h"

/******************************************************************************
 * Macros
 *****************************************************************************/

#define KERNEL_BLOCKING_DEBUG 0
#define METRIC_HIDDEN         0

// -----------------------------------------------------
// Predefined events
// -----------------------------------------------------

#define EVNAME_KERNEL_BLOCK     "BLOCKTIME"
#define METRIC_NAME_BLOCKTIME   "BLOCKTIME (sec)"
#define EVNAME_CONTEXT_SWITCHES "CNTXT_SWTCH"


//******************************************************************************
// forward declaration
//******************************************************************************



//******************************************************************************
// local variables
//******************************************************************************

// metric index for kernel blocking
// usually each thread has the same metric index, so it's safe to make it global
// for each thread (I hope).
static int metric_blocking_index = -1;
static kind_info_t *blocktime_kind;
static event_custom_t event_kernel_blocking;


static __thread u64          time_cs_out = 0;    // time when leaving the application process
static __thread cct_node_t  *cct_kernel  = NULL; // cct of the last access to kernel
static __thread u32          cpu = 0;           // cpu of the last sample
static __thread u32          pid = 0, tid = 0;  // last pid/tid

/******************************************************************************
 * private operations
 *****************************************************************************/

static void
blame_kernel_time(event_thread_t *current_event, cct_node_t *cct_kernel,
    perf_mmap_data_t *mmap_data)
{
  // make sure the time is is zero or positive
  if (mmap_data->time < time_cs_out) {
    TMSG(LINUX_PERF, "old t: %l, c: %d, p: %d, td: %d -- vs -- t: %l, c: %d, p: %d, td: %d",
        time_cs_out, cpu, pid, tid, mmap_data->time, mmap_data->cpu, mmap_data->pid, mmap_data->tid);
    return;
  }

  // compute the blocking time: current_time - time_last_cs_out
  // Linux kernel should give us time in nanosec (see the sched_clock()) so we need
  // to convert it to sec by multiply it with 1e-09
  const double SEC_PER_NANOSEC = 0.000000001;

  uint64_t delta = mmap_data->time - time_cs_out;
  double   delta_in_sec = delta * SEC_PER_NANOSEC;

  // ----------------------------------------------------------------
  // blame the time spent in the kernel to the cct kernel
  // ----------------------------------------------------------------

  cct_metric_data_increment(metric_blocking_index, cct_kernel,
      (cct_metric_data_t){.r = delta_in_sec});

  // ----------------------------------------------------------------
  // it's important to always count the number of samples for debugging purpose
  // ----------------------------------------------------------------
// fix issue #556: since we use blocktime metric kind, we don't have access to perf info
//                 we don't need these 2 lines as they are used for debug purpose.
//
//  thread_data_t *td = hpcrun_get_thread_data();
//  td->core_profile_trace_data.perf_event_info[metric_blocking_index].num_samples++;
}

/***********************************************************************
 * Warning: There is a bug in Linux kernel concerning monitoring context switch
 *   by 1st-party profiler. For 1st party profiler (or self-profiler), it's
 *   impossible to get the duration time during the context-switch to the kernel
 *   if sleep or any clock routine is used. The reason is that the kernel doesn't
 *   deliver context-switch IN, only context-switch OUT.
 *   For more details see https://www.mail-archive.com/linux-kernel@vger.kernel.org/msg1424933.html
 *
 *   Blocktime event depends on context-switch event. If the kernel fails to
 *   deliver context-switch IN, then we are screwed, the result is completely incorrect.
 *
 *   Another solution is to use approximate blocktime (original code).
 *
 * to compute approximate kernel blocking time:
 *  time_outside_kernel - time_inside_kernel
 *
 ***********************************************************************/
void
kernel_block_handler( event_thread_t *current_event, sample_val_t sv,
    perf_mmap_data_t *mmap_data)
{
  if (metric_blocking_index < 0)
    return; // not initialized or something wrong happens in the initialization

  if (mmap_data == NULL) {

    // somehow, at the end of the execution, a sample event is still triggered
    // and in this case, the arguments are null. Is this our bug ? or gdb ?

    return;
  }

  struct perf_event_attr *attr = &(current_event->event->attr);

  if (attr->config == PERF_COUNT_SW_CONTEXT_SWITCHES &&
      attr->type   == PERF_TYPE_SOFTWARE) {

    // context switch event contains 3 records:
    //  (1) sample record (time, period, cct)
    //  (2) context-switch out (time)
    //  (3) context-switch in  (time)
    //
    // compute the blocktime = context_switch_IN - context_switch_OUT

    if (mmap_data->header_type == PERF_RECORD_SAMPLE) {
      // (1) sample record: store the current cct for further usage
      cct_kernel = sv.sample_node;
      return;
    }

    if (mmap_data->header_misc == PERF_RECORD_MISC_SWITCH_OUT) {
      // (2) leaving the process, entering the kernel: store the time
      time_cs_out = mmap_data->time;
      cpu = mmap_data->cpu;
      pid = mmap_data->pid;
      tid = mmap_data->tid;

    } else {
      // (3) leaving the kernel, entering the process: compute the block time
      if (cct_kernel != NULL && time_cs_out>0)
        blame_kernel_time(current_event, cct_kernel, mmap_data);

      time_cs_out  = 0;
      cct_kernel = NULL;
    }
  }
}


/***************************************************************
 * Register events to compute blocking time in the kernel
 * We use perf's sofrware context switch event to compute the
 * time spent inside the kernel. For this, we need to sample every time
 * a context switch occurs, and compute the time when entering the
 * kernel vs leaving the kernel. See perf_event_handler.
 * We need two metrics for this:
 * - blocking time metric to store the time spent in the kernel
 * - context switch metric to store the number of context switches
 ****************************************************************/
static void
register_blocking(kind_info_t *kb_kind, event_info_t *event_desc)
{
  blocktime_kind = hpcrun_metrics_new_kind();
  // ------------------------------------------
  // create metric to compute blocking time
  // ------------------------------------------
  event_desc->metric_custom->metric_index =
    hpcrun_set_new_metric_info_and_period(
      blocktime_kind, METRIC_NAME_BLOCKTIME,
      MetricFlags_ValFmt_Real, 1 /* period */, metric_property_none);

  event_desc->metric_custom->metric_desc =
    hpcrun_id2metric_linked(event_desc->metric_custom->metric_index);

  metric_blocking_index = event_desc->metric_custom->metric_index;

  // ------------------------------------------
  // create metric to store context switches
  // ------------------------------------------
  event_desc->hpcrun_metric_id =
    hpcrun_set_new_metric_info_and_period(
      kb_kind, EVNAME_CONTEXT_SWITCHES,
      MetricFlags_ValFmt_Real, 1 /* period*/, metric_property_none);

  event_desc->metric_desc =
    hpcrun_id2metric_linked(event_desc->hpcrun_metric_id);

  event_desc->metric_desc->flags.fields.show = METRIC_HIDDEN;

  // ------------------------------------------
  // set context switch event description to be used when creating
  //  perf event of this type on each thread
  // ------------------------------------------
  /* PERF_SAMPLE_STACK_USER may also be good to use */
  u64 sample_type = PERF_SAMPLE_IP   | PERF_SAMPLE_TID       |
      PERF_SAMPLE_TIME | PERF_SAMPLE_CALLCHAIN |
      PERF_SAMPLE_CPU  | PERF_SAMPLE_PERIOD;

  struct perf_event_attr *attr = &(event_desc->attr);
  attr->config = PERF_COUNT_SW_CONTEXT_SWITCHES;
  attr->type   = PERF_TYPE_SOFTWARE;

  perf_util_attr_init( EVNAME_KERNEL_BLOCK, attr,
      true        /* use_period*/,
      1           /* sample every context switch*/,
      sample_type /* need additional info for sample type */
  );

  event_desc->attr.context_switch = 1;
  event_desc->attr.sample_id_all = 1;
  hpcrun_close_kind(blocktime_kind);
}


/******************************************************************************
 * interface operations
 *****************************************************************************/

void kernel_blocking_init()
{
  event_kernel_blocking.name         = EVNAME_KERNEL_BLOCK;
  event_kernel_blocking.desc         = "Approximation of a thread's blocking time in the Linux kernel."
                                       " This event is only available on Linux kernel 4.3 or newer.";
  event_kernel_blocking.register_fn  = register_blocking;   // call backs
  event_kernel_blocking.handler_fn   = NULL;            // No call backs: we want all event to call us
  event_kernel_blocking.metric_index = 0;               // these fields to be defined later
  event_kernel_blocking.metric_desc  = NULL;            // these fields to be defined later

  event_custom_register(&event_kernel_blocking);
}
