# Texas Hold'em Lookup Table Evaluator System

## 概述

这是一个高性能的德州扑克手牌评估系统，通过预计算的查找表实现超高速评估。系统提供多维度的胜率分析，支持从翻牌前到河牌的所有游戏阶段。

## 特性

- ✅ **超高性能**: 平均 59M+ 次评估/秒
- ✅ **多维度分析**: 提供对不同手牌类型的胜率分析
- ✅ **全阶段支持**: 翻牌前/翻牌/转牌/河牌
- ✅ **向后兼容**: 支持所有旧版API
- ✅ **内存高效**: 仅需 9MB 查找表
- ✅ **编译时生成**: 查找表在构建时自动生成

## 性能表现

### 基准测试结果

| 阶段 | 评估速度 (次/秒) | 响应时间 |
|------|------------------|----------|
| 翻牌前 | 297M | 3.4 纳秒 |
| 翻牌 | 35M | 28 纳秒 |
| 转牌 | 35M | 29 纳秒 |
| 河牌 | 134M | 7.5 纳秒 |
| **平均** | **59M** | **17 纳秒** |

### 内存使用

| 组件 | 大小 | 说明 |
|------|------|------|
| 翻牌查找表 | 223KB | 169×169 索引 |
| 转牌查找表 | 2.9MB | 169×169×13 索引 |
| 河牌查找表 | 29KB | 7462 直接映射 |
| 潜力查找表 | 6.3MB | 传统潜力计算 |
| **总计** | **9.05MB** | 全部查找表 |

## API 文档

### 核心数据结构

```c
typedef struct {
    int equity_vs_all;           // 对所有可能手牌的胜率 (0-10000)
    int equity_vs_pair_sets;     // 对一对、两对、三条的胜率 (0-10000)
} holdem_evaluation_t;
```

### 主要函数

#### 1. 多维度评估函数
```c
holdem_evaluation_t evaluate_holdem_multidimensional(int* cards, int card_count);
```
- **功能**: 计算多维度胜率分析
- **参数**:
  - `cards`: 卡牌数组 (编码：rank*4 + suit)
  - `card_count`: 卡牌数量 (2=翻牌前, 5=翻牌, 6=转牌, 7=河牌)
- **返回**: 多维度评估结果

#### 2. 兼容性函数
```c
int evaluate_holdem_with_potential(int* cards, int card_count);
```
- **功能**: 返回总体胜率 (向后兼容)
- **返回**: 胜率值 (0-10000)

#### 3. 索引计算函数
```c
int get_hole_index(int c1, int c2);                    // 底牌索引 (0-168)
int get_flop_index(int c1, int c2, int c3);           // 翻牌纹理索引
int get_turn_index(int turn_card, unsigned long long known_cards);
int get_river_index(int river_card, unsigned long long known_cards);
```

### 卡牌编码

卡牌使用整数编码：`rank * 4 + suit`

- **牌面**: 0-12 (2, 3, 4, 5, 6, 7, 8, 9, T, J, Q, K, A)
- **花色**: 0-3 (梅花, 方块, 红心, 黑桃)

示例：
- As (黑桃A): 12 * 4 + 3 = 51
- Ks (黑桃K): 11 * 4 + 3 = 47
- 2c (梅花2): 0 * 4 + 0 = 0

## 使用示例

### 基本使用

```c
#include "phevaluator/evaluator_holdem_potential.h"

int main() {
    // 翻牌前评估：口袋AA
    int pocket_aces[] = {51, 50}; // As, Ah
    holdem_evaluation_t result = evaluate_holdem_multidimensional(pocket_aces, 2);
    printf("口袋AA总体胜率: %.1f%%\n", result.equity_vs_all / 100.0);
    printf("对子/三条胜率: %.1f%%\n", result.equity_vs_pair_sets / 100.0);

    // 翻牌评估：As Ks on Js 9s 8h
    int flop_hand[] = {51, 47, 43, 39, 32}; // 坚果同花听牌
    result = evaluate_holdem_multidimensional(flop_hand, 5);
    printf("同花听牌胜率: %.1f%%\n", result.equity_vs_all / 100.0);

    return 0;
}
```

### 性能测试

