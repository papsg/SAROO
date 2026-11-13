`timescale 1ns/1ps
/* Adapted from paps-g 8a31651a. Observe SDRAM command pins and full row coverage. */
module tb;
    reg clk=0, reset=1, scheduled=0, manual=0;
    always #5 clk=~clk;
    wire ext_refresh=scheduled | manual;
    reg [11:0] cfg=12'hf0d;
    reg [1:0] cmd_req=0;
    wire cmd_ack, data_valid, cke, cs, ras, cas, we, oe;
    wire [12:0] address;
    wire [1:0] bank, mask;
    wire [15:0] data, dout;
`ifdef SAROO_SDRAM_CAPTURE
    reg capture_clk = 0;
    always @(clk) capture_clk <= #6.5 clk;
`endif
    tsdram dut(reset,clk,
`ifdef SAROO_SDRAM_CAPTURE
        capture_clk,
`endif
        cfg,ext_refresh,cmd_req,cmd_ack,2'b00,
        26'b0,16'b0,data,data_valid,cke,cs,ras,cas,we,address,bank,mask,16'h1234,dout,oe);

    integer cycle=0, released=0, base=0, mode=0, external_count=0;
    integer por_rows=0, refreshes=0, previous=-1, min_gap=9999999, max_gap=0;
    integer last_edge=-1, row_cycle[0:8191], row=0, row_gap, max_row=0;
    reg mode_loaded=0, armed=0, drive_external=1;
    always @(posedge clk) begin
        cycle=cycle+1;
        #1;
        if (reset && cke && !mode_loaded) begin
            if ({cs,ras,cas,we}==4'b0001) por_rows=por_rows+1;
            if ({cs,ras,cas,we}==4'b0000) begin
                if (por_rows!=2 || cycle-released!=20091)
                    $fatal(1,"FC1 changed the power-on sequence");
                mode_loaded=1;
            end
        end
        if (reset && dut.m_state==1 && !dut.ref_req && !armed) begin
            if (!mode_loaded) $fatal(1,"controller left startup before mode set");
            armed=1;
            base=cycle+20;
        end
        if (armed && {cs,ras,cas,we}==4'b0001) begin
            if (mode==0 && (last_edge<0 || cycle-last_edge>18))
                $fatal(1,"AUTO REFRESH outside the FC1 idle window, offset=%0d",cycle-last_edge);
            if (previous>=0) begin
                if (cycle-previous<7) $fatal(1,"tRFC violation");
                if (cycle-previous<min_gap) min_gap=cycle-previous;
                if (cycle-previous>max_gap) max_gap=cycle-previous;
            end
            if (refreshes>=8192) begin
                row_gap=cycle-row_cycle[row];
                if (row_gap>max_row) max_row=row_gap;
                if (row_gap>6080000) $fatal(1,"8,192-row refresh bound exceeded");
            end
            row_cycle[row]=cycle;
            row=(row+1)%8192;
            previous=cycle;
            refreshes=refreshes+1;
        end
    end

    always @(negedge clk) begin
        scheduled=0;
        if (mode==4 && reset && !armed && cycle>released && (cycle%1432)==0)
            scheduled=1;
        if (armed && drive_external && cycle>=base && ((cycle-base)%1432)==0) begin
            scheduled=1;
            external_count=external_count+1;
            last_edge=cycle;
            if ((mode==1 && external_count==4) || mode==3) drive_external=0;
        end
    end

    task issue_at_expiry;
        begin
            while (!(dut.m_state==1 && dut.ref_count==1)) @(negedge clk);
            cmd_req=refreshes[0] ? 2'b01 : 2'b10;
            while (!cmd_ack) @(negedge clk);
            cmd_req=0;
        end
    endtask

    initial begin
        if (!$value$plusargs("mode=%d",mode)) mode=0;
        if (mode==2 || mode==4 || mode==5) cfg=12'hc00;
        if (mode>=4) drive_external=0;
        #1; reset=0; repeat (2) @(posedge clk);
        @(negedge clk); released=cycle; reset=1;
        wait(armed);
        case (mode)
            0,1: begin
                while (refreshes<8200) @(negedge clk);
                if (min_gap!=7 || (mode==0 && max_gap!=1425) ||
                    (mode==1 && (external_count!=4 || max_gap<1500)))
                    $fatal(1,"pair or fallback interval mismatch");
            end
            2: begin
                while (external_count<8) @(negedge clk);
                drive_external=0;
                repeat (4096) @(negedge clk);
                if (refreshes!=16 || min_gap!=7)
                    $fatal(1,"zero reload did not keep exactly one pair per event");
            end
            3: begin
                while (external_count<1) @(negedge clk);
                while (refreshes<8200) issue_at_expiry();
                if (min_gap!=7 || max_gap<1500 || !max_row)
                    $fatal(1,"traffic fallback not observed");
            end
            4: begin
                repeat (1700) @(negedge clk);
                if (!mode_loaded || refreshes!=0) $fatal(1,"FC1 leaked into power-on initialization");
            end
            5: begin
                @(negedge clk); manual=1;
                @(negedge clk); manual=0;
                while (!dut.ref_ack) @(negedge clk);
                manual=1;
                @(negedge clk); manual=0;
                repeat (50) @(negedge clk);
                if (refreshes!=4 || min_gap!=7) $fatal(1,"new FC1 edge lost during acknowledgment");
            end
            default: $fatal(1,"invalid mode");
        endcase
        $display("PASS mode=%0d refreshes=%0d min_gap=%0d max_gap=%0d max_row=%0d",
                 mode,refreshes,min_gap,max_gap,max_row);
        $finish;
    end
    initial begin #120000000; $fatal(1,"fixture deadline"); end
endmodule
