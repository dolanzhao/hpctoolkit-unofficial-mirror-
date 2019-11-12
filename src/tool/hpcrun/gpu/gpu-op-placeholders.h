#ifndef gpu_op_placeholders_h
#define gpu_op_placeholders_h



//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/utilities/ip-normalized.h>
#include <hpcrun/hpcrun-placeholders.h>



//******************************************************************************
// type declarations
//******************************************************************************

typedef enum gpu_placeholder_type_t {
  gpu_placeholder_type_copy    = 0, // general copy, d2d d2a, or a2d
  gpu_placeholder_type_copyin  = 1,
  gpu_placeholder_type_copyout = 2,
  gpu_placeholder_type_alloc   = 3,
  gpu_placeholder_type_delete  = 4,
  gpu_placeholder_type_kernel  = 5,
  gpu_placeholder_type_sync    = 6,
  gpu_placeholder_type_trace   = 7,

  gpu_placeholder_type_count   = 8
} gpu_placeholder_type_t;


typedef uint32_t gpu_op_placeholder_flags_t;


typedef struct gpu_op_ccts_t {
  cct_node_t *ccts[gpu_placeholder_type_count];
} gpu_op_ccts_t;


//******************************************************************************
// global data
//******************************************************************************

extern gpu_op_placeholder_flags_t gpu_op_placholder_flags_all;
extern gpu_op_placeholder_flags_t gpu_op_placholder_flags_none;



//******************************************************************************
// interface operations
//******************************************************************************

ip_normalized_t
gpu_op_placeholder_ip
(
 gpu_placeholder_type_t type
);


void
gpu_op_ccts_insert
(
 cct_node_t *api_node,
 gpu_op_ccts_t *gpu_op_ccts,
 gpu_op_placeholder_flags_t flags
);


void
gpu_op_placeholder_flags_set
(
 gpu_op_placeholder_flags_t *flags,
 gpu_placeholder_type_t type
);


bool
gpu_op_placeholder_flags_is_set
(
 gpu_op_placeholder_flags_t flags,
 gpu_placeholder_type_t type
);



#endif
