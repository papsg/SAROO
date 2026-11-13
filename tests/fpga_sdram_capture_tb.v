// Adapted from paps-g 3abb5c6c. Score data at the cache's sampling edge.
`timescale 1ns/1ps

module tb;
    reg reset = 1, clk = 0, sdclk = 0;
    reg [1:0] cmd_req = 0;
    wire cmd_ack, data_valid, sd_cke, sd_cs, sd_ras, sd_cas, sd_we;
    wire [15:0] cmd_dout;
    reg [15:0] dq = 16'hzzzz;
    reg [15:0] words [0:3];
    reg read_seen = 0;
    integer tsac = 6, seen = 0, i;
    realtime last_main_edge;

`ifdef SAROO_SDRAM_CAPTURE
    localparam realtime PHASE = 6.5;
`else
    localparam realtime PHASE = 5.0;
`endif
    always #5 clk = ~clk;
    always @(clk) sdclk <= #PHASE clk;
    always @(posedge clk) last_main_edge = $realtime;

    tsdram #(.tCK(300)) dut(
        .reset(reset), .clk(clk),
`ifdef SAROO_SDRAM_CAPTURE
        .capture_clk(sdclk),
`endif
        .reg_sdram(12'hf0d), .ext_refresh(1'b0), .cmd_req(cmd_req), .cmd_ack(cmd_ack),
        .cmd_mask(2'b00), .cmd_addr(32'h00123458), .cmd_din(16'b0),
        .cmd_dout(cmd_dout), .data_valid(data_valid), .sd_din(dq),
        .sd_cke(sd_cke), .sd_cs(sd_cs), .sd_ras(sd_ras), .sd_cas(sd_cas), .sd_we(sd_we)
    );

    always @(posedge sdclk) begin
        if ($realtime - last_main_edge != PHASE)
            $fatal(1, "incorrect forwarded clock phase");
        if (reset && sd_cke && {sd_cs, sd_ras, sd_cas, sd_we} == 4'b0101) begin
            if (read_seen) $fatal(1, "duplicate READ command");
            read_seen = 1;
        end
    end

    // CL2 starts driving after T1. Each new value is guaranteed by tSAC
    // and held for tOH after the following clock. Model the unknown interval.
    initial begin
        wait (read_seen);
        #13;
        for (i = 0; i < 4; i = i + 1) begin
            dq = 16'hxxxx;
            #(tsac - 3) dq = words[i];
            #(13 - tsac);
        end
        dq = 16'hzzzz;
    end

    always @(posedge clk) if (reset && data_valid) begin
        if (!read_seen || seen >= 4 || cmd_dout !== words[seen])
            $fatal(1, "wrong CL2 word %0d: %h (expected %h)", seen, cmd_dout, words[seen]);
        seen = seen + 1;
    end

    initial begin
        if ($value$plusargs("tsac=%d", tsac) && (tsac < 3 || tsac > 6))
            $fatal(1, "tSAC must be 3..6 ns");
        words[0] = 16'h1234;
        words[1] = 16'h5678;
        words[2] = 16'h9abc;
        words[3] = 16'hdef0;
        #1 reset = 0;
        #16 reset = 1;
        wait (sd_cke && dut.m_state == 1);
        @(negedge clk) cmd_req = 2'b10;
        wait (cmd_ack);
        @(negedge clk) cmd_req = 0;
        wait (seen == 4);
        repeat (4) @(negedge clk);
        $display("PASS four CL2 burst words in order at tSAC=%0d ns", tsac);
        $finish;
    end

    initial begin
        #20000 $fatal(1, "burst timeout: %0d words received", seen);
    end
endmodule
