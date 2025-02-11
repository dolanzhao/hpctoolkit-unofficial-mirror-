#include "torch-monitor-api.h"

#include "../../cct/cct.h"
#include "../../metrics.h"
#include "../../safe-sampling.h"
#include "../../sample_event.h"
#include "../../thread_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <torch_monitor.h>

#include "torch-monitor-function-cct-map.h"
#include "torch-monitor-logical.h"
#include "torch-monitor-op-placeholders.h"
#include "torch-monitor-thread-obj.h"

#include <assert.h>

#define TORCH_MONITOR_CALL(func, args)                     \
  do {                                                     \
    torch_monitor_status_t status = func args;             \
    if (status != TORCH_MONITOR_STATUS_SUCCESS) {          \
      fprintf(stderr, "Torch monitor status: %d", status); \
      exit(1);                                             \
    }                                                      \
  } while (0)

static bool torch_monitor_enabled = false;

bool torch_monitor_status_get(void) { return torch_monitor_enabled; }

static cct_node_t *forward_forward_cct_lookup(cct_node_t *cct) {
  // Upward until find the forward cct node
  while (cct != NULL) {
    cct_addr_t *addr = hpcrun_cct_addr(cct);
    ip_normalized_t forward_ip_norm = torch_monitor_op_placeholder_ip(
        torch_monitor_op_placeholder_type_forward);
    if (addr->ip_norm.lm_id == forward_ip_norm.lm_id &&
        addr->ip_norm.lm_ip == forward_ip_norm.lm_ip) {
      break;
    }
    cct = hpcrun_cct_parent(cct);
  }
  assert(cct != NULL);
  return cct;
}

static cct_node_t *forward_cct_get(torch_monitor_thread_obj_t *thread_obj) {
  hpcrun_metricVal_t zero_metric_incr = {.i = 0};
  int zero_metric_id = 0;  // nothing to see here

  ucontext_t uc;
  getcontext(&uc);  // current context, where unwind will begin

  bool unsafe = hpcrun_safe_enter();

  cct_node_t *cct =
      hpcrun_sample_callpath(&uc, zero_metric_id, zero_metric_incr, 0, 1, NULL)
          .sample_node;

  if (unsafe) hpcrun_safe_exit();

  return cct;
}

static void cached_cct_cleanup(torch_monitor_thread_obj_t *thread_obj) {
  thread_obj->prev_cct = NULL;

  TORCH_MONITOR_MSG("Cleanup prev_cct");
}

static void forward_function_callback(
    const torch_monitor_callback_data_t *callback_data,
    torch_monitor_thread_obj_t *thread_obj) {
  uint64_t forward_thread_id = callback_data->current_thread_id;
  int64_t sequence_number = callback_data->data.op_data.sequence_number;
  uint32_t nested_level = callback_data->data.op_data.nested_level;
  const char *function_name = callback_data->data.op_data.name;

  thread_obj->thread_state |= TORCH_MONITOR_THREAD_STATE_FORWARD;

  TORCH_MONITOR_MSG("Enter forward level %u state %p function %s sequence_number %d", nested_level,
                    thread_obj->thread_state, function_name, sequence_number);

  if (nested_level == 0) {
    // nested_level == 0: This op is the entry to aten,
    // all subsequent ops should have use a different cache
    cached_cct_cleanup(thread_obj);
  }

  if (sequence_number == -1) {
    // sequence_number == -1: This op may not have a corresponding backward call
    return;
  }

  cct_node_t *function_cct = NULL;
  if (thread_obj->prev_cct == NULL) {
    // Sebsequent calls are forward functions
    thread_obj->domain = TORCH_MONITOR_DOMAIN_FUNCTION;
    // Save function ip_norm so future unwinding can use ip_norm to add a function
    // node
    thread_obj->function_ip_norm = torch_monitor_function_ip(function_name);
    // Get the function cct using unwinding
    cct_node_t *cct = forward_cct_get(thread_obj);
    cct_node_t *forward_cct = forward_forward_cct_lookup(cct);
    if (torch_monitor_native_stack_status_get()) {
      // XXX(Keren): rethink the native mode
      // In the native mode, prev_cct can only be cached after unwinding is done
      thread_obj->prev_cct = forward_cct;
    }
    function_cct = hpcrun_cct_parent(forward_cct);
  } else {
    function_cct = hpcrun_cct_parent(thread_obj->prev_cct);
  }

  // A node in a computation graph can be without backward operations.
  // In this case, the <forward_thread_id, sequence_number> pair could repeat,
  // so that we could have multiple pair->cct mappings.
  // We memoize the last pair->cct mapping and use the cct to relate the
  // correponsding backward op.
  function_key_t key = {.forward_thread_id = forward_thread_id,
                        .sequence_number = sequence_number};
  torch_monitor_function_cct_map_entry_t *entry =
      torch_monitor_function_cct_map_lookup(key);
  if (entry == NULL) {
    TORCH_MONITOR_MSG("Insert forward_thread_id %lu sequence_number %ld",
                      forward_thread_id, sequence_number);
    torch_monitor_function_cct_map_insert(key, function_cct);
  } else {
    TORCH_MONITOR_MSG("Update forward_thread_id %lu sequence_number %ld",
                      forward_thread_id, sequence_number);
    // We can update without holding a lock.
    // When a forward op is in progress, its backward op has not started
    torch_monitor_function_cct_map_entry_cct_update(entry, function_cct);
  }
}

