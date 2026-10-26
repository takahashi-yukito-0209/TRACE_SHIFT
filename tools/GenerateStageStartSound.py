"""開始合図用の短い二音のチャイムを、外部素材なしでPCM WAVへ生成する。"""

import math
import struct
import wave
from pathlib import Path


# <summary>短い立ち上がりと減衰を付けた開始音を再生成する。</summary>
def main():
    sample_rate = 48000  # 出力のサンプルレート
    duration = 0.24  # 合図全体の長さ
    samples = []  # 16bit PCMに変換する波形
    for index in range(round(sample_rate * duration)):  # 生成するサンプル番号
        time = index / sample_rate  # 音の開始からの秒数
        value = 0.0  # 二つの音を重ねる振幅
        for onset, frequency in ((0.0, 659.255), (0.065, 987.767)):  # 各音の発音時刻と周波数
            age = time - onset  # 各音が鳴り始めてからの秒数
            if age >= 0.0:
                envelope = min(age / 0.005, 1.0) * math.exp(-age / 0.065)  # クリックを避ける立ち上がりと減衰
                value += 0.18 * envelope * math.sin(2.0 * math.pi * frequency * age)
        value *= min((duration - time) / 0.02, 1.0)
        samples.append(round(max(-1.0, min(value, 1.0)) * 32767))
    output = Path(__file__).resolve().parents[1] / "project/resources/sounds/se/stage_start.wav"  # 既存の音声検索先に置く生成物
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output), "wb") as audio:  # PCM WAVの保存先
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(sample_rate)
        audio.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print(output)


if __name__ == "__main__":
    main()
