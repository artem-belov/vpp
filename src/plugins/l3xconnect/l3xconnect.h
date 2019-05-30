/*
 * Copyright (c) 2019 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __included_l3xconnect_h__
#define __included_l3xconnect_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/elog.h>

typedef struct {
    u32 sw_if_index;
    u32 xs_sw_if_index;
} l3xconnect_if_node_t;

typedef struct {
    /* API message ID base */
    u16 msg_id_base;

    l3xconnect_if_node_t* sw_if_pool;
    uword *sw_if_hash;

    /* convenience */
    vnet_main_t * vnet_main;
} l3xconnect_main_t;

extern l3xconnect_main_t l3xconnect_main;

extern vlib_node_registration_t l3xconnect_node;

always_inline void 
l3xconnect_add_if_hash_node(l3xconnect_main_t * l3xm, u32 sw_if_index,
                                   u32 xs_sw_if_index) 
{
  
  l3xconnect_if_node_t *if_node = NULL;

  uword *p;
  
  p = hash_get (l3xm->sw_if_hash, sw_if_index);
  if (p) 
    {
      if_node = pool_elt_at_index (l3xm->sw_if_pool, p[0]);
      if_node->xs_sw_if_index = xs_sw_if_index;
    } 
  else 
    {
      pool_get (l3xm->sw_if_pool, if_node);
      if_node->sw_if_index = sw_if_index;
      if_node->xs_sw_if_index = xs_sw_if_index;
      hash_set(l3xm->sw_if_hash, sw_if_index, if_node - l3xm->sw_if_pool);
    }
}

always_inline l3xconnect_if_node_t* 
l3xconnect_get_if_hash_node(l3xconnect_main_t * l3xm, u32 sw_if_index) 
{
  uword *p;
  
  p = hash_get (l3xm->sw_if_hash, sw_if_index);
  if (p) 
    return pool_elt_at_index (l3xm->sw_if_pool, p[0]);
    
  return NULL;
}

always_inline void 
l3xconnect_del_if_hash_node(l3xconnect_main_t * l3xm, u32 sw_if_index) 
{
  l3xconnect_if_node_t *if_node = NULL;
  u32 if_index = 0;
  u32 index = 0;

  hash_foreach(if_index, index, l3xm->sw_if_hash,
  ({
      if_node = pool_elt_at_index (l3xm->sw_if_pool, index);
      if (if_node->sw_if_index == sw_if_index || if_node->xs_sw_if_index == sw_if_index)
        {
          hash_unset (l3xm->sw_if_hash, sw_if_index);
          pool_put (l3xm->sw_if_pool, if_node);
        }
  }));
}

#define L3XCONNECT_PLUGIN_BUILD_VER "1.0"

#endif /* __included_l3xconnect_h__ */
