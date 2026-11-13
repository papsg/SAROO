// Adapted from paps-g e345a327 and eefa5aca. SDRAM command and byte-mask scoreboard.
`timescale 1ns/1ps
/* The four production modules use only declaration-order normalization. The scoreboard records emitted WRITE
 * commands; it does not model physical SDRAM or Saturn bus timing. */
module tb;
    reg clk=0, reset=1;
    always #5 clk=~clk;
    reg [1:0] cs=0,rd=0,wr=0;
    reg [25:0] addr_a=0,addr_b=0;
    reg [15:0] data_a=0,data_b=0;
    reg [1:0] mask_a=0,mask_b=0;
    wire wait_a,wait_b;
    wire [15:0] read_a,read_b,sd_data;
    wire sd_cke,sd_cs,sd_ras,sd_cas,sd_we;
    wire [12:0] sd_addr;
    wire [1:0] sd_ba,sd_dqm;
`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(clk) capture_clk <= #6.5 clk;
`endif
    memhub dut(reset,clk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif

        cs[0],rd[0],wr[0],mask_a,wait_a,addr_a,data_a,read_a,
        cs[1],rd[1],wr[1],mask_b,wait_b,addr_b,data_b,read_b,
        sd_cke,sd_cs,sd_ras,sd_cas,sd_we,sd_addr,sd_ba,sd_dqm,sd_data,
        1'b0,12'hf0d
`ifdef SAROO_FC1_WINDOW
        ,1'b0
`endif
`ifdef SAROO_FMC_READY
        ,
`endif
    );

    reg [15:0] issued [0:255];
    reg [12:0] row [0:3];
    reg [25:0] issued_addr;
    integer completed=0;
    always @(posedge clk) begin
        #1;
        if (reset && sd_cke) begin
            if ({sd_cs,sd_ras,sd_cas,sd_we}==4'b0011)
                row[sd_ba]=sd_addr;
            if ({sd_cs,sd_ras,sd_cas,sd_we}==4'b0100) begin
                issued_addr={row[sd_ba],sd_ba,sd_addr[9:0],1'b0};
                if (issued_addr>=512 || ^sd_data===1'bx)
                    $fatal(1,"invalid emitted WRITE address/data");
                if (!sd_dqm[0]) issued[issued_addr[8:1]][7:0]=sd_data[7:0];
                if (!sd_dqm[1]) issued[issued_addr[8:1]][15:8]=sd_data[15:8];
                completed=completed+1;
            end
        end
    end

    integer port, phase, write_mask, cycles, before_count, i;
    function [15:0] merged(input [15:0] old_value, value, input [1:0] mask);
        merged={mask[1] ? old_value[15:8] : value[15:8],
                mask[0] ? old_value[7:0] : value[7:0]};
    endfunction
    task tick; begin @(posedge clk); #2; end endtask
    task start_write(input integer p,input [25:0] a,input [15:0] d,input [1:0] mask);
        begin
            @(negedge clk);
            cs=1<<p;wr=1<<p;
            if (p==0) begin addr_a=a;data_a=d;mask_a=mask;end
            else begin addr_b=a;data_b=d;mask_b=mask;end
            tick();
            if ((p==0 ? wait_a : wait_b)!==1'b0)
                $fatal(1,"WRITE did not enter wait");
            @(negedge clk);wr=0;
        end
    endtask
    task finish_write(input integer p,input [25:0] a,input [15:0] expected,
                      input integer expected_count);
        begin
            cycles=0;
            while ((p==0 ? wait_a : wait_b)!==1'b1 && cycles<25000) begin
                tick();cycles=cycles+1;
            end
            if (cycles==25000)
                $fatal(1,"write never completed: state=%0d ref_ack=%b ref_count=%0d",
                       dut._tsd.m_state,dut._tsd.ref_ack,dut._tsd.ref_count);
            if (completed!=expected_count || issued[a[8:1]]!==expected)
                $fatal(1,"port %0d acknowledged before its WRITE command: count %0d/%0d data %h/%h",
                       p,completed,expected_count,issued[a[8:1]],expected);
            @(negedge clk);cs=0;
            repeat(12) tick();
            if (completed!=expected_count) $fatal(1,"duplicate WRITE command");
        end
    endtask
    initial begin
        if (!$value$plusargs("port=%d",port)) port=0;
        if (!$value$plusargs("phase=%d",phase)) phase=0;
        if (!$value$plusargs("mask=%d",write_mask)) write_mask=0;
        if(port<0 || port>1 || phase<0 || phase>5 || write_mask<0 || write_mask>3)
            $fatal(1,"invalid fixture selection");
        for(i=0;i<256;i=i+1) issued[i]=16'h1111;
        #1;reset=0;repeat(2) tick();@(negedge clk);reset=1;
        start_write(port,26'h20,16'h1234,0);
        if (phase==0) begin
            while ((port==0 ? dut.cmd_ack_a : dut.cmd_ack_b)!==1'b1) tick();
        end else if (phase==1) begin
            while (dut.cmd_req!==2'b01) tick();
        end else if (phase==2) begin
            finish_write(port,26'h20,16'h1234,1);
        end else if (phase==4 || phase==5) begin
            finish_write(port,26'h20,16'h1234,1);
            if (phase==4) begin
                while (dut._tsd.ref_ack!==1'b1) tick();
            end else begin
                while ({sd_cs,sd_ras,sd_cas,sd_we}!==4'b0001) tick();
                if (dut._tsd.ref_ack!==0)
                    $fatal(1,"refresh issue did not retire its acknowledgment");
            end
        end else begin
            while (dut.cmd_ack!==1'b1) tick();
        end
        // Assert between clock edges, then hold across two more edges.
        #1;reset=0;cs=0;wr=0;
        repeat(2) tick();@(negedge clk);reset=1;
        repeat(2) tick();before_count=completed;
        start_write(port,26'h40,16'h5678,write_mask[1:0]);
        finish_write(port,26'h40,merged(16'h1111,16'h5678,write_mask[1:0]),before_count+1);
        start_write(port^1,26'h42,16'h9abc,0);
        finish_write(port^1,26'h42,16'h9abc,before_count+2);
        for(i=0;i<256;i=i+1)
            if(i!=16 && i!=32 && i!=33 && issued[i]!==16'h1111)
                $fatal(1,"unrelated location modified");
        if(issued[16] !== (phase==1 ? 16'h1111 : 16'h1234))
            $fatal(1,"unexpected pre-reset WRITE outcome");
        $display("PASS port=%0d reset_phase=%0d mask=%0d: new writes await their own command",port,phase,write_mask);
        $finish;
    end
    initial begin #1000000;$fatal(1,"fixture deadline");end
endmodule
