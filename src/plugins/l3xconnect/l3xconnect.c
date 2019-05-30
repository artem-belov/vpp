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
/**
 * @file
 * @brief L3xconnect Plugin, plugin API / trace / CLI handling.
 */

#include <vnet/l2/l2_input.h>
#include <vnet/interface_funcs.h>

#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <l3xconnect/l3xconnect.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

/* define message IDs */
#include <l3xconnect/l3xconnect_msg_enum.h>

/* define message structures */
#define vl_typedefs
#include <l3xconnect/l3xconnect_all_api_h.h> 
#undef vl_typedefs

/* define generated endian-swappers */
#define vl_endianfun
#include <l3xconnect/l3xconnect_all_api_h.h> 
#undef vl_endianfun

/* instantiate all the print functions we know about */
#define vl_print(handle, ...) vlib_cli_output (handle, __VA_ARGS__)
#define vl_printfun
#include <l3xconnect/l3xconnect_all_api_h.h> 
#undef vl_printfun

/* Get the API version number */
#define vl_api_version(n,v) static u32 api_version=(v);
#include <l3xconnect/l3xconnect_all_api_h.h>
#undef vl_api_version

#define REPLY_MSG_ID_BASE l3xm->msg_id_base
#include <vlibapi/api_helper_macros.h>

/* List of message types that this plugin understands */

#define foreach_l3xconnect_plugin_api_msg                           \
_(L3XCONNECT_MACSWAP_ENABLE_DISABLE, l3xconnect_macswap_enable_disable)

/* *INDENT-OFF* */
VLIB_PLUGIN_REGISTER () = {
    .version = L3XCONNECT_PLUGIN_BUILD_VER,
    .description = "L3xconnect of VPP Plugin",
};
/* *INDENT-ON* */

l3xconnect_main_t l3xconnect_main;

static clib_error_t *
l3xconnect_interface_add_del (vnet_main_t * vnm,
				u32 sw_if_index, u32 is_add)
{
  l3xconnect_main_t *l3xm = &l3xconnect_main;
  
  printf("l3xconnect %d %d \n", sw_if_index, is_add);
  if (!is_add) {
    l3xconnect_del_if_hash_node(l3xm, sw_if_index);
  }

  return (NULL);
}

VNET_SW_INTERFACE_ADD_DEL_FUNCTION (l3xconnect_interface_add_del);

/**
 * @brief Enable/disable the macswap plugin. 
 *
 * Action function shared between message handler and debug CLI.
 */

int 
l3xconnect_macswap_enable_disable (l3xconnect_main_t * l3xm, u32 sw_if_index,
                                   u32 xs_sw_if_index, int enable_disable)
{
  vnet_sw_interface_t * sw;
  int rv = 0;

  /* Utterly wrong? */
  if (pool_is_free_index (l3xm->vnet_main->interface_main.sw_interfaces, 
                          sw_if_index))
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;

  /* Not a physical port? */
  sw = vnet_get_sw_interface (l3xm->vnet_main, sw_if_index);
  if (sw->type != VNET_SW_INTERFACE_TYPE_HARDWARE)
    return VNET_API_ERROR_INVALID_SW_IF_INDEX;
  
  l3xconnect_add_if_hash_node(l3xm, sw_if_index, xs_sw_if_index);
  
  vnet_feature_enable_disable ("device-input", "l3xconnect",
                               sw_if_index, enable_disable, NULL, 0);

  return rv;
}

static clib_error_t *
int_l3_xc (vlib_main_t * vm,
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd)
{
  l3xconnect_main_t * l3xm = &l3xconnect_main;
  vnet_main_t *vnm = vnet_get_main ();
  clib_error_t *error = 0;
  u32 sw_if_index;
  u32 xc_sw_if_index;
  u32 is_del = 0;

  if (unformat (input, "del"))
    is_del = 1;

  if (!unformat_user (input, unformat_vnet_sw_interface, vnm, &sw_if_index))
    {
      error = clib_error_return (0, "unknown interface `%U'",
				 format_unformat_error, input);
      goto done;
    }

  if (!unformat_user
      (input, unformat_vnet_sw_interface, vnm, &xc_sw_if_index))
    {
      error = clib_error_return (0, "unknown peer interface `%U'",
				 format_unformat_error, input);
      goto done;
    }

  /* set the interface mode */
  if (set_int_l2_mode (vm, vnm, MODE_L3, sw_if_index, 0,
		       L2_BD_PORT_TYPE_NORMAL, 0, 0))
    {
      error = clib_error_return (0, "invalid configuration for interface",
				 format_unformat_error, input);
      goto done;
    }

  printf("%d %d %d\n", sw_if_index, xc_sw_if_index, !is_del);
  
  l3xconnect_macswap_enable_disable(l3xm, sw_if_index, xc_sw_if_index, !is_del);

done:
  return error;
}

