// Adapted from paps-g 6b156a19.
`timescale 1ns/1ps

// Run the production memory RTL against a logical CL2/BL4 response.
// Each request pulses rd for one clock and holds its address until nwait.
module tb;
    reg clk = 0, reset = 1;
    always #5 clk = ~clk;
    reg [1:0] cs = 0, rd = 0;
    reg [25:0] addr_a = 0, addr_b = 0;
    wire wait_a, wait_b;
    wire [15:0] read_a, read_b, sd_data;
    wire sd_cke, sd_cs, sd_ras, sd_cas, sd_we;
    wire [12:0] sd_addr;
    wire [1:0] sd_ba, sd_dqm;
`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(clk) capture_clk <= #6.5 clk;
`endif
    memhub dut(reset, clk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif

        cs[0], rd[0], 1'b0, 2'b00, wait_a, addr_a, 16'b0, read_a,
        cs[1], rd[1], 1'b0, 2'b00, wait_b, addr_b, 16'b0, read_b,
        sd_cke, sd_cs, sd_ras, sd_cas, sd_we, sd_addr, sd_ba, sd_dqm, sd_data,
        1'b0, 12'hf0d
`ifdef SAROO_FC1_WINDOW
        ,1'b0
`endif
`ifdef SAROO_FMC_READY
        ,
`endif
    );

    function [15:0] contents(input [25:0] address);
        contents = 16'h9000 ^ address[16:1] ^ {7'b0, address[25:17]};
    endfunction

    reg [12:0] row [0:3];
    reg [25:0] command_address, stream_address;
    reg [15:0] model_data;
    reg model_drive = 0;
    integer read_delay = 0, read_left = 0, reads = 0;
    assign sd_data = model_drive ? model_data : 16'hzzzz;

    always @(posedge clk) begin
        #2;
        if (!reset) begin
            read_delay = 0;
            read_left = 0;
            model_drive = 0;
        end else begin
            if (read_delay > 0)
                read_delay = read_delay - 1;
            model_drive = read_delay == 0 && read_left > 0;
            if (model_drive) begin
                model_data = contents(stream_address);
                stream_address = stream_address + 2;
                read_left = read_left - 1;
            end
            if (sd_cke) begin
                case ({sd_cs, sd_ras, sd_cas, sd_we})
                4'b0011: row[sd_ba] = sd_addr;
                4'b0100: $fatal(1, "read-only callers emitted WRITE");
                4'b0101: begin
                    command_address = {row[sd_ba], sd_ba, sd_addr[9:0], 1'b0};
                    if (command_address[2:0] !== 3'b000 || read_left || read_delay)
                        $fatal(1, "unaligned READ or overlapping SDRAM burst");
                    stream_address = command_address;
                    read_delay = 2;
                    read_left = 4;
                    reads = reads + 1;
                end
                endcase
            end
        end
    end

    task tick;
        begin
            @(posedge clk);
            #3;
        end
    endtask

    task read_check(input integer port, input [25:0] address, input must_hit);
        integer cycles;
        begin
            @(negedge clk);
            cs = 1 << port;
            rd = 1 << port;
            if (port == 0)
                addr_a = address;
            else
                addr_b = address;
            tick();
            if (must_hit && (port == 0 ? wait_a : wait_b) !== 1'b1)
                $fatal(1, "a valid word added a wait state");
            @(negedge clk);
            rd = 0;
            cycles = 0;
            while ((port == 0 ? wait_a : wait_b) !== 1'b1 && cycles < 25000) begin
                tick();
                cycles = cycles + 1;
            end
            if (cycles == 25000)
                $fatal(1, "read never completed");
            if ((port == 0 ? read_a : read_b) !== contents(address))
                $fatal(1, "invalid cache data: port=%0d address=%h actual=%h expected=%h reads=%0d",
                    port, address, (port == 0 ? read_a : read_b), contents(address), reads);
        end
    endtask

    localparam RESET_TAG = 0, SAME_LINE = 1, OTHER_LINE = 2;
    integer mode, port, first_word, next_word, gap, i, expected_reads;
    reg [25:0] first_line, next_line;

    initial begin
        if (!$value$plusargs("mode=%d", mode)) mode = SAME_LINE;
        if (!$value$plusargs("port=%d", port)) port = 0;
        if (!$value$plusargs("first_word=%d", first_word)) first_word = 0;
        if (!$value$plusargs("next_word=%d", next_word)) next_word = 3;
        if (!$value$plusargs("gap=%d", gap)) gap = 0;
        if (mode < RESET_TAG || mode > OTHER_LINE || port < 0 || port > 1 ||
            first_word < 0 || first_word > 3 || next_word < 0 || next_word > 3 || gap < 0 || gap > 8)
            $fatal(1, "invalid fixture selection");
        #1;
        reset = 0;
        repeat (2) tick();
        @(negedge clk);
        reset = 1;

        // Fill a distinct line first so an early completion exposes old data.
        read_check(port, 26'h20, 0);
        @(negedge clk);
        cs = 0;
        repeat (12) tick();
        if (mode == RESET_TAG) begin
            // The reset tag compares equal even though all four words are invalid.
            reset = 0;
            repeat (2) tick();
            @(negedge clk);
            reset = 1;
            first_line = 26'h2aaaaa8;
        end else begin
            first_line = 26'h40;
        end
        next_line = mode == OTHER_LINE ? 26'h80 : first_line;
        expected_reads = mode == OTHER_LINE ? 3 : 2;
        read_check(port, first_line + 2 * first_word, 0);
        repeat (gap) tick();
        read_check(port, next_line + 2 * next_word, next_line == first_line && next_word <= first_word);
        @(negedge clk);
        cs = 0;
        repeat (12) tick();
        for (i = 0; i < 4; i = i + 1)
            read_check(port, next_line + 2 * i, 1);
        if (reads != expected_reads)
            $fatal(1, "unexpected SDRAM READ count: %0d expected %0d", reads, expected_reads);
        $display("PASS valid cache reads: mode=%0d port=%0d words=%0d/%0d gap=%0d reads=%0d",
            mode, port, first_word, next_word, gap, reads);
        $finish;
    end

    initial begin
        #1000000;
        $fatal(1, "fixture deadline");
    end
endmodule
