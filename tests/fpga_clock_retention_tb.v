// Adapted from paps-g bd0aab2e. Exercise reset and both sides of the bus.
`timescale 1ns/1ps

module cdcfifo(input sclr, clock, rdreq, wrreq, input [15:0] data,
               output reg [15:0] q, output empty, full, output [10:0] usedw);
    reg occupied;
    assign usedw = {10'b0, occupied};
    assign empty = !occupied;
    assign full = 0;
    always @(posedge clock) begin
        if (sclr) begin
            occupied <= 0;
            q <= 0;
        end else if (wrreq) begin
            occupied <= 1;
            q <= data;
        end else if (rdreq) occupied <= 0;
    end
endmodule

module tb;
    reg ST_GPIO0 = 1, SS_RST = 1;
    reg SS_FC1 = 1, SS_CS0 = 1, SS_CS1 = 1, SS_CS2 = 1;
    reg SS_RD = 1, SS_WR0 = 1, SS_WR1 = 1, SS_TIM1 = 0;
    reg [23:0] SS_ADDR = 0;
    tri [15:0] SS_DATA, ST_AD;
    reg [15:0] saturn_word, mcu_word;
    reg saturn_drive = 0, mcu_drive = 0;
    reg ST_CS = 1, ST_ALE = 1, ST_RD = 1, ST_WR = 1;
    wire SS_IRQ, SS_WAIT, SS_DATA_DIR, SS_DATA_OE, SS_SSEL, SS_OUTEN, ST_WAIT;
    assign SS_DATA = saturn_drive ? saturn_word : 16'hzzzz;
    assign ST_AD = mcu_drive ? mcu_word : 16'hzzzz;

    SSMaster dut(
        .CLK_50M(1'b0), .ST_GPIO0(ST_GPIO0), .ST_GPIO3(1'b0),
        .ST_CLK(1'b0), .ST_CS(ST_CS), .ST_ALE(ST_ALE), .ST_RD(ST_RD), .ST_WR(ST_WR),
        .ST_AD(ST_AD), .ST_ADDR(8'b0), .ST_BL0(1'b0), .ST_BL1(1'b0), .ST_WAIT(ST_WAIT),
        .SS_MCLK(1'b0), .SS_RST(SS_RST), .SS_SCLK(1'b0), .SS_FC0(1'b1), .SS_FC1(SS_FC1),
        .SS_TIM0(1'b0), .SS_TIM1(SS_TIM1), .SS_TIM2(1'b0), .SS_AAS(1'b1), .SS_ADDR(SS_ADDR),
        .SS_DATA(SS_DATA), .SS_CS0(SS_CS0), .SS_CS1(SS_CS1), .SS_CS2(SS_CS2),
        .SS_RD(SS_RD), .SS_WR0(SS_WR0), .SS_WR1(SS_WR1),
        .SS_IRQ(SS_IRQ), .SS_WAIT(SS_WAIT), .SS_DATA_DIR(SS_DATA_DIR), .SS_DATA_OE(SS_DATA_OE),
        .SS_SSEL(SS_SSEL), .SS_OUTEN(SS_OUTEN),
        .ST_MCLK(1'b0), .ST_BCK(1'b0), .ST_LRCK(1'b0), .ST_SDO(1'b0), .EPCS_DO(1'b0)
    );

    task clocks(input integer cycles);
        repeat (cycles) @(negedge dut.mclk);
    endtask

    task mcu_write(input [7:0] offset, input [15:0] value);
        ST_CS = 0; ST_ALE = 0; mcu_drive = 1; mcu_word = {8'b0, offset} >> 1;
        #10 ST_ALE = 1;
        #5 ST_WR = 0; mcu_word = value;
        wait (ST_WAIT === 1);
        #20 ST_WR = 1; mcu_drive = 0; ST_CS = 1;
        #10;
    endtask

    task saturn_write(input [23:0] address, input [15:0] value);
        SS_ADDR = address;
        #36 SS_CS2 = 0; SS_WR0 = 0; SS_WR1 = 0;
        #36 SS_TIM1 = 1; saturn_drive = 1; saturn_word = value;
        #36 SS_TIM1 = 0; saturn_drive = 0;
        #36 SS_CS2 = 1; SS_WR0 = 1; SS_WR1 = 1;
        #36;
    endtask

    task check_quiet;
        if ({SS_DATA_OE, SS_DATA_DIR, SS_WAIT, SS_IRQ} !== 4'b1010 || SS_DATA !== 16'hzzzz)
            $fatal(1, "cartridge outputs active during SYSRES");
    endtask

    task cycle_reset(input [15:0] mode, input integer hold_clocks);
        reg [31:0] timer_before;
        saturn_write(24'h007004, mode);
        saturn_write(24'h007014, 16'hbeef);
        saturn_write(24'h007010, 16'h4007);
        saturn_write(24'h00000c, 16'hffff);
        saturn_write(24'h000018, 16'h1234);
        saturn_write(24'h000024, 16'h5678);
        mcu_write(8'h04, 16'h0003);
        mcu_write(8'h08, 16'h4321);
        mcu_write(8'h20, 16'h123a);
        mcu_write(8'h28, 16'h0001);
        if (SS_IRQ !== 1 || dut.st_irq_cdc !== 1 || dut.st_irq_cmd !== 1)
            $fatal(1, "valid command and IRQ setup failed");
        timer_before = dut.ss_reg_timer;

        SS_ADDR = 24'h007000; SS_RD = 0; SS_CS2 = 0;
        clocks(4);
        if (SS_DATA !== 16'h5253) $fatal(1, "control read did not reach the bus");
        SS_RST = 0;
        clocks(4);
        check_quiet();
        if (SS_SSEL !== ~mode[15] || SS_OUTEN !== ~mode[15] || dut.ss_reg_ctrl !== mode)
            $fatal(1, "SYSRES changed the CD selection or cartridge map");
        if (dut.ss_reg_cmd !== 16'h4007 || dut.ss_reg_data !== 16'hbeef || dut.st_irq_cmd !== 1)
            $fatal(1, "SYSRES retired the public mailbox");
        if (dut.ss_hirq !== 16'h0001 || dut.ss_hirq_mask !== 16'hffff ||
            dut.ss_cr1 !== 16'h1234 || dut.ss_cr4 !== 16'h5678 ||
            dut.st_irq_cdc !== 1 || dut.ss_in_cmd !== 1)
            $fatal(1, "SYSRES lost accepted CD state");
        if (dut.fifo_empty !== 0 || dut.fifo_q !== 16'h4321 || dut.ss_reg_timer < timer_before)
            $fatal(1, "SYSRES reset the FIFO or private clock");

        // The MCU may publish a response while the Saturn port is quiet.
        mcu_write(8'h20, 16'h123b);
        mcu_write(8'h28, 16'h0040);
        mcu_write(8'h06, 16'h0001);
        if (dut.ss_resp1 !== 16'h123b || dut.ss_hirq !== 16'h0041 || dut.st_irq_cdc !== 0)
            $fatal(1, "MCU completion during SYSRES was lost");
        clocks(hold_clocks);
        SS_RST = 1;
        clocks(1);
        check_quiet();
        clocks(3);
        if (SS_DATA !== 16'h5253 || SS_IRQ !== 1)
            $fatal(1, "data and retained interrupt did not return");
        if (mode[15]) begin
            SS_ADDR = 24'h080018;
            clocks(3);
            if (SS_DATA !== 16'h123b) $fatal(1, "retained response unavailable to Saturn");
        end
        SS_CS2 = 1; SS_RD = 1;
        mcu_write(8'h04, 16'h0103);
        clocks(3);
        if (dut.fifo_empty !== 1) $fatal(1, "explicit FIFO reset failed");
        mcu_write(8'h2c, 16'hffff);
    endtask

    initial begin
        #1 ST_GPIO0 = 0;
        #20 ST_GPIO0 = 1;
        clocks(7);
        cycle_reset(16'hb000, 6);
        cycle_reset(16'ha000, 13);
        cycle_reset(16'h3000, 23);

        // Leave a write in its address phase when reset takes ownership.
        SS_ADDR = 24'h007014; SS_CS2 = 0; SS_WR0 = 0; SS_WR1 = 0;
        clocks(3);
        if (dut.ss_write_armed !== 1) $fatal(1, "write never entered the address phase");
        SS_RST = 0;
        clocks(5);
        SS_TIM1 = 1; saturn_drive = 1; saturn_word = 16'hbad0;
        clocks(7);
        SS_RST = 1;
        clocks(6);
        // A trailing TIM1 edge on a select spanning SYSRES is still
        // part of the retired transfer.
        SS_TIM1 = 0;
        clocks(7);
        if (dut.ss_wr_start !== 0 || dut.ss_write_pending !== 0 || dut.ss_reg_data !== 16'hbeef)
            $fatal(1, "partial write survived cartridge reset");
        SS_CS2 = 1; SS_WR0 = 1; SS_WR1 = 1; saturn_drive = 0;
        clocks(4);
        saturn_write(24'h007014, 16'h600d);
        if (dut.ss_reg_data !== 16'h600d) $fatal(1, "fresh write after SYSRES failed");

        ST_GPIO0 = 0;
        clocks(3);
        if (dut.ss_reg_ctrl !== 16'h0100 || dut.ss_hirq !== 0 || dut.ss_hirq_mask !== 0 ||
            dut.ss_cr1 !== 0 || dut.ss_resp1 !== 0 || dut.ss_reg_cmd !== 0 ||
            dut.ss_reg_data !== 0 || dut.ss_reg_timer !== 0)
            $fatal(1, "full FPGA reset failed to clear its owners");
        $display("PASS three SYSRES pulses, retained CD and mailbox, partial-write retirement");
        $finish;
    end

    initial #100000 $fatal(1, "fixture deadline");
endmodule
