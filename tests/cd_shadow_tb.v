// Exercise the stock register blocks with the mask emitted by actual MCU code.
module cd_shadow_tb;
    reg mclk = 0;
    always #5 mclk = ~mclk;
    reg NRESET = 0, st_wr_start = 0, ss_wr_start = 0, SS_CS2 = 1;
    reg [24:0] fsmc_addr = 0;
    reg [23:0] SS_ADDR = 0;
    reg [15:0] ST_AD = 0, SS_DATA = 0;
    wire [15:0] value;
    wire request, enabled;
    integer mcu_mask;
    cd_shadow dut(.*);

    task mcu_write(input [24:0] addr, input [15:0] data);
        @(negedge mclk);
        fsmc_addr = addr;
        ST_AD = data;
        st_wr_start = 1;
        @(negedge mclk);
        st_wr_start = 0;
    endtask

    task saturn_write(input [23:0] addr, input [15:0] data);
        @(negedge mclk);
        SS_ADDR = addr;
        SS_DATA = data;
        SS_CS2 = 0;
        ss_wr_start = 1;
        @(negedge mclk);
        SS_CS2 = 1;
        ss_wr_start = 0;
    endtask

    initial begin
        if (!$value$plusargs("mcu_mask=%d", mcu_mask)) $fatal(1, "missing MCU result");
        @(negedge mclk);
        NRESET = 1;
        mcu_write(25'h28, 16'h02a1);
        if (enabled !== 0 || request !== 0 || value !== 16'h02a1) $fatal(1, "initial state");
        saturn_write(24'h000008, 16'hfffe);
        saturn_write(24'h000024, 16'h0000);
        if (enabled !== 0 || request !== 1 || value !== 16'h02a0) $fatal(1, "shadow command");
        mcu_write(25'h06, 16'h0001);
        if (request !== 0 || value !== 16'h02a0) $fatal(1, "retire request");
        if (mcu_mask != 0) mcu_write(25'h28, mcu_mask[15:0]);
        saturn_write(24'h007004, 16'h8100);
        if (enabled !== 1 || request !== 0 || value !== 16'h02a1)
            $fatal(1, "readiness or unrelated events lost at takeover: %04x", value);
        $display("pass: physical drive shadow");
        $finish;
    end
endmodule
