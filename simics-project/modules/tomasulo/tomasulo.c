/*
  © 2016 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.


  Simple instrumentation tool that just counts how instructions that are
  executed on the connected processors.
*/

#include <simics/simulator/conf-object.h>
#include <simics/simulator-iface/instrumentation-tool.h>
#include <simics/model-iface/cpu-instrumentation.h>
#include <simics/device-api.h>
#include "xed-interface.h"

conf_class_t *connection_class;

/* The connection (between tool and provider) */
typedef struct {
        conf_object_t obj;
        conf_object_t *provider;

        const cpu_instrumentation_subscribe_interface_t *cis_iface;
        const cpu_cached_instruction_interface_t *cci_iface;
} connection_t;

/* The Simics instrumentation tool */
typedef struct {
        conf_object_t obj;
        int next_connection_number;
        VECT(connection_t *) connections;        
} tomasulo_t;

FORCE_INLINE tomasulo_t *
tool_of_obj(conf_object_t *obj) { return (tomasulo_t *)obj; }

FORCE_INLINE connection_t *
conn_of_obj(conf_object_t *obj) { return (connection_t *)obj; }

/*************************** Connection Object ********************************/

// class_data_t(connection_t)::alloc_object
static conf_object_t *
ic_alloc(void *arg)
{
        connection_t *conn = MM_ZALLOC(1, connection_t);
        return &conn->obj;
}

// class_data_t(connection_t)::pre_delete_instance
static void
ic_pre_delete_connection(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->remove_connection_callbacks(conn->provider, obj);
}

// class_data_t(connection_t)::delete_instance
static int
ic_delete_connection(conf_object_t *obj)
{
        MM_FREE(obj);
        return 0;
}

// instrumentation_connection::enable
static void
ic_enable(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->enable_connection_callbacks(conn->provider, obj);
}

// instrumentation_connection::disable
static void
ic_disable(conf_object_t *obj)
{
        connection_t *conn = conn_of_obj(obj);
        conn->cis_iface->disable_connection_callbacks(conn->provider, obj);

}

static void 
tomasulo_algorithm (conf_object_t * obj, conf_object_t * cpu, 
		instruction_handle_t *i_handle,
		lang_void * user_data) 
{
	SIM_log_info(1, obj, 0, "TOMASULO WAS CALLED");	
}

static void
cached_instruction(conf_object_t *obj, conf_object_t *cpu,
                   cached_instruction_handle_t *ci_handle,
                   instruction_handle_t *iq_handle,
                   lang_void *user_data)
{
        connection_t *conn = conn_of_obj(obj);
	conn->cis_iface->register_instruction_before_cb(cpu, obj, tomasulo_algorithm, NULL);
}


/* Help function which creates a new connection and returns it. */
static connection_t *
new_connection(tomasulo_t *tool, conf_object_t *provider, attr_value_t args)
{
        strbuf_t sb = SB_INIT;
        
        sb_addfmt(&sb, "%s_%d",
                  SIM_object_name(&tool->obj),
                  tool->next_connection_number);
        conf_object_t *conn_obj = SIM_create_object(
                connection_class, sb_str(&sb), args);
        sb_free(&sb);
        
        if (!conn_obj)
                return NULL;

        tool->next_connection_number++;
        connection_t *conn = conn_of_obj(conn_obj);
        conn->provider = provider;

        /* Cache interfaces */
        conn->cis_iface = SIM_C_GET_INTERFACE(
                provider, cpu_instrumentation_subscribe);
        conn->cci_iface = SIM_C_GET_INTERFACE(
                provider, cpu_cached_instruction);

        /* Request cached_instruction to be called when an instruction is
           cached in Simics. */
        conn->cis_iface->register_cached_instruction_cb(
                provider, conn_obj, cached_instruction, NULL);

        return conn;
}

static attr_value_t
get_provider(void *param, conf_object_t *obj, attr_value_t *idx)
{
        connection_t *conn = conn_of_obj(obj);
        return SIM_make_attr_object(conn->provider);
}

/************************** Tool Object ********************************/

// instruction_count_tool::alloc
static conf_object_t *
it_alloc(void *arg)
{
        tomasulo_t *tool = MM_ZALLOC(1, tomasulo_t);
        VINIT(tool->connections);
        return &tool->obj;
}

static int
it_delete_connection(conf_object_t *obj)
{
        tomasulo_t *tool = tool_of_obj(obj);
        ASSERT_FMT(VEMPTY(tool->connections),
                   "%s deleted with active connections", SIM_object_name(obj));
        MM_FREE(tool);
        return 0;
}

// instrumentation_tool::connect()
static conf_object_t *
it_connect(conf_object_t *obj, conf_object_t *provider, attr_value_t args)
{
        tomasulo_t *tool = tool_of_obj(obj);
        connection_t *conn = new_connection(tool, provider, args);
        if (!conn)
                return NULL;
        
        VADD(tool->connections, conn);
        
        return &conn->obj;
}

// instrumentation_tool::disconnect()
static void
it_disconnect(conf_object_t *obj, conf_object_t *conn_obj)
{
        tomasulo_t *tool = tool_of_obj(obj);
        VREMOVE_FIRST_MATCH(tool->connections, conn_of_obj(conn_obj));
        SIM_delete_object(conn_obj);
}

static attr_value_t
get_connections(void *param, conf_object_t *obj, attr_value_t *idx)
{
        tomasulo_t *tool = tool_of_obj(obj);
        
        attr_value_t ret = SIM_alloc_attr_list(VLEN(tool->connections));
        VFORI(tool->connections, i) {
                SIM_attr_list_set_item(
                        &ret, i,
                        SIM_make_attr_object(&VGET(tool->connections, i)->obj));
        }
        return ret;
}

static void
init_tool_class(void)
{
        const class_data_t funcs = {
                .alloc_object = it_alloc,
                .delete_instance = it_delete_connection,
                .class_desc = "Implements the tomasulo algorithm",
                .description = "This tool takes all the instructions from the provider"
                " and applies the tomasulo algorithm on them."
                " processor.",
                .kind = Sim_Class_Kind_Session
        };

        conf_class_t *cl = SIM_register_class("tomasulo", &funcs);
        
        static const instrumentation_tool_interface_t it_iface = {
                .connect = it_connect,
                .disconnect = it_disconnect
        };
        SIM_REGISTER_INTERFACE(cl, instrumentation_tool, &it_iface);        

        SIM_register_typed_attribute(
               cl, "connections",
               get_connections, NULL,
               NULL, NULL,
               Sim_Attr_Pseudo,
               "[o*]", NULL,
               "The connection objects currently used.");
}

static void
init_connection_class(void)
{
        const class_data_t funcs = {
                .alloc_object = ic_alloc,
                .pre_delete_instance = ic_pre_delete_connection,
                .class_desc = "instrumentation connection",
                .description = "Instrumentation connection.",
                .delete_instance = ic_delete_connection,
                .kind = Sim_Class_Kind_Session
        };
        connection_class = SIM_register_class("tomasulo_connection_t",
                                              &funcs);
        
        static const instrumentation_connection_interface_t ic_iface = {
                .enable = ic_enable,
                .disable = ic_disable,
        };
        SIM_REGISTER_INTERFACE(connection_class,
                               instrumentation_connection, &ic_iface);

        SIM_register_typed_attribute(
                connection_class, "provider",
                get_provider, NULL,
                NULL, NULL,
                Sim_Attr_Pseudo,
                "o", NULL,
                "The provider of the connection.");

}

void
init_local(void)
{
        init_tool_class();
        init_connection_class();
}

