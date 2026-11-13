// Adapted from paps-g 8b86806f. Verify CS1 at the Saturn pins.
`timescale 1ns/1ps

module tb;
    reg reset = 1, cs1 = 1, cs2 = 1, rd = 1, wr = 1, tim1 = 0;
    reg [23:0] address = 0;
    reg [15:0] control = 0;
    tri [15:0] data;
    assign data = !wr ? control : 16'hzzzz;

    SSMaster dut(
        .CLK_50M(1'b0), .ST_GPIO0(reset), .ST_GPIO3(1'b0),
        .ST_CLK(1'b0), .ST_CS(1'b1), .ST_ALE(1'b1), .ST_RD(1'b1), .ST_WR(1'b1),
        .ST_ADDR(8'b0), .ST_BL0(1'b0), .ST_BL1(1'b0),
        .SS_MCLK(1'b0), .SS_RST(1'b1), .SS_SCLK(1'b0), .SS_FC0(1'b1), .SS_FC1(1'b1),
        .SS_TIM0(1'b0), .SS_TIM1(tim1), .SS_TIM2(1'b0), .SS_AAS(1'b1), .SS_ADDR(address),
        .SS_DATA(data), .SS_CS0(1'b1), .SS_CS1(cs1), .SS_CS2(cs2),
        .SS_RD(rd), .SS_WR0(wr), .SS_WR1(wr),
        .ST_MCLK(1'b0), .ST_BCK(1'b0), .ST_LRCK(1'b0), .ST_SDO(1'b0), .EPCS_DO(1'b0)
    );

    task select_cart(input [1:0] mode);
        address = 24'h007004;
        #36 cs2 = 0; wr = 0;
        #36 tim1 = 1; control = {2'b0, mode, 12'h100};
        #36 tim1 = 0;
        #36 wr = 1; cs2 = 1;
        #36;
        if (dut.ss_reg_ctrl[13:12] !== mode)
            $fatal(1, "cartridge mode was not selected");
    endtask

    task check_data(input [23:0] location, input [15:0] expected);
        address = location;
        #1;
        if (data !== expected)
            $fatal(1, "CS1 %06h: expected %04h, got %04h", location, expected, data);
    endtask

    task check_cart(input [1:0] mode, input [15:0] id);
        select_cart(mode);
        cs1 = 0; rd = 0;
        check_data(24'hfffffe, id);
        check_data(24'hffffff, id);
        for (integer word = 0; word < 32767; word = word + 1)
            check_data(24'hff0000 + 2 * word, 16'hffff);
        // The bits above the former 64 KiB alias must also participate.
        for (integer bit_index = 16; bit_index < 24; bit_index = bit_index + 1)
            check_data(24'hfffffe ^ (24'b1 << bit_index), 16'hffff);
        cs1 = 1;
        check_data(24'hfffffe, 16'hzzzz);
        cs1 = 0; rd = 1;
        check_data(24'hfffffe, 16'hzzzz);
        cs1 = 1;
    endtask

    initial begin
        #1 reset = 0;
        #30 reset = 1;
        #60;
        check_cart(2'b00, 16'hffff);
        check_cart(2'b01, 16'hffff);
        check_cart(2'b10, 16'hff5a);
        check_cart(2'b11, 16'hff5c);
        $display("PASS terminal CS1 word and 32767 neighboring words in all four modes");
        $finish;
    end
    initial #150000 $fatal(1, "fixture deadline");
endmodule
