// Adapted from paps-g 92f619b0. Exercise the actual DIR and OE equations.
`timescale 1ns/1ps

module tb;
    reg SS_FC1 = 1, SS_CS0 = 1, SS_CS1 = 1, SS_CS2 = 1, SS_RD = 1;
    reg SS_WR0 = 1, SS_WR1 = 1, ss_cdc_cs = 0, ss_cdc_en = 1;
    reg ss_bus_ready = 1;
    wire SS_DATA_DIR, SS_DATA_OE;
    data_direction_dut dut(.*);

    task check(input expected_oe, input expected_dir, input string label);
        #1;
        if ({SS_DATA_OE, SS_DATA_DIR} !== {expected_oe, expected_dir})
            $fatal(1, "%s: expected OE/DIR %b%b, got %b%b",
                   label, expected_oe, expected_dir, SS_DATA_OE, SS_DATA_DIR);
    endtask

    initial begin
        check(1, 0, "idle bus is isolated and prepared for writes");
        SS_RD = 0;
        check(1, 1, "read direction changes before chip select");
        SS_CS0 = 0;
        check(0, 1, "CS0 read drives toward Saturn");
        SS_CS0 = 1; SS_CS1 = 0;
        check(0, 1, "CS1 read drives toward Saturn");
        SS_CS1 = 1;
        check(1, 1, "read release isolates before RD changes");
        SS_RD = 1;
        check(1, 0, "RD release prepares the next write");

        SS_CS0 = 0; SS_WR0 = 0;
        check(0, 0, "single-lane write drives toward FPGA");
        SS_WR0 = 1; SS_WR1 = 0;
        check(0, 0, "other lane drives toward FPGA");
        SS_WR0 = 0;
        check(0, 0, "both lanes drive toward FPGA");

        SS_WR0 = 1; SS_WR1 = 1; SS_CS0 = 1; SS_CS2 = 0;
        SS_RD = 0; ss_cdc_cs = 1; ss_cdc_en = 0;
        check(1, 1, "native CD read leaves the FPGA isolated");
        ss_cdc_en = 1;
        check(0, 1, "emulated CD read drives toward Saturn");
        SS_FC1 = 0;
        check(1, 1, "FC1 refresh isolates the transceiver");
        SS_FC1 = 1; SS_WR0 = 0;
        check(0, 0, "a write lane overrides overlapping RD");
        $display("PASS 13 Saturn data-direction and isolation cases");
        $finish;
    end
endmodule
