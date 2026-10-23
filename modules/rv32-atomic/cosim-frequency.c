/*
  cosim-frequency.c - CPU frequency interfaces

  © 2026 Intel Corporation

  This software and the related documents are Intel copyrighted materials, and
  your use of them is governed by the express license under which they were
  provided to you ("License"). Unless the License provides otherwise, you may
  not use, modify, copy, publish, distribute, disclose or transmit this software
  or the related documents without Intel's prior written permission.

  This software and the related documents are provided as is, with no express or
  implied warranties, other than those that are expressly stated in the License.


  Parts of this product may be derived from systems developed at the Swedish
  Institute of Computer Science (SICS). Licensed from SICS.

  Copyright (c) 1991-1997 SICS
*/

#include "cosim-frequency.h"
#include <simics/processor-api.h>
#include <simics/base/log.h>

#include "generic-cycle-iface.h"
#ifndef COSIM_HEADER
 #define COSIM_HEADER "cosim.h"
#endif
#include COSIM_HEADER

static void
cpu_set_freq_hz(cosim_t *cosim, uint64 new_freq)
{
        uint64 old_freq = cosim->freq_hz;
        if (new_freq != old_freq) {
                VT_clock_frequency_about_to_change(cosim_to_conf_obj(cosim));
                if (old_freq != 0) {
                        cosim->time_offset = adjusted_time_offset(
                                cosim->time_offset, cosim->current_cycle,
                                old_freq, new_freq);
                        ASSERT(!bigtime_is_illegal(cosim->time_offset));
                        rescale_time_events(&cosim->cycle_queue,
                                            old_freq, new_freq);
                }
                cosim->freq_hz = new_freq;
                VT_clock_frequency_change(cosim_to_conf_obj(cosim), cosim->freq_hz);
                frequency_target_t *tgt;
                VFOREACH(cosim->frequency_targets, tgt) {
                        tgt->iface->set(tgt->object, new_freq, 1);
                }
        }
}

static void
set_freq_hz_rational(cosim_t *cpu, uint64 numerator, uint64 denominator)
{
        uint64 freq = 1;
        if (numerator == 0 || denominator == 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cpu), 0,
                            "Got invalid frequency from bus, setting to 1 Hz");
        } else {
                freq = (numerator + (denominator / 2)) / denominator;
                if (freq == 0) {
                        /* very low frequency, round up to 1 Hz */
                        freq = 1;
                }
        }

        if (cpu->freq_hz != freq)
                cpu_set_freq_hz(cpu, freq);
}

/*
 * frequency attribute functions
 */
static set_error_t
set_frequency(conf_object_t *obj, attr_value_t *val)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);
        conf_object_t *new_source;
        const char *new_port;

        if (SIM_attr_is_list(*val)) {
                attr_value_t *vect = SIM_attr_list(*val);
                if (SIM_attr_is_integer(vect[0])) {
                        uint64 num = SIM_attr_integer(vect[0]);
                        uint64 den = SIM_attr_integer(vect[1]);
                        if (num == 0 || den == 0)
                                return Sim_Set_Illegal_Value;
                        set_freq_hz_rational(cpu, num, den);
                        new_source = NULL;
                        new_port = NULL;
                } else {
                        new_source = SIM_attr_object(vect[0]);
                        new_port = SIM_attr_string(vect[1]);
                }
        } else {
                new_source = SIM_attr_object(*val);
                new_port = NULL;
        }

        conf_object_t *cur_source = cpu->frequency_dispatcher;
        char *cur_port = cpu->frequency_dispatcher_port;
        int equal = new_source == cur_source;
        if (equal && new_source != NULL) {
                if (new_port == NULL)
                        equal = cur_port == NULL;
                else
                        equal = cur_port != NULL
                                && strcmp(new_port, cur_port) != 0;
        }
        if (equal) {
                return Sim_Set_Ok;
        } else {
                // Source changed, update subscriptions
                const simple_dispatcher_interface_t *iface;
                if (new_source != NULL) {
                        iface = SIM_c_get_port_interface(
                                new_source,
                                SIMPLE_DISPATCHER_INTERFACE,
                                new_port);

                        if (iface == NULL) {
                                SIM_LOG_ERROR(
                                        cosim_to_conf_obj(cpu), 0,
                                        "Object '%s' does not implement the "
                                        SIMPLE_DISPATCHER_INTERFACE
                                        " interface",
                                        SIM_object_name(new_source));
                                return Sim_Set_Interface_Not_Found;
                        }
                } else {
                        iface = NULL;
                }

                if (cur_source != NULL)
                        cpu->frequency_dispatcher_iface->unsubscribe(
                                cur_source, obj, NULL);
                cpu->frequency_dispatcher = new_source;
                if (cur_port != NULL)
                        MM_FREE(cur_port);
                cpu->frequency_dispatcher_port = new_port == NULL
                        ? NULL : MM_STRDUP(new_port);
                cpu->frequency_dispatcher_iface = iface;
                /* if not configured, the subscription is delayed
                   until finalize_frequency. */
                if (SIM_object_is_configured(obj) && new_source != NULL)
                        iface->subscribe(new_source, obj, NULL);
                return Sim_Set_Ok;
        }
}

