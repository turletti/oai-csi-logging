#!/usr/bin/env python3
"""
CSI Logger Data Parser and Visualizer for OAI - v2.0.0
Multi-UE support with per-RNTI CSV output and visualization
"""

import csv
import sys
from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt
import argparse
from collections import defaultdict

class OAICSIRecord:
    def __init__(self, row):
        try:
            self.frame = int(row['frame'])
            self.slot = int(row['slot'])
            self.rnti = int(row['rnti'], 16) if isinstance(row['rnti'], str) and row['rnti'].startswith('0x') else int(row['rnti'])
            self.rb = int(row['rb'])
            self.subcarrier = int(row['subcarrier'])
            self.real = int(row['real'])
            self.imag = int(row['imag'])
            self.magnitude = np.sqrt(self.real**2 + self.imag**2)
            self.phase = np.arctan2(self.imag, self.real)
        except (KeyError, ValueError) as e:
            raise ValueError(f"Invalid CSV row: {e}")

class OAICSIParser:
    def __init__(self, filepath):
        self.filepath = Path(filepath)
        self.records = []
        self.records_by_rnti = defaultdict(list)
        self.rnti_list = []

    def parse(self):
        print(f"[Parser] Reading {self.filepath.name}")
        with open(self.filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row_num, row in enumerate(reader, start=2):
                try:
                    rec = OAICSIRecord(row)
                    self.records.append(rec)
                    self.records_by_rnti[rec.rnti].append(rec)
                except ValueError as e:
                    print(f"WARNING: Row {row_num}: {e}")
                    continue

        if not self.records:
            print("ERROR: No valid records")
            return False

        self.rnti_list = sorted(set(r.rnti for r in self.records))
        print(f"[Parser] {len(self.records)} records from {len(self.rnti_list)} UEs: {[f'0x{r:04x}' for r in self.rnti_list]}")
        return True

    def get_statistics(self):
        if not self.records:
            return
        mags = [r.magnitude for r in self.records]
        print(f"\n=== OAI CSI Statistics ===")
        print(f"Total records: {len(self.records)}")
        print(f"UEs: {len(self.rnti_list)}")
        print(f"Magnitude: min={min(mags):.1f} max={max(mags):.1f} mean={np.mean(mags):.1f}")

class OAICSIVisualizer:
    def __init__(self, parser):
        self.parser = parser

    def plot_global_rb_distribution(self):
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
            return []
        
        figs = []
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
        figs.append(('rb_magnitude', fig))
        return figs

def main():
    parser = argparse.ArgumentParser(description='OAI CSI Visualizer v2.0.0 - Multi-UE')
    parser.add_argument('csi_file', nargs='?', help='Path to CSI CSV file')
    parser.add_argument('--output', help='Output directory for PNG files')
    parser.add_argument('--stats', action='store_true', help='Show statistics only')
    args = parser.parse_args()

    if not args.csi_file:
        parser.print_help()
        sys.exit(1)

    csi_parser = OAICSIParser(args.csi_file)
    if not csi_parser.parse():
        sys.exit(1)

    csi_parser.get_statistics()
    if args.stats:
        return

    viz = OAICSIVisualizer(csi_parser)
    
    if args.output:
        out = Path(args.output)
        out.mkdir(parents=True, exist_ok=True)
        
        fig = viz.plot_global_rb_distribution()
        fig.savefig(out / 'global_rb_magnitude.png', dpi=100)
        plt.close(fig)
        print(f"Saved global_rb_magnitude.png")
        
        for rnti in csi_parser.rnti_list:
            figs = viz.plot_per_rnti(rnti)
            for name, fig in figs:
                fname = f'rnti_0x{rnti:04x}_{name}.png'
                fig.savefig(out / fname, dpi=100)
                plt.close(fig)
                print(f"Saved {fname}")
        
        print(f"Done - plots in {out}")

if __name__ == '__main__':
    main()
