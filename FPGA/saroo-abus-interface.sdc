# Exclude only the first TIM1 synchronizer register. Constrain the sampled
# payload, which has two full mclk periods to settle before consumption.
set_false_path -from [get_ports {SS_TIM1}] -to [get_registers {ss_tim1_meta}]
set_max_delay 8.0 -from [get_ports {SS_ADDR[*] SS_DATA[*] SS_CS0 SS_CS1 SS_CS2 SS_WR0 SS_WR1}] -to [get_registers {ss_addr_sample[*] ss_data_sample[*] ss_cs_sample[*] ss_mask_sample[*]}]
