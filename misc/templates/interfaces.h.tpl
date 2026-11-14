/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented).
 */

#ifndef INTERFACES_{{interface.name|upper}}_H
#define INTERFACES_{{interface.name|upper}}_H

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
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; the parameters of the operations are
 * named.
 *
 * Modified by Kirill Brazhnikov, 2026-11-12: Doxygen comments for the file,
 * its type and the type's members (the descriptions are in the interface's
 * configuration: description).
 */


#include <lib/common.h>
{% for add_h in interface.additional_h_files %}
    #include {{add_h}}
{% endfor %}

{% for func in interface.functions %}
/**
 * @typename `{{interface.name}}_{{func.name}}_fn_t`
 *
 * @description
 * The function {{func.name}} of the {{interface.name}} interface.
 * {{func.get('description', '') | wordwrap(73, False, '\n * ')}}
 */
typedef {{func.return_type}} (*{{interface.name}}_{{func.name}}_fn_t)(
{% for t in func.args_type %}
    {{t}} arg{{loop.index0}}{{ ',' if not loop.last else ');' }}
{% endfor %}

{% endfor %}
/**
 * @typename `{{interface.name}}`
 *
 * @description
 * The operations of a port of the {{interface.name}} interface: those of
 * the component that implements it.
 * {{interface.get('description', '') | wordwrap(73, False, '\n * ')}}
 */
typedef struct {
  {% for func in interface.functions %}
    {{interface.name}}_{{func.name}}_fn_t {{func.name}};
    /**< {{func.get('description', '') | wordwrap(66, False, '\n     * ')}}
     */
    {% endfor %}
} {{interface.name}};


#endif

