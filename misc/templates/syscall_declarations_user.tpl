{# TODO: Copyright there #}
{# Modified by Kirill Brazhnikov, 2026-10-31: syscall arguments are cast
 # to uintptr_t (64-bit support). #}
{# Modified by Kirill Brazhnikov, 2026-11-08: a Doxygen header for each
 # syscall function (the format of the function documentation, see
 # README.md), with the directions of its arguments. #}
{# Modified by Kirill Brazhnikov, 2026-11-14: the syscall functions (static
 # inline functions of a header) have a plain comment instead of a Doxygen
 # header; the table of the directions of their arguments is removed;
 # pok_ret_t is renamed pok_ret_e (the enum types are named with _e). #}
{% macro full_type(arg) %}{{arg.arg_type}}{%endmacro%}
/*
 * Performs the syscall
 * {{sd.syscall_id}}:
 * its arguments are passed to the kernel as uintptr_t (generated from
 * syscall_map_arinc.h.in).
 */
static inline pok_ret_e {{sd.func}}(
{%- for arg in sd.args %}
{{full_type(arg)}} {{arg.name}}{%if not loop.last %},
    {%+endif%}
{%- else%}void
{%- endfor%}
)
{
    return pok_syscall{{sd.args | length}}({{sd.syscall_id}}
{%- for arg in sd.args %},
        (uintptr_t){{arg.name}}
{%- endfor%});
}
{# Selfcheck: remove syscall id definition after we assign function for it. #}
// Syscall should be accessed only by function
#undef {{sd.syscall_id}}
