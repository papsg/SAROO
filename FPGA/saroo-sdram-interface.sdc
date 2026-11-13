# Samsung K4S511632D-UC75 CL2 timing at the selected 100 MHz clock.
set sdram_clock {_mpll|altpll_component|auto_generated|pll1|clk[1]}
set sdram_dq [get_ports {SD_DQ[*]}]
set sdram_out [get_ports {SD_CKE SD_CS SD_RAS SD_CAS SD_WE SD_ADDR[*] SD_BA[*] SD_DQM[*] SD_DQ[*]}]
set_input_delay -clock $sdram_clock -reference_pin [get_ports SD_CLK] -max 6.0 $sdram_dq
set_input_delay -clock $sdram_clock -reference_pin [get_ports SD_CLK] -min 3.0 $sdram_dq
set_output_delay -clock $sdram_clock -reference_pin [get_ports SD_CLK] -max 1.5 $sdram_out
set_output_delay -clock $sdram_clock -reference_pin [get_ports SD_CLK] -min -0.8 $sdram_out
