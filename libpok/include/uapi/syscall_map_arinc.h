/*
 * GENERATED! DO NOT MODIFY!
 *
 * Instead of modifying this file, modify the one it generated from (kernel/include/uapi/syscall_map_arinc.h.in).
 */
/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: added the
 * POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE syscall, used by
 * REPORT_APPLICATION_MESSAGE.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: added the
 * POK_SYSCALL_ERROR_HANDLER_CONFIGURE syscall, used by
 * CONFIGURE_ERROR_HANDLER.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: deviations are
 * documented where a guideline can't be followed. Parameters named "time"
 * are renamed (libpok defines the function time()), and the parameters of
 * pok_thread_delayed_start() have the names of its definition.
 *
 * Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: deviations are
 * documented where a guideline can't be followed. In the kernel, the
 * syscall implementations are no longer declared again next to their
 * wrappers (Rule 8.5).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions (the function headers are in
 * misc/templates/syscall_declarations_*.tpl).
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the structures and enums are used
 * by their typedef names: msection_t, msection_wq_t; the comment naming the
 * removed alias pok_thread_status (libpok) is removed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * pok_partition_mode_e (was pok_partition_mode_t); the Doxygen comment of the
 * file is removed (header files are not documented).
 */

/*
 * MISRA C:2025 Rule 11.4, Rule 11.6 and Rule 10.5 deviation: the wrappers
 * pass their arguments to the kernel as uintptr_t (see pok_syscall_args_t),
 * so pointers and Boolean values are converted to integers; the kernel
 * converts them back.
 */


