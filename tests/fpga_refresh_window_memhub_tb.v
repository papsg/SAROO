`timescale 1ns/1ps
/* Adapted from paps-g 8a31651a. Use the production synchronizer and memhub. */
module tb;
    reg mclk=0, reset=1;
    always #5 mclk=~mclk;
    reg SS_FC1=1, SS_CS0=1, SS_RD=1;
    wire SS_CS1=1'b1, SS_CS2=1'b1;
    `include "fc1-synchronizer.inc"

    reg st_cs=0, st_rd=0, st_wr=0;
    wire ss_wait, st_wait, cke, cs, ras, cas, we;
    wire [15:0] ss_rdata, st_rdata, data;
    wire [12:0] address;
    wire [1:0] bank, mask;
`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(mclk) capture_clk <= #6.5 mclk;
`endif
    memhub dut(reset,mclk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif

        1'b1,ss_rd_start,1'b0,2'b00,ss_wait,26'h1000,16'b0,ss_rdata,
        st_cs,st_rd,st_wr,2'b01,st_wait,26'h2000,16'hface,st_rdata,
        cke,cs,ras,cas,we,address,bank,mask,data,ss_refresh,12'hf0d
`ifdef SAROO_FC1_WINDOW
        ,refresh_active
`endif
    );

    integer cycle=0, base=0, edge_cycle=-1, start_cycle=-1, cmd_cycle=-1, ack_cycle=-1;
    integer st_admit=-1, st_ack=-1, rows=0, first_refresh=-1, writes=0, read_delay=0, read_left=0;
    integer low_clocks=18, prebusy=-999, write_mcu=0;
    reg armed=0, driving=0;
    assign data=driving ? 16'h1234 : 16'hzzzz;

    always @(posedge mclk) begin
        cycle=cycle+1;
        #2;
        if (read_delay>0) read_delay=read_delay-1;
        driving=read_delay==0 && read_left>0;
        if (driving) read_left=read_left-1;
        if (reset && dut._tsd.m_state==1 && !dut._tsd.ref_req && !armed) begin
            armed=1;
            base=cycle+20;
        end
        if (ss_refresh && edge_cycle<0) edge_cycle=cycle;
        if (ss_rd_start && start_cycle<0) start_cycle=cycle;
        if (dut.cmd_req && !dut.act_ab && cmd_cycle<0) cmd_cycle=cycle;
        if (dut.cmd_ack && !dut.act_ab && ack_cycle<0) ack_cycle=cycle;
        if (dut.cmd_req && dut.act_ab && st_admit<0) st_admit=cycle;
        if (dut.cmd_ack && dut.act_ab && st_ack<0) st_ack=cycle;
        if (armed && cke) case ({cs,ras,cas,we})
            4'b0001: begin
                if (!rows) first_refresh=cycle;
                else if (cycle-first_refresh!=7) $fatal(1,"bad tRFC in FC1 pair");
                rows=rows+1;
            end
            4'b0101: begin read_delay=2; read_left=4; end
            4'b0100: begin
                #1;
                if (data!==16'hface || mask!==2'b01) $fatal(1,"wrong MCU write data or mask");
                writes=writes+1;
            end
        endcase
    end

    always @(negedge mclk) if (armed) begin
        if (cycle==base) SS_FC1=0;
        if (cycle==base+low_clocks) begin SS_FC1=1; SS_CS0=0; SS_RD=0; end
        if (cycle==base+low_clocks+8) begin SS_CS0=1; SS_RD=1; end
        if (cycle==base+prebusy) begin st_cs=1; st_rd=!write_mcu; st_wr=write_mcu; end
        else if (cycle==base+prebusy+1) begin st_rd=0; st_wr=0; end
    end

    initial begin
        if (!$value$plusargs("low=%d",low_clocks)) low_clocks=18;
        if (!$value$plusargs("prebusy=%d",prebusy)) prebusy=-999;
        if (!$value$plusargs("write=%d",write_mcu)) write_mcu=0;
        #1; reset=0; repeat (2) @(posedge mclk); @(negedge mclk); reset=1;
        wait(armed);
        while (cycle<base+low_clocks+80) @(negedge mclk);
        if (ack_cycle<0 || ss_wait!==1'b1 || ss_rdata!==16'h1234 || rows!=2)
            $fatal(1,"Saturn read or FC1 pair incomplete");
        if (prebusy>=0 && st_admit<=edge_cycle+low_clocks)
            $fatal(1,"new MCU request occupied the synchronized FC1 window");
        if (prebusy!=-999 && (st_ack<0 || st_wait!==1'b1 ||
            (!write_mcu && st_rdata!==16'h1234) || writes!=write_mcu))
            $fatal(1,"MCU request did not complete");
        $display("PASS start=%0d cmd=%0d ack=%0d",start_cycle-edge_cycle,
                 cmd_cycle-edge_cycle,ack_cycle-edge_cycle);
        $finish;
    end
    initial begin #1000000; $fatal(1,"fixture deadline"); end
endmodule
