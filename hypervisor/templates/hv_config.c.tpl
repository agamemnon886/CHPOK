{#
 # Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #
 # 1. Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #
 # 2. Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 # "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 # LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 # A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 # HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 # SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 # LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 # DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 # THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 #
 # Template of the VM table (hv_vm_config_t, include/hv/config.h),
 # rendered by hvconfig.py.
 #}
{%- macro port(name) -%}
{%- if name -%}0x{{ '%X' % ports[name] }}U{%- else -%}HV_PORT_NONE{%- endif -%}
{%- endmacro -%}
/*
 * GENERATED! DO NOT MODIFY!
 *
 * VM table of the hypervisor (hvconfig.py, from
 * {{ source }}).
 */

#include <hv/config.h>

{% for vm in config.vms %}
extern const uint8_t hv_image_{{ vm.index }}[];
extern const uint64_t hv_image_{{ vm.index }}_size;
{% endfor %}

const hv_vm_config_t hv_vm_configs[{{ config.vms | length }}] = {
{% for vm in config.vms %}
    {
        .name = "{{ vm.name }}",
        .memory_size = 0x{{ '%X' % vm.memory }}UL,
        .nb_cpus = {{ vm.cpus }}U,
        .console_port = {{ port(vm.console) }},
        .gdb_port = {{ port(vm.gdb) }},
        .image = hv_image_{{ vm.index }},
        .image_size = &hv_image_{{ vm.index }}_size,
        .load_addr = 0x{{ '%X' % vm.load_addr }}U,
        .bss_end = 0x{{ '%X' % vm.bss_end }}U,
        .entry = 0x{{ '%X' % vm.entry }}U,
    },
{% endfor %}
};

const uint32_t hv_nb_vms = {{ config.vms | length }}U;

const uint16_t hv_console_port = {{ port(config.console) }};
