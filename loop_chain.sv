module test #(localparam N=5) (
  input logic i_value,
  output logic o_value);

  logic [N-1:0] stages;

  assign stages[0] = i_value;
  assign o_value = stages[1];

  always_comb begin
    for (int i=1; i<N-1; i++) begin
      stages[i] = stages[i-1];
    end
  end

endmodule
