#current_voltage

# Create processing_system7
cell xilinx.com:ip:processing_system7:5.5 ps_0 {
  PCW_IMPORT_BOARD_PRESET cfg/red_pitaya.xml
  PCW_USE_S_AXI_HP0 1
} {
  M_AXI_GP0_ACLK ps_0/FCLK_CLK0
  S_AXI_HP0_ACLK ps_0/FCLK_CLK0
}

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
  make_external {FIXED_IO, DDR}
  Master Disable
  Slave Disable
} [get_bd_cells ps_0]

# Create util_ds_buf
cell xilinx.com:ip:util_ds_buf:2.1 buf_0 {
  C_SIZE 2
  C_BUF_TYPE IBUFDS
} {
  IBUF_DS_P daisy_p_i
  IBUF_DS_N daisy_n_i
}

# Create util_ds_buf
cell xilinx.com:ip:util_ds_buf:2.1 buf_1 {
  C_SIZE 2
  C_BUF_TYPE OBUFDS
} {
  OBUF_DS_P daisy_p_o
  OBUF_DS_N daisy_n_o
}

# Create proc_sys_reset
cell xilinx.com:ip:proc_sys_reset:5.0 rst_0


# Create axis_red_pitaya_adc
cell pavel-demin:user:axis_red_pitaya_adc:1.0 adc_0 {} {
  adc_clk_p adc_clk_p_i
  adc_clk_n adc_clk_n_i
  adc_dat_a adc_dat_a_i
  adc_dat_b adc_dat_b_i
  adc_csn adc_csn_o
}

# Create c_counter_binary
cell xilinx.com:ip:c_counter_binary:12.0 cntr_0 {
  Output_Width 32
} {
  CLK adc_0/adc_clk
}

# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_0 {
  DIN_WIDTH 32 DIN_FROM 25 DIN_TO 25 DOUT_WIDTH 1
} {
  Din cntr_0/Q
 }


# Create axi_cfg_register
cell pavel-demin:user:axi_cfg_register:1.0 cfg_0 {
  CFG_DATA_WIDTH 320
  AXI_ADDR_WIDTH 32
  AXI_DATA_WIDTH 32
}

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {
  Master /ps_0/M_AXI_GP0
  Clk Auto
} [get_bd_intf_pins cfg_0/S_AXI]

set_property RANGE 4K [get_bd_addr_segs ps_0/Data/SEG_cfg_0_reg0]
set_property OFFSET 0x40000000 [get_bd_addr_segs ps_0/Data/SEG_cfg_0_reg0]

# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_1 {
  DIN_WIDTH 320 DIN_FROM 134 DIN_TO 128 DOUT_WIDTH 7
} {
  Din cfg_0/cfg_data
}

# Create xlconcat
cell xilinx.com:ip:xlconcat:2.1 concat_0 {
  IN1_WIDTH 7
} {
  In0 slice_0/Dout
  In1 slice_1/Dout
  dout led_o
}


# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_pktzr_reset {
  DIN_WIDTH 320 DIN_FROM 0 DIN_TO 0
} {
  Din cfg_0/cfg_data
}

# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_write_enable {
  DIN_WIDTH 320 DIN_FROM 1 DIN_TO 1
} {
  Din cfg_0/cfg_data
}


# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_record_length {
  DIN_WIDTH 320 DIN_FROM 63 DIN_TO 32 DOUT_WIDTH 32
} {
  Din cfg_0/cfg_data
}

# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_trig_record {
  DIN_WIDTH 320 DIN_FROM 3 DIN_TO 3
} {
  Din cfg_0/cfg_data
}

# Create xlconstant
cell xilinx.com:ip:xlconstant:1.1 const_1

# Create axis_clock_converter
cell xilinx.com:ip:axis_clock_converter:1.1 fifo_ADC { 
} {
  S_AXIS adc_0/M_AXIS
  s_axis_aclk adc_0/adc_clk
  s_axis_aresetn const_1/dout
  m_axis_aclk ps_0/FCLK_CLK0
  m_axis_aresetn rst_0/peripheral_aresetn
}



# Create gpio_trigger
cell pavel-demin:user:gpio_trigger:1.0 trigger_0 {
	GPIO_DATA_WIDTH 8
} {
  gpio_data exp_p_tri_io
  soft_trig slice_trig_record/Dout
  aclk ps_0/FCLK_CLK0
  aresetn slice_pktzr_reset/Dout
}

# Create xlconstant
cell xilinx.com:ip:xlconstant:1.1 const_pktzr_length {
  CONST_WIDTH 32
  CONST_VAL 2048
}

# Create axis_broadcaster
cell xilinx.com:ip:axis_broadcaster:1.1 bcast_ADC {
  S_TDATA_NUM_BYTES 4
  M_TDATA_NUM_BYTES 4
 } {
  S_AXIS fifo_ADC/M_AXIS
  aclk ps_0/FCLK_CLK0
  aresetn rst_0/peripheral_aresetn
}


# Create blk_mem_gen
cell xilinx.com:ip:blk_mem_gen:8.3 waveform_bram {
    MEMORY_TYPE True_Dual_Port_RAM
    USE_BRAM_BLOCK Stand_Alone
    WRITE_WIDTH_A 16
    READ_WIDTH_A 16
    WRITE_DEPTH_A 131072
    WRITE_WIDTH_B 32
    READ_WIDTH_B 32
    WRITE_DEPTH_B 65536
    ENABLE_A Always_Enabled
    ENABLE_B Always_Enabled
    REGISTER_PORTB_OUTPUT_OF_MEMORY_PRIMITIVES false
  }


