# 北斗 B1C 信号主码 Python 仿真与评估

## 1. 任务目标

依据《北斗卫星导航系统空间信号接口控制文件 公开服务信号 B1C（1.0 版）》，本文完成了 B1C 信号主码的 Python 仿真，并在此基础上做了正确性校验和相关特性分析。具体工作包括：按照 ICD 定义生成 B1C 数据分量和导频分量主码，对结果与 ICD 表 5-2、表 5-3 的头 24 位和尾 24 位进行一致性校验，并围绕平衡性、自相关、不同 PRN 间互相关、功率谱以及截断后有限长度互相关开展分析。本文只讨论 B1C 主码本身，不涉及导频子码生成和 B1C 复合调制仿真。

## 2. 理论基础

根据 ICD 第 5 章，B1C 主码由长度为 `N=10243` 的 Weil 码截短得到。主码长度为 `10230`，码速率为 `1.023 Mcps`，周期为 `10 ms`。Weil 码由两个 Legendre 序列经相位差 `w` 组合得到，主码则从 Weil 码的第 `p` 位开始循环截取 `10230` 个码片，其中 `p` 采用一基编号。对应公式为：

1. Weil 码

   `W(k; w) = L(k) xor L((k + w) mod N), k = 0, 1, ..., N-1`

2. 主码截断

   `c(n; w, p) = W((n + p - 1) mod N; w), n = 0, 1, ..., 10229`

其中 `L(k)` 表示 Legendre 序列，`w` 是相位差，`p` 是截取点。ICD 给出了 63 组数据分量参数和 63 组导频分量参数，因此总共有 126 组主码。为了评价这些主码的使用特性，本文主要关注五类指标：一是平衡性，用来判断码片分布是否存在明显直流偏置；二是周期自相关，用来观察主峰是否尖锐、旁瓣是否足够低；三是周期互相关，用来衡量不同 PRN 之间的可分辨性；四是有限长度非周期互相关，用来模拟实际接收机不能积累完整主码时的区分性能；五是功率谱，用来观察频域能量分布以及直流分量是否被抑制。

对于截断长度为 `L` 的双极性序列 `a_L(n)` 和 `b_L(n)`，本文采用归一化非周期互相关

`R_ab^(L)(tau) = (1 / L) * sum a_L(n) b_L(n + tau)`

并关注 `max |R_ab^(L)(tau)|`。这样做的原因很直接：接收机真正容易出问题的不是平均互相关偏大，而是某个延迟点突然出现较高伪峰；伪峰越高，正确峰和错误峰就越难分开。

## 3. 程序实现

代码实现主要由参数表、主码生成器、分析脚本和专项校验脚本四部分组成。`b1c_parameters.py` 固化了 ICD 表 5-2 和表 5-3 中全部主码参数；`b1c_maincode.py` 负责生成主码、提供双极性映射以及周期相关和非周期相关函数；`analyze_b1c_maincode.py` 负责生成图表和统计结果；`validate_table52.py` 则对表 5-2 做专项校验。整体流程并不复杂：先由平方剩余集合构造 Legendre 序列，再用参数 `w` 生成 Weil 码，随后按截取点 `p` 循环截取出长度为 `10230` 的主码，最后计算相关指标并输出图表。

核心生成过程如下：

```python
def weil_code(phase_diff):
    legendre = legendre_sequence()
    return np.bitwise_xor(legendre, np.roll(legendre, -phase_diff))

def primary_code(prn, channel="data", bipolar=False):
    phase_diff, truncation_point, _, _ = CHANNEL_TABLES[channel][prn]
    code = weil_code(phase_diff)
    start = truncation_point - 1
    indices = (np.arange(PRIMARY_CODE_LENGTH) + start) % WEIL_LENGTH
    primary = code[indices]
    return 1 - 2 * primary.astype(np.int8) if bipolar else primary
```

其中 `channel` 用来区分数据分量和导频分量。两类主码的生成流程相同，只是同一 `PRN` 在两张参数表中对应的 `w` 和 `p` 不一样。为了保证程序不是“看起来能跑”，而是真正和 ICD 对齐，代码中还加入了头尾 24 码片校验逻辑：

