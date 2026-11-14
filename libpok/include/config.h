/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-01: enabled the ARINC 653
 * mutexes (POK_NEEDS_ARINC653_MUTEX).
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the unused macros POK_NEEDS_TIMER,
 * POK_NEEDS_LIBC_STDLIB, POK_CONFIG_NEEDS_FUNC_MEMCPY,
 * POK_CONFIG_NEEDS_FUNC_MEMSET, POK_CONFIG_NEEDS_FUNC_MEMCMP,
 * POK_CONFIG_NEEDS_FUNC_STRCMP, POK_CONFIG_NEEDS_FUNC_STRNCMP,
 * POK_CONFIG_NEEDS_FUNC_STRCPY, POK_CONFIG_NEEDS_FUNC_STRNCPY,
 * POK_CONFIG_NEEDS_FUNC_STRLEN, POK_CONFIG_NEEDS_FUNC_STREQ,
 * POK_CONFIG_NEEDS_FUNC_ITOA, POK_CONFIG_NB_BUFFERS,
 * POK_CONFIG_BUFFER_DATA_SIZE, POK_CONFIG_NB_BLACKBOARDS,
 * POK_CONFIG_BLACKBOARD_DATA_SIZE, POK_NEEDS_SEMAPHORES,
 * POK_CONFIG_ARINC653_NB_SEMAPHORES, POK_CONFIG_ARINC653_NB_EVENTS and
 * POK_CONFIG_NB_EVENTS are removed.
 */

/**
 * @macroname `POK_NEEDS_THREADS`
 *
 * @description
 * Builds the thread services of libpok (core/thread.h).
 */
#define POK_NEEDS_THREADS 1

/**
 * @macroname `POK_NEEDS_PARTITIONS`
 *
 * @description
 * Builds the partition services of libpok (the partition system calls and
 * core/partition.h).
 */
#define POK_NEEDS_PARTITIONS 1

/**
 * @macroname `POK_NEEDS_DEBUG`
 *
 * @description
 * Debugging output; libpok's code doesn't test it (the kernel has a switch of
 * the same name).
 */
#define POK_NEEDS_DEBUG 1

/**
 * @macroname `POK_NEEDS_CONSOLE`
 *
 * @description
 * The console; libpok's code doesn't test it (the kernel has a switch of the
 * same name).
 */
#define POK_NEEDS_CONSOLE 1

/**
 * @macroname `POK_NEEDS_LIBMATH`
 *
 * @description
 * Builds the mathematical library (libm).
 */
#define POK_NEEDS_LIBMATH 1
//#define POK_NEEDS_ZERO_DIVISION_EXCEPTION 1

/* Configuration from kernel starts*/

/**
 * @macroname `POK_NEEDS_THREAD_SLEEP`
 *
 * @description
 * Builds the system calls that put the current thread to sleep (a switch of the
 * kernel's configuration).
 */
#define POK_NEEDS_THREAD_SLEEP 1
/* Configuration from kernel ends*/

/**
 * @macroname `POK_CONFIG_NEEDS_FUNC_STRNCASECMP`
 *
 * @description
 * Builds strncasecmp().
 */
#define POK_CONFIG_NEEDS_FUNC_STRNCASECMP    1

/**
 * @macroname `POK_CONFIG_NEEDS_FUNC_UDIVDI3`
 *
 * @description
 * Builds __udivdi3(), the 64-bit unsigned division that the compiler calls.
 */
#define POK_CONFIG_NEEDS_FUNC_UDIVDI3        1

/**
 * @macroname `POK_NEEDS_MIDDLEWARE`
 *
 * @description
 * The intra-partition communication of POK; libpok's code doesn't test it.
 */
#define POK_NEEDS_MIDDLEWARE 1

/**
 * @macroname `POK_NEEDS_ARINC653_PARTITION`
 *
 * @description
 * Builds the APEX partition services.
 */
#define POK_NEEDS_ARINC653_PARTITION 1

