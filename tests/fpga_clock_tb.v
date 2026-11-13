// Adapted from paps-g fb67ac50 and 86e2af4e. FPGA register-level model.
module tb;
    reg mclk=0, ss_wr_start=0, SS_CS2=0;
    reg NRESET=1;
    reg [23:0] SS_ADDR=24'h89700c;
    wire [15:0] value;
    clock_dut dut(.*);
    always #5 mclk=~mclk;
    integer initial_phase, cycle, edges, previous_edge, periods;
    reg [31:0] before_count;
    reg [15:0] expected;

    task check_read(input [23:0] address);
    begin
        @(negedge mclk); SS_ADDR=address;
        before_count=dut.ss_reg_timer;
        expected=address[1] ? before_count[15:0] : before_count[31:16];
        @(posedge mclk); #1;
        if(value!==expected) $fatal(1,"wrong timer halfword %h != %h",value,expected);
    end
    endtask

    task check_write(input [23:0] address,input cs2,input clear_counter);
    begin
        @(negedge mclk);
        dut.ss_timer_feed=50; dut.ss_reg_timer=32'h76543210;
        SS_ADDR=address; SS_CS2=cs2; ss_wr_start=1;
        @(posedge mclk); #1;
        if(dut.ss_reg_timer!==(clear_counter ? 32'd0 : 32'h76543210))
            $fatal(1,"incorrect timer reset decode");
        @(negedge mclk); ss_wr_start=0; SS_CS2=0;
    end
    endtask

    initial begin
        /* Every concrete seven-bit startup phase converges to the /100 cycle.
         * This does not assert a particular physical power-up phase. */
        for(initial_phase=0;initial_phase<128;initial_phase=initial_phase+1) begin
            @(negedge mclk);
            dut.ss_timer_feed=initial_phase; dut.ss_reg_timer=0;
            edges=0; previous_edge=-1; periods=0;
            for(cycle=0;cycle<350;cycle=cycle+1) begin
                before_count=dut.ss_reg_timer;
                @(posedge mclk); #1; edges=edges+1;
                if(dut.ss_reg_timer!=before_count) begin
                    if(dut.ss_reg_timer!==before_count+1) $fatal(1,"wrong counter step");
                    if(previous_edge>=0 && edges-previous_edge!=100) $fatal(1,"wrong divider period");
                    previous_edge=edges; periods=periods+1;
                end
                @(negedge mclk);
            end
            if(periods<2) $fatal(1,"divider failed to converge");
        end
        check_write(24'h89700c,0,1);
        check_write(24'h89700e,0,1);
        check_write(24'h89700c,1,0);
        check_write(24'h89600c,0,0);
        check_write(24'h897010,0,0);
        @(negedge mclk); dut.ss_timer_feed=50; dut.ss_reg_timer=32'h1234ffff;
        check_read(24'h89700c); check_read(24'h89700e);
        /* The SCU two-beat CS2 alias must select the same timer halves.
         * CPU-side single-beat duplication is covered by test_menu_clock. */
        check_read(24'h81700c); check_read(24'h81700e);
        check_write(24'h81700c,0,1);
        @(negedge mclk); dut.ss_timer_feed=0; dut.ss_reg_timer=32'hffffffff;
        @(posedge mclk); #1;
        if(dut.ss_reg_timer!==0) $fatal(1,"counter failed full carry");
        check_read(24'h89700c); check_read(24'h89700e);
        $display("pass: 128 startup phases, /100 cadence, reset decode, split read mux and full carry");
        $finish;
    end
endmodule
