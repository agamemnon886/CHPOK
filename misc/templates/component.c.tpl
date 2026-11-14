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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types in
 * the generated code are replaced with the fixed-width types from
 * <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: discarded
 * printf() results are cast to void; the static wrappers aren't named with
 * reserved identifiers.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the activity function of a
 * component without an activity discards its unused parameter.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions (the format of the function documentation, see
 * README.md).
 *
 * Modified by Kirill Brazhnikov, 2026-11-11: in @design, each string
 * literal of the "WRONG CONFIG" message is a code span of its own; the
 * parameters' directions come from the interface (args_dir).
 */

/**
 * @file
 * The glue code of the component {{component.name}}: the wrappers of its
 * in ports, the calls of its out ports, and its init and activity
 * functions (generated from misc/templates/component.c.tpl).
 */

#include <lib/common.h>
#include "{{component.name}}_gen.h"

{% macro args(func) %}
{% for i in range(1, func.args_type|length) %}, {{func.args_type[i]}} arg{{i}}{%endfor%}
{% endmacro %}

{#
 # spans(items, tail): the lines of a list of code spans in a @design
 # step, "a, b and c" as the Design sections write it; tail follows the
 # last one.
 #}
{% macro spans(items, tail) %}
{% for it in items %}
 *    `{{it}}`
{%- if loop.revindex > 2 %},{% elif loop.revindex == 2 %} and
{%- else %}{{tail}}{% endif %}

{% endfor %}
{% endmacro %}

{#
 # call_step(callee, items): the @design step of a call with arguments
 # whose result is returned.
 #}
{#
 # arg_dir(func, i): the direction of argument i of an interface function:
 # its args_dir in the interface's YAML file, or in.
 #}
{% macro arg_dir(func, i) -%}
{%- if func.args_dir is defined %}{{ func.args_dir[i] }}{% else %}in{% endif -%}
{%- endmacro %}

{% macro call_step(callee, items) %}
{% if items|length == 1 %}
 * {{callee}} with the argument
 *    `{{items[0]}}`, and return its result.
{% else %}
 * {{callee}} with the arguments
{{ spans(items, ', and return its result.') }}
{%- endif %}
{% endmacro %}


{% for p in component.in_ports %}
  {% for i_func in interfaces[p.type].functions %}
    {% set impl = p.implementation[i_func.name] %}
    {% set ns = namespace(items=['(' ~ component.name ~ '*) arg0']) %}
    {% for i in range(1, i_func.args_type|length) %}
      {% set ns.items = ns.items + ['arg' ~ i] %}
    {% endfor %}
/**
 * @funcname `wrapper_{{impl}}`
 *
 * @description
 * The {{i_func.name}} operation of the in port {{p.name}}: calls
 * {{impl}}() of the component.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `arg0` | `self_t *` | [{{ arg_dir(i_func, 0) }}] The component. |
{% for i in range(1, i_func.args_type|length) %}
 * | `arg{{i}}` | `{{i_func.args_type[i]}}` |
    {{- ' [' ~ arg_dir(i_func, i) ~ '] Argument ' ~ i ~ ' of ' }}
    {{- i_func.name ~ '. |' }}
{% endfor %}
{% if i_func.return_type != 'void' %}
 *
 * @return The result of {{impl}}().
{% endif %}
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 {{ call_step('1. Call `' ~ impl ~ '()`', ns.items) | trim }}
 *
 * @calls
 * - `{{impl}}()`
 */
    static {{i_func.return_type}} wrapper_{{p.implementation[i_func.name]}}(
        self_t *arg0{{args(i_func)}})
    {
        return {{p.implementation[i_func.name]}}(({{component.name}}*) arg0{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
    }

  {% endfor %}
{% endfor %}


{% for p in component.out_ports %}
 {% if p.is_array %}
  {% set port = 'self->out.' ~ p.name ~ '[idx]' %}
 {% else %}
  {% set port = 'self->out.' ~ p.name %}
 {% endif %}
  {% for i_func in interfaces[p.type].functions %}
    {% set ns = namespace(items=[port ~ '.owner']) %}
    {% for i in range(1, i_func.args_type|length) %}
      {% set ns.items = ns.items + ['arg' ~ i] %}
    {% endfor %}
/**
 * @funcname `{{component.name}}_call_{{p.name}}_{{i_func.name}}
 {%- if p.is_array %}_by_index{% endif %}`
 *
 * @description
 * Calls the {{i_func.name}} operation of the component linked to the out
 * port {{p.name}}{% if p.is_array %} at an index{% endif %}; a port that
 * isn't linked is reported (a configuration error).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
{% if p.is_array %}
 * | `idx` | `int32_t` | [in] The index in the port array. |
{% endif %}
 * | `self` | `{{component.name}} *` | [in] The component. |
{% for i in range(1, i_func.args_type|length) %}
 * | `arg{{i}}` | `{{i_func.args_type[i]}}` |
    {{- ' [' ~ arg_dir(i_func, i) ~ '] Argument ' ~ i ~ ' of ' }}
    {{- i_func.name ~ '. |' }}
{% endfor %}
{% if i_func.return_type != 'void' %}
 *
 * @return The result of the operation.
{% endif %}
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `{{port}}.ops` is NULL:
 *    1. Call `printf()` with the argument
 *       `"WRONG CONFIG: out port {{p.name}} of component "`
 *       `"{{component.name}} was not initialized\n"`.
 {{ call_step('2. Call the function `' ~ port ~ '.ops->' ~ i_func.name
              ~ '`', ns.items) | trim }}
 *
 * @calls
 * - `printf()`
 * - `{{port}}.ops->{{i_func.name}}()` (indirect)
 */
 {% if p.is_array %}
      {{i_func.return_type}} {{component.name}}_call_{{p.name}}_{{i_func.name}}_by_index(int32_t idx, {{component.name}} *self{{args(i_func)}})
      {
         if (self->out.{{p.name}}[idx].ops == NULL) {
             (void) printf("WRONG CONFIG: out port {{p.name}} of component "
                           "{{component.name}} was not initialized\n");
             //fatal_error?
         }
         return self->out.{{p.name}}[idx].ops->{{i_func.name}}(self->out.{{p.name}}[idx].owner{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
      }
 {% else %}
      {{i_func.return_type}} {{component.name}}_call_{{p.name}}_{{i_func.name}}({{component.name}} *self{{args(i_func)}})
      {
         if (self->out.{{p.name}}.ops == NULL) {
             (void) printf("WRONG CONFIG: out port {{p.name}} of component "
                           "{{component.name}} was not initialized\n");
             //fatal_error?
         }
         return self->out.{{p.name}}.ops->{{i_func.name}}(self->out.{{p.name}}.owner{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
      }
 {% endif %}
  {% endfor %}
{% endfor %}


{% set ns = namespace(step=0) %}
/**
 * @funcname `__{{component.name}}_init__`
 *
 * @description
 * Initializes the component (from the glue code at partition start): the
 * operations of its in ports{% if component.init_func %}, then its init
 * function {{component.init_func}}(){% endif %}.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `{{component.name}} *` |
    {%- if component.init_func %} [in,out]
    {%- elif component.in_ports %} [out]
    {%- else %} [in]{% endif %} The component. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
{% for p in component.in_ports %}
    {% for i_func, comp_func in p.implementation.items() %}
        {% set ns.step = ns.step + 1 %}
 * {{ns.step}}. Set `self->in.{{p.name}}.ops.{{i_func}}` to
    {{- ' `wrapper_' ~ comp_func ~ '`.' }}
    {% endfor %}
{% endfor %}
{% if component.init_func %}
    {% set ns.step = ns.step + 1 %}
 * {{ns.step}}. Call `{{component.init_func}}()` with the argument `self`.
{% endif %}
{% if ns.step == 0 %}
 * 1. Do nothing.
{% endif %}
 *
 * @calls
{% if component.init_func %}
 * - `{{component.init_func}}()`
{% else %}
 * None.
{% endif %}
 */
void __{{component.name}}_init__({{component.name}} *self)
{
    {% for p in component.in_ports %}
        {% for i_func, comp_func in p.implementation.items() %}
            self->in.{{p.name}}.ops.{{i_func}} = wrapper_{{comp_func}};
        {% endfor %}
    {% endfor %}

    {% if component.init_func %}
        {{component.init_func}}(self);
    {% endif %}
}

/**
 * @funcname `__{{component.name}}_activity__`
 *
 * @description
 * One step of the component's activity (called in the loop of the glue
 * code){% if component.activity %}: its activity function
 * {{component.activity}}(){% else %}: the component has none{% endif %}.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `self` | `{{component.name}} *` |
    {%- if component.activity %} [in,out]{% else %} [in]{% endif %}
    {{- ' The component. |' }}
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
{% if component.activity %}
 * 1. Call `{{component.activity}}()` with the argument `self`.
{% else %}
 * 1. Mark `self` as not used.
{% endif %}
 *
 * @calls
{% if component.activity %}
 * - `{{component.activity}}()`
{% else %}
 * None.
{% endif %}
 */
void __{{component.name}}_activity__({{component.name}} *self)
{
    {% if component.activity %}
        {{component.activity}}(self);
    {% else %}
        (void) self; // the component has no activity
    {% endif %}
}