# Create axi_bram_writer
cell pavel-demin:user:axi_bram_writer:1.0 waveform_writer {
  AXI_DATA_WIDTH 32
  AXI_ADDR_WIDTH 32
  BRAM_DATA_WIDTH 32
  BRAM_ADDR_WIDTH 17
} {
  BRAM_PORTA waveform_bram/BRAM_PORTB
}

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {
  Master /ps_0/M_AXI_GP0
  Clk Auto
} [get_bd_intf_pins waveform_writer/S_AXI]

set_property RANGE 256K [get_bd_addr_segs ps_0/Data/SEG_waveform_writer_reg0]
set_property OFFSET 0x40040000 [get_bd_addr_segs ps_0/Data/SEG_waveform_writer_reg0]

# Create xlslice
cell xilinx.com:ip:xlslice:1.0 slice_measure_pulse {
  DIN_WIDTH 320 DIN_FROM 319 DIN_TO 160
} {
  Din cfg_0/cfg_data
}

# Create axis_measure_pulse
cell pavel-demin:user:axis_measure_pulse:1.0 measure_pulse {
    AXIS_TDATA_WIDTH 16
    BRAM_DATA_WIDTH 16
    BRAM_ADDR_WIDTH 9
} {
  cfg_data slice_measure_pulse/Dout
  s_axis bcast_ADC/m01_axis
  BRAM_PORTA waveform_bram/BRAM_PORTA
  aclk ps_0/FCLK_CLK0
  aresetn slice_pktzr_reset/Dout
}


# Create axis_circular_packetizer
cell pavel-demin:user:axis_circular_packetizer:1.0 pktzr_0 {
  AXIS_TDATA_WIDTH 32
  CNTR_WIDTH 26
  CONTINUOUS FALSE
} {
  S_AXIS bcast_ADC/m00_axis
  cfg_data slice_record_length/Dout
  trigger trigger_0/trigger
  aclk ps_0/FCLK_CLK0
  aresetn slice_pktzr_reset/Dout
}



# Create axis_dwidth_converter
cell xilinx.com:ip:axis_dwidth_converter:1.1 conv_0 {
  S_TDATA_NUM_BYTES.VALUE_SRC USER
  S_TDATA_NUM_BYTES 4
  M_TDATA_NUM_BYTES 8
} {
  S_AXIS pktzr_0/M_AXIS
  aclk ps_0/FCLK_CLK0
  aresetn slice_write_enable/Dout
}

# Create xlconstant
cell xilinx.com:ip:xlconstant:1.1 const_2 {
  CONST_WIDTH 32
  CONST_VAL 268435456
}

# Create axis_ram_writer
cell pavel-demin:user:axis_ram_writer:1.0 writer_0 {
  ADDR_WIDTH 25
} {
  S_AXIS conv_0/M_AXIS
  M_AXI ps_0/S_AXI_HP0
  cfg_data const_2/dout
  aclk ps_0/FCLK_CLK0
  aresetn slice_write_enable/Dout
}

assign_bd_address [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM]


# Create clk_wiz
cell xilinx.com:ip:clk_wiz:5.2 pll_0 {
  PRIMITIVE PLL
  PRIM_IN_FREQ.VALUE_SRC USER
  PRIM_IN_FREQ 125.0
  CLKOUT1_USED true
  CLKOUT1_REQUESTED_OUT_FREQ 250.0
} {
  clk_in1 adc_0/adc_clk
}



# Create dac_clock_converter
cell xilinx.com:ip:axis_clock_converter:1.1 fifo_DAC {
  TDATA_NUM_BYTES.VALUE_SRC USER
  TDATA_NUM_BYTES 4
} {
  S_AXIS measure_pulse/M_AXIS
  s_axis_aclk ps_0/FCLK_CLK0
  s_axis_aresetn rst_0/peripheral_aresetn
  m_axis_aclk adc_0/adc_clk
  m_axis_aresetn const_1/dout
}

# Create axis_zeroer
cell pavel-demin:user:axis_zeroer:1.0 zeroer_DAC {
  AXIS_TDATA_WIDTH 32
} {
  S_AXIS fifo_DAC/M_AXIS
  aclk adc_0/adc_clk
}

# Create axis_red_pitaya_dac
cell pavel-demin:user:axis_red_pitaya_dac:1.0 dac_0 {} {
  aclk adc_0/adc_clk
  ddr_clk pll_0/clk_out1
  locked pll_0/locked
  S_AXIS zeroer_DAC/M_AXIS
  dac_clk dac_clk_o
  dac_rst dac_rst_o
  dac_sel dac_sel_o
  dac_wrt dac_wrt_o
  dac_dat dac_dat_o
}


# Create xlconstant
cell xilinx.com:ip:xlconstant:1.1 const_sts {
  CONST_WIDTH 31
  CONST_VAL 0
}

# Create xlconcat
cell xilinx.com:ip:xlconcat:2.1 concat_sts {
  NUM_PORTS 4
  IN0_WIDTH 32
  IN1_WIDTH 32
  IN2_WIDTH 1
  IN3_WIDTH 31
} {
  In0 writer_0/sts_data
  In1 pktzr_0/trigger_pos
  In2 trigger_0/trigger
  In3 const_sts/dout
}

# Create axi_sts_register
cell pavel-demin:user:axi_sts_register:1.0 sts_0 {
  STS_DATA_WIDTH 128
  AXI_ADDR_WIDTH 32
  AXI_DATA_WIDTH 32
} {
  sts_data concat_sts/dout
}

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config {
  Master /ps_0/M_AXI_GP0
  Clk Auto
} [get_bd_intf_pins sts_0/S_AXI]

set_property RANGE 4K [get_bd_addr_segs ps_0/Data/SEG_sts_0_reg0]
set_property OFFSET 0x40001000 [get_bd_addr_segs ps_0/Data/SEG_sts_0_reg0]