```python
def validate_primary_codes():
    failures = []
    for channel, table in CHANNEL_TABLES.items():
        for prn, (_, _, head_octal, tail_octal) in table.items():
            code = primary_code(prn, channel)
            if not np.array_equal(code[:24], octal24_to_bits(head_octal)):
                failures.append(f"{channel} PRN{prn} head mismatch")
            if not np.array_equal(code[-24:], octal24_to_bits(tail_octal)):
                failures.append(f"{channel} PRN{prn} tail mismatch")
    return failures
```

在分析部分，程序先把 `0/1` 主码映射成 `+1/-1` 双极性序列，再用 FFT 计算周期相关函数和有限长度非周期互相关。这样处理后，相关结果更符合导航接收机分析中的常见表达方式，数值比较也更直观。

```python
def periodic_correlation(code_a, code_b=None):
    if code_b is None:
        code_b = code_a
    seq_a = to_bipolar(code_a)
    seq_b = to_bipolar(code_b)
    spectrum = np.fft.fft(seq_a) * np.conj(np.fft.fft(seq_b))
    corr = np.fft.ifft(spectrum).real
    corr = np.roll(corr, len(corr) // 2)
    lags = np.arange(-len(corr) // 2, len(corr) - len(corr) // 2)
    return lags, corr / len(seq_a)

def aperiodic_correlation(code_a, code_b=None):
    if code_b is None:
        code_b = code_a
    seq_a = to_bipolar(code_a)
    seq_b = to_bipolar(code_b)
    nfft = 1 << (len(seq_a) + len(seq_b) - 2).bit_length()
    spectrum = np.fft.fft(seq_a, n=nfft) * np.fft.fft(seq_b[::-1], n=nfft)
    corr = np.fft.ifft(spectrum).real[: len(seq_a) + len(seq_b) - 1]
    lags = np.arange(-(len(seq_b) - 1), len(seq_a))
    return lags, corr / len(seq_a)
```

## 4. 校验结果

为了确认实现严格符合 ICD，程序对全部 126 组主码都做了头 24 位和尾 24 位校验，运行结果为：

```text
all 126 B1C primary codes validated against the ICD tables
```

这说明主码生成公式、数据分量与导频分量参数调用方式，以及截取点 `p` 的一基索引实现都是正确的。对表 5-2 的专项校验结果同样全部通过，即 63/63 个数据分量 PRN 全部匹配，没有发现任何头 24 位或尾 24 位不一致的情况。这一步先把主码生成的正确性压实了，后面的相关分析才有依据。

## 5. 仿真结果分析

先看时域特性。`output_eval/b1c_data_pilot_prn1_first_200chips.png` 给出了 PRN1 数据分量和导频分量主码前 200 个码片的双极性波形。两类主码都在 `+1` 和 `-1` 之间快速跳变，没有明显的短周期重复结构，时域上表现出典型的伪随机特征。进一步统计全部 63 组数据分量主码和 63 组导频分量主码后可以看到，两类主码都严格平衡：每条主码中 `1` 和 `0` 的个数都各为 `5115`，最大平衡误差为 `0`，双极性均值也为 `0.0`。这说明主码本身不存在直流偏置，后面频谱中直流分量被压低，其实和这里的平衡性结果是一致的。

![数据分量与导频分量前200码片](output_eval/b1c_data_pilot_prn1_first_200chips.png)

周期自相关结果见 `output_eval/b1c_data_pilot_prn1_autocorrelation.png`。对导航测距码来说，零延迟主峰是否突出、非零延迟旁瓣是否够低，是最直观的一组指标。以 PRN1 为例，数据分量主峰为 `1.0000`，最大绝对旁瓣为 `0.027175`，旁瓣均方根值为 `0.009026`；导频分量主峰同样为 `1.0000`，最大绝对旁瓣为 `0.026393`，旁瓣均方根值为 `0.009062`。如果放到全部 PRN 范围内看，数据分量最差旁瓣出现在 PRN23，对应 `0.027566`，导频分量最差旁瓣出现在 PRN54，对应值同样为 `0.027566`。这些结果说明，数据分量和导频分量在自相关主峰尖锐性上处于相近水平，旁瓣整体也压得比较低，满足码捕获和码跟踪的基本要求。

![数据分量与导频分量自相关](output_eval/b1c_data_pilot_prn1_autocorrelation.png)

