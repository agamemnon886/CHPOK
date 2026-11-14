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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the prototypes are
 * named; __<component>_init__() and __<component>_activity__() are
 * declared.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its types and their members (the descriptions are in the component's
 * configuration: description and state_doc); the function declarations are
 * hidden from Doxygen (\cond).
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef {{component.name}}_GEN_H
#define {{component.name}}_GEN_H

{% for h in component.additional_h_files %}
    #include {{h}}
{% endfor %}

{% for p in component.in_ports %}
    #include <interfaces/{{p.type}}_gen.h>
{% endfor %}

{% for p in component.out_ports %}
    #include <interfaces/{{p.type}}_gen.h>
{% endfor %}

/**
 * @typename `{{component.name}}_state`
 *
 * @description
 * The state of an instance of the {{component.name}} component
 * (state_struct in its configuration).
 */
typedef struct {
 {% if component.state_struct %}
  {%for name, type in component.state_struct.items()%}
    {{type}} {{name}};
    /**< {{component.get('state_doc', {}).get(name, '')
        | wordwrap(66, False, '\n     * ')}} */
  {% endfor %}
 {% endif %}
}{{component.name}}_state;

{% macro port_list(ports) %}
  {%- for p in ports %}{{p.name}} ({{p.type}}){{ ', ' if not loop.last }}
  {%- else %}none{% endfor %}
{%- endmacro %}
/**
 * @typename `{{component.name}}`
 *
 * @description
 * An instance of the {{component.name}} component: its state and its
 * ports.
 * {{component.get('description', '') | wordwrap(73, False, '\n * ')}}
 */
typedef struct {
    {{component.name}}_state state; /**< The state of the instance. */
    struct {
        {% for p in component.in_ports %}
            struct {
                {{p.type}} ops;
            } {{p.name}};
        {% endfor %}
    } in;
    /**< The in ports, each with the operations of its interface that the
     * component implements: {{port_list(component.in_ports)}}. */
    struct {
        {% for p in component.out_ports %}
          {% if p.is_array %}
            struct {
                {{p.type}} *ops;
                self_t *owner;
            } *{{p.name}};
          {% else %}
            struct {
                {{p.type}} *ops;
                self_t *owner;
            } {{p.name}};
          {% endif %}
        {% endfor %}
    } out;
    /**< The out ports, each with the operations of the component it is
     * connected to and that component (owner):
     * {{port_list(component.out_ports)}}. */
} {{component.name}};


/** \cond */
{% macro args(func)%}
    {% for arg in func.args_type[1:]%}, {{arg}} arg{{loop.index}}{%endfor%}
{% endmacro %}

{% for p in component.in_ports %}
  {% for i_func in interfaces[p.type].functions %}
      {{i_func.return_type}} {{p.implementation[i_func.name]}}(
          {{component.name}} *self{{args(i_func)}});
  {% endfor %}
{% endfor %}

{% for p in component.out_ports %}
  {% for i_func in interfaces[p.type].functions %}
    {% if p.is_array %}
      {{i_func.return_type}}
      {{component.name}}_call_{{p.name}}_{{i_func.name}}_by_index(
          int32_t idx, {{component.name}} *self{{args(i_func)}});
    {% else %}
      {{i_func.return_type}}
      {{component.name}}_call_{{p.name}}_{{i_func.name}}(
          {{component.name}} *self{{args(i_func)}});
    {% endif %}
  {% endfor %}
{% endfor %}



{% if component.init_func %}
    void {{component.init_func}}({{component.name}} *self);
{% endif %}

{% if component.activity%}
    void {{component.activity}}({{component.name}} *self);
{% endif %}

/*
 * MISRA C:2025 Rule 5.10 deviation: the glue code generated from
 * misc/templates/glue.c.tpl calls the component's init and activity
 * functions by these names.
 */
void __{{component.name}}_init__({{component.name}} *self);
void __{{component.name}}_activity__({{component.name}} *self);
/** \endcond */


#endif