VLIB_CLI_COMMAND (int_l3_xc_cli, static) = {
  .path = "set interface l3 xconnect",
  .short_help = "set interface l3 xconnect <interface> <peer interface>",
  .function = int_l3_xc,
};

static clib_error_t *
show_int_l3_xc (vlib_main_t * vm,
                                   unformat_input_t * input,
                                   vlib_cli_command_t * cmd)
{
  l3xconnect_main_t * l3xm = &l3xconnect_main;
  vnet_main_t *vnm = vnet_get_main ();
  l3xconnect_if_node_t *if_node = NULL;
  u32 if_index = 0;
  u32 index = 0;

  hash_foreach(if_index, index, l3xm->sw_if_hash,
  ({
    if_node = pool_elt_at_index (l3xm->sw_if_pool, index);
    vlib_cli_output (vm, "%U -> %U", format_vnet_sw_if_index_name, vnm, if_node->sw_if_index, 
                                    format_vnet_sw_if_index_name, vnm, if_node->xs_sw_if_index);
  }));

  return NULL;
}

VLIB_CLI_COMMAND (show_int_l3_xc_cli, static) = {
  .path = "show interface l3 xconnect",
  .short_help = "show interface l3 xconnect <interface>",
  .function = show_int_l3_xc,
};

/**
 * @brief Plugin API message handler.
 */
static void vl_api_l3xconnect_macswap_enable_disable_t_handler
(vl_api_l3xconnect_macswap_enable_disable_t * mp)
{
  vl_api_l3xconnect_macswap_enable_disable_reply_t * rmp;
  l3xconnect_main_t * l3xm = &l3xconnect_main;
  int rv;

  rv = l3xconnect_macswap_enable_disable (l3xm, ntohl(mp->sw_if_index), 
                      ntohl(mp->xc_sw_if_index), (int) (mp->enable_disable));
  
  REPLY_MACRO(VL_API_L3XCONNECT_MACSWAP_ENABLE_DISABLE_REPLY);
}

/**
 * @brief Set up the API message handling tables.
 */
static clib_error_t *
l3xconnect_plugin_api_hookup (vlib_main_t *vm)
{
  l3xconnect_main_t * l3xm = &l3xconnect_main;
#define _(N,n)                                                  \
    vl_msg_api_set_handlers((VL_API_##N + l3xm->msg_id_base),     \
                           #n,					\
                           vl_api_##n##_t_handler,              \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1); 
    foreach_l3xconnect_plugin_api_msg;
#undef _

    return 0;
}

#define vl_msg_name_crc_list
#include <l3xconnect/l3xconnect_all_api_h.h>
#undef vl_msg_name_crc_list

static void 
setup_message_id_table (l3xconnect_main_t * l3xm, api_main_t *am)
{
#define _(id,n,crc) \
  vl_msg_api_add_msg_name_crc (am, #n "_" #crc, id + l3xm->msg_id_base);
  foreach_vl_msg_name_crc_l3xconnect;
#undef _
}

/**
 * @brief Initialize the l3xconnect plugin.
 */
static clib_error_t * l3xconnect_init (vlib_main_t * vm)
{
  l3xconnect_main_t * l3xm = &l3xconnect_main;
  clib_error_t * error = 0;
  u8 * name;

  l3xm->vnet_main =  vnet_get_main ();

  l3xm->sw_if_hash = hash_create (0, sizeof (uword));

  name = format (0, "l3xconnect_%08x%c", api_version, 0);

  /* Ask for a correctly-sized block of API message decode slots */
  l3xm->msg_id_base = vl_msg_api_get_msg_ids 
      ((char *) name, VL_MSG_FIRST_AVAILABLE);

  error = l3xconnect_plugin_api_hookup (vm);

  /* Add our API messages to the global name_crc hash table */
  setup_message_id_table (l3xm, &api_main);

  vec_free(name);

  return error;
}

VLIB_INIT_FUNCTION (l3xconnect_init);

/**
 * @brief Hook the l3xconnect plugin into the VPP graph hierarchy.
 */
VNET_FEATURE_INIT (l3xconnect, static) = 
{
  .arc_name = "device-input",
  .node_name = "l3xconnect",
  .runs_before = VNET_FEATURES ("ethernet-input"),
};
