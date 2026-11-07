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
 */

#include <lib/common.h>
#include "{{component.name}}_gen.h"

{% macro args(func) %}
{% for i in range(1, func.args_type|length) %}, {{func.args_type[i]}} arg{{i}}{%endfor%}
{% endmacro %}


{% for p in component.in_ports %}
  {% for i_func in interfaces[p.type].functions %}
    static {{i_func.return_type}} wrapper_{{p.implementation[i_func.name]}}(
        self_t *arg0{{args(i_func)}})
    {
        return {{p.implementation[i_func.name]}}(({{component.name}}*) arg0{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
    }

  {% endfor %}
{% endfor %}


{% for p in component.out_ports %}
 {% if p.is_array %}
  {% for i_func in interfaces[p.type].functions %}
      {{i_func.return_type}} {{component.name}}_call_{{p.name}}_{{i_func.name}}_by_index(int32_t idx, {{component.name}} *self{{args(i_func)}})
      {
         if (self->out.{{p.name}}[idx].ops == NULL) {
             (void) printf("WRONG CONFIG: out port {{p.name}} of component "
                           "{{component.name}} was not initialized\n");
             //fatal_error?
         }
         return self->out.{{p.name}}[idx].ops->{{i_func.name}}(self->out.{{p.name}}[idx].owner{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
      }
  {% endfor %}
 {% else %}
  {% for i_func in interfaces[p.type].functions %}
      {{i_func.return_type}} {{component.name}}_call_{{p.name}}_{{i_func.name}}({{component.name}} *self{{args(i_func)}})
      {
         if (self->out.{{p.name}}.ops == NULL) {
             (void) printf("WRONG CONFIG: out port {{p.name}} of component "
                           "{{component.name}} was not initialized\n");
             //fatal_error?
         }
         return self->out.{{p.name}}.ops->{{i_func.name}}(self->out.{{p.name}}.owner{%for i in range(1, i_func.args_type|length)%}, arg{{i}}{%endfor%});
      }
  {% endfor %}
 {% endif %}
{% endfor %}


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

void __{{component.name}}_activity__({{component.name}} *self)
{
    {% if component.activity %}
        {{component.activity}}(self);
    {% else %}
        (void) self; // the component has no activity
    {% endif %}
}