```c
#include <time.h>

void performance_test() {
    int test_hand[] = {51, 47, 43, 39, 32};
    clock_t start = clock();

    for (int i = 0; i < 1000000; i++) {
        holdem_evaluation_t result = evaluate_holdem_multidimensional(test_hand, 5);
        (void)result; // 避免编译器优化
    }

    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("100万次评估用时: %.4f秒\n", elapsed);
    printf("评估速度: %.0f次/秒\n", 1000000.0 / elapsed);
}
```

## 构建说明

### 系统要求

- CMake 3.14+
- GCC 4.8+ 或 Clang 3.4+
- 至少 50MB 可用内存（构建时）

### 构建步骤

```bash
# 1. 克隆/下载源代码
cd PokerHandEvaluator/cpp

# 2. 运行自动构建脚本
./build_and_test_lookup_tables.sh

# 或手动构建：
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make generate_potential_tables  # 生成查找表
make pheval                     # 构建主库
```

### 集成到项目

1. **链接库文件**:
```cmake
target_link_libraries(your_project pheval)
```

2. **包含头文件**:
```c
#include "phevaluator/evaluator_holdem_potential.h"
```

3. **确保查找表文件存在**:
   - `evaluator_holdem_potential_tables.h` 需在构建时生成

## 技术细节

### 查找表设计

#### 1. 翻牌阶段查找表
- **索引**: `[hole_index][board_texture]`
- **大小**: 169 × 169 = 28,561 条记录
- **内容**: `holdem_evaluation_t` 结构体

#### 2. 转牌阶段查找表
- **索引**: `[hole_index][board_texture][turn_rank]`
- **大小**: 169 × 169 × 13 = 371,293 条记录
- **内容**: `holdem_evaluation_t` 结构体

#### 3. 河牌阶段查找表
- **索引**: `[hand_rank]`
- **大小**: 7,462 条记录
- **内容**: 直接胜率映射

### 底牌索引算法

底牌使用169种标准化组合：
- 13个口袋对 (AA, KK, ..., 22)
- 78个同花组合 (AKs, AQs, ..., 32s)
- 78个不同花组合 (AKo, AQo, ..., 32o)

### 公共牌纹理计算

公共牌纹理考虑：
- 对子/三条结构 (权重 50×)
- 同花听牌可能 (权重 25)
- 顺子听牌强度 (权重 12×)
- 高牌等级 (权重 1×)

## 故障排除

### 常见问题

**Q: 编译时找不到查找表文件**
A: 确保先运行 `generate_potential_tables` 生成查找表文件

**Q: 链接错误 "undefined reference"**
A: 检查是否正确链接了 `libpheval.a` 库

**Q: 性能不如预期**
A: 确认使用了 Release 模式编译 (`-O3` 优化)

**Q: 内存占用过高**
A: 查找表在程序加载时占用 9MB，这是正常的

### 调试技巧

1. **验证卡牌编码**:
```c
printf("卡牌编码: rank=%d, suit=%d\n", card/4, card%4);
```

2. **检查索引范围**:
```c
int hole_idx = get_hole_index(c1, c2);
assert(hole_idx >= 0 && hole_idx < 169);
```

3. **比较不同评估方法**:
```c
// 比较新旧API结果
holdem_evaluation_t new_result = evaluate_holdem_multidimensional(cards, count);
int legacy_result = evaluate_holdem_with_potential(cards, count);
assert(new_result.equity_vs_all == legacy_result);
```

## 许可证

本项目遵循原始 PokerHandEvaluator 项目的许可证条款。

## 贡献

欢迎提交 Issues 和 Pull Requests！主要改进方向：

1. 查找表压缩算法优化
2. 更精确的胜率计算模型
3. 支持更多手牌类型分析
4. 性能进一步优化

## 更新日志

### v4.0 - 查找表系统
- ✅ 实现完整的查找表系统
- ✅ 多维度胜率分析
- ✅ 平均 59M+ 评估/秒性能
- ✅ 完整向后兼容性
- ✅ 自动化构建和测试

### v3.x - 改进潜力计算
- ✅ 修复负分问题
- ✅ 基于改进度的潜力计算
- ✅ 更精确的概率模型

---

**联系信息**: 如有技术问题，请查阅测试用例或提交 Issue。