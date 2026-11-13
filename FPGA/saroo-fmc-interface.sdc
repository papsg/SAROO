# NEx and NADV assert NWAIT directly. A destination register releases it.
set_max_delay 12.0 -from [get_ports {ST_CS ST_ALE}] -to [get_ports {ST_WAIT}]
set_max_delay 5.0 -from [get_registers {st_wait_ready}] -to [get_ports {ST_WAIT}]

# NADV and mclk are asynchronous. Exclude only the event's first stage.
# Bound the settled address and data bundles through their capture edges.
set_false_path -from [get_registers {ale_toggle}] -to [get_registers {ale_meta}]
set_max_delay 10.0 -from [get_registers {fsmc_addr_latched[*]}] -to [get_registers {fsmc_addr[*]}]
set_max_delay 10.0 -from [get_ports {ST_WR ST_BL0 ST_BL1 ST_AD[*]}] -to [get_registers {*}]
