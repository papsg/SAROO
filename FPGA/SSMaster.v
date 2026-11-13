
///////////////////////////////////////////////////////
// Module: SEGA Saturn Master Flash Card             //
///////////////////////////////////////////////////////

// version 0.1: first step.
//         0.2: for HW1.2
//         0.3:
//         0.4: support put_sector_data
//         0.5: CDC bug fix


module SSMaster(
	// System
	CLK_50M,
	// SDRAM
	SD_CKE, SD_CLK, SD_CS, SD_WE, SD_CAS, SD_RAS, SD_ADDR, SD_BA, SD_DQM, SD_DQ,
	// PSRAM
	PSRAM_CS,

	// SS system
	SS_MCLK, SS_RST,
	// SS I2S output
	SS_SCLK, SS_SSEL, SS_BCK, SS_LRCK, SS_SD,
	// SS ABUS
	SS_FC0, SS_FC1, SS_TIM0, SS_TIM1, SS_TIM2, SS_AAS,
	SS_ADDR, SS_DATA, SS_CS0, SS_CS1, SS_CS2, SS_RD, SS_WR0, SS_WR1, SS_WAIT, SS_IRQ,
	SS_DATA_OE, SS_DATA_DIR, SS_OUTEN,

	// STM32 FSMC
	ST_CLK, ST_AD, ST_ADDR, ST_CS, ST_RD, ST_WR, ST_BL0, ST_BL1, ST_ALE, ST_WAIT,
	// STM32 GPIO
	ST_GPIO0, ST_GPIO2, ST_GPIO3,
	// STM32 I2S
	ST_MCLK, ST_BCK, ST_LRCK, ST_SDO,

	// DEBUG LED
	LED0, LED1,
	// SPI Flash
	EPCS_CS, EPCS_CLK, EPCS_DI, EPCS_DO
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// System
	input CLK_50M;
	
	// SPI Flash
	output EPCS_CS;
	output EPCS_CLK;
	output EPCS_DI;
	input  EPCS_DO;

	// SDRAM
	output SD_CKE;
	output SD_CLK;
	output SD_CS;
	output SD_WE;
	output SD_CAS;
	output SD_RAS;
	output[12:0] SD_ADDR;
	output[ 1:0] SD_BA;
	output[ 1:0] SD_DQM;
	inout[15:0] SD_DQ;

	// PSRAM
	output PSRAM_CS;

	// SS System
	input SS_MCLK;
	input SS_RST;

	// SS I2S
	input SS_SCLK;
	output SS_SSEL;
	output SS_BCK;
	output SS_LRCK;
	output SS_SD;

	// SS ABUS
	input SS_FC0;
	input SS_FC1;
	input SS_TIM0;
	input SS_TIM1;
	input SS_TIM2;
	input SS_AAS;
	input[23:0] SS_ADDR;
	inout[15:0] SS_DATA;
	input SS_CS0;
	input SS_CS1;
	input SS_CS2;
	input SS_RD;
	input SS_WR0;
	input SS_WR1;
	output SS_WAIT;
	output SS_IRQ;
	output SS_DATA_OE;
	output SS_DATA_DIR;
	output SS_OUTEN;

	// STM32 FSMC
	input ST_CLK;
	inout[15:0] ST_AD;
	input[ 7:0] ST_ADDR;
	input ST_CS;
	input ST_RD;
	input ST_WR;
	input ST_BL0;
	input ST_BL1;
	input ST_ALE;
	output ST_WAIT;
	
	// STM32 GPIO
	input ST_GPIO0;
	output ST_GPIO2;
	input ST_GPIO3;

	// STM32 I2S
	input ST_MCLK;
	input ST_BCK;
	input ST_LRCK;
	input ST_SDO;

	// DEBUG LED
	output LED0;
	output LED1;

///////////////////////////////////////////////////////
// Debug LED                                         //
///////////////////////////////////////////////////////

	assign LED0 = 1'b0;
	assign LED1 = ss_reg_ctrl[8];

	wire NRESET = ST_GPIO0;
	assign ST_GPIO2 = ST_IRQ;


///////////////////////////////////////////////////////
// SPI Flash                                         //
///////////////////////////////////////////////////////

	reg[3:1] st_reg_spi;

	assign EPCS_CS  = st_reg_spi[3];
	assign EPCS_CLK = st_reg_spi[2];
	assign EPCS_DI  = st_reg_spi[1];

	
///////////////////////////////////////////////////////
// I2S                                               //
///////////////////////////////////////////////////////

	//assign ST_MCLK = SS_SCLK;
	assign SS_BCK  = ST_BCK;
	assign SS_LRCK = ST_LRCK;
	assign SS_SD   = ST_SDO;


///////////////////////////////////////////////////////
// PLL: 50M -> 100M                                  //
///////////////////////////////////////////////////////


	wire mclk;
	wire sdclk;
	wire pll_locked;
	mainpll _mpll(CLK_50M, mclk, sdclk, pll_locked);


///////////////////////////////////////////////////////
// STM32 FSMC                                        //
///////////////////////////////////////////////////////

	// NADV latches the address and publishes one event per FMC halfword.
	reg[24:0] fsmc_addr_latched;
	reg ale_toggle;
	always @(negedge NRESET or posedge ST_ALE)
	begin
		if(!NRESET) begin
			fsmc_addr_latched <= 0;
			ale_toggle <= 0;
		end else if(!ST_CS) begin
			fsmc_addr_latched <= {ST_ADDR, ST_AD, 1'b0};
			ale_toggle <= ~ale_toggle;
		end
	end

	(* altera_attribute = "-name ADV_NETLIST_OPT_ALLOWED NEVER_ALLOW; -name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON" *)
	reg ale_meta;
	(* altera_attribute = "-name ADV_NETLIST_OPT_ALLOWED NEVER_ALLOW; -name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON" *)
	reg ale_sync;
	reg ale_accepted;
	reg[24:0] fsmc_addr;
	reg[1:0] st_mask_held;
	reg[15:0] st_wdata_held;
	wire st_access_start = ale_sync ^ ale_accepted;

	always @(negedge NRESET or posedge mclk)
	begin
		if(!NRESET) begin
			ale_meta <= 0;
			ale_sync <= 0;
			ale_accepted <= 0;
			fsmc_addr <= 0;
			st_mask_held <= 0;
			st_wdata_held <= 0;
		end else begin
			ale_meta <= ale_toggle;
			ale_sync <= ale_meta;
			// Settle the address while the event crosses both registers.
			fsmc_addr <= fsmc_addr_latched;
			if(st_access_start) begin
				st_mask_held <= {ST_BL1, ST_BL0};
				st_wdata_held <= ST_AD;
				ale_accepted <= ale_sync;
			end
		end
	end

	// The MCU holds the data phase while NWAIT is asserted. Consumers on
	// the admission edge see the settled pins. Later consumers use the copy.
	wire st_rd_start = st_access_start && ST_WR;
	wire st_wr_start = st_access_start && !ST_WR;
	wire[1:0] st_mask = st_access_start ? {ST_BL1, ST_BL0} : st_mask_held;
	wire[15:0] st_wdata = st_access_start ? ST_AD : st_wdata_held;


///////////////////////////////////////////////////////
// STM32 FPGA control register                       //
///////////////////////////////////////////////////////

	reg[15:0] st_reg_ctrl;
	reg[15:0] ss_resp1;
	reg[15:0] ss_resp2;
	reg[15:0] ss_resp3;
	reg[15:0] ss_resp4;

	wire st_fifo_dir   = st_reg_ctrl[9];
	wire st_fifo_reset = st_reg_ctrl[8];
	wire st_fifo_write = (st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h08);
	wire st_fifo_read  = (st_rd_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h08);

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			st_reg_ctrl <= 0;
			st_reg_spi <= 3'b111;
			st_reg_sdram <= 12'b11_1100001101; // type:64MB  refcnt: 0x30d(7.81us)
		end else if(st_wr_start==1) begin
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h04) st_reg_ctrl <= st_wdata;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h16) {st_reg_sdram, st_reg_spi} <= st_wdata[15:1];
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h20) ss_resp1 <= st_wdata;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h22) ss_resp2 <= st_wdata;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h24) ss_resp3 <= st_wdata;
			if(fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h26) ss_resp4 <= st_wdata;
		end
	end



