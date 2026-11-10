// paps-g FPGA-HIRQ-001 clear/clear scenario, adapted to original FPGA05.
`timescale 1ns/1ps
module tb;
  reg mclk=0, NRESET=1, st_wr_start=0, ss_wr_start=0, ss_cdc_cs=0;
  reg [24:0] fsmc_addr=0;
  reg [23:0] SS_ADDR=0;
  reg [15:0] ST_AD=0, SS_DATA=16'hffff;
  wire [15:0] value;
  hirq_dut dut(.*);
  integer mode=0, count=0, a, b, pattern, op, host, decode;
  reg [15:0] old_bits, mcu_bits, host_mask;
  reg [31:0] seed=32'h17c0ffee;

  task tick;
    begin #1; mclk=1; #1; mclk=0; #1; end
  endtask

  task check_update(input [15:0] previous, input [15:0] mcu,
                    input [15:0] saturn, input integer operation,
                    input integer acknowledge, input integer bad_decode);
    reg [15:0] expected;
    reg mcu_valid, host_valid;
    integer bitno;
    begin
      NRESET=0; #1; NRESET=1;
      st_wr_start=1; fsmc_addr=25'h28; ST_AD=previous;
      ss_wr_start=0; ss_cdc_cs=0; tick;
      if(value !== previous) $fatal(1,"Initial register state mismatch");
      st_wr_start=operation!=0; fsmc_addr=operation==2 ? 25'h2c : 25'h28;
      ST_AD=mcu; SS_DATA=saturn; ss_wr_start=acknowledge;
      ss_cdc_cs=1; SS_ADDR=8;
      if(bad_decode==1) fsmc_addr[24]=1;
      if(bad_decode==2) ss_cdc_cs=0;
      if(bad_decode==3) SS_ADDR=16;
      if(bad_decode==4) fsmc_addr[7:0]=8'h2e;
      mcu_valid=bad_decode!=1 && bad_decode!=4;
      host_valid=acknowledge && bad_decode!=2 && bad_decode!=3;
      expected=previous;
      // Independent per-bit oracle. Preserve FPGA05 MCU-SET priority. Only
      // simultaneous CLEAR requests are combined by this narrow correction.
      for(bitno=0;bitno<16;bitno=bitno+1) begin
        if(operation==1 && mcu_valid) begin
          if(mcu[bitno]) expected[bitno]=1;
        end else begin
          if(operation==2 && mcu_valid && mcu[bitno]) expected[bitno]=0;
          if(host_valid && !saturn[bitno]) expected[bitno]=0;
        end
      end
      tick;
      if(value !== expected) begin
        $display("FAIL mode=%0d old=%04x mcu=%04x saturn=%04x op=%0d ack=%0d decode=%0d got=%04x expected=%04x",
                 mode,previous,mcu,saturn,operation,acknowledge,bad_decode,value,expected);
        $fatal(1,"Lost interrupt clear or changed priority");
      end
      count=count+1;
    end
  endtask

  initial begin
    if(!$value$plusargs("mode=%d",mode)) mode=0;
    if(mode==0) begin
      // Known FPGA05 SET/CLEAR loss is deliberately unchanged by this port:
      // old0001 | set0004 gives0005 despite the simultaneous Saturn clear.
      check_update(16'h0001,16'h0004,16'hfffe,1,1,0);
      for(a=0;a<16;a=a+1)
        for(pattern=0;pattern<4;pattern=pattern+1)
          for(op=0;op<3;op=op+1)
            for(host=0;host<2;host=host+1)
              for(decode=0;decode<5;decode=decode+1) begin
                old_bits=pattern==0?0:pattern==1?16'hffff:pattern==2?16'h5555:16'haaaa;
                mcu_bits=16'h1<<a;
                host_mask=~((16'h1<<a)|(16'h1<<((a+5)%16)));
                if(op!=2 || host==0 || decode!=0)
                  check_update(old_bits,mcu_bits,host_mask,op,host,decode);
              end
    end else if(mode==1) begin
      check_update(16'h0005,16'h0004,16'hfffe,2,1,0);
      for(pattern=0;pattern<4;pattern=pattern+1)
        for(a=0;a<16;a=a+1)
          for(b=0;b<16;b=b+1) begin
            old_bits=pattern==0?0:pattern==1?16'hffff:pattern==2?16'h5555:16'haaaa;
            check_update(old_bits,16'h1<<a,~(16'h1<<b),2,1,0);
          end
      for(a=0;a<4096;a=a+1) begin
        seed=1664525*seed+1013904223;old_bits=seed[15:0];
        seed=1664525*seed+1013904223;mcu_bits=seed[31:16];
        seed=1664525*seed+1013904223;host_mask=seed[15:0];
        check_update(old_bits,mcu_bits,host_mask,2,1,0);
      end
    end else $fatal(1,"Unknown test mode");
    st_wr_start=1;fsmc_addr=25'h28;ST_AD=16'hffff;ss_wr_start=0;tick;
    NRESET=0; #1;
    if(value!==0) $fatal(1,"Asynchronous reset failed");
    $display("PASS %0d register updates; mode=%0d",count,mode);
    $finish;
  end
endmodule
