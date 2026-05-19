#!/usr/bin/env python3
"""
CSI Visualizer v3 - Flexible MIMO
Parses CSV with JSON metadata header
Supports: per-RB, per-subcarrier, antenna/port selection, subcarrier sampling
"""

import csv
import json
import sys
from pathlib import Path
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt
import argparse

class CSIMetadata:
    """Parse JSON metadata from CSV header"""
    def __init__(self):
        self.granularity = "rb"
        self.nb_antenna_rx = 1
        self.nb_ports_tx = 1
        self.antenna_selection = [0]
        self.port_selection = [0]
        self.subcarrier_sampling = 1
    
    @staticmethod
    def from_csv_header(csv_file):
        """Read JSON metadata from first line of CSV"""
        meta = CSIMetadata()
        with open(csv_file, 'r') as f:
            first_line = f.readline().strip()
            if first_line.startswith('#'):
                json_str = first_line[2:]  # Remove "# "
                try:
                    data = json.loads(json_str)
                    meta.granularity = data.get('granularity', 'rb')
                    meta.nb_antenna_rx = data.get('nb_antenna_rx', 1)
                    meta.nb_ports_tx = data.get('nb_ports_tx', 1)
                    meta.antenna_selection = data.get('antenna_selection', [0])
                    meta.port_selection = data.get('port_selection', [0])
                    meta.subcarrier_sampling = data.get('subcarrier_sampling', 1)
                except json.JSONDecodeError:
                    print("WARNING: Could not parse JSON metadata")
        return meta

class CSIRecord:
    """Single CSI measurement"""
    def __init__(self, row, has_mimo, has_subcarrier):
        self.frame = int(row['frame'])
        self.slot = int(row['slot'])
        self.rnti = int(row['rnti'], 16) if isinstance(row['rnti'], str) and row['rnti'].startswith('0x') else int(row['rnti'])
        
        idx = 3
        if has_mimo:
            self.ant_rx = int(row['ant_rx'])
            self.port_tx = int(row['port_tx'])
            idx += 2
        else:
            self.ant_rx = 0
            self.port_tx = 0
        
        self.rb = int(row['rb'])
        idx += 1
        
        if has_subcarrier:
            self.subcarrier = int(row['subcarrier'])
            idx += 1
        else:
            self.subcarrier = 0
        
        self.real = int(row['real'])
        self.imag = int(row['imag'])
        self.magnitude = np.sqrt(self.real**2 + self.imag**2)

class CSIParser:
    """Parse CSI v3 CSV"""
    def __init__(self, filepath):
        self.filepath = Path(filepath)
        self.metadata = CSIMetadata.from_csv_header(filepath)
        self.records = []
        self.records_by_rnti = defaultdict(list)
        self.rnti_list = []
    
    def parse(self):
        print(f"[Parser] Reading {self.filepath.name}")
        print(f"[Parser] Metadata: granularity={self.metadata.granularity}, "
              f"MIMO={self.metadata.nb_antenna_rx}x{self.metadata.nb_ports_tx}")
        
        has_mimo = self.metadata.nb_antenna_rx > 1 or self.metadata.nb_ports_tx > 1
        has_subcarrier = self.metadata.granularity == "subcarrier"
        
        with open(self.filepath, 'r') as f:
            # Skip JSON header
            first_line = f.readline()
            if not first_line.startswith('#'):
                f.seek(0)
            
            reader = csv.DictReader(f)
            for row_num, row in enumerate(reader, start=2):
                try:
                    rec = CSIRecord(row, has_mimo, has_subcarrier)
                    self.records.append(rec)
                    self.records_by_rnti[rec.rnti].append(rec)
                except (KeyError, ValueError) as e:
                    print(f"WARNING: Row {row_num}: {e}")
        
        if not self.records:
            print("ERROR: No valid records")
            return False
        
        self.rnti_list = sorted(set(r.rnti for r in self.records))
        print(f"[Parser] {len(self.records)} records from {len(self.rnti_list)} UEs")
        return True
    
    def get_statistics(self):
        if not self.records:
            return
        
        mags = [r.magnitude for r in self.records]
        print(f"\n=== CSI v3 Statistics ===")
        print(f"Records: {len(self.records)}")
        print(f"UEs: {len(self.rnti_list)}")
        print(f"Magnitude: min={min(mags):.1f} max={max(mags):.1f} mean={np.mean(mags):.1f}")

class CSIVisualizer:
    """Visualize CSI v3 data"""
    def __init__(self, parser):
        self.parser = parser
    
    def plot_global_rb(self):
        rbs = sorted(set(r.rb for r in self.parser.records))
        mags_by_rb = defaultdict(list)
        for r in self.parser.records:
            mags_by_rb[r.rb].append(r.magnitude)
        rb_mags = [np.mean(mags_by_rb[rb]) for rb in rbs]
        
        fig, ax = plt.subplots(figsize=(12, 5))
        ax.bar(rbs, rb_mags, alpha=0.7, color='steelblue')
        ax.set_title('CSI Magnitude Distribution (All UEs)')
        ax.set_xlabel('RB')
        ax.set_ylabel('Magnitude')
        ax.grid(True, alpha=0.3, axis='y')
        return fig
    
    def plot_per_rnti(self, rnti):
        recs = self.parser.records_by_rnti[rnti]
        if not recs:
            return None
        
        rbs = sorted(set(r.rb for r in recs))
        mags_by_rb = defaultdict(list)
        for r in recs:
            mags_by_rb[r.rb].append(r.magnitude)
        rb_mags = [np.mean(mags_by_rb[rb]) for rb in rbs]
        
        fig, ax = plt.subplots(figsize=(12, 5))
        ax.bar(rbs, rb_mags, alpha=0.7, color='steelblue')
        ax.set_title(f'CSI Magnitude - RNTI 0x{rnti:04x}')
        ax.set_xlabel('RB')
        ax.set_ylabel('Magnitude')
        ax.grid(True, alpha=0.3, axis='y')
        return fig

def main():
    parser = argparse.ArgumentParser(description='CSI Visualizer v3 - Flexible MIMO')
    parser.add_argument('csi_file', nargs='?', help='Path to CSI CSV file')
    parser.add_argument('--output', help='Output directory for PNG files')
    parser.add_argument('--stats', action='store_true', help='Show statistics only')
    args = parser.parse_args()
    
    if not args.csi_file:
        parser.print_help()
        sys.exit(1)
    
    csi_parser = CSIParser(args.csi_file)
    if not csi_parser.parse():
        sys.exit(1)
    
    csi_parser.get_statistics()
    if args.stats:
        return
    
    viz = CSIVisualizer(csi_parser)
    
    if args.output:
        out = Path(args.output)
        out.mkdir(parents=True, exist_ok=True)
        
        fig = viz.plot_global_rb()
        fig.savefig(out / 'global_rb_magnitude.png', dpi=100)
        plt.close(fig)
        print(f"Saved global_rb_magnitude.png")
        
        for rnti in csi_parser.rnti_list:
            fig = viz.plot_per_rnti(rnti)
            if fig:
                fname = f'rnti_0x{rnti:04x}_rb_magnitude.png'
                fig.savefig(out / fname, dpi=100)
                plt.close(fig)
                print(f"Saved {fname}")
        
        print(f"Done - plots in {out}")

if __name__ == '__main__':
    main()