///////////////////////////////////////////////////////
// Saturn to STM32: HIRQ register                    //
///////////////////////////////////////////////////////

	reg[15:0] ss_hirq;
	wire ss_hirq_clear = ss_wr_start && ss_write_cdc_cs && ss_write_addr[5:2]==4'b00_10;

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			ss_hirq <= 0;
		end else begin
			if(     st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h28)
				// STM32 set
				ss_hirq <= ss_hirq|st_wdata;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h2c)
				// Both simultaneous clear requests must take effect.
				ss_hirq <= ss_hirq&(~st_wdata)&
					(ss_hirq_clear ? ss_write_data : 16'hffff);
			else if(ss_hirq_clear)
				// Saturn reset
				ss_hirq <= ss_hirq&ss_write_data;
//			else if(ss_wr_start==1 && ss_cdc_cs==1 && SS_ADDR[5:2]==4'b10_01 && ss_cr1==0)
//				// Saturn write CR4
//				// Auto ack getStatus command
//				ss_hirq <= ss_hirq|16'b1;
		end
	end


///////////////////////////////////////////////////////
// Saturn to STM32: CDC request                      //
///////////////////////////////////////////////////////

	reg st_irq_cdc;

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			st_irq_cdc <= 1'b0;
		end else begin
			if(ss_wr_start && ss_write_cdc_cs && ss_write_addr[5:2]==4'b10_01)
				// SS write CR4 and not a getStatus command
				st_irq_cdc <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_cdc <= st_irq_cdc&(~st_wdata[0]);
		end
	end

///////////////////////////////////////////////////////
// Saturn to STM32: CMD request                      //
///////////////////////////////////////////////////////

	reg st_irq_cmd;

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			st_irq_cmd <= 1'b0;
		end else begin
			if(ss_wr_start && ss_write_reg_cs && ss_write_addr[5:2]==4'b01_00)
				st_irq_cmd <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_cmd <= st_irq_cmd&(~st_wdata[1]);
		end
	end

///////////////////////////////////////////////////////
// CDC FIFO dma end                                  //
///////////////////////////////////////////////////////

	reg st_irq_fifo;
	reg[2:0] fifo_usedw_d;
	reg fifo_empty_d;

	always @(posedge mclk)
	begin
		fifo_usedw_d <= fifo_usedw[10:8];
		fifo_empty_d <= fifo_empty;
	end

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			st_irq_fifo <= 1'b0;
		end else begin
			if(fifo_empty_d==0 && fifo_empty==1)
				// empty
				st_irq_fifo <= 1'b1;
			else if(fifo_usedw_d==3'b001 && fifo_usedw[10:8]==3'b000)
				// 256->255
				st_irq_fifo <= 1'b1;
			else if(fifo_usedw_d==3'b011 && fifo_usedw[10:8]==3'b100)
				// 1023->1024
				st_irq_fifo <= 1'b1;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h06)
				st_irq_fifo <= st_irq_fifo&(~st_wdata[2]);
		end
	end

///////////////////////////////////////////////////////
// Saturn ss_in_cmd                                  //
///////////////////////////////////////////////////////

	reg ss_in_cmd;

	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			ss_in_cmd <= 1'b0;
		end else begin
			if(ss_rd_start==1 && ss_cdc_cs==1 && SS_ADDR[5:2]==4'b10_01)
				// Read CR4
				ss_in_cmd <= 1'b0;
			else if(ss_wr_start && ss_write_cdc_cs && ss_write_addr[5:2]==4'b10_01)
				// Write CR4
				ss_in_cmd <= 1'b1;
		end
	end


///////////////////////////////////////////////////////
// STM32 read data                                   //
///////////////////////////////////////////////////////

	reg[15:0] st_reg_data_out;

	always @(posedge mclk)
	begin
		st_reg_data_out <= 
						(fsmc_addr[7:0]==8'h00)? 16'h5253 : // ID: "SR"
						(fsmc_addr[7:0]==8'h02)? 16'h1205 : // ver: HW1.2 && SW0.5
						(fsmc_addr[7:0]==8'h04)? st_reg_ctrl :
						(fsmc_addr[7:0]==8'h06)? st_reg_stat :
						(fsmc_addr[7:0]==8'h08)? st_fifo_data_out :
						(fsmc_addr[7:0]==8'h0a)? st_fifo_data_out :
						(fsmc_addr[7:0]==8'h0c)? st_fifo_stat :
						(fsmc_addr[7:0]==8'h0e)? st_fifo_rcnt :
						(fsmc_addr[7:0]==8'h10)? ss_reg_cmd :
						(fsmc_addr[7:0]==8'h12)? ss_reg_data :
						(fsmc_addr[7:0]==8'h14)? ss_reg_ctrl :
						(fsmc_addr[7:0]==8'h16)? {st_reg_sdram, EPCS_CS, EPCS_CLK, EPCS_DI, EPCS_DO} :

						(fsmc_addr[7:0]==8'h18)? ss_resp1 :
						(fsmc_addr[7:0]==8'h1a)? ss_resp2 :
						(fsmc_addr[7:0]==8'h1c)? ss_resp3 :
						(fsmc_addr[7:0]==8'h1e)? ss_resp4 :

						(fsmc_addr[7:0]==8'h20)? ss_cr1 :
						(fsmc_addr[7:0]==8'h22)? ss_cr2 :
						(fsmc_addr[7:0]==8'h24)? ss_cr3 :
						(fsmc_addr[7:0]==8'h26)? ss_cr4 :
						(fsmc_addr[7:0]==8'h28)? ss_hirq :
						(fsmc_addr[7:0]==8'h2a)? ss_hirq_mask :

						16'hffff;
	end


	reg st_read_enable;
	wire st_read_reset = !NRESET || ST_CS || !ST_ALE || ST_RD;
	always @(posedge st_read_reset or posedge mclk)
	begin
		if(st_read_reset)
			st_read_enable <= 0;
		else if(st_rd_start)
			st_read_enable <= 1;
	end

	assign ST_AD = st_read_enable? (
						(fsmc_addr[24]==0)? st_reg_data_out : st_ram_data_out
					) : 16'hzzzz;


	wire st_ram_cs = !ST_CS && fsmc_addr[24];

	wire[15:0] st_reg_stat = {
		pll_locked, ST_IRQ, 1'b0, ss_in_cmd, 4'b0,
		5'b0, st_irq_fifo, st_irq_cmd, st_irq_cdc
	};

	wire[15:0] st_fifo_stat = {4'b0, fifo_full, fifo_usedw};


	wire st_irq_fifoen = st_reg_ctrl[2];
	wire st_irq_cmd_en = st_reg_ctrl[1];
	wire st_irq_cdc_en = st_reg_ctrl[0];

	wire ST_IRQ = ( (st_irq_cdc_en==1 && st_irq_cdc==1) ||
					(st_irq_cmd_en==1 && st_irq_cmd==1) ||
					(st_irq_fifoen==1 && st_irq_fifo==1) );



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// SATURN ABUS                                       //
///////////////////////////////////////////////////////

	reg sscs_s0, sscs_s1, sscs_s2;
	reg ssfc1_s0, ssfc1_s1, ssfc1_s2;

	always @(posedge mclk)
	begin
		sscs_s0 <= (SS_CS0 & SS_CS1 & SS_CS2) | ~SS_FC1;
		sscs_s1 <= sscs_s0;
		sscs_s2 <= sscs_s1;

		ssfc1_s0 <= SS_FC1;
		ssfc1_s1 <= ssfc1_s0;
		ssfc1_s2 <= ssfc1_s1;
	end

	wire ss_refresh = (ssfc1_s2==1 && ssfc1_s1==0);

	wire ss_rd_start = (sscs_s2==1 && sscs_s1==0 && SS_RD==0);

	// TIM1 marks the write data phase. Synchronize the event and sample
	// the settled bus bundle before sending one write to the consumers.
	(* altera_attribute = "-name ADV_NETLIST_OPT_ALLOWED NEVER_ALLOW; -name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON" *)
	reg ss_tim1_meta;
	(* altera_attribute = "-name ADV_NETLIST_OPT_ALLOWED NEVER_ALLOW; -name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS; -name DONT_MERGE_REGISTER ON; -name PRESERVE_REGISTER ON" *)
	reg ss_tim1_sync;
	reg ss_tim1_previous;
	reg ss_write_armed;
	reg ss_write_pending;
	reg ss_wr_start;
	reg[23:0] ss_addr_sample;
	reg[15:0] ss_data_sample;
	reg[2:0] ss_cs_sample;
	reg[1:0] ss_mask_sample;
	reg[23:0] ss_write_addr;
	reg[15:0] ss_write_data;
	reg[2:0] ss_write_cs;
	reg[1:0] ss_write_mask;
	reg ss_write_ram_alias;

	wire ss_select_start = sscs_s2 && !sscs_s1;
	wire ss_select_end = !sscs_s2 && sscs_s1;
	wire ss_tim1_event = ss_tim1_sync ^ ss_tim1_previous;

	always @(negedge NRESET or posedge mclk)
	begin
		if(!NRESET) begin
			ss_tim1_meta <= 0;
			ss_tim1_sync <= 0;
			ss_tim1_previous <= 0;
			ss_write_armed <= 0;
			ss_write_pending <= 0;
			ss_wr_start <= 0;
			ss_addr_sample <= 0;
			ss_data_sample <= 0;
			ss_cs_sample <= 3'b111;
			ss_mask_sample <= 2'b11;
			ss_write_addr <= 0;
			ss_write_data <= 0;
			ss_write_cs <= 3'b111;
			ss_write_mask <= 2'b11;
			ss_write_ram_alias <= 0;
		end else begin
			ss_tim1_meta <= SS_TIM1;
			ss_tim1_sync <= ss_tim1_meta;
			ss_tim1_previous <= ss_tim1_sync;
			ss_addr_sample <= SS_ADDR;
			ss_data_sample <= SS_DATA;
			ss_cs_sample <= {SS_CS2, SS_CS1, SS_CS0};
			ss_mask_sample <= {SS_WR0, SS_WR1};
			ss_wr_start <= 0;
			ss_write_pending <= 0;

			if(ss_select_start)
				ss_write_armed <= 1;
			else if(ss_select_end)
				ss_write_armed <= 0;
			else if(ss_write_armed && ss_tim1_event) begin
				ss_write_armed <= 0;
				ss_write_pending <= 1;
			end

			if(ss_write_pending && ss_cs_sample!=3'b111 && ss_mask_sample!=2'b11) begin
				ss_wr_start <= 1;
				ss_write_addr <= ss_addr_sample;
				ss_write_data <= ss_data_sample;
				ss_write_cs <= ss_cs_sample;
				ss_write_mask <= ss_mask_sample;
				ss_write_ram_alias <= ss_addr_sample[23:22]==2'b01 && ss_cs0_type==2'b10;
			end
		end
	end

	assign SS_DATA =
					(SS_RD==0 && SS_CS0==0)? ss_ram_data_out :
					(SS_RD==0 && SS_CS1==0)? ss_cs1_data_out :
					(SS_RD==0 && ss_reg_cs==1)? ss_bcr_data_out :
					(SS_RD==0 && ss_cdc_cs==1)? ss_cdc_data_out :
					16'hzzzz;

	assign SS_DATA_OE = ~SS_FC1 | (SS_CS0==1 && SS_CS1==1 && (SS_CS2==1 || (ss_cdc_cs==1 && SS_RD==0 && ss_cdc_en==0)));

	assign SS_DATA_DIR = (SS_WR0 & SS_WR1);

///////////////////////////////////////////////////////
// SATURN System Control                             //
///////////////////////////////////////////////////////

	reg[15:0] ss_bcr_data_out;
	reg[15:0] ss_reg_ctrl;
	reg[15:0] ss_reg_cmd;
	reg[15:0] ss_reg_data;
	reg[31:0] ss_reg_timer;

	// 00: Bootrom
	// 01: Data Cart
	// 10: RAM Cart: 1MBytes 
	// 11: RAM Cart: 4MBytes 
	wire[1:0] ss_cs0_type = ss_reg_ctrl[13:12];
	wire[15:0] ss_cs1_data_out = (SS_ADDR[23:16]==8'hff && ss_cs0_type[1]==1)?
									(
										(ss_cs0_type[0]==0)? 16'hff5a : 16'hff5c
									) : 16'hffff;

	wire[15:0] ss_reg_stat = {4'b0, fifo_full, fifo_usedw};


	// 0x25807000
	wire ss_reg_cs = (SS_CS2==0 && SS_ADDR[14:12]==3'b111);
	wire ss_write_reg_cs = (!ss_write_cs[2] && ss_write_addr[14:12]==3'b111);

	always @(posedge mclk)
	begin
		ss_bcr_data_out <= 
			(SS_ADDR[5:1]==5'b00_000)? 16'h5253 : // ID: "SR"
			(SS_ADDR[5:1]==5'b00_001)? 16'h1205 : // ver: HW1.2 && SW0.5
			(SS_ADDR[5:2]==4'b00_01 )? ss_reg_ctrl :
			(SS_ADDR[5:2]==4'b00_10 )? ss_reg_stat :
			(SS_ADDR[5:1]==5'b00_110)? ss_reg_timer[31:16] :
			(SS_ADDR[5:1]==5'b00_111)? ss_reg_timer[15: 0] :
			(SS_ADDR[5:2]==4'b01_00 )? ss_reg_cmd :
			(SS_ADDR[5:2]==4'b01_01 )? ss_reg_data :
			(SS_ADDR[5:2]==4'b01_10 )? ss_fifo_data_out :
			16'h0000;
	end


	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0)
			ss_reg_ctrl <= 16'h0100;
		else if(ss_wr_start && ss_write_reg_cs && ss_write_addr[5:2]==4'b00_01)
			ss_reg_ctrl <= ss_write_data;
	end

	// Saturn set and STM32 reset
	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0)
			ss_reg_cmd <= 4'b0000;
		else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h10)
			ss_reg_cmd <= 0;
		else if(ss_wr_start && ss_write_reg_cs && ss_write_addr[5:2]==4'b01_00)
			ss_reg_cmd <= ss_write_data;
	end

	// Saturn or STM32 set
	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0) begin
			ss_reg_data <= 1'b0;
		end else begin
			if(ss_wr_start && ss_write_reg_cs && ss_write_addr[5:2]==4'b01_01)
				ss_reg_data <= ss_write_data;
			else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h12)
				ss_reg_data <= st_wdata;
		end
	end



	reg[6:0] ss_timer_feed;

	always @(posedge mclk)
	begin
		if(ss_timer_feed==7'd99)
			ss_timer_feed <= 0;
		else
			ss_timer_feed <= ss_timer_feed+7'b1;
	end

	always @(posedge mclk)
	begin
		if(ss_wr_start && ss_write_reg_cs && ss_write_addr[5:2]==4'b00_11)
			ss_reg_timer <= 0;
		else if(ss_timer_feed==0) begin
			ss_reg_timer <= ss_reg_timer+32'b1;
		end
	end


///////////////////////////////////////////////////////
// SATURN CDC FIFO                                   //
///////////////////////////////////////////////////////

	wire ss_fifo_read  = ss_rd_start==1 && (ss_cdc_cs==1 && SS_ADDR[5:2]==0);
	wire ss_fifo_write = ss_wr_start && ss_write_cdc_cs && ss_write_addr[5:2]==0;

	wire fifo_reset = (NRESET==0 || st_fifo_reset==1);
	wire fifo_empty;
	wire fifo_full;
	wire fifo_rd = (st_fifo_dir)? st_fifo_read  : ss_fifo_read;
	wire fifo_wr = (st_fifo_dir)? ss_fifo_write : st_fifo_write;
	wire[15:0] fifo_q;
	wire[15:0] fifo_wdata = st_fifo_dir ? {ss_write_data[7:0], ss_write_data[15:8]} : st_wdata;
	wire[10:0] fifo_usedw;

	wire[15:0] ss_fifo_data_out = {fifo_q[7:0], fifo_q[15:8]};
	wire[15:0] st_fifo_data_out = fifo_q;

	cdcfifo _cdcfifo(
				.sclr(fifo_reset),
				.clock(mclk),
				.rdreq(fifo_rd),
				.q(fifo_q),
				.wrreq(fifo_wr),
				.data(fifo_wdata),
				.empty(fifo_empty),
				.usedw(fifo_usedw),
				.full(fifo_full)
			);

	// Saturn set and STM32 reset
	reg[15:0] st_fifo_rcnt;
	always @(negedge NRESET or posedge mclk)
	begin
		if(NRESET==0)
			st_fifo_rcnt <= 4'b0000;
		else if(st_wr_start==1 && fsmc_addr[24]==0 && fsmc_addr[7:0]==8'h0e)
			st_fifo_rcnt <= 0;
		else if(ss_fifo_read)
			st_fifo_rcnt <= st_fifo_rcnt+1'b1;
	end


///////////////////////////////////////////////////////
// SATURN CDC                                        //
///////////////////////////////////////////////////////

	reg[15:0] ss_cdc_data_out;
	reg[15:0] ss_hirq_mask;
	reg[15:0] ss_cr1;
	reg[15:0] ss_cr2;
	reg[15:0] ss_cr3;
	reg[15:0] ss_cr4;

	// enable CDC read out
	wire ss_cdc_en = ss_reg_ctrl[15];

	// 0x25800000
	wire ss_cdc_cs = (SS_CS2==0 && SS_ADDR[14:12]==3'b000);
	wire ss_write_cdc_cs = (!ss_write_cs[2] && ss_write_addr[14:12]==3'b000);

	always @(posedge mclk)
	begin
		if(ss_wr_start && ss_write_cdc_cs) begin
			if(ss_write_addr[5:2]==4'b00_11) ss_hirq_mask <= ss_write_data;
			if(ss_write_addr[5:2]==4'b01_10) ss_cr1 <= ss_write_data;
			if(ss_write_addr[5:2]==4'b01_11) ss_cr2 <= ss_write_data;
			if(ss_write_addr[5:2]==4'b10_00) ss_cr3 <= ss_write_data;
			if(ss_write_addr[5:2]==4'b10_01) ss_cr4 <= ss_write_data;
		end
	end

	always @(posedge mclk)
	begin
		ss_cdc_data_out <= 
			(SS_ADDR[5:2]==4'b00_00)? ss_fifo_data_out :
			(SS_ADDR[5:2]==4'b00_10)? ss_hirq :
			(SS_ADDR[5:2]==4'b00_11)? ss_hirq_mask :
			(SS_ADDR[5:2]==4'b01_10)? ss_resp1 :
			(SS_ADDR[5:2]==4'b01_11)? ss_resp2 :
			(SS_ADDR[5:2]==4'b10_00)? ss_resp3 :
			(SS_ADDR[5:2]==4'b10_01)? ss_resp4 :
			16'h0000;
	end

	assign SS_SSEL = ~ss_cdc_en;

	assign SS_IRQ = (ss_hirq&ss_hirq_mask)==0? 1'b0: 1'b1;

	assign SS_OUTEN = ~ss_cdc_en;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// SDRAM                                             //
///////////////////////////////////////////////////////

	reg[11:0] st_reg_sdram;

	// STM32 is LittleEndian system
	wire[25:0] st_ram_addr = {2'b0, fsmc_addr[23:0]};
	wire[15:0] st_ram_data_out;
	wire st_ram_ready;
	
	// Saturn CS0 memmap:
	// 02000000 - 02400000 :  4MB ROM Cart
	// 02400000 - 02800000 :  4MB RAM Cart
	// 02800000 - 03000000 :  8MB Free Space
	// 03000000 - 04000000 : 16MB Free Space
	// Saturn is BigEndian system
	wire ss_ram_wr_start = ss_wr_start && !ss_write_cs[0];
	wire ss_ram_cs = !SS_CS0 || ss_ram_wr_start;
	wire[15:0] ss_ram_din = {ss_write_data[7:0], ss_write_data[15:8]};
	wire[25:0] ss_write_ram_addr = {
		2'b0, ss_write_addr[23:21],
		ss_write_ram_alias ? 2'b00 : ss_write_addr[20:19], ss_write_addr[18:0]
	};
	wire[15:0] ss_ram_dout;
	wire[15:0] ss_ram_data_out = {ss_ram_dout[7:0], ss_ram_dout[15:8]};
	wire ss_ram_wait;


	reg[25:0] ss_ram_addr;
	always @(posedge mclk)
	begin
		ss_ram_addr[25:24] <= 2'b0;
		ss_ram_addr[23:21] <= SS_ADDR[23:21];

		if(SS_ADDR[23:22]==2'b01 && ss_cs0_type==2'b10)
			ss_ram_addr[20:19] <= 2'b00;
		else
			ss_ram_addr[20:19] <= SS_ADDR[20:19];

		ss_ram_addr[18: 0] <= SS_ADDR[18:0];
	end

	memhub _mh(
		NRESET, mclk, sdclk,
		ss_ram_cs, ss_rd_start, ss_ram_wr_start, ss_write_mask, ss_ram_wait, ss_ram_addr, ss_ram_din, ss_ram_dout,
		st_ram_cs, st_rd_start, st_wr_start, st_mask, , st_ram_addr, st_wdata, st_ram_data_out,
		SD_CKE, SD_CS, SD_RAS, SD_CAS, SD_WE, SD_ADDR, SD_BA, SD_DQM, SD_DQ,
		ss_refresh, st_reg_sdram, ~ssfc1_s1 | ~ssfc1_s2, st_ram_ready, ss_write_ram_addr
	);

	assign SD_CLK = sdclk;
	assign PSRAM_CS = 1'b1;

	// NEx asserts NWAIT immediately. NADV also re-arms it for each
	// halfword of a wider transfer with NEx held low.
	wire st_wait_reset = !NRESET || ST_CS || !ST_ALE;
	reg st_wait_ready;
	reg st_wait_address_seen;
	always @(posedge st_wait_reset or posedge mclk)
	begin
		if(st_wait_reset) begin
			st_wait_ready <= 0;
			st_wait_address_seen <= 0;
		end else begin
			if(st_access_start) st_wait_address_seen <= 1;
			if(st_wait_address_seen && st_ram_ready) st_wait_ready <= 1;
		end
	end

	assign SS_WAIT = ss_ram_wait;
	assign ST_WAIT = ST_CS | (ST_ALE & st_wait_ready);

endmodule