#include <uapi/types.h>
#include <uapi/thread_types.h>
#include <uapi/partition_types.h>
#include <uapi/partition_arinc_types.h>
#include <uapi/port_types.h>
#include <uapi/error_arinc_types.h>
#include <uapi/memblock_types.h>
#include <uapi/msection.h>

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_CREATE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_create(const char* name,
    void* entry,
    const pok_thread_attr_t* attr,
    pok_thread_id_t* thread_id)
{
    return pok_syscall4(POK_SYSCALL_THREAD_CREATE,
        (uintptr_t)name,
        (uintptr_t)entry,
        (uintptr_t)attr,
        (uintptr_t)thread_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_CREATE

#ifdef POK_NEEDS_THREAD_SLEEP
/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_SLEEP:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_sleep(const pok_time_t* duration)
{
    return pok_syscall1(POK_SYSCALL_THREAD_SLEEP,
        (uintptr_t)duration);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_SLEEP
#endif

#ifdef POK_NEEDS_THREAD_SLEEP_UNTIL
/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_SLEEP_UNTIL:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_sleep_until(const pok_time_t* until)
{
    return pok_syscall1(POK_SYSCALL_THREAD_SLEEP_UNTIL,
        (uintptr_t)until);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_SLEEP_UNTIL
#endif
/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_PERIOD:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_sched_end_period(void)
{
    return pok_syscall0(POK_SYSCALL_THREAD_PERIOD);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_PERIOD

#if defined (POK_NEEDS_THREAD_SUSPEND) || defined (POK_NEEDS_ERROR_HANDLING)
/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_SUSPEND:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_suspend(const pok_time_t* timeout)
{
    return pok_syscall1(POK_SYSCALL_THREAD_SUSPEND,
        (uintptr_t)timeout);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_SUSPEND
#endif

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_STATUS:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_get_status(pok_thread_id_t thread_id,
    char* name,
    void** entry,
    pok_thread_status_t* status)
{
    return pok_syscall4(POK_SYSCALL_THREAD_STATUS,
        (uintptr_t)thread_id,
        (uintptr_t)name,
        (uintptr_t)entry,
        (uintptr_t)status);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_STATUS

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_DELAYED_START:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_delayed_start(pok_thread_id_t id,
    const pok_time_t* delay_time)
{
    return pok_syscall2(POK_SYSCALL_THREAD_DELAYED_START,
        (uintptr_t)id,
        (uintptr_t)delay_time);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_DELAYED_START

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_SET_PRIORITY:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_set_priority(pok_thread_id_t thread_id,
    uint32_t priority)
{
    return pok_syscall2(POK_SYSCALL_THREAD_SET_PRIORITY,
        (uintptr_t)thread_id,
        (uintptr_t)priority);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_SET_PRIORITY

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_RESUME:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_resume(pok_thread_id_t thread_id)
{
    return pok_syscall1(POK_SYSCALL_THREAD_RESUME,
        (uintptr_t)thread_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_RESUME

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_SUSPEND_TARGET:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_suspend_target(pok_thread_id_t thread_id)
{
    return pok_syscall1(POK_SYSCALL_THREAD_SUSPEND_TARGET,
        (uintptr_t)thread_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_SUSPEND_TARGET

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_YIELD:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_yield(void)
{
    return pok_syscall0(POK_SYSCALL_THREAD_YIELD);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_YIELD

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_REPLENISH:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_sched_replenish(const pok_time_t* budget)
{
    return pok_syscall1(POK_SYSCALL_THREAD_REPLENISH,
        (uintptr_t)budget);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_REPLENISH

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_STOP:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_stop_target(pok_thread_id_t thread_id)
{
    return pok_syscall1(POK_SYSCALL_THREAD_STOP,
        (uintptr_t)thread_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_STOP

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_STOPSELF:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_stop(void)
{
    return pok_syscall0(POK_SYSCALL_THREAD_STOPSELF);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_STOPSELF

/*
 * Performs the syscall
 * POK_SYSCALL_THREAD_FIND:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_thread_find(const char* name,
    pok_thread_id_t* id)
{
    return pok_syscall2(POK_SYSCALL_THREAD_FIND,
        (uintptr_t)name,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_THREAD_FIND


/*
 * Performs the syscall
 * POK_SYSCALL_RESCHED:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_resched(void)
{
    return pok_syscall0(POK_SYSCALL_RESCHED);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_RESCHED

/*
 * Performs the syscall
 * POK_SYSCALL_MSECTION_ENTER_HELPER:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_msection_enter_helper(msection_t* section)
{
    return pok_syscall1(POK_SYSCALL_MSECTION_ENTER_HELPER,
        (uintptr_t)section);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MSECTION_ENTER_HELPER

/*
 * Performs the syscall
 * POK_SYSCALL_MSECTION_WAIT:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_msection_wait(msection_t* section,
    const pok_time_t* timeout)
{
    return pok_syscall2(POK_SYSCALL_MSECTION_WAIT,
        (uintptr_t)section,
        (uintptr_t)timeout);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MSECTION_WAIT

/*
 * Performs the syscall
 * POK_SYSCALL_MSECTION_NOTIFY:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_msection_notify(msection_t* section,
    pok_thread_id_t thread_id)
{
    return pok_syscall2(POK_SYSCALL_MSECTION_NOTIFY,
        (uintptr_t)section,
        (uintptr_t)thread_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MSECTION_NOTIFY

/*
 * Performs the syscall
 * POK_SYSCALL_MSECTION_WQ_NOTIFY:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_msection_wq_notify(msection_t* section,
    msection_wq_t* wq,
    pok_bool_t is_all)
{
    return pok_syscall3(POK_SYSCALL_MSECTION_WQ_NOTIFY,
        (uintptr_t)section,
        (uintptr_t)wq,
        (uintptr_t)is_all);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MSECTION_WQ_NOTIFY

/*
 * Performs the syscall
 * POK_SYSCALL_MSECTION_WQ_SIZE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e jet_msection_wq_size(msection_t* section,
    msection_wq_t* wq,
    size_t* size)
{
    return pok_syscall3(POK_SYSCALL_MSECTION_WQ_SIZE,
        (uintptr_t)section,
        (uintptr_t)wq,
        (uintptr_t)size);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MSECTION_WQ_SIZE


#ifdef POK_NEEDS_PARTITIONS
/*
 * Performs the syscall
 * POK_SYSCALL_PARTITION_SET_MODE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_partition_set_mode_current(pok_partition_mode_e mode)
{
    return pok_syscall1(POK_SYSCALL_PARTITION_SET_MODE,
        (uintptr_t)mode);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_PARTITION_SET_MODE

/*
 * Performs the syscall
 * POK_SYSCALL_PARTITION_GET_STATUS:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_current_partition_get_status(pok_partition_status_t* status)
{
    return pok_syscall1(POK_SYSCALL_PARTITION_GET_STATUS,
        (uintptr_t)status);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_PARTITION_GET_STATUS

/*
 * Performs the syscall
 * POK_SYSCALL_PARTITION_INC_LOCK_LEVEL:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_current_partition_inc_lock_level(int32_t* lock_level)
{
    return pok_syscall1(POK_SYSCALL_PARTITION_INC_LOCK_LEVEL,
        (uintptr_t)lock_level);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_PARTITION_INC_LOCK_LEVEL

/*
 * Performs the syscall
 * POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_current_partition_dec_lock_level(int32_t* lock_level)
{
    return pok_syscall1(POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL,
        (uintptr_t)lock_level);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_PARTITION_DEC_LOCK_LEVEL
#endif


#ifdef POK_NEEDS_ERROR_HANDLING
/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_HANDLER_CREATE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_thread_create(uint32_t stack_size,
    void* entry)
{
    return pok_syscall2(POK_SYSCALL_ERROR_HANDLER_CREATE,
        (uintptr_t)stack_size,
        (uintptr_t)entry);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_HANDLER_CREATE

/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_HANDLER_CONFIGURE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_thread_configure(uint32_t concurrency_control,
    int32_t processor_core_id)
{
    return pok_syscall2(POK_SYSCALL_ERROR_HANDLER_CONFIGURE,
        (uintptr_t)concurrency_control,
        (uintptr_t)processor_core_id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_HANDLER_CONFIGURE

/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_raise_application_error(const char* msg,
    size_t msg_size)
{
    return pok_syscall2(POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR,
        (uintptr_t)msg,
        (uintptr_t)msg_size);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_RAISE_APPLICATION_ERROR

/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_GET:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_get(pok_error_status_t* status,
    void* msg)
{
    return pok_syscall2(POK_SYSCALL_ERROR_GET,
        (uintptr_t)status,
        (uintptr_t)msg);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_GET

/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_report_application_message(const char* msg,
    size_t msg_size)
{
    return pok_syscall2(POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE,
        (uintptr_t)msg,
        (uintptr_t)msg_size);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_REPORT_APPLICATION_MESSAGE
#endif

/*
 * Performs the syscall
 * POK_SYSCALL_ERROR_RAISE_OS_ERROR:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_error_raise_os_error(const char* msg,
    size_t msg_size)
{
    return pok_syscall2(POK_SYSCALL_ERROR_RAISE_OS_ERROR,
        (uintptr_t)msg,
        (uintptr_t)msg_size);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_ERROR_RAISE_OS_ERROR


   /* Middleware syscalls */
#ifdef POK_NEEDS_PORTS_SAMPLING
/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_create(const char* name,
    pok_port_size_t size,
    pok_port_direction_t direction,
    const pok_time_t* refresh,
    pok_port_id_t* id)
{
    return pok_syscall5(POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE,
        (uintptr_t)name,
        (uintptr_t)size,
        (uintptr_t)direction,
        (uintptr_t)refresh,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_CREATE

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_write(pok_port_id_t id,
    const void* data,
    pok_port_size_t len)
{
    return pok_syscall3(POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE,
        (uintptr_t)id,
        (uintptr_t)data,
        (uintptr_t)len);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_WRITE

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_READ:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_read(pok_port_id_t id,
    void* data,
    pok_port_size_t* len,
    pok_bool_t* valid)
{
    return pok_syscall4(POK_SYSCALL_MIDDLEWARE_SAMPLING_READ,
        (uintptr_t)id,
        (uintptr_t)data,
        (uintptr_t)len,
        (uintptr_t)valid);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_READ

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_ID:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_id(const char* name,
    pok_port_id_t* id)
{
    return pok_syscall2(POK_SYSCALL_MIDDLEWARE_SAMPLING_ID,
        (uintptr_t)name,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_ID

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_status(pok_port_id_t id,
    pok_port_sampling_status_t* status)
{
    return pok_syscall2(POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS,
        (uintptr_t)id,
        (uintptr_t)status);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_STATUS

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_sampling_check(pok_port_id_t id)
{
    return pok_syscall1(POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_SAMPLING_CHECK
#endif /* POK_NEEDS_PORTS_SAMPLING */

#ifdef POK_NEEDS_PORTS_QUEUEING
/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_create_packed(const char* name,
    const pok_port_queuing_create_arg_t* arg,
    pok_port_id_t* id)
{
    return pok_syscall3(POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE,
        (uintptr_t)name,
        (uintptr_t)arg,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_CREATE

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_send(pok_port_id_t id,
    const void* data,
    pok_port_size_t len,
    const pok_time_t* timeout)
{
    return pok_syscall4(POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND,
        (uintptr_t)id,
        (uintptr_t)data,
        (uintptr_t)len,
        (uintptr_t)timeout);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_SEND

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_receive(pok_port_id_t id,
    const pok_time_t* timeout,
    void* data,
    pok_port_size_t* len)
{
    return pok_syscall4(POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE,
        (uintptr_t)id,
        (uintptr_t)timeout,
        (uintptr_t)data,
        (uintptr_t)len);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_RECEIVE

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_ID:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_id(const char* name,
    pok_port_id_t* id)
{
    return pok_syscall2(POK_SYSCALL_MIDDLEWARE_QUEUEING_ID,
        (uintptr_t)name,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_ID

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_status(pok_port_id_t id,
    pok_port_queuing_status_t* status)
{
    return pok_syscall2(POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS,
        (uintptr_t)id,
        (uintptr_t)status);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_STATUS

/*
 * Performs the syscall
 * POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_port_queuing_clear(pok_port_id_t id)
{
    return pok_syscall1(POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR,
        (uintptr_t)id);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MIDDLEWARE_QUEUEING_CLEAR

#endif /* POK_NEEDS_PORTS_QUEUEING */


/*
 * Performs the syscall
 * POK_SYSCALL_MEMORY_BLOCK_GET_STATUS:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e pok_memory_block_get_status(const char* name,
    jet_memory_block_status_t* status)
{
    return pok_syscall2(POK_SYSCALL_MEMORY_BLOCK_GET_STATUS,
        (uintptr_t)name,
        (uintptr_t)status);
}
// Syscall should be accessed only by function
#undef POK_SYSCALL_MEMORY_BLOCK_GET_STATUS
