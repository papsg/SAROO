// Adapted from paps-g 36c4de4b. Original FPGA command registers.
module tb;
    reg clk=0, reset_n=0, mcu_write=0, saturn_write=0;
    reg [24:0] mcu_address=0;
    reg [23:0] saturn_address=0;
    reg [15:0] mcu_data=0, saturn_data=0;
    wire [15:0] command, result;
    wire pending;
    command_dut dut(clk,reset_n,mcu_write,saturn_write,1'b1,
                    mcu_address,saturn_address,mcu_data,saturn_data,command,result,pending);
    always #5 clk=~clk;
    task swrite(input [23:0] address, input [15:0] data);
        @(negedge clk); saturn_address=address; saturn_data=data; saturn_write=1;
        @(negedge clk); saturn_write=0;
    endtask
    task mwrite(input [24:0] address, input [15:0] data);
        @(negedge clk); mcu_address=address; mcu_data=data; mcu_write=1;
        @(negedge clk); mcu_write=0;
    endtask
    integer opcode, tag;
    initial begin
        @(negedge clk); reset_n=1;
        for(tag=0;tag<=32;tag=tag+32)
        for(opcode=1;opcode<=14;opcode=opcode+1) begin
            swrite(24'h14,16'ha5c3); swrite(24'h10,opcode|tag);
            if(!pending || command!==(opcode|tag) || result!==16'ha5c3)
                $fatal(1,"request lost");
            mwrite(25'h06,16'h0002);
            repeat(4) @(negedge clk);
            if(pending || command!==(opcode|tag) || result!==16'ha5c3)
                $fatal(1,"IRQ retirement changed the request");
            mwrite(25'h12,tag);
            if(command!==(opcode|tag) || result!==tag)
                $fatal(1,"reply must precede command completion");
            mwrite(25'h10,0);
            if(command!==0 || result!==tag) $fatal(1,"completion lost");
            swrite(24'h10,opcode|tag);
            reset_n=0; #1;
            if(command!==0 || result!==0 || pending!==0)
                $fatal(1,"reset left a success acknowledgment");
            @(negedge clk); reset_n=1;
        end
        $display("PASS 28 menu/file/save requests: IRQ retirement, ordered reply, reset");
        $finish;
    end
endmodule
