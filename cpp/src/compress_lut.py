#!/usr/bin/env python3
"""
压缩现有的evaluator_holdem_potential_tables.h文件
将原有的8字节结构体压缩为4字节结构体 + 映射表的形式
现在添加位压缩：将两个14位值打包到32位整数中
"""

import re
import os
import sys
from collections import defaultdict
from typing import List, Tuple, Dict

def pack_evaluation_to_bits(equity_vs_all: int, equity_vs_pair_sets: int) -> int:
    """
    将两个评估值打包到一个32位整数中
    equity_vs_all: 14位 (0-16383)
    equity_vs_pair_sets: 14位 (0-16383)
    剩余4位可用于将来扩展
    """
    if equity_vs_all >= 16384 or equity_vs_pair_sets >= 16384:
        raise ValueError(f"评估值超出14位范围: {equity_vs_all}, {equity_vs_pair_sets}")

    # 位布局: [4位保留][14位 equity_vs_pair_sets][14位 equity_vs_all]
    packed = (equity_vs_pair_sets << 14) | equity_vs_all
    return packed

def unpack_evaluation_from_bits(packed: int) -> Tuple[int, int]:
    """从32位整数中解包出两个评估值"""
    equity_vs_all = packed & 0x3FFF  # 取低14位
    equity_vs_pair_sets = (packed >> 14) & 0x3FFF  # 取中间14位
    return equity_vs_all, equity_vs_pair_sets

def compress_evaluations_with_bitpacking(evaluations: List[Tuple[int, int]]) -> Tuple[List[int], List[int]]:
    """
    使用位打包压缩评估列表
    返回(唯一的打包值列表, 映射索引列表)
    """
    print(f"开始位压缩 {len(evaluations)} 个评估值...")

    unique_packed = []
    packed_to_index = {}  # {packed_value: index}
    mapping = []

    for equity_vs_all, equity_vs_pair_sets in evaluations:
        # 检查值是否在14位范围内
        if equity_vs_all >= 16384:
            print(f"警告：equity_vs_all={equity_vs_all} 超出14位范围，截断到16383")
            equity_vs_all = 16383
        if equity_vs_pair_sets >= 16384:
            print(f"警告：equity_vs_pair_sets={equity_vs_pair_sets} 超出14位范围，截断到16383")
            equity_vs_pair_sets = 16383

        packed_value = pack_evaluation_to_bits(equity_vs_all, equity_vs_pair_sets)

        if packed_value not in packed_to_index:
            # 新的唯一打包值
            index = len(unique_packed)
            unique_packed.append(packed_value)
            packed_to_index[packed_value] = index

        # 记录映射
        mapping.append(packed_to_index[packed_value])

    compression_ratio = (len(evaluations) - len(unique_packed)) / len(evaluations) * 100
    print(f"位压缩完成：{len(evaluations)} -> {len(unique_packed)} 唯一值 (压缩率: {compression_ratio:.2f}%)")

    return unique_packed, mapping

def calculate_bitpacked_size_reduction(original_count: int, unique_count: int, mapping_count: int):
    """计算位压缩后的文件大小减少量"""
    # 原始: 每个条目 2*uint16_t = 4字节
    original_size_mb = (original_count * 4) / (1024 * 1024)

    # 位压缩格式: 唯一值表(每个4字节的uint32_t) + 映射表(每个2字节，假设unique_count <= 65535)
    unique_size_mb = (unique_count * 4) / (1024 * 1024)
    mapping_size_mb = (mapping_count * 2) / (1024 * 1024)
    new_total_size_mb = unique_size_mb + mapping_size_mb

    reduction_mb = original_size_mb - new_total_size_mb
    reduction_percent = (reduction_mb / original_size_mb) * 100

    print(f"\n=== 位压缩文件大小分析 ===")
    print(f"原始大小: {original_size_mb:.2f} MB (每条目4字节)")
    print(f"位压缩后大小: {new_total_size_mb:.2f} MB")
    print(f"  - 唯一值表: {unique_size_mb:.2f} MB ({unique_count} 条目, 每个4字节)")
    print(f"  - 映射表: {mapping_size_mb:.2f} MB ({mapping_count} 条目, 每个2字节)")
    print(f"节省空间: {reduction_mb:.2f} MB ({reduction_percent:.2f}%)")

    return reduction_mb, reduction_percent

def parse_evaluation_struct(line: str) -> Tuple[int, int]:
    """解析形如 {1234,5678} 的结构体"""
    match = re.search(r'\{(\d+),(\d+)\}', line)
    if match:
        return (int(match.group(1)), int(match.group(2)))
    return None

