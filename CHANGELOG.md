# Changelog / 変更履歴

## Unreleased
- (EN) Fix tile-buffer reuse race on internal-RAM (DMA) sprites: the render loop now waits for each tile's DMA before reusing the single buffer, so a tile is no longer overwritten mid-transfer. Add opt-in `setDoubleBuffer(true)` (two ping-pong buffers) that overlaps a tile's DMA with the next tile's draw. See SPEC §10.5. Add a real-hardware benchmark under `bench/`.
- (JA) 内蔵RAM（DMA）スプライトでのタイルバッファ再利用レースを修正：描画ループが各タイルの DMA を待ってから単一バッファを再利用するようにし、転送中のタイル上書きを防止。`setDoubleBuffer(true)`（2枚 ping-pong）を opt-in で追加し、あるタイルの DMA と次タイルの描画を重ねる。SPEC §10.5 参照。実機ベンチマークを `bench/` に追加。

## 0.2.0
- (EN) Initial commit
- (JA) 初期コミット