/**
 * @macroname `POK_NEEDS_ARINC653_PROCESS`
 *
 * @description
 * Builds the APEX process services.
 */
#define POK_NEEDS_ARINC653_PROCESS 1

/**
 * @macroname `POK_NEEDS_ARINC653_ERROR`
 *
 * @description
 * Builds the APEX health monitoring services.
 */
#define POK_NEEDS_ARINC653_ERROR 1

/**
 * @macroname `POK_NEEDS_ARINC653_SAMPLING`
 *
 * @description
 * Builds the APEX sampling port services.
 */
#define POK_NEEDS_ARINC653_SAMPLING 1

/**
 * @macroname `POK_NEEDS_ARINC653_QUEUEING`
 *
 * @description
 * Builds the APEX queuing port services.
 */
#define POK_NEEDS_ARINC653_QUEUEING 1

/**
 * @macroname `POK_NEEDS_ARINC653_TIME`
 *
 * @description
 * Builds the APEX time services.
 */
#define POK_NEEDS_ARINC653_TIME 1

/**
 * @macroname `POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE`
 *
 * @description
 * Builds only the services that the configuration needs (core/dependencies.h
 * derives the switches of libpok from the APEX ones); without it, the core
 * services are all built.
 */
#define POK_CONFIG_OPTIMIZE_FOR_GENERATED_CODE 1

/**
 * @macroname `POK_NEEDS_BUFFERS`
 *
 * @description
 * The buffers of POK's middleware; libpok's code doesn't test it (the kernel
 * has a switch of the same name).
 */
#define POK_NEEDS_BUFFERS 1

/**
 * @macroname `POK_NEEDS_ARINC653_BUFFER`
 *
 * @description
 * Builds the APEX buffer services.
 */
#define POK_NEEDS_ARINC653_BUFFER 1

/**
 * @macroname `POK_NEEDS_BLACKBOARDS`
 *
 * @description
 * The blackboards of POK's middleware; libpok's code doesn't test it (the
 * kernel has a switch of the same name).
 */
#define POK_NEEDS_BLACKBOARDS 1

/**
 * @macroname `POK_NEEDS_ARINC653_BLACKBOARD`
 *
 * @description
 * Builds the APEX blackboard services.
 */
#define POK_NEEDS_ARINC653_BLACKBOARD 1

/**
 * @macroname `POK_NEEDS_ARINC653_SEMAPHORE`
 *
 * @description
 * Builds the APEX semaphore services.
 */
#define POK_NEEDS_ARINC653_SEMAPHORE 1

/**
 * @macroname `POK_NEEDS_ARINC653_MUTEX`
 *
 * @description
 * Builds the APEX mutex services.
 */
#define POK_NEEDS_ARINC653_MUTEX 1

/**
 * @macroname `POK_NEEDS_EVENTS`
 *
 * @description
 * The events of POK; libpok's code doesn't test it (the kernel has a switch of
 * the same name).
 */
#define POK_NEEDS_EVENTS 1

/**
 * @macroname `POK_NEEDS_ARINC653_EVENT`
 *
 * @description
 * Builds the APEX event services.
 */
#define POK_NEEDS_ARINC653_EVENT 1

/**
 * @macroname `POK_CONFIG_NB_THREADS`
 *
 * @description
 * The number of threads of the partition: the variable pok_config_nb_threads of
 * the generated deployment code.
 */
#define POK_CONFIG_NB_THREADS pok_config_nb_threads

#include <stdint.h>
extern uint32_t pok_config_nb_buffers;
extern uint32_t pok_config_buffer_data_size;
extern uint32_t pok_config_nb_blackboards;
extern uint32_t pok_config_blackboard_data_size;
extern uint32_t pok_config_arinc653_nb_semaphores;
extern uint32_t pok_config_arinc653_nb_events;
extern uint32_t pok_config_nb_events;
extern uint32_t pok_config_nb_threads;
