// Adapted from paps-g 9ba50fc5. Exercise the complete Saturn write paths.
`timescale 1ns/1ps

module tb;
    reg reset = 1;
    reg mcu_cs = 1, mcu_ale = 1, mcu_wr = 1, mcu_drive = 0;
    reg [15:0] mcu_payload = 0;
    wire [15:0] mcu_ad;
    wire mcu_wait;
    assign mcu_ad = mcu_drive ? mcu_payload : 16'hzzzz;

    reg fc1 = 1, tim1 = 0;
    reg [2:0] selects = 3'b111;
    reg [1:0] lanes = 2'b11;
    reg [23:0] address = 0;
    reg [15:0] payload = 0;
    wire [15:0] data_bus;
    wire ss_wait, cke, sdclk, sdcs, ras, cas, we;
    wire [12:0] sd_addr;
    wire [1:0] bank, dqm;
    wire [15:0] dq;
    assign data_bus = payload;

    SSMaster dut(
        .CLK_50M(1'b0), .ST_GPIO0(reset), .ST_GPIO3(1'b0),
        .ST_CLK(1'b0), .ST_CS(mcu_cs), .ST_ALE(mcu_ale), .ST_RD(1'b1), .ST_WR(mcu_wr),
        .ST_AD(mcu_ad), .ST_ADDR(8'b0), .ST_BL0(1'b0), .ST_BL1(1'b0), .ST_WAIT(mcu_wait),
        .SS_MCLK(1'b0), .SS_RST(1'b1), .SS_SCLK(1'b0), .SS_FC0(1'b1), .SS_FC1(fc1),
        .SS_TIM0(1'b1), .SS_TIM1(tim1), .SS_TIM2(1'b1), .SS_AAS(1'b1), .SS_ADDR(address),
        .SS_DATA(data_bus), .SS_CS0(selects[0]), .SS_CS1(selects[1]), .SS_CS2(selects[2]),
        .SS_RD(1'b1), .SS_WR0(lanes[1]), .SS_WR1(lanes[0]), .SS_WAIT(ss_wait),
        .ST_MCLK(1'b0), .ST_BCK(1'b0), .ST_LRCK(1'b0), .ST_SDO(1'b0), .EPCS_DO(1'b0),
        .SD_CKE(cke), .SD_CLK(sdclk), .SD_CS(sdcs), .SD_RAS(ras), .SD_CAS(cas), .SD_WE(we),
        .SD_ADDR(sd_addr), .SD_BA(bank), .SD_DQM(dqm), .SD_DQ(dq)
    );

    integer events = 0, fifo_events = 0, memory_writes = 0;
    reg [15:0] fifo_word;
    reg [12:0] rows [0:3];
    reg [25:0] expected_address;
    reg [15:0] expected_word;
    reg [1:0] expected_lanes;
    reg expect_ram = 0, hold_arbiter = 0;
    always @(posedge dut.mclk) if (reset) begin
        if (dut.ss_wr_start) events = events + 1;
        if (dut.fifo_wr) begin
            fifo_word = dut.fifo_wdata;
            fifo_events = fifo_events + 1;
        end
    end
    always @(posedge sdclk) if (reset && cke) begin
        case ({sdcs, ras, cas, we})
        4'b0011: rows[bank] = sd_addr;
        4'b0100: begin
            if (!expect_ram || {rows[bank], bank, sd_addr[9:0], 1'b0} !== expected_address ||
                dq !== expected_word || dqm !== expected_lanes)
                $fatal(1, "SDRAM did not get the admitted Saturn address, data and lanes: %h %h %b",
                       {rows[bank], bank, sd_addr[9:0], 1'b0}, dq, dqm);
            memory_writes = memory_writes + 1;
            expect_ram = 0;
        end
        endcase
    end

    task mcu_write(input [7:0] offset, input [15:0] value);
        begin
            mcu_cs = 0; mcu_ale = 0; mcu_drive = 1;
            mcu_payload = {8'b0, offset} >> 1;
            #10 mcu_ale = 1;
            #5 mcu_wr = 0; mcu_payload = value;
            wait (mcu_wait === 1);
            #20 mcu_wr = 1; mcu_drive = 0; mcu_cs = 1;
            #12;
        end
    endtask

    task transfer(input [23:0] location, input [15:0] word,
                  input [2:0] which, input [1:0] mask, input integer skew,
                  input toggle_tim1, input want_write);
        integer before_count;
        begin
            before_count = events;
            address = location; payload = ~word;
            #36 selects = which; lanes = mask;
            #36;
            if (toggle_tim1) tim1 = ~tim1;
            #(skew) payload = word;
            #(36-skew);
            if (toggle_tim1) tim1 = ~tim1;
            payload = ~word; address = ~location;
            #36 selects = 3'b111; lanes = 2'b11;
            if (hold_arbiter) begin
                // Hold the request at the arbitration boundary while live
                // Saturn pins move on to the next cycle.
                release dut._mh.state;
                hold_arbiter = 0;
            end
            #120;
            if (events != before_count + want_write)
                $fatal(1, "expected %0d write event after TIM1, observed %0d",
                       want_write, events - before_count);
        end
    endtask

    integer idle = 0, skew = 0;
    initial begin
        if ($value$plusargs("idle=%d", idle)) begin end
        if ($value$plusargs("skew=%d", skew)) begin end
        tim1 = idle[0];
        #1 reset = 0;
        #20 reset = 1;
        #220000;
        // A selected bus with lane strobes but no data-phase indication
        // must not produce a stale register write.
        transfer(24'h007004, 16'h4000, 3'b011, 0, skew, 0, 0);
        transfer(24'h007004, 16'ha000, 3'b011, 0, skew, 1, 1);
        if (dut.ss_reg_ctrl !== 16'ha000) $fatal(1, "BCR control captured stale data");
        transfer(24'h007014, 16'hcafe, 3'b011, 0, skew, 1, 1);
        transfer(24'h007010, 16'h7e05, 3'b011, 0, skew, 1, 1);
        if (dut.ss_reg_data !== 16'hcafe || dut.ss_reg_cmd !== 16'h7e05 ||
            dut.st_irq_cmd !== 1) $fatal(1, "command handoff lost payload or IRQ");

        transfer(24'h00000c, 16'h45ff, 3'b011, 0, skew, 1, 1);
        transfer(24'h000018, 16'ha1b2, 3'b011, 0, skew, 1, 1);
        transfer(24'h00001c, 16'hc3d4, 3'b011, 0, skew, 1, 1);
        transfer(24'h000020, 16'he5f6, 3'b011, 0, skew, 1, 1);
        transfer(24'h000024, 16'h9abc, 3'b011, 0, skew, 1, 1);
        if ({dut.ss_hirq_mask, dut.ss_cr1, dut.ss_cr2, dut.ss_cr3, dut.ss_cr4}
            !== 80'h45ff_a1b2_c3d4_e5f6_9abc || dut.st_irq_cdc !== 1)
            $fatal(1, "CD register or IRQ capture mismatch");
        mcu_write(8'h28, 16'hf0f0);
        transfer(24'h000008, 16'h7ffe, 3'b011, 0, skew, 1, 1);
        if (dut.ss_hirq !== 16'h70f0) $fatal(1, "wrong HIRQ clear mask");
        mcu_write(8'h04, 16'h0200);
        transfer(24'h000000, 16'h1234, 3'b011, 0, skew, 1, 1);
        if (fifo_events != 1 || fifo_word !== 16'h3412)
            $fatal(1, "FIFO write lost the captured Saturn word");

        expected_address = 26'h440040; expected_word = 16'hb2a1;
        expected_lanes = 2'b01; expect_ram = 1;
        hold_arbiter = 1;
        force dut._mh.state = 2'd2;
        transfer(24'h5c0040, 16'ha1b2, 3'b110, 2'b01, skew, 1, 1);
        if (expect_ram || memory_writes != 1 || ss_wait !== 1)
            $fatal(1, "1 MiB RAM alias write did not complete");
        transfer(24'h007004, 16'hb000, 3'b011, 0, skew, 1, 1);
        expected_address = 26'h5c0040; expected_word = 16'hd4c3;
        expected_lanes = 2'b10; expect_ram = 1;
        transfer(24'h5c0040, 16'hc3d4, 3'b110, 2'b10, skew, 1, 1);
        if (expect_ram || memory_writes != 2 || ss_wait !== 1)
            $fatal(1, "4 MiB RAM write did not complete");
        transfer(24'h007004, 16'h0000, 3'b011, 2'b11, skew, 1, 0);
        if (dut.ss_reg_ctrl !== 16'hb000) $fatal(1, "idle byte strobes changed control");
        reset = 0;
        #1;
        if (dut.ss_wr_start !== 0 || dut.ss_reg_ctrl !== 16'h0100)
            $fatal(1, "reset left a write active");
        $display("PASS %0d Saturn writes, %0d SDRAM commands and %0d FIFO command",
                 events, memory_writes, fifo_events);
        $finish;
    end

    initial begin
        #1000000 $fatal(1, "fixture deadline");
    end
endmodule
