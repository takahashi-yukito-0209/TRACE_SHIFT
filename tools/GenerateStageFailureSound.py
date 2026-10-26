"""開始音と同じ音色で、落下失敗を伝える短い下降チャイムを生成する。"""

import math
import struct
import wave
from pathlib import Path


# <summary>短い立ち上がりと減衰を付けた失敗音をPCM WAVへ保存する。</summary>
def main():
    sample_rate = 48000  # 出力のサンプルレート
    duration = 0.24  # 下降チャイムの長さ
    samples = []  # 16bit PCMへ変換する波形
    for index in range(round(sample_rate * duration)):  # 波形のサンプル番号
        time = index / sample_rate  # 音の開始からの秒数
        value = 0.0  # 二音を重ねた振幅
        for onset, frequency in ((0.0, 659.255), (0.065, 440.0)):  # 開始音と同じ音域から下がる発音時刻と周波数
            age = time - onset  # 各音が鳴り始めてからの秒数
            if age >= 0.0:
                envelope = min(age / 0.005, 1.0) * math.exp(-age / 0.065)  # クリックを抑える立ち上がりと減衰
                value += 0.18 * envelope * math.sin(2.0 * math.pi * frequency * age)
        value *= min((duration - time) / 0.02, 1.0)
        samples.append(round(max(-1.0, min(value, 1.0)) * 32767))
    output = Path(__file__).resolve().parents[1] / "project/resources/sounds/se/stage_failure.wav"  # 既存の音声検索先に配置する失敗音
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output), "wb") as audio:  # PCM WAVの保存先
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(sample_rate)
        audio.writeframes(struct.pack(f"<{len(samples)}h", *samples))
    print(output)


if __name__ == "__main__":
    main()