static attr_value_t
get_frequency(conf_object_t *obj)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);
        conf_object_t *source = cpu->frequency_dispatcher;
        const char *port = cpu->frequency_dispatcher_port;
        if (source == NULL) {
                return SIM_make_attr_list(
                        2,
                        SIM_make_attr_uint64(cpu->freq_hz),
                        SIM_make_attr_uint64(1));
        } else if (port == NULL) {
            return SIM_make_attr_object(source);
        } else {
            return SIM_make_attr_list(2, SIM_make_attr_object(source),
                                       SIM_make_attr_string(port));
        }
}

/*
 * freq_mhz attribute functions
 */
static set_error_t
set_freq_mhz(conf_object_t *obj, attr_value_t *val)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);

        uint64 freq;
        if (SIM_attr_is_integer(*val))
                freq = 1000000 * SIM_attr_integer(*val);
        else if (SIM_attr_is_floating(*val))
                freq = (uint64)(SIM_attr_floating(*val) * 1000000 + 0.5);
        else
                return Sim_Set_Illegal_Value; /* should not happen */

        if (freq <= 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cpu), 0,
                              "CPU frequency must be positive");
                return Sim_Set_Illegal_Value;
        }

        /* Use the real frequency attribute setter, to be sure
           everything is done correctly. */
        attr_value_t q = SIM_make_attr_list(2,
                                            SIM_make_attr_uint64(freq),
                                            SIM_make_attr_uint64(1));
        /* dont_care and idx are irrelevant for both attributes, so
           just pass them on */
        set_error_t result = set_frequency(obj, &q);
        SIM_attr_free(&q);
        return result;
}

static attr_value_t
get_freq_mhz(conf_object_t *obj)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);
        if (cpu->freq_hz % 1000000 == 0)
                return SIM_make_attr_uint64(cpu->freq_hz / 1000000);
        return SIM_make_attr_floating(cpu->freq_hz / 1000000.0);
}

/* Find whether (target, port) listens to this bus. If so, return
   index in target list; if not, return -1. TODO: Code is duplicated
   in frequency-bus.dml */
static int
find_frequency_target(frequency_target_list_t *targets, conf_object_t *object,
                      const char *port)
{
        VFORI(*targets, i) {
                frequency_target_t *tgt;
                tgt = &VGET(*targets, i);

                int equal = 1;
                if (tgt->object != object) {
                        equal = 0;
                } else {
                        if (port == NULL) {
                                if (tgt->port_name != NULL)
                                        equal = 0;
                        } else {
                                if (tgt->port_name == NULL
                                    || strcmp(tgt->port_name, port) != 0)
                                        equal = 0;
                        }
                }
                if (equal)
                        return i;
        }
        return -1;
}

/*
 * frequency dispatcher interface functions
 */
static void
frequency_dispatcher_subscribe(conf_object_t *obj,
                               conf_object_t *target,
                               const char *port)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);

        int dup = find_frequency_target(&cpu->frequency_targets,
                                        target, port);
        if (dup >= 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cpu), 0,
                              "simple_dispatcher.subscribe:"
                              " Object '%s' registered twice on the same port",
                              SIM_object_name(target));
                return;
        }
        const frequency_listener_interface_t *iface =
                SIM_c_get_port_interface(target, FREQUENCY_LISTENER_INTERFACE,
                                         port);
        if (iface == NULL) {
                SIM_LOG_ERROR(
                        cosim_to_conf_obj(cpu), 0,
                        "simple_dispatcher.subscribe:"
                        " Object '%s' does not implement the %s interface",
                        SIM_object_name(target), FREQUENCY_LISTENER_INTERFACE);
                return;
        }

        frequency_target_t one_target;
        one_target.object = target;
        one_target.port_name = port == NULL ? NULL : MM_STRDUP(port);
        one_target.iface = iface;

        VADD(cpu->frequency_targets, one_target);
        iface->set(target, cpu->freq_hz, 1);
}

static void
frequency_dispatcher_unsubscribe(conf_object_t *obj,
                                 conf_object_t *target,
                                 const char *port)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);

        frequency_target_list_t *tgts = &cpu->frequency_targets;
        int dup = find_frequency_target(tgts,
                                        target, port);
        if (dup < 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cpu), 0,
                              "simple_dispatcher.unsubscribe:"
                              " Object %s port %s doesn't currently listen",
                              SIM_object_name(target),
                              port == NULL ? "none" : port);
        } else {
                MM_FREE(VGET(*tgts, dup).port_name);
                VREMOVE(*tgts, dup);
        }
}

