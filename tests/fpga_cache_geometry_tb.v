`timescale 1ns/1ps
// Adapted from paps-g 7b9e51b1 and e265bd35. The pin model identifies
// cells by row/bank/column regardless of the current controller geometry.
module tb;
    reg clk = 0, reset = 1;
    always #5 clk = ~clk;
    reg [1:0] cs = 0, rd = 0, wr = 0;
    reg [25:0] address [0:1];
    reg [15:0] payload [0:1];
    reg [1:0] mask [0:1];
    reg [11:0] config_word = 12'hf0d;
    wire [1:0] ready, sd_ba, sd_dqm;
    wire [15:0] result [0:1];
    wire [15:0] sd_data;
    wire cke, ncs, nras, ncas, nwe;
    wire [12:0] sd_addr;

`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(clk) capture_clk <= #6.5 clk;
`endif
    memhub dut(reset, clk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif

        cs[0], rd[0], wr[0], mask[0], ready[0], address[0], payload[0], result[0],
        cs[1], rd[1], wr[1], mask[1], ready[1], address[1], payload[1], result[1],
        cke, ncs, nras, ncas, nwe, sd_addr, sd_ba, sd_dqm, sd_data, 1'b0, config_word
`ifdef SAROO_FC1_WINDOW
        ,1'b0
`endif
`ifdef SAROO_FMC_READY
        ,