static void backward_function_cct_update(
    cct_node_t *cct, torch_monitor_thread_obj_t *thread_obj) {
  thread_data_t *td = hpcrun_get_thread_data();
  thread_obj->function_cct = hpcrun_cct_insert_path_return_leaf(
      td->core_profile_trace_data.epoch->csdata.tree_root, cct);
}

static void backward_function_callback(
    const torch_monitor_callback_data_t *callback_data,
    torch_monitor_thread_obj_t *thread_obj) {
  uint64_t forward_thread_id = callback_data->data.op_data.forward_thread_id;
  int64_t sequence_number = callback_data->data.op_data.sequence_number;
  uint32_t nested_level = callback_data->data.op_data.nested_level;

  thread_obj->thread_state |= TORCH_MONITOR_THREAD_STATE_BACKWARD;

  TORCH_MONITOR_MSG("Enter backward level %u state %p sequence_number %d", nested_level,
                    thread_obj->thread_state, sequence_number);

  if (sequence_number == -1) {
    // This op may not have a corresponding backward call
    return;
  }

  // Overwrite forward domain, even upcoming forward calls are in the backward
  // domain
  thread_obj->domain = TORCH_MONITOR_DOMAIN_BACKWARD_FUNCTION;

  function_key_t key = {.forward_thread_id = forward_thread_id,
                        .sequence_number = sequence_number};

  torch_monitor_function_cct_map_entry_t *entry =
      torch_monitor_function_cct_map_lookup(key);
  if (entry != NULL) {
    TORCH_MONITOR_MSG("Lookup forward_thread_id %lu sequence_number %ld",
                      forward_thread_id, sequence_number);
    cct_node_t *cct = torch_monitor_function_cct_map_entry_cct_get(entry);
    assert(cct != NULL);
    backward_function_cct_update(cct, thread_obj);
  }
}

static void forward_cleanup_callback(
    const torch_monitor_callback_data_t *callback_data,
    torch_monitor_thread_obj_t *thread_obj) {
  uint32_t nested_level = callback_data->data.op_data.nested_level;

  TORCH_MONITOR_MSG("Exit forward level %u state %p", nested_level,
                    thread_obj->thread_state);
}

static void backward_cleanup_callback(
    const torch_monitor_callback_data_t *callback_data,
    torch_monitor_thread_obj_t *thread_obj) {
  uint32_t nested_level = callback_data->data.op_data.nested_level;

  TORCH_MONITOR_MSG("Exit backward level %u state %p", nested_level,
                    thread_obj->thread_state);
}

static void torch_monitor_callback(
    torch_monitor_callback_site_t callback_site,
    torch_monitor_callback_data_t *callback_data) {
  bool unsafe = hpcrun_safe_enter();

  TORCH_MONITOR_MSG("Enter torch_monitor_callback");

  torch_monitor_thread_obj_t *thread_obj = torch_monitor_thread_obj_get();

  if (callback_site == TORCH_MONITOR_CALLBACK_ENTER) {
    if (callback_data->domain == TORCH_MONITOR_DOMAIN_FUNCTION) {
      forward_function_callback(callback_data, thread_obj);
    } else if (callback_data->domain ==
               TORCH_MONITOR_DOMAIN_BACKWARD_FUNCTION) {
      backward_function_callback(callback_data, thread_obj);
    }
    // TODO(Keren)
    // else if (callback_data->domain == TORCH_MONITOR_DOMAIN_MEMORY) {
    //}
  } else {
    if (callback_data->domain == TORCH_MONITOR_DOMAIN_FUNCTION) {
      forward_cleanup_callback(callback_data, thread_obj);
    } else if (callback_data->domain ==
               TORCH_MONITOR_DOMAIN_BACKWARD_FUNCTION) {
      backward_cleanup_callback(callback_data, thread_obj);
    }
  }

  TORCH_MONITOR_MSG("Exit torch_monitor_callback");

  if (unsafe) hpcrun_safe_exit();
}

void torch_monitor_start(bool native_stack) {
  TORCH_MONITOR_MSG("Enter torch_monitor_start");

  torch_monitor_enabled = true;

  torch_monitor_logical_register(native_stack);

  TORCH_MONITOR_CALL(torch_monitor_domain_enable,
                     (TORCH_MONITOR_DOMAIN_FUNCTION));
  TORCH_MONITOR_CALL(torch_monitor_domain_enable,
                     (TORCH_MONITOR_DOMAIN_BACKWARD_FUNCTION));
  TORCH_MONITOR_CALL(torch_monitor_callback_subscribe,
                     (torch_monitor_callback));
  TORCH_MONITOR_CALL(torch_monitor_init, ());

  TORCH_MONITOR_MSG("Exit torch_monitor_start");
}

void torch_monitor_stop(void) {
  TORCH_MONITOR_MSG("Enter torch_monitor_start");

  torch_monitor_enabled = false;

  torch_monitor_logical_unregister();

  TORCH_MONITOR_CALL(torch_monitor_finalize, ());

  TORCH_MONITOR_MSG("Exit torch_monitor_stop");
}
