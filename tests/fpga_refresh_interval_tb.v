`timescale 1ns/1ps
/* Adapted from paps-g cd22aeb1. The scoreboard observes emitted AUTO
 * REFRESH commands; it does not model analog retention or board clocks. */
module tb;
    reg clk=0, reset=1, ext_refresh=0;
    always #5 clk=~clk;
    reg [11:0] cfg=12'hf0d;
    reg [1:0] cmd_req=0, cmd_mask=0;
    reg [31:0] cmd_addr=0;
    reg [15:0] cmd_din=0;
    wire cmd_ack, data_valid;
    wire [15:0] cmd_dout;
    wire sd_cke,sd_cs,sd_ras,sd_cas,sd_we,sd_oe;
    wire [12:0] sd_addr;
    wire [1:0] sd_ba,sd_dqm;
    wire [15:0] sd_dout;

`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(clk) capture_clk <= #6.5 clk;
`endif
    tsdram dut(reset,clk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif
        cfg,ext_refresh,cmd_req,cmd_ack,cmd_mask,
        cmd_addr,cmd_din,cmd_dout,data_valid,sd_cke,sd_cs,sd_ras,sd_cas,
        sd_we,sd_addr,sd_ba,sd_dqm,16'h1234,sd_dout,sd_oe);

    integer cycles=0,last_refresh=-1,interval=0,max_interval=0,refreshes=0;
    integer mode=0,target=0,limit=0,cfg_value=781,i;
    reg armed=0;
    always @(posedge clk) begin
        cycles=cycles+1;
        #1;
        if(reset && dut.m_state==1) armed=1;
        if(armed && {sd_cs,sd_ras,sd_cas,sd_we}==4'b0001) begin
            if(last_refresh>=0) begin
                interval=cycles-last_refresh;
                if(interval>max_interval) max_interval=interval;
                if(interval>limit)
                    $fatal(1,"refresh interval %0d exceeds %0d",interval,limit);
            end
            last_refresh=cycles;
            refreshes=refreshes+1;
        end
    end

    task issue_at_expiry(input [1:0] request);
        begin
            while(!(dut.m_state==1 && dut.ref_count==1)) @(negedge clk);
            cmd_req=request;
            while(!cmd_ack) @(negedge clk);
            cmd_req=0;
        end
    endtask

    task pulse_external_early;
        begin
            while(!(dut.m_state==1 && dut.ref_count>16'd100)) @(negedge clk);
            ext_refresh=1;
            @(negedge clk);ext_refresh=0;
        end
    endtask

    task pulse_external_with_ack;
        begin
            while(dut.ref_ack!==1'b1) @(negedge clk);
            ext_refresh=1;
            @(negedge clk);ext_refresh=0;
        end
    endtask

    initial begin
        if(!$value$plusargs("mode=%d",mode)) mode=0;
        if(!$value$plusargs("target=%d",target)) target=32;
        if(!$value$plusargs("limit=%d",limit)) limit=781;
        if(!$value$plusargs("cfg=%d",cfg_value)) cfg_value=781;
        if(mode<0 || mode>4 || target<1 || limit<1 || cfg_value<0 || cfg_value>1023)
            $fatal(1,"invalid fixture selection");
        cfg={2'b11,cfg_value[9:0]};
        #1;reset=0;repeat(2) @(posedge clk);@(negedge clk);reset=1;
        while(refreshes<1) @(negedge clk);
        if(mode==0) begin
            while(refreshes<target+1) @(negedge clk);
        end else if(mode==1) begin
            for(i=0;i<target;i=i+1) begin
                issue_at_expiry(i[0] ? 2'b01 : 2'b10);
                while(refreshes<i+2) @(negedge clk);
            end
        end else if(mode==2) begin
            for(i=0;i<target;i=i+1) begin
                pulse_external_early();
                while(refreshes<i+2) @(negedge clk);
            end
        end else if(mode==3) begin
            for(i=0;i<target;i=i+1) begin
                pulse_external_with_ack();
                while(refreshes<i+3) @(negedge clk);
            end
        end else begin
            repeat(2048) @(negedge clk);
            if(refreshes!=1)
                $fatal(1,"zero reload did not preserve autonomous refresh disable");
        end
        $display("PASS mode=%0d refreshes=%0d max_interval=%0d config=%0d",
                 mode,refreshes,max_interval,cfg[9:0]);
        $finish;
    end
    initial begin #1000000000;$fatal(1,"fixture deadline");end
endmodule