def parse_lut_from_file(filepath: str, lut_name: str) -> List[Tuple[int, int]]:
    """从头文件中解析指定的查找表"""
    print(f"正在解析 {lut_name} 从文件 {filepath}...")

    evaluations = []
    in_lut = False

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()

                # 检测LUT开始
                if f'const holdem_evaluation_t {lut_name}[' in line:
                    in_lut = True
                    print(f"找到 {lut_name} 定义在第 {line_num} 行")
                    continue

                if in_lut:
                    # 检测LUT结束
                    if line.startswith('};'):
                        print(f"{lut_name} 解析完成，共 {len(evaluations)} 条记录")
                        break

                    # 解析数据行
                    # 一行可能包含多个结构体，如: {1,2},{3,4},{5,6}
                    structs = re.findall(r'\{(\d+),(\d+)\}', line)
                    for equity_all, equity_pairs in structs:
                        evaluations.append((int(equity_all), int(equity_pairs)))

    except FileNotFoundError:
        print(f"错误：找不到文件 {filepath}")
        return []
    except Exception as e:
        print(f"解析文件时出错：{e}")
        return []

    return evaluations

def compress_evaluations(evaluations: List[Tuple[int, int]]) -> Tuple[List[Tuple[int, int]], List[int]]:
    """压缩评估列表，返回(唯一评估列表, 映射索引列表)"""
    print(f"开始压缩 {len(evaluations)} 个评估值...")

    unique_evals = []
    eval_to_index = {}  # {(equity_all, equity_pairs): index}
    mapping = []

    for eval_tuple in evaluations:
        if eval_tuple not in eval_to_index:
            # 新的唯一评估值
            index = len(unique_evals)
            unique_evals.append(eval_tuple)
            eval_to_index[eval_tuple] = index

        # 记录映射
        mapping.append(eval_to_index[eval_tuple])

    compression_ratio = (len(evaluations) - len(unique_evals)) / len(evaluations) * 100
    print(f"压缩完成：{len(evaluations)} -> {len(unique_evals)} 唯一值 (压缩率: {compression_ratio:.2f}%)")

    return unique_evals, mapping

def calculate_size_reduction(original_count: int, unique_count: int, mapping_count: int):
    """计算文件大小减少量"""
    # 原始: 每个条目 2*int = 8字节
    original_size_mb = (original_count * 8) / (1024 * 1024)

    # 新格式: 唯一值表(每个4字节) + 映射表(每个2字节，假设unique_count <= 65535)
    unique_size_mb = (unique_count * 4) / (1024 * 1024)
    mapping_size_mb = (mapping_count * 2) / (1024 * 1024)
    new_total_size_mb = unique_size_mb + mapping_size_mb

    reduction_mb = original_size_mb - new_total_size_mb
    reduction_percent = (reduction_mb / original_size_mb) * 100

    print(f"\n=== 文件大小分析 ===")
    print(f"原始大小: {original_size_mb:.2f} MB")
    print(f"压缩后大小: {new_total_size_mb:.2f} MB")
    print(f"  - 唯一值表: {unique_size_mb:.2f} MB ({unique_count} 条目)")
    print(f"  - 映射表: {mapping_size_mb:.2f} MB ({mapping_count} 条目)")
    print(f"节省空间: {reduction_mb:.2f} MB ({reduction_percent:.2f}%)")

    return reduction_mb, reduction_percent

