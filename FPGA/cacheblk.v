
///////////////////////////////////////////////////////
// Module: Cache Block manager                       //
///////////////////////////////////////////////////////

module cacheblk(
	reset, clk,
	cache_addr, cache_data1d, cache_valid, data_valid, cdata,
	cache_update, umask, uaddr, udata,
	cache_invalid, iaddr
);

///////////////////////////////////////////////////////
// Pins                                              //
///////////////////////////////////////////////////////

	// system
	input reset;
	input clk;

	// cache interface
	output[25:3] cache_addr;
	output[63:0] cache_data1d;
	output[ 3:0] cache_valid;
	input data_valid;
	input[15:0] cdata;

	// cache update
	input cache_update;
	input[ 1:0] umask;
	input[25:0] uaddr;
	input[15:0] udata;


	// cache incalid
	input cache_invalid;
	input[25:0] iaddr;


///////////////////////////////////////////////////////
// cache block manager                               //
///////////////////////////////////////////////////////

	reg[15:0] cache_data[3:0];
	reg[25:3] cache_addr;
	reg[ 3:0] cache_valid;
	reg[ 1:0] dcnt;
	
	assign cache_data1d = {cache_data[3],cache_data[2],cache_data[1],cache_data[0]};


	// cache_data
	always @(posedge clk)
	begin
		if(cache_update && uaddr[25:3]==cache_addr) begin
			if(umask[1]==0) cache_data[uaddr[2:1]][15:8] <= udata[15:8];
			if(umask[0]==0) cache_data[uaddr[2:1]][ 7:0] <= udata[ 7:0];
		end else if(data_valid) begin
			// 是否已经被cache_update填充?
			if(cache_valid[dcnt]==0) begin
				cache_data[dcnt] <= cdata;
			end
		end
	end

	// cache_addr
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cache_addr <= 23'h555555;
		end else if(cache_invalid)begin
			cache_addr <= iaddr[25:3];
		end
	end

	// cache_valid
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			cache_valid <= 0;
		end else if(cache_invalid)begin
			cache_valid <= 0;
		end else if(data_valid) begin
			cache_valid[dcnt] <= 1'b1;
		end
	end

	// dcnt
	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			dcnt <= 0;
		end else if(cache_invalid)begin
			dcnt <= 0;
		end else if(data_valid) begin
			dcnt <= dcnt+1'b1;
		end
	end


endmodule

