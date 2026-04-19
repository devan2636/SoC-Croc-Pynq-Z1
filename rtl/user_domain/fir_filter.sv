// Devandri Suherman
// Bandung Institute of Technology
// FIR filter peripheral for the user domain.

module fir_filter #(
  parameter obi_pkg::obi_cfg_t ObiCfg = obi_pkg::ObiDefaultConfig,
  parameter type obi_req_t = logic,
  parameter type obi_rsp_t = logic,
  parameter int unsigned TapCount = 16,
  parameter int unsigned SampleCount = 160
) (
  input  logic   clk_i,
  input  logic   rst_ni,
  input  obi_req_t obi_req_i,
  output obi_rsp_t obi_rsp_o
);

  localparam int unsigned DataWidth = ObiCfg.DataWidth;
  localparam int unsigned TapIdxW = (TapCount <= 1) ? 1 : $clog2(TapCount);
  localparam int unsigned SampleIdxW = (SampleCount <= 1) ? 1 : $clog2(SampleCount);

  typedef enum logic [1:0] {
    Idle,
    Run
  } state_e;

  typedef enum logic [5:0] {
    CtrlReg       = 6'd0,
    StatusReg     = 6'd4,
    CoeffIdxReg   = 6'd8,
    CoeffDataReg  = 6'd12,
    InputIdxReg   = 6'd16,
    InputDataReg  = 6'd20,
    OutputIdxReg  = 6'd24,
    OutputDataReg = 6'd28,
    LengthReg     = 6'd32
  } mode_t;

  logic req_q, we_q;
  logic [ObiCfg.AddrWidth-1:0] addr_q;
  logic [ObiCfg.IdWidth-1:0] id_q;
  logic [ObiCfg.DataWidth-1:0] wdata_q;
  logic [ObiCfg.DataWidth-1:0] rdata_q;
  logic rsp_err_q;

  logic [TapIdxW-1:0] coeff_idx_q;
  logic [SampleIdxW-1:0] input_idx_q;
  logic [SampleIdxW-1:0] output_idx_q;
  logic [SampleIdxW-1:0] sample_count_q;
  logic [SampleIdxW-1:0] sample_idx_q;
  state_e state_q;
  logic busy_q;
  logic done_q;

  logic signed [DataWidth-1:0] coeff_mem [TapCount];
  logic signed [DataWidth-1:0] input_mem [SampleCount];
  logic signed [DataWidth-1:0] output_mem [SampleCount];

  mode_t mode;
  logic write_clear;
  logic write_start;
  logic write_coeff_idx;
  logic write_coeff_data;
  logic write_input_idx;
  logic write_input_data;
  logic write_output_idx;
  logic write_length;
  logic [TapIdxW-1:0] coeff_idx_d;
  logic [SampleIdxW-1:0] input_idx_d;
  logic [SampleIdxW-1:0] output_idx_d;
  logic [SampleIdxW-1:0] sample_count_d;
  logic [DataWidth-1:0] read_data_d;
  logic rsp_err_d;

  assign mode = mode_t'(obi_req_i.a.addr[5:0]);

  always_comb begin
    coeff_idx_d = coeff_idx_q;
    input_idx_d = input_idx_q;
    output_idx_d = output_idx_q;
    sample_count_d = sample_count_q;
    read_data_d = rdata_q;
    rsp_err_d = 1'b0;
    write_clear = 1'b0;
    write_start = 1'b0;
    write_coeff_idx = 1'b0;
    write_coeff_data = 1'b0;
    write_input_idx = 1'b0;
    write_input_data = 1'b0;
    write_output_idx = 1'b0;
    write_length = 1'b0;

    if (obi_req_i.req) begin
      if (obi_req_i.a.we) begin
        unique case (mode)
          CtrlReg: begin
            write_clear = obi_req_i.a.wdata[1];
            write_start = obi_req_i.a.wdata[0];
          end

          CoeffIdxReg: begin
            write_coeff_idx = 1'b1;
            if (obi_req_i.a.wdata[TapIdxW-1:0] < TapCount[TapIdxW-1:0]) begin
              coeff_idx_d = obi_req_i.a.wdata[TapIdxW-1:0];
            end else begin
              coeff_idx_d = TapCount[TapIdxW-1:0] - 1'b1;
            end
          end

          CoeffDataReg: begin
            write_coeff_data = 1'b1;
          end

          InputIdxReg: begin
            write_input_idx = 1'b1;
            if (obi_req_i.a.wdata[SampleIdxW-1:0] < SampleCount[SampleIdxW-1:0]) begin
              input_idx_d = obi_req_i.a.wdata[SampleIdxW-1:0];
            end else begin
              input_idx_d = SampleCount[SampleIdxW-1:0] - 1'b1;
            end
          end

          InputDataReg: begin
            write_input_data = 1'b1;
          end

          OutputIdxReg: begin
            write_output_idx = 1'b1;
            if (obi_req_i.a.wdata[SampleIdxW-1:0] < SampleCount[SampleIdxW-1:0]) begin
              output_idx_d = obi_req_i.a.wdata[SampleIdxW-1:0];
            end else begin
              output_idx_d = SampleCount[SampleIdxW-1:0] - 1'b1;
            end
          end

          LengthReg: begin
            write_length = 1'b1;
            if (obi_req_i.a.wdata[SampleIdxW-1:0] == '0) begin
              sample_count_d = '0;
            end else if (obi_req_i.a.wdata[SampleIdxW-1:0] > SampleCount[SampleIdxW-1:0]) begin
              sample_count_d = SampleCount[SampleIdxW-1:0];
            end else begin
              sample_count_d = obi_req_i.a.wdata[SampleIdxW-1:0];
            end
          end

          default: rsp_err_d = 1'b1;
        endcase
      end else begin
        unique case (mode)
          StatusReg: begin
            read_data_d = {30'd0, done_q, busy_q};
          end

          CoeffIdxReg: begin
            read_data_d = {{(DataWidth-TapIdxW){1'b0}}, coeff_idx_q};
          end

          CoeffDataReg: begin
            read_data_d = coeff_mem[coeff_idx_q];
          end

          InputIdxReg: begin
            read_data_d = {{(DataWidth-SampleIdxW){1'b0}}, input_idx_q};
          end

          InputDataReg: begin
            read_data_d = input_mem[input_idx_q];
          end

          OutputIdxReg: begin
            read_data_d = {{(DataWidth-SampleIdxW){1'b0}}, output_idx_q};
          end

          OutputDataReg: begin
            read_data_d = output_mem[output_idx_q];
          end

          LengthReg: begin
            read_data_d = {{(DataWidth-SampleIdxW){1'b0}}, sample_count_q};
          end

          default: rsp_err_d = 1'b1;
        endcase
      end
    end
  end

  function automatic logic signed [DataWidth-1:0] fir_compute_output(
    input int unsigned sample_idx
  );
    longint signed acc;
    longint signed scaled;

    function automatic logic signed [31:0] clamp_q15_to_i32(input longint signed value);
      if (value > 32'sd32767) begin
        return 32'sd32767;
      end else if (value < -32'sd32768) begin
        return -32'sd32768;
      end else begin
        return logic signed [31:0]'(value);
      end
    endfunction

    acc = 0;
    for (int tap = 0; tap < TapCount; tap++) begin
      if (sample_idx >= tap) begin
        acc += longint'($signed(coeff_mem[tap])) * longint'($signed(input_mem[sample_idx - tap]));
      end
    end

    scaled = acc >>> 15;
    return logic signed [DataWidth-1:0]'(clamp_q15_to_i32(scaled));
  endfunction

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      req_q <= 1'b0;
      we_q <= 1'b0;
      addr_q <= '0;
      id_q <= '0;
      wdata_q <= '0;
      rdata_q <= '0;
      rsp_err_q <= 1'b0;

      coeff_idx_q <= '0;
      input_idx_q <= '0;
      output_idx_q <= '0;
      sample_count_q <= SampleCount[SampleIdxW-1:0];
      sample_idx_q <= '0;
      state_q <= Idle;
      busy_q <= 1'b0;
      done_q <= 1'b0;

      for (int i = 0; i < TapCount; i++) begin
        coeff_mem[i] <= '0;
      end

      for (int i = 0; i < SampleCount; i++) begin
        input_mem[i] <= '0;
        output_mem[i] <= '0;
      end
    end else begin
      req_q <= obi_req_i.req;
      we_q <= obi_req_i.a.we;
      addr_q <= obi_req_i.a.addr;
      id_q <= obi_req_i.a.aid;
      wdata_q <= obi_req_i.a.wdata;
      rdata_q <= read_data_d;
      rsp_err_q <= rsp_err_d;

      if (write_clear) begin
        state_q <= Idle;
        busy_q <= 1'b0;
        done_q <= 1'b0;
        sample_idx_q <= '0;
        for (int i = 0; i < SampleCount; i++) begin
          input_mem[i] <= '0;
          output_mem[i] <= '0;
        end
      end else begin
        if (write_coeff_idx) begin
          coeff_idx_q <= coeff_idx_d;
        end

        if (write_coeff_data) begin
          coeff_mem[coeff_idx_q] <= $signed(obi_req_i.a.wdata);
        end

        if (write_input_idx) begin
          input_idx_q <= input_idx_d;
        end

        if (write_input_data) begin
          input_mem[input_idx_q] <= $signed(obi_req_i.a.wdata);
        end

        if (write_output_idx) begin
          output_idx_q <= output_idx_d;
        end

        if (write_length) begin
          sample_count_q <= sample_count_d;
        end

        if (write_start && (sample_count_q != '0) && (state_q == Idle)) begin
          state_q <= Run;
          busy_q <= 1'b1;
          done_q <= 1'b0;
          sample_idx_q <= '0;
        end

        if (state_q == Run) begin
          output_mem[sample_idx_q] <= fir_compute_output(sample_idx_q);

          if ((sample_idx_q + 1'b1) >= sample_count_q) begin
            state_q <= Idle;
            busy_q <= 1'b0;
            done_q <= 1'b1;
          end else begin
            sample_idx_q <= sample_idx_q + 1'b1;
          end
        end
      end
    end
  end

  assign obi_rsp_o.gnt = obi_req_i.req;
  assign obi_rsp_o.rvalid = req_q;
  assign obi_rsp_o.r.rid = id_q;
  assign obi_rsp_o.r.err = rsp_err_q;
  assign obi_rsp_o.r.r_optional = '0;
  assign obi_rsp_o.r.rdata = rdata_q;

endmodule