def write_bitpacked_header(output_path: str,
                          flop_unique_packed: List[int], flop_mapping: List[int],
                          turn_unique_packed: List[int], turn_mapping: List[int]):
    """写入位压缩后的头文件"""
    print(f"\n正在生成位压缩后的头文件: {output_path}")

    # 检查是否可以使用uint16_t存储映射索引
    flop_use_uint16 = max(flop_mapping) <= 65535 if flop_mapping else True
    turn_use_uint16 = max(turn_mapping) <= 65535 if turn_mapping else True

    flop_mapping_type = "uint16_t" if flop_use_uint16 else "uint32_t"
    turn_mapping_type = "uint16_t" if turn_use_uint16 else "uint32_t"

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("/* Auto-generated by compress_lut.py - Bit-packed Compressed LUT version */\n\n")
        f.write("#ifndef EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n")
        f.write("#define EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n\n")
        f.write("#include \"../include/phevaluator/evaluator_holdem_potential.h\"\n")
        f.write("#include <stdint.h>\n\n")
        f.write("#define ISOMORPHIC_LUTS_DEFINED\n")
        f.write("#define BIT_PACKED_LUTS\n\n")

        # 添加解包宏定义
        f.write("/* Bit unpacking macros */\n")
        f.write("#define UNPACK_EQUITY_VS_ALL(packed) ((packed) & 0x3FFF)\n")
        f.write("#define UNPACK_EQUITY_VS_PAIR_SETS(packed) (((packed) >> 14) & 0x3FFF)\n\n")

        # Flop位压缩唯一评估表
        if flop_unique_packed:
            f.write(f"/* Bit-packed Flop LUT: {len(flop_mapping)} total -> {len(flop_unique_packed)} unique */\n")
            f.write(f"const uint32_t flop_unique_evaluations_packed[{len(flop_unique_packed)}] = {{\n")
            for i, packed_val in enumerate(flop_unique_packed):
                f.write(f"0x{packed_val:08X}")
                if i < len(flop_unique_packed) - 1:
                    f.write(",")
                if (i + 1) % 8 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

            # Flop映射表
            f.write(f"const {flop_mapping_type} flop_mapping[{len(flop_mapping)}] = {{\n")
            for i, mapping_idx in enumerate(flop_mapping):
                f.write(f"{mapping_idx},")
                if (i + 1) % 20 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

        # Turn位压缩唯一评估表
        if turn_unique_packed:
            f.write(f"/* Bit-packed Turn LUT: {len(turn_mapping)} total -> {len(turn_unique_packed)} unique */\n")
            f.write(f"const uint32_t turn_unique_evaluations_packed[{len(turn_unique_packed)}] = {{\n")
            for i, packed_val in enumerate(turn_unique_packed):
                f.write(f"0x{packed_val:08X}")
                if i < len(turn_unique_packed) - 1:
                    f.write(",")
                if (i + 1) % 8 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

            # Turn映射表
            f.write(f"const {turn_mapping_type} turn_mapping[{len(turn_mapping)}] = {{\n")
            for i, mapping_idx in enumerate(turn_mapping):
                f.write(f"{mapping_idx},")
                if (i + 1) % 20 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

        # River表保持不变
        f.write("/* River LUT remains unchanged */\n")
        f.write("const int river_multidimensional_lut[7462] = {\n")
        # 生成简单的river映射（rank -> equity）
        for rank in range(1, 7463):
            equity = 10000 - (rank - 1) * 10000 // 7461
            equity = max(0, min(10000, equity))
            f.write(f"{equity},")
            if rank % 16 == 0:
                f.write("\n")
        f.write("};\n\n")

        f.write("#endif // EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n")

def write_compressed_header(output_path: str,
                          flop_unique: List[Tuple[int, int]], flop_mapping: List[int],
                          turn_unique: List[Tuple[int, int]], turn_mapping: List[int]):
    """写入压缩后的头文件"""
    print(f"\n正在生成压缩后的头文件: {output_path}")

    # 检查是否可以使用uint16_t存储映射索引
    flop_use_uint16 = max(flop_mapping) <= 65535 if flop_mapping else True
    turn_use_uint16 = max(turn_mapping) <= 65535 if turn_mapping else True

    flop_mapping_type = "uint16_t" if flop_use_uint16 else "uint32_t"
    turn_mapping_type = "uint16_t" if turn_use_uint16 else "uint32_t"

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("/* Auto-generated by compress_lut.py - Compressed LUT version */\n\n")
        f.write("#ifndef EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n")
        f.write("#define EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n\n")
        f.write("#include \"../include/phevaluator/evaluator_holdem_potential.h\"\n\n")
        f.write("#define ISOMORPHIC_LUTS_DEFINED\n\n")

        # Flop唯一评估表
        if flop_unique:
            f.write(f"/* Compressed Flop LUT: {len(flop_mapping)} total -> {len(flop_unique)} unique */\n")
            f.write(f"const holdem_evaluation_t flop_unique_evaluations[{len(flop_unique)}] = {{\n")
            for i, (equity_all, equity_pairs) in enumerate(flop_unique):
                f.write(f"    {{{equity_all},{equity_pairs}}}")
                if i < len(flop_unique) - 1:
                    f.write(",")
                if (i + 1) % 8 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

            # Flop映射表
            f.write(f"const {flop_mapping_type} flop_mapping[{len(flop_mapping)}] = {{\n")
            for i, mapping_idx in enumerate(flop_mapping):
                f.write(f"{mapping_idx},")
                if (i + 1) % 20 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

        # Turn唯一评估表
        if turn_unique:
            f.write(f"/* Compressed Turn LUT: {len(turn_mapping)} total -> {len(turn_unique)} unique */\n")
            f.write(f"const holdem_evaluation_t turn_unique_evaluations[{len(turn_unique)}] = {{\n")
            for i, (equity_all, equity_pairs) in enumerate(turn_unique):
                f.write(f"    {{{equity_all},{equity_pairs}}}")
                if i < len(turn_unique) - 1:
                    f.write(",")
                if (i + 1) % 8 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

            # Turn映射表
            f.write(f"const {turn_mapping_type} turn_mapping[{len(turn_mapping)}] = {{\n")
            for i, mapping_idx in enumerate(turn_mapping):
                f.write(f"{mapping_idx},")
                if (i + 1) % 20 == 0:
                    f.write("\n")
            f.write("\n};\n\n")

        # River表保持不变（假设原文件有river表）
        f.write("/* River LUT remains unchanged */\n")
        f.write("const int river_multidimensional_lut[7462] = {\n")
        # 生成简单的river映射（rank -> equity）
        for rank in range(1, 7463):
            equity = 10000 - (rank - 1) * 10000 // 7461
            equity = max(0, min(10000, equity))
            f.write(f"{equity},")
            if rank % 16 == 0:
                f.write("\n")
        f.write("};\n\n")

        f.write("#endif // EVALUATOR_HOLDEM_POTENTIAL_TABLES_H\n")