再看不同 PRN 之间的周期互相关。`output_eval/b1c_data_pilot_prn1_prn2_crosscorrelation.png` 给出了 PRN1 与 PRN2 的互相关结果，同时脚本也统计了同一通道内全部 PRN 对的最差情况。数据分量中，PRN1 与 PRN2 的最大绝对周期互相关值为 `0.038123`，均方根值为 `0.009871`；导频分量中，这组 PRN 的最大绝对周期互相关值为 `0.032258`，均方根值为 `0.009944`。如果看全部 PRN 对，数据分量最差互相关对为 PRN38 与 PRN45，对应最大绝对值 `0.043206`；导频分量最差互相关对为 PRN23 与 PRN34，对应最大绝对值 `0.042424`。这些数值都明显低于自相关主峰 `1.0`，说明在完整主码周期内，不同卫星 PRN 之间仍然具有较好的可分辨性。

![数据分量与导频分量互相关](output_eval/b1c_data_pilot_prn1_prn2_crosscorrelation.png)

除了完整周期相关，本文还考察了主码被截断后的有限长度互相关。实际接收机并不总能积累完整的 `10230` 个码片，弱信号捕获、动态环境下的短时相关或者较短的相干积分时间，都会让接收机只利用主码的一部分。因此，这里选取 `256`、`512`、`1023`、`2046`、`4092`、`8192` 和 `10230` 这些长度，对每个通道内全部不同 PRN 对逐一计算非周期互相关，并取 `max |R_ab^(L)(tau)|` 作为该长度下的伪峰指标。这样看的是最坏情况，而不是平均情况，因为真正影响接收机判决的往往就是某个延迟点冒出来的高伪峰。

`output_eval/b1c_truncation_crosscorr.png` 给出了随截断长度变化的趋势。从统计结果看，数据分量最差 PRN 对的最大互相关由 `L=256` 时的 `0.2695` 下降到 `L=10230` 时的 `0.0359`，导频分量则由 `0.2773` 下降到 `0.0350`。代表性 PRN1 与 PRN2 也表现出相同趋势：数据分量从 `0.1484` 下降到 `0.0259`，导频分量从 `0.1758` 下降到 `0.0288`。这说明随着积累长度增加，更多码片参与相关，正负项抵消更充分，伪峰会整体下降，接收机区分不同 PRN 的难度也随之降低。换句话说，有限长度下最需要警惕的就是短码长带来的高伪峰，而不是完整周期条件下的平均互相关。

![截断长度与互相关最大值](output_eval/b1c_truncation_crosscorr.png)

最差 PRN 对在不同截断长度下的统计结果如下表所示：

| 截断长度 L | 数据分量最差 `max |R|` | 导频分量最差 `max |R|` |
| --- | --- | --- |
| 256 | 0.2695 | 0.2773 |
| 512 | 0.2051 | 0.1934 |
| 1023 | 0.1417 | 0.1486 |
| 2046 | 0.1012 | 0.1007 |
| 4092 | 0.0743 | 0.0694 |
| 8192 | 0.0402 | 0.0414 |
| 10230 | 0.0359 | 0.0350 |

最后看功率谱。`output_eval/b1c_data_pilot_power_spectrum.png` 给出了对全部 63 组数据分量主码和 63 组导频分量主码分别求平均后得到的归一化功率谱。两类主码的频谱都关于零频对称，这和实值双极性码序列的频域特征一致。由于两类主码都严格平衡，直流分量被完全抑制，统计结果中 `dc_relative_power = 0`。在 `0.25 Rc` 附近，数据分量相对峰值功率约为 `-1.734 dB`，导频分量约为 `-2.373 dB`。整体上看，数据分量和导频分量的频谱包络比较接近，只是由于参数 `w` 和截取点 `p` 不同，局部谱线分布并不完全一样。从主码层面说，这样的频谱特性已经说明两类主码都具有较好的频谱分散性，直流分量也压得比较干净。

![数据分量与导频分量功率谱](output_eval/b1c_data_pilot_power_spectrum.png)

## 6. 结论

本文完成了北斗 B1C 数据分量和导频分量共 126 组主码的 Python 仿真，并通过 ICD 表 5-2 和表 5-3 的头尾码片校验确认了实现的正确性。仿真结果表明，B1C 主码具有严格平衡的码片分布、尖锐的自相关主峰以及较低的不同 PRN 间互相关；在有限长度条件下，随着积累长度增加，互相关伪峰会继续下降，不同 PRN 的可分辨性也随之提升。功率谱分析进一步说明，两类主码都不存在明显直流谱峰，频谱分散性较好。就本文的工作范围而言，当前实现已经能够作为后续 B1C 捕获、跟踪、扩频接收以及进一步调制仿真的基础模块。
