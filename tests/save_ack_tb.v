// Adapted from paps-g 74999ab0. Public command register block model.
module tb;
    reg clk = 0, reset_n = 0, mcu_write = 0, saturn_write = 0;
    reg [24:0] mcu_address = 0;
    reg [23:0] saturn_address = 0;
    reg [15:0] mcu_data = 0, saturn_data = 0;
    wire [15:0] command, result;
    command_dut dut(clk, reset_n, mcu_write, saturn_write, 1'b1,
                    mcu_address, saturn_address, mcu_data, saturn_data, command, result);
    always #5 clk = ~clk;
    task swrite(input [23:0] address, input [15:0] data);
        @(negedge clk); saturn_address = address; saturn_data = data; saturn_write = 1;
        @(negedge clk); saturn_write = 0;
    endtask
    task mwrite(input [24:0] address, input [15:0] data);
        @(negedge clk); mcu_address = address; mcu_data = data; mcu_write = 1;
        @(negedge clk); mcu_write = 0;
    endtask
    integer opcode, tag;
    initial begin
        @(negedge clk); reset_n = 1;
        for (tag = 32; tag <= 32768; tag = tag * 1024)
        for (opcode = 6; opcode <= 12; opcode = opcode + 1) if (opcode != 8) begin
            swrite(24'h14, 16'ha5c3); swrite(24'h10, opcode | tag);
            if (command !== (opcode | tag) || result !== 16'ha5c3)
                $fatal(1, "tag or complete argument lost");
            mwrite(25'h12, tag);
            if (command !== (opcode | tag) || result !== tag)
                $fatal(1, "reply must precede command clear");
            mwrite(25'h10, 16'h0000);
            if (command !== 0 || result !== tag) $fatal(1, "completion lost");
            swrite(24'h14, 16'ha5c3); swrite(24'h10, opcode | tag);
            reset_n = 0; #1;
            if (command !== 0 || result !== 0) $fatal(1, "unexpected reset state");
            @(negedge clk); reset_n = 1;
        end
        $display("PASS 12 tagged file/save opcodes: ordered reply, cleared reset");
        $finish;
    end
endmodule