static void
port_frequency_dispatcher_subscribe(conf_object_t *pobj,
                                    conf_object_t *target,
                                    const char *port)
{
        frequency_dispatcher_subscribe(SIM_port_object_parent(pobj),
                                       target, port);
}

static void
port_frequency_dispatcher_unsubscribe(conf_object_t *pobj,
                                      conf_object_t *target,
                                      const char *port)
{
        frequency_dispatcher_unsubscribe(SIM_port_object_parent(pobj),
                                         target, port);
}

/*
 * frequency listener interface functions
 */
static void
frequency_listener_set(conf_object_t *obj,
                       uint64 numerator, uint64 denominator)
{
        cosim_t *cpu = conf_obj_to_cosim(obj);
        set_freq_hz_rational(cpu, numerator, denominator);
}

void
instantiate_frequency(cosim_t *cosim)
{
        VINIT(cosim->frequency_targets);
        cosim->frequency_dispatcher = NULL;
        cosim->frequency_dispatcher_port = NULL;
}

void
finalize_frequency(cosim_t *cosim)
{
        conf_object_t *dispatcher = cosim->frequency_dispatcher;
        if (dispatcher != NULL) {
                const simple_dispatcher_interface_t *iface
                        = cosim->frequency_dispatcher_iface;
                SIM_require_object(dispatcher);
                cosim->freq_hz = 0;
                iface->subscribe(dispatcher, cosim_to_conf_obj(cosim), NULL);
                if (cosim->freq_hz == 0) {
                        SIM_LOG_ERROR(cosim_to_conf_obj(cosim), 0,
                                      "Frequency dispatcher %s did not set"
                                      " a frequency immediately upon"
                                      " subscription."
                                      " Using frequency 1 MHz for now.",
                                      SIM_object_name(dispatcher));
                        cosim->freq_hz = 1000000;
                }
        } else if (cosim->freq_hz == 0) {
                SIM_LOG_ERROR(cosim_to_conf_obj(cosim), 0,
                              "Neither the freq_mhz attribute nor the"
                              " frequency attribute was set in the"
                              " configuration. Either one must be defined."
                              " Using frequency 1 MHz for now.");
                cosim->freq_hz = 1000000;
        }
}

/*
 * time offset attribute functions
 */
static set_error_t
set_time_offset(conf_object_t *obj, attr_value_t *val)
{
        cosim_t *cosim = conf_obj_to_cosim(obj);
        cosim->time_offset = time_offset_from_attr(*val);
        return Sim_Set_Ok;
}

static attr_value_t
get_time_offset(conf_object_t *obj)
{
        const cosim_t *cosim = conf_obj_to_cosim(obj);
        return time_offset_as_attr(cosim->time_offset);
}

/*
  Register the interfaces of a CPU that are related to reading and
  writing the CPU frequency.
*/
void
register_frequency_interfaces(conf_class_t *cls)
{
        static const simple_dispatcher_interface_t port_sdi = {
                .subscribe = port_frequency_dispatcher_subscribe,
                .unsubscribe = port_frequency_dispatcher_unsubscribe,
        };
        conf_class_t *freq_port = SIM_register_simple_port(
                cls, "port.cpu_frequency",
                "Broadcasts changes in CPU frequency.");
        SIM_REGISTER_INTERFACE(freq_port, simple_dispatcher, &port_sdi);

        static const simple_dispatcher_interface_t sdi = {
                .subscribe = frequency_dispatcher_subscribe,
                .unsubscribe = frequency_dispatcher_unsubscribe,
        };
        /* Registering the dispatcher interface on a port: If a CPU
           scales the input frequency, it might be sensible to add
           ports for broadcasting changes in frequency multiplier and
           input frequency. */
        SIM_REGISTER_PORT_INTERFACE(cls, simple_dispatcher, &sdi,
                                    "cpu_frequency",
                                    "Broadcasts changes in CPU frequency.");

        static const frequency_listener_interface_t fi = {
                .set = frequency_listener_set
        };
        SIM_REGISTER_INTERFACE(cls, frequency_listener, &fi);

        SIM_register_attribute(
                cls, "freq_mhz",
                get_freq_mhz, set_freq_mhz,
                Sim_Attr_Pseudo | Sim_Init_Phase_Pre1,
                "i|f",
                "Processor clock frequency in MHz.");

        SIM_register_attribute(
                cls, "frequency",
                get_frequency, set_frequency,
                Sim_Attr_Optional | Sim_Init_Phase_Pre1,
                "[ii]|o|[os]",
                "Processor clock frequency in Hz, as a rational number"
                " [numerator, denominator].");

        SIM_register_attribute(
                cls, "time_offset",
                get_time_offset,
                set_time_offset,
                Sim_Attr_Optional | Sim_Attr_Internal | Sim_Init_Phase_Pre1,
                TIME_OFFSET_ATTRTYPE,
                "An internal coefficient used to correlate the cycle counter"
                " to the absolute time.");
}