`endif
    );

    reg [12:0] rows [0:3];
    reg [24:0] burst_cell, last_read_cell, changed_cell;
    reg [15:0] model_data, changed_value, write_value;
    reg driving = 0, changed = 0, write_pending = 0;
    reg [24:0] write_cell;
    reg [1:0] write_mask;
    integer read_delay = 0, words_left = 0, reads = 0, writes = 0, activates = 0;
    assign sd_data = driving ? model_data : 16'hzzzz;

    function [24:0] cell_at(input [25:0] a, input integer kind);
        integer col_bits, row_bits;
        reg [12:0] row;
        reg [1:0] bank;
        reg [9:0] col;
        begin
            col_bits = (kind == 3) ? 10 : ((kind == 0) ? 8 : 9);
            row_bits = (kind >= 2) ? 13 : 12;
            row = (a >> (col_bits+3)) & ((1 << row_bits)-1);
            bank = a >> (col_bits+1);
            col = (a >> 1) & ((1 << col_bits)-1);
            cell_at = {row,bank,col};
        end
    endfunction

    function [15:0] value_at(input [24:0] at);
        begin
            if (changed && at == changed_cell) value_at = changed_value;
            else value_at = 16'h4b72 ^ (at[24:12]*73) ^ (at[11:10]*509) ^ at[9:0];
        end
    endfunction

    reg [24:0] pin_cell;
    always @(posedge clk) begin
        #2;
        if (read_delay) read_delay = read_delay-1;
        driving = !read_delay && words_left != 0;
        if (driving) begin
            model_data = value_at(burst_cell);
            burst_cell = burst_cell+1'b1;
            words_left = words_left-1;
        end
        if (reset && cke) case ({ncs,nras,ncas,nwe})
            4'b0011: begin
                rows[sd_ba] = sd_addr;
                activates = activates+1;
            end
            4'b0101: begin
                if (read_delay || words_left || driving || sd_addr[1:0] || !sd_addr[10])
                    $fatal(1, "invalid READ command");
                last_read_cell = {rows[sd_ba],sd_ba,sd_addr[9:0]};
                burst_cell = last_read_cell;
                read_delay = 2;
                words_left = 4;
                reads = reads+1;
            end
            4'b0100: begin
                #1; // Let the resolved data bus settle after drive changes.
                pin_cell = {rows[sd_ba],sd_ba,sd_addr[9:0]};
                if (!write_pending || driving || pin_cell !== write_cell ||
                    sd_data !== write_value || sd_dqm !== write_mask || !sd_addr[10])
                    $fatal(1, "WRITE command did not match the physical request");
                changed_value = value_at(pin_cell);
                if (!sd_dqm[1]) changed_value[15:8] = sd_data[15:8];
                if (!sd_dqm[0]) changed_value[7:0] = sd_data[7:0];
                changed_cell = pin_cell;
                changed = 1;
                write_pending = 0;
                writes = writes+1;
            end
        endcase
    end

    task tick; begin @(posedge clk); #4; end endtask

    task begin_read(input integer port, input [25:0] a);
        begin
            address[port] = a;
            cs[port] = 1;
            rd[port] = 1;
            tick();
            @(negedge clk);
            rd[port] = 0;
        end
    endtask

    task finish_read(input integer port, input [25:0] a, input integer kind);
        reg [15:0] expected;
        begin
            expected = value_at(cell_at(a, kind));
            while (ready[port] !== 1'b1) tick();
            if (result[port] !== expected)
                $fatal(1, "stale or misaddressed cache data: port=%0d kind=%0d addr=%h got=%h expected=%h",
                    port, kind, a, result[port], expected);
            @(negedge clk);
            cs[port] = 0;
        end
    endtask

    task read_one(input integer port, input [25:0] a, input integer kind, input integer misses);
        integer count;
        begin
            count = reads;
            @(negedge clk);
            begin_read(port, a);
            finish_read(port, a, kind);
            repeat (12) tick();
            if (reads != count+misses ||
                (misses == 1 && last_read_cell !== cell_at({a[25:3],3'b0}, kind)))
                $fatal(1, "wrong READ command or count: port=%0d kind=%0d addr=%h", port, kind, a);
        end
    endtask

    task write_one(input integer port, input [25:0] a, input integer kind, input [1:0] byte_mask);
        begin
            @(negedge clk);
            write_pending = 1;
            write_cell = cell_at(a, kind);
            write_value = 16'hb7f1;
            write_mask = byte_mask;
            address[port] = a;
            payload[port] = write_value;
            mask[port] = byte_mask;
            cs[port] = 1;
            wr[port] = 1;
            tick();
            @(negedge clk);
            wr[port] = 0;
            while (ready[port] !== 1'b1) tick();
            if (write_pending || writes != 1) $fatal(1, "unissued or repeated WRITE");
            @(negedge clk);
            cs[port] = 0;
            repeat (12) tick();
        end
    endtask

    integer phase, kind, old_kind, port, bit_index, mask_arg, low, count, target;
    reg [25:0] base_addr, other_addr, switch_addr;
    initial begin
        if (!$value$plusargs("phase=%d",phase)) phase = 0;
        if (!$value$plusargs("kind=%d",kind)) kind = 0;
        if (!$value$plusargs("old=%d",old_kind)) old_kind = 3;
        if (!$value$plusargs("port=%d",port)) port = 0;
        if (!$value$plusargs("bit=%d",bit_index)) bit_index = 23;
        if (!$value$plusargs("mask=%d",mask_arg)) mask_arg = 0;
        if (!$value$plusargs("low=%d",low)) low = 0;
        address[0] = 0; address[1] = 0;
        payload[0] = 0; payload[1] = 0;
        mask[0] = 0; mask[1] = 0;
        config_word = {(phase <= 1) ? kind[1:0] : old_kind[1:0],10'h30d};
        #1; reset = 0; repeat (2) tick();
        @(negedge clk); reset = 1;
        repeat (12) tick();

        base_addr = 26'h001bac;
        other_addr = base_addr | (26'd1 << bit_index);
        switch_addr = low ? 26'h0000806 : 26'h3800c06;
        case (phase)
            0,1: begin
                read_one(port^1, base_addr, kind, 1);
                read_one(port, other_addr, kind, 1);
                write_one(port, other_addr, kind, mask_arg[1:0]);
                read_one(port^1, base_addr, kind, 0);
                read_one(port, other_addr, kind, 0);
                if (phase == 0) begin
                    read_one(port, base_addr, kind, 0);
                    read_one(port^1, other_addr, kind, 0);
                end
            end
            2: begin
                read_one(port^1, switch_addr, old_kind, 1);
                read_one(port, switch_addr, old_kind, 1);
                count = reads;
                @(negedge clk);
                config_word = {kind[1:0],10'h30d};
                begin_read(port, switch_addr);
                finish_read(port, switch_addr, kind);
                repeat (12) tick();
                if (reads != count+1 || last_read_cell !== cell_at({switch_addr[25:3],3'b0},kind))
                    $fatal(1, "same-edge geometry change did not refetch");
                read_one(port^1, switch_addr, kind, 1);
            end
            3,4,5: begin
                count = reads;
                target = activates+1;
                @(negedge clk);
                begin_read(port, switch_addr);
                if (phase == 3) while (activates != target) tick();
                if (phase == 4) while (!reads || words_left != 3) tick();
                if (phase == 5) while (!reads || words_left != 1) tick();
                @(negedge clk);
                config_word = {kind[1:0],10'h30d};
                finish_read(port, switch_addr, old_kind);
                if (reads != count+1 || last_read_cell !== cell_at({switch_addr[25:3],3'b0},old_kind))
                    $fatal(1, "geometry changed the accepted READ command");
                read_one(port^1, switch_addr, kind, 1);
                read_one(port, switch_addr, kind, 1);
            end
            default: $fatal(1, "unknown fixture phase");
        endcase
        $display("PASS SDRAM physical address and cache ownership");
        $finish;
    end

    initial begin #2000000; $fatal(1, "fixture deadline"); end
endmodule
