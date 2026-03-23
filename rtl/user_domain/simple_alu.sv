//////////////////////////////////////////////////////////////////////////////
//
// Filename: simple_alu.sv
//
// Description:
// Fixed Point ALU of parameterizeable width and fraction bits
//
// Author(s): Fauzan Ibrahim
// Created: 2026-03-17
//
// Revision History:
// [Date] - [Author] - [Description of Change]
// 2026-03-17 - FI - Initial creation of module.
//
//////////////////////////////////////////////////////////////////////////////

module simple_alu #(
	/// The OBI configuration for all ports.
  parameter obi_pkg::obi_cfg_t           ObiCfg      = obi_pkg::ObiDefaultConfig,
  /// The request struct.
  parameter type                         obi_req_t   = logic,
  /// The response struct.
  parameter type                         obi_rsp_t   = logic,
	
	// Fixed Point ALU PArameter
	parameter WIDTH_IN1 = 16,
  parameter WIDTH_IN2 = 16,
	parameter FRAC_IN1 = 8,
	parameter FRAC_IN2 = 8,

	parameter WIDTH_OUT = 32,
	parameter FRAC_OUT = 16 
) (
  /// Clock
  input  logic clk_i,
  /// Active-low reset
  input  logic rst_ni,

  /// OBI request interface
  input  obi_req_t obi_req_i,
  /// OBI response interface
  output obi_rsp_t obi_rsp_o
);

  // Define some registers to hold the requests fields
  logic req_d, req_q;
  logic we_d, we_q;
  logic [ObiCfg.AddrWidth-1:0] addr_d, addr_q;
  logic [ObiCfg.IdWidth-1:0] id_d, id_q;
  logic [ObiCfg.DataWidth-1:0] wdata;
  // Signals to handle 2 data inputs from wdata
  logic signed [ObiCfg.DataWidth/2-1:0] wdata1,wdata2;

  // Signals used to create the response
  logic [ObiCfg.DataWidth-1:0] result_d, result_q, data_q, data_d; // Data field of the obi response
  logic [1:0] error_d, error_q; // How many errors were detected
  logic rsp_err ; // Error field of the obi respons
  
  // Note to avoid writing trivial always_ff statements we can use this macro defined in registers.svh 
  `FF(req_q, req_d, '0);
  `FF(id_q , id_d , '0);
  `FF(we_q , we_d , '0);
  `FF(result_q , result_d , '0);
  `FF(addr_q , addr_d , '0);
  `FF(data_q, data_d, '0);
  
  assign req_d = obi_req_i.req;
  assign id_d = obi_req_i.a.aid;
  assign we_d = obi_req_i.a.we;
  assign addr_d = obi_req_i.a.addr;
  assign wdata = obi_req_i.a.wdata;

  // ---------------- internal signals ----------------
  typedef enum logic [5:0] {
    Add         = 0,
    Substract   = 4,
    Multiply    = 8,
    Divide      = 12,
    ReadResult  = 16,
    Invalid     = 24
  } mode_t;

  mode_t mode;

  assign mode = mode_t'(addr_d[5:0]); 
  
  // ---------------------------------------------------
  // build the 31-bit (31,26) word and collect Hamming parity
  // ---------------------------------------------------
  always_comb begin

    rsp_err = '0;
    result_d = result_q;
    data_d = data_q;

    // Fixed point 16-bit in Q8.8 Format
    wdata1 = wdata[31:16];
    wdata2 = wdata[15:0];

    if (req_d) begin
      if (we_d) begin
        case(mode) 
          Add: result_d = add(wdata1,wdata2);          
                    
          Substract: result_d = sub(wdata1,wdata2); 

          Multiply: result_d = mult(wdata1,wdata2);

          Divide: result_d = div(wdata1,wdata2);

          default: rsp_err = '1;
          
        endcase
      end else begin    
        case(mode) 
          ReadResult: data_d = result_q;

          default: rsp_err = '1;
        endcase
      end
    end
  end

  // Wire the response
  // A channel
  assign obi_rsp_o.gnt = obi_req_i.req;
  // R channel:
  assign obi_rsp_o.rvalid = req_q;
  assign obi_rsp_o.r.rid = id_q;
  assign obi_rsp_o.r.err = rsp_err;
  assign obi_rsp_o.r.r_optional = '0;

  assign obi_rsp_o.r.rdata = data_q;


  function automatic logic [31:0] add(logic signed [15:0] in1, logic signed [15:0] in2);
    logic signed [31:0] result = '0;

    result = (in1 + in2) <<< 8;
    return result;
    
  endfunction

  function automatic logic [31:0] sub(logic signed [15:0] in1, logic signed [15:0] in2);
    logic signed [31:0] result = '0;

    result = (in1 - in2) <<< 8;
    return result;
    
  endfunction

  function automatic logic [31:0] mult(logic signed [15:0] in1, logic signed [15:0] in2);
    logic signed [31:0] result = '0;

    result = in1 * in2;
    return result;
    
  endfunction

  function automatic logic [31:0] div(logic signed [15:0] in1, logic signed [15:0] in2);
    logic signed [31:0] numerator = '0;
    logic signed [31:0] result = '0;

    if (in2 == 0) result = 32'shCAFEF00D; // Division by zero
    else begin
      numerator = in1 <<< 8;
      result = numerator / in2;
    end

    result = in1 * in2;
    return result;
    
  endfunction
    
endmodule