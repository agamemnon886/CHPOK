{# Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: user pointers are
   marked JET_USER (__user was a reserved identifier). -#}
{# Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025: the wrappers of
   syscalls without arguments cast the unused args to void. -#}
{# Modified by Kirill Brazhnikov, 2026-11-03: MISRA C:2025 Rule 8.5: the
   syscall implementation is no longer declared again here; the core header
   that <core/syscall.h> includes declares it. Long lines are wrapped at 80
   columns. -#}
{# TODO: Copyright there -#}
{% macro full_type(arg) -%}
{{arg.arg_type}}{%if arg.is_pointer%} JET_USER{%endif%}
{%- endmacro %}
static inline pok_ret_t pok_syscall_wrapper_{{sd.syscall_id}}(const pok_syscall_args_t* args)
{
{% if not sd.args %}
    (void) args;
{% endif %}
    return {{sd.func}}({% for arg in sd.args %}{{''}}
        ({{full_type(arg)}})args->arg{{loop.index}}{%if not loop.last %},{%+endif%}
                       {%- endfor%}
);
}
