{# TODO: Copyright there #}
{# Modified by Kirill Brazhnikov, 2026-10-31: syscall arguments are cast
 # to uintptr_t (64-bit support). #}
{% macro full_type(arg) %}{{arg.arg_type}}{%endmacro%}
static inline pok_ret_t {{sd.func}}(
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
