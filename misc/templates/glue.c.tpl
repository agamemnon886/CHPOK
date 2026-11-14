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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions (the format of the function documentation, see
 * README.md).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the port operations have the type
 * port_ops_t of lib/common.h; the duplicate declarations of the init and
 * activity functions are removed.
 */

/**
 * @file
 * The glue code of the components of a partition: the component
 * instances, their initialization and links, and the loop of their
 * activities (generated from misc/templates/glue.c.tpl).
 */

#include <lib/common.h>

{% for i in components%}
    #include <{{i.type}}_gen.h>
        {% if i.name in port_array_dict %}
         {% for port, size in port_array_dict[i.name].items() %}
            port_ops_t {{i.name}}_array_for_{{port}}[{{size + 1}}];
         {% endfor %}
        {% endif %}
        {{i.type}} {{i.name}} = {
            {% if i.state %}
            .state = {
              {% for name, val in i.state.items()%}
                .{{name}} = {{val}},
              {% endfor %}
            },
            {% endif %}

            {% if i.name in port_array_dict %}
            .out = {
             {% for port, size in port_array_dict[i.name].items() %}
                .{{port}} = (void *){{i.name}}_array_for_{{port}},
             {% endfor %}
            }
            {% endif %}
        };

{% endfor %}


{% set ns = namespace(step=0) %}
/**
 * @funcname `__components_init__`
 *
 * @description
 * Initializes the components of the partition: each one, then the links
 * from their out ports to the in ports of other components.
 *
 * @globals
{% for i in components %}
{% if loop.first %}
 * | Name | Type | Description |
 * |------|------|-------------|
{% endif %}
 * | `{{i.name}}` | `{{i.type}}` | The component instance {{i.name}} |
{% else %}
 * None.
{% endfor %}
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
{% for i in components %}
{% set ns.step = ns.step + 1 %}
 * {{ns.step}}. Call `__{{i.type}}_init__()` with the argument `&{{i.name}}`.
{% endfor %}
{% for l in links %}
{% if l.from.array_index is defined %}
{% set out = l.from.instance ~ '.out.' ~ l.from.port ~ '['
    ~ l.from.array_index ~ ']' %}
{% else %}
{% set out = l.from.instance ~ '.out.' ~ l.from.port %}
{% endif %}
{% set ns.step = ns.step + 1 %}
 * {{ns.step}}. Set `{{out}}.ops` to `&{{l.to.instance}}.in.{{l.to.port}}.ops`.
{% set ns.step = ns.step + 1 %}
 * {{ns.step}}. Set `{{out}}.owner` to `&{{l.to.instance}}`.
{% endfor %}
{% if ns.step == 0 %}
 * 1. Do nothing.
{% endif %}
 *
 * @calls
{% for i in components %}
 * - `__{{i.type}}_init__()`
{% else %}
 * None.
{% endfor %}
 */
void __components_init__()
{
    {% for i in components%}
            __{{i.type}}_init__(&{{i.name}});

    {% endfor %}

    {% for l in links %}
      {% if l.from.array_index is defined%}
        {{l.from.instance}}.out.{{l.from.port}}[{{l.from.array_index}}].ops = &{{l.to.instance}}.in.{{l.to.port}}.ops;
        {{l.from.instance}}.out.{{l.from.port}}[{{l.from.array_index}}].owner = &{{l.to.instance}};
      {% else %}
        {{l.from.instance}}.out.{{l.from.port}}.ops = &{{l.to.instance}}.in.{{l.to.port}}.ops;
        {{l.from.instance}}.out.{{l.from.port}}.owner = &{{l.to.instance}};
      {% endif %}
    {% endfor %}

}

/**
 * @funcname `__components_activity__`
 *
 * @description
 * The loop of the partition's components: forever, one step of the
 * activity of each component in turn.
 *
 * @globals
{% for i in components %}
{% if loop.first %}
 * | Name | Type | Description |
 * |------|------|-------------|
{% endif %}
 * | `{{i.name}}` | `{{i.type}}` | The component instance {{i.name}} |
{% else %}
 * None.
{% endfor %}
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. While `1` is true:
{% for i in components %}
 *    {{loop.index}}. Call `__{{i.type}}_activity__()` with the argument
 *       `&{{i.name}}`.
{% else %}
 *    1. Do nothing.
{% endfor %}
 *
 * @calls
{% for i in components %}
 * - `__{{i.type}}_activity__()`
{% else %}
 * None.
{% endfor %}
 */
void __components_activity__()
{
    while (1) {
        {% for i in components%}
                __{{i.type}}_activity__(&{{i.name}});
        {% endfor %}
    }

}
