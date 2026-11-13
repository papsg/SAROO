// Adapted from paps-g eefa5aca.
`timescale 1ns/1ps
/* All four production memory modules run. This logical SDRAM model decodes
 * emitted commands, byte masks and a CL2/BL4 read stream. It is not an
 * electrical SDRAM model or a physical Saturn/FMC timing qualification. */
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
    );

    reg [15:0] memory [0:255];
    reg [12:0] row [0:3];
    reg [25:0] command_address;
    reg [15:0] model_data;
    reg model_drive=0;
    integer read_delay=0, read_left=0, read_word=0;
    integer writes=0,reads=0;
    reg [1:0] pending=0;
    reg [25:0] request_addr [0:1];
    reg [15:0] request_data [0:1];
    reg [1:0] request_mask [0:1];
    integer matched;
    assign sd_data=model_drive ? model_data : 16'hzzzz;
    always @(posedge clk) begin
        #2;
        if (read_delay>0) begin
            read_delay=read_delay-1;
            if (read_delay==0) begin
                model_data=memory[read_word];model_drive=1;
                read_word=read_word+1;read_left=read_left-1;
            end
        end else if (read_left>0) begin
            model_data=memory[read_word];model_drive=1;
            read_word=read_word+1;read_left=read_left-1;
        end else model_drive=0;
        if (reset && sd_cke) begin
            case ({sd_cs,sd_ras,sd_cas,sd_we})
            4'b0011: row[sd_ba]=sd_addr;
            4'b0100: begin
                command_address={row[sd_ba],sd_ba,sd_addr[9:0],1'b0};
                if (command_address>=512 || ^sd_data===1'bx || model_drive)
                    $fatal(1,"invalid emitted WRITE address/data or read collision");
                matched=-1;
                if (pending[0] && command_address==request_addr[0] && sd_data===request_data[0] && sd_dqm===request_mask[0]) matched=0;
                if (pending[1] && command_address==request_addr[1] && sd_data===request_data[1] && sd_dqm===request_mask[1]) matched=1;
                if (matched<0) $fatal(1,"WRITE does not match any pending caller request");
                pending[matched]=0;
                if (!sd_dqm[0]) memory[command_address[8:1]][7:0]=sd_data[7:0];
                if (!sd_dqm[1]) memory[command_address[8:1]][15:8]=sd_data[15:8];
                writes=writes+1;
            end
            4'b0101: begin
                command_address={row[sd_ba],sd_ba,sd_addr[9:0],1'b0};
                if (command_address>=512 || command_address[2:0]!=0 || read_left || read_delay)
                    $fatal(1,"invalid emitted READ address or overlapping burst");
                read_word=command_address[8:1];read_delay=2;read_left=4;reads=reads+1;
            end
            endcase
        end
    end

    task tick; begin @(posedge clk); #3; end endtask
    task read_check(input integer port,input [25:0] address);
        integer cycles;
        reg [15:0] expected;
        begin
            expected=memory[address[8:1]];
            @(negedge clk);cs=1<<port;rd=1<<port;
            if (port==0) addr_a=address;else addr_b=address;
            tick();@(negedge clk);rd=0;cycles=0;
            while ((port==0 ? wait_a : wait_b)!==1'b1 && cycles<25000) begin
                tick();cycles=cycles+1;
            end
            if (cycles==25000) $fatal(1,"read never completed");
            if ((port==0 ? read_a : read_b)!==expected)
                $fatal(1,"cache disagrees with SDRAM: port=%0d address=%h actual=%h expected=%h writes=%0d",
                       port,address,(port==0 ? read_a : read_b),expected,writes);
            @(negedge clk);cs=0;repeat(12) tick();
        end
    endtask
    task issue_write(input integer port,input [25:0] address,input [15:0] value,input [1:0] mask);
        begin
            if (pending[port]) $fatal(1,"caller issued a second outstanding write");
            pending[port]=1;request_addr[port]=address;request_data[port]=value;request_mask[port]=mask;
            cs[port]=1;wr[port]=1;
            if (port==0) begin addr_a=address;data_a=value;mask_a=mask;end
            else begin addr_b=address;data_b=value;mask_b=mask;end
        end
    endtask
    task fill_write_check(input integer reader,input integer phase,input integer word,input [1:0] mask);
        integer cycles,p,j,read_count;
        reg [15:0] expected;
        begin
            // The writer caches this line; the reader still has the old line.
            read_check(reader^1,26'h40);
            expected=memory[32];
            @(negedge clk);cs=1<<reader;rd=1<<reader;
            if (reader==0) addr_a=26'h40;else addr_b=26'h40;
            tick();@(negedge clk);rd=0;
            cycles=0;
            while (dut.data_valid!==1'b1 && cycles<25000) begin tick();cycles=cycles+1;end
            if (cycles==25000) $fatal(1,"read fill never started");
            repeat(phase) tick();
            @(negedge clk);issue_write(reader^1,26'h40+2*word,16'ha1a2,mask);
            tick();@(negedge clk);wr=0;
            cycles=0;
            while ((wait_a!==1'b1 || wait_b!==1'b1) && cycles<25000) begin tick();cycles=cycles+1;end
            if (cycles==25000 || writes!=1 || pending) $fatal(1,"fill/write did not complete");
            // The already-issued read precedes the write in SDRAM command order.
            if ((reader==0 ? read_a:read_b)!==expected) $fatal(1,"in-flight read result changed");
            @(negedge clk);cs=0;repeat(12) tick();read_count=reads;
            for(p=0;p<2;p=p+1)
                for(j=0;j<4;j=j+1) read_check(p,26'h40+2*j);
            if (reads!=read_count || reads!=4 || writes!=1) $fatal(1,"lost fill or duplicate command");
            for(j=0;j<256;j=j+1)
                if (j!=32+word && memory[j]!==16'h1000+j) $fatal(1,"fill/write changed unrelated memory");
            $display("PASS write during cache fill: reader=%0d phase=%0d word=%0d mask=%0d",reader,phase,word,mask);
        end
    endtask
    integer i,port,word_a,word_b,mask0,mask1,skew,first,cycles,read_count,fill_phase;
    initial begin
        if (!$value$plusargs("word_a=%d",word_a)) word_a=0;
        if (!$value$plusargs("word_b=%d",word_b)) word_b=1;
        if (!$value$plusargs("mask_a=%d",mask0)) mask0=0;
        if (!$value$plusargs("mask_b=%d",mask1)) mask1=0;
        if (!$value$plusargs("skew=%d",skew)) skew=0;
        if (!$value$plusargs("first=%d",first)) first=0;
        if (!$value$plusargs("fill_phase=%d",fill_phase)) fill_phase=-1;
        if (word_a<0 || word_a>3 || word_b<0 || word_b>3 || mask0<0 || mask0>3 ||
            mask1<0 || mask1>3 || skew<0 || skew>32 || first<0 || first>1 || fill_phase < -1 || fill_phase>3)
            $fatal(1,"invalid fixture selection");
        for(i=0;i<256;i=i+1) memory[i]=16'h1000+i;
        #1;reset=0;repeat(2) tick();@(negedge clk);reset=1;
        // Fill both caches from genuine emitted READ commands before writes.
        read_check(0,26'h20);read_check(1,26'h20);
        read_count=reads;
        for(port=0;port<2;port=port+1)
            for(i=0;i<4;i=i+1) read_check(port,26'h20+2*i);
        if (reads!=read_count || reads!=2) $fatal(1,"warm-cache setup did not use two fills");
        if (fill_phase>=0) begin
            fill_write_check(first,fill_phase,word_a,mask0);$finish;
        end
        @(negedge clk);
        issue_write(first,26'h20+2*(first ? word_b:word_a),first ? 16'hb1b2:16'ha1a2,first ? mask1[1:0]:mask0[1:0]);
        if (!skew) issue_write(first^1,26'h20+2*(first ? word_a:word_b),first ? 16'ha1a2:16'hb1b2,first ? mask0[1:0]:mask1[1:0]);
        tick();@(negedge clk);wr=0;
        if (skew) begin
            repeat(skew-1) tick();@(negedge clk);
            issue_write(first^1,26'h20+2*(first ? word_a:word_b),first ? 16'ha1a2:16'hb1b2,first ? mask0[1:0]:mask1[1:0]);
            tick();@(negedge clk);wr=0;
        end
        cycles=0;
        while ((wait_a!==1'b1 || wait_b!==1'b1) && cycles<25000) begin tick();cycles=cycles+1;end
        if (cycles==25000 || writes!=2 || pending) $fatal(1,"both writes did not complete exactly once");
        @(negedge clk);cs=0;repeat(12) tick();
        for(port=0;port<2;port=port+1)
            for(i=0;i<4;i=i+1) read_check(port,26'h20+2*i);
        if (reads!=read_count || writes!=2) $fatal(1,"lost warm cache or duplicated command");
        for(i=0;i<256;i=i+1)
            if(i!=16+word_a && i!=16+word_b && memory[i]!==16'h1000+i)
                $fatal(1,"unrelated SDRAM word changed");
        $display("PASS coherent completed writes: words=%0d/%0d masks=%0d/%0d skew=%0d first=%0d",
                 word_a,word_b,mask0,mask1,skew,first);
        $finish;
    end
    initial begin #1000000;$fatal(1,"fixture deadline");end
endmodule