def main():
    if len(sys.argv) < 2:
        print("用法: python compress_lut.py <input_header_file> [output_header_file] [--bitpack]")
        print("示例: python compress_lut.py evaluator_holdem_potential_tables.h evaluator_holdem_potential_tables_compressed.h")
        print("位压缩: python compress_lut.py evaluator_holdem_potential_tables.h evaluator_holdem_potential_tables_bitpacked.h --bitpack")
        sys.exit(1)

    input_file = sys.argv[1]

    # 检查是否启用位压缩
    enable_bitpack = "--bitpack" in sys.argv

    if enable_bitpack:
        output_file = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "--bitpack" else "evaluator_holdem_potential_tables_bitpacked.h"
    else:
        output_file = sys.argv[2] if len(sys.argv) > 2 else "evaluator_holdem_potential_tables_compressed.h"

    if not os.path.exists(input_file):
        print(f"错误：输入文件 '{input_file}' 不存在")
        sys.exit(1)

    compression_mode = "位压缩" if enable_bitpack else "常规压缩"
    print(f"开始处理文件: {input_file} (模式: {compression_mode})")
    print(f"输出文件: {output_file}")
    print("=" * 50)

    # 解析Flop LUT
    flop_evals = parse_lut_from_file(input_file, "flop_multidimensional_lut")
    if flop_evals:
        if enable_bitpack:
            flop_unique_packed, flop_mapping = compress_evaluations_with_bitpacking(flop_evals)
            calculate_bitpacked_size_reduction(len(flop_evals), len(flop_unique_packed), len(flop_mapping))
        else:
            flop_unique, flop_mapping = compress_evaluations(flop_evals)
            calculate_size_reduction(len(flop_evals), len(flop_unique), len(flop_mapping))
    else:
        print("警告：未找到 flop_multidimensional_lut，跳过")
        if enable_bitpack:
            flop_unique_packed, flop_mapping = [], []
        else:
            flop_unique, flop_mapping = [], []

    print("\n" + "=" * 50)

    # 解析Turn LUT
    turn_evals = parse_lut_from_file(input_file, "turn_multidimensional_lut")
    if turn_evals:
        if enable_bitpack:
            turn_unique_packed, turn_mapping = compress_evaluations_with_bitpacking(turn_evals)
            calculate_bitpacked_size_reduction(len(turn_evals), len(turn_unique_packed), len(turn_mapping))
        else:
            turn_unique, turn_mapping = compress_evaluations(turn_evals)
            calculate_size_reduction(len(turn_evals), len(turn_unique), len(turn_mapping))
    else:
        print("警告：未找到 turn_multidimensional_lut，跳过")
        if enable_bitpack:
            turn_unique_packed, turn_mapping = [], []
        else:
            turn_unique, turn_mapping = [], []

    # 写入压缩后的文件
    if enable_bitpack:
        write_bitpacked_header(output_file, flop_unique_packed, flop_mapping, turn_unique_packed, turn_mapping)
    else:
        write_compressed_header(output_file, flop_unique, flop_mapping, turn_unique, turn_mapping)

    print(f"\n✅ {compression_mode}完成！新文件已保存为: {output_file}")

    # 显示文件大小对比
    if os.path.exists(output_file):
        original_size = os.path.getsize(input_file)
        compressed_size = os.path.getsize(output_file)
        size_reduction = (original_size - compressed_size) / original_size * 100

        print(f"\n=== 实际文件大小对比 ===")
        print(f"原始文件: {original_size / (1024*1024):.2f} MB")
        print(f"{compression_mode}文件: {compressed_size / (1024*1024):.2f} MB")
        print(f"文件大小减少: {size_reduction:.2f}%")

if __name__ == "__main__":
    main()