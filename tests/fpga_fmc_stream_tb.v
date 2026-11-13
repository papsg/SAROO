// Adapted from paps-g 44fe8abd. Drive the FMC pins of the complete FPGA top.
`timescale 1ns/1ps

module mainpll(input inclk0, output reg c0 = 0, c1 = 0, output locked);
    integer phase = 0;
    initial begin
        if ($value$plusargs("phase=%d", phase)) begin end
        #(phase); forever #5 c0 = ~c0;
    end
    always @(c0) c1 <= #6.5 c0;
    assign locked = 1'b1;
endmodule

`ifndef SAROO_FIFO_MODEL
module cdcfifo(input sclr, clock, rdreq, wrreq, input [15:0] data,
               output [15:0] q, output empty, full, output [10:0] usedw);
    assign q = 0;
    assign empty = 1;
    assign full = 0;
    assign usedw = 0;
endmodule
`endif

`ifndef SAROO_ABUS_TEST
module tb;
    reg reset = 1, cs = 1, ale = 1, rd = 1, wr = 1, drive = 0, fc1 = 1;
    reg [15:0] pin_data = 0;
    reg [7:0] addr = 0;
    reg [1:0] mask = 0;
    wire [15:0] ad, dq;
    wire nwait, cke, sdclk, sdcs, ras, cas, we;
    wire [12:0] sd_addr;
    wire [1:0] bank, dqm;
    assign ad = drive ? pin_data : 16'hzzzz;

    SSMaster dut(
        .CLK_50M(1'b0), .ST_GPIO0(reset), .ST_GPIO3(1'b0),
        .ST_CLK(1'b0), .ST_CS(cs), .ST_ALE(ale), .ST_RD(rd), .ST_WR(wr),
        .ST_AD(ad), .ST_ADDR(addr), .ST_BL0(mask[0]), .ST_BL1(mask[1]), .ST_WAIT(nwait),
        .SS_MCLK(1'b0), .SS_RST(1'b1), .SS_SCLK(1'b0), .SS_FC0(1'b1), .SS_FC1(fc1),
        .SS_TIM0(1'b1), .SS_TIM1(1'b1), .SS_TIM2(1'b1), .SS_AAS(1'b1), .SS_ADDR(24'b0),
        .SS_CS0(1'b1), .SS_CS1(1'b1), .SS_CS2(1'b1), .SS_RD(1'b1), .SS_WR0(1'b1), .SS_WR1(1'b1),
        .ST_MCLK(1'b0), .ST_BCK(1'b0), .ST_LRCK(1'b0), .ST_SDO(1'b0), .EPCS_DO(1'b0),
        .SD_CKE(cke), .SD_CLK(sdclk), .SD_CS(sdcs), .SD_RAS(ras), .SD_CAS(cas), .SD_WE(we),
        .SD_ADDR(sd_addr), .SD_BA(bank), .SD_DQM(dqm), .SD_DQ(dq)
    );

    localparam WORDS = 512;
    reg [15:0] memory [0:WORDS-1];
    reg [12:0] active_row [0:3];
    reg [15:0] model_data = 16'hzzzz;
    reg [25:0] cell_address;
    integer first_word, remaining = 0, latency = 0;
    integer writes = 0, fills = 0, write_requests = 0, read_requests = 0;
    integer expected_writes = 0, expected_reads = 0, register_writes = 0;
    reg [23:0] expected_address;
    reg [15:0] expected_data;
    reg [1:0] expected_mask;
    realtime committed_at, ready_at;

    assign dq = model_data;
    always @(posedge sdclk) if (cke && reset) begin
        case ({sdcs, ras, cas, we})
        4'b0011: active_row[bank] = sd_addr;
        4'b0100: begin
            cell_address = {active_row[bank], bank, sd_addr[9:0], 1'b0};
            if (cell_address !== {2'b0, expected_address} || dq !== expected_data ||
                dqm !== expected_mask || writes != expected_writes - 1)
                $fatal(1, "wrong SDRAM write: address=%h data=%h mask=%b", cell_address, dq, dqm);
            if (!dqm[0]) memory[cell_address[9:1]][7:0] = dq[7:0];
            if (!dqm[1]) memory[cell_address[9:1]][15:8] = dq[15:8];
            writes = writes + 1;
            committed_at = $realtime;
        end
        4'b0101: begin
            cell_address = {active_row[bank], bank, sd_addr[9:0], 1'b0};
            if (remaining || latency || cell_address >= WORDS * 2 || cell_address[2:0])
                $fatal(1, "unexpected SDRAM read: %h", cell_address);
            first_word = cell_address >> 1;
            latency = 2;
            remaining = 4;
            fills = fills + 1;
        end
        endcase
        // tOH=3 ns and tSAC=6 ns, starting one edge after the READ.
        if (latency == 2) latency = 1;
        else if (remaining) begin
            latency = 0;
            model_data <= #3 16'hxxxx;
            model_data <= #6 memory[first_word];
            first_word = first_word + 1;
            remaining = remaining - 1;
        end else model_data <= #3 16'hzzzz;
    end

    always @(posedge dut.mclk) if (reset && !cs) begin
        if (dut.st_wr_start) write_requests = write_requests + 1;
        if (dut.st_rd_start) read_requests = read_requests + 1;
    end
    always @(posedge nwait) if (!cs && ale) ready_at = $realtime;

    integer tail = 20, group = 2, refresh = 0;
    task begin_access(input [24:0] location, input [1:0] lanes);
        begin
            cs = 0; ale = 0; wr = 1; rd = 1; drive = 1;
            addr = location[24:17]; pin_data = location[16:1]; mask = lanes;
            #1;
            if (nwait !== 0) $fatal(1, "NWAIT did not assert during the address phase");
            #9 ale = 1;
            #1;
            if (nwait !== 0) $fatal(1, "second halfword reused the previous NWAIT release");
            #4;
        end
    endtask

    task await_ready;
        integer polls;
        begin
            polls = 0;
            while (nwait !== 1 && polls < 500) begin
                #1; polls = polls + 1;
            end
            if (nwait !== 1) $fatal(1, "NWAIT timeout");
            #0.1;
        end
    endtask

    task finish_access(input last);
        begin
            repeat ((tail-5)/5) begin
                #5;
                if (nwait !== 1) $fatal(1, "NWAIT reasserted inside the data phase");
            end
            wr = 1; rd = 1; drive = 0;
            #1;
            if (ad !== 16'hzzzz) $fatal(1, "FPGA drives AD after the read strobe");
            #4;
            if (last) cs = 1;
            #10;
        end
    endtask

    task write_half(input [24:0] location, input [15:0] value,
                    input [1:0] lanes, input last);
        begin
            if (location[24]) begin
                expected_writes = expected_writes + 1;
                expected_address = location[23:0];
                expected_data = value;
                expected_mask = lanes;
            end else register_writes = register_writes + 1;
            begin_access(location, lanes);
            wr = 0; pin_data = value;
            await_ready();
            if (location[24] && (writes != expected_writes || ready_at < committed_at))
                $fatal(1, "NWAIT released before the write reached the SDRAM pins");
            if (location[24] && ready_at - committed_at > 4.0)
                $fatal(1, "NWAIT missed the SDRAM write completion edge");
            finish_access(last);
        end
    endtask

    task read_half(input [24:0] location, input [15:0] expected, input last);
        begin
            expected_reads = expected_reads + 1;
            begin_access(location, 0);
            rd = 0; drive = 0;
            await_ready();
            if (ad !== expected)
                $fatal(1, "read at NWAIT release %h: expected %h, received %h", location, expected, ad);
            finish_access(last);
        end
    endtask

    integer i;
    realtime started, elapsed;
    initial begin
        if ($value$plusargs("tail=%d", tail)) begin end
        if ($value$plusargs("group=%d", group)) begin end
        if ($value$plusargs("refresh=%d", refresh)) begin end
        if ((tail != 20 && tail != 30) || (group != 1 && group != 2 && group != 4))
            $fatal(1, "invalid fixture timing");
        #1 reset = 0;
        #20 reset = 1;
        #220000;
        read_half(25'h0, 16'h5253, 0);
        read_half(25'h2, 16'h1205, 1);
        write_half(25'h4, 16'h0010, 0, 0);
        read_half(25'h4, 16'h0010, 1);
        started = $realtime;
        for (i = 0; i < WORDS; i = i + 1)
            write_half(25'h1000000 + 2*i, 16'h4000 ^ i, 0, i % group == group - 1);
        elapsed = $realtime - started;
        for (i = 0; i < 32; i = i + 1) begin
            write_half(25'h1000000 + 2*i, 16'ha55a, 1, 0);
            write_half(25'h1000000 + 2*i, 16'h5aa5, 2, 1);
        end
        for (i = 0; i < 32; i = i + 1) begin
            read_half(25'h1000000 + 2*i, 16'ha5a5, 0);
            read_half(25'h10003c0 + 2*i, 16'h41e0 ^ i, 1);
        end
        if (writes != expected_writes || write_requests != writes + register_writes ||
            read_requests != expected_reads || fills < 2)
            $fatal(1, "incorrect totals: write=%0d/%0d, read=%0d/%0d, fills=%0d",
                   writes, write_requests, read_requests, expected_reads, fills);
        $display("PASS %0d writes, %0d reads, %0d fills, %0.2f MB/s",
                 writes, read_requests, fills, 2*WORDS*1000/elapsed);
        $finish;
    end

    initial if (refresh) begin
        #221000;
        forever begin
            fc1 = 0; #180; fc1 = 1; #15420;
        end
    end
    initial begin
        #2000000 $fatal(1, "fixture deadline");
    end
endmodule
`endif
