"""Check the public FPGA timer divider, reset and read mux in isolation."""
# Adapted from paps-g fb67ac50 and 86e2af4e.
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('iverilog') and shutil.which('vvp'), 'requires Icarus Verilog')
class FPGAClockTests(unittest.TestCase):
    def test_public_fpga05_timer(self):
        raw = (ROOT / 'FPGA/SSMaster.v').read_bytes()
        start = raw.index(b'\treg[6:0] ss_timer_feed;')
        counter = raw[start:raw.index(b'\n///////////////////////////////////////////////////////', start)]
        start = raw.index(b'\twire ss_reg_cs =')
        mux = raw[start:raw.index(b'\n\talways @(negedge NRESET', start)]
        module = b'''module clock_dut(input mclk, NRESET, ss_wr_start, SS_CS2,
input [23:0] SS_ADDR, output [15:0] value);
reg [31:0] ss_reg_timer;
reg [15:0] ss_bcr_data_out;
wire [15:0] ss_reg_ctrl=0, ss_reg_stat=0, ss_reg_cmd=0, ss_reg_data=0, ss_fifo_data_out=0;
wire [23:0] ss_write_addr = SS_ADDR;
wire [2:0] ss_write_cs = {SS_CS2, 2'b11};
''' + mux + counter + b'\nassign value = ss_bcr_data_out;\nendmodule\n'
        with tempfile.TemporaryDirectory(prefix='saroo-fpga-clock-') as temp:
            target = Path(temp)
            dut, sim = target / 'clock.v', target / 'clock.vvp'
            dut.write_bytes(module)
            result = subprocess.run(
                ['iverilog', '-g2012', '-s', 'tb', '-o', str(sim), str(dut),
                 str(ROOT / 'tests/fpga_clock_tb.v')],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run(['vvp', str(sim)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('pass: 128 startup phases', result.stdout)


if __name__ == '__main__':
    unittest.main()
