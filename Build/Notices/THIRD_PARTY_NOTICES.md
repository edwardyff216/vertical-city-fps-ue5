# 第三方归属 · PixelFX

本目录 20 个技能的 40 张序列图都由 **PixelFX** 运行库实际绘制。生成源码 `src/build.mjs` 和 `src/kit.mjs` 在运行时，从本地 PixelFX 检出目录 `import` 它的核心、效果库和工具；目录位置通过 `--pixelfx <dir>` 或环境变量 `PIXELFX_DIR` 传入。本目录不复制 PixelFX 源码，只保留许可证全文。

## 上游

| 项 | 值 |
|---|---|
| 仓库 | https://github.com/patch-gg/pixelfx |
| 版本 | 0.3.1（`package.json`） |
| 提交 | `5294518ce7c72e48b6100a744596e0acf57f2826` |
| 本次使用的检出位置 | `D:/Game 仓库，一些点子/tmp/pixelfx-pilot` |
| 许可证 | MIT，版权 © 2026 Patch、© 2026 Hugo Duprez，全文见 `LICENSE-PixelFX.txt` |
| Claude 技能 | 按仓库内 `skills/pixelfx/SKILL.md` 的流程制作：起草、渲染、看帧、修改，多轮迭代；约定见 `docs/AUTHORING.md` |

根据上游 `NOTICE.md` 和各效果文件头部的说明，`src/core/sim.js`、`src/fx/lightning/_lib.js` 以及下表中标注了「Hugo 署名」的效果，改编自 Hugo Duprez 的 Sprite Fusion 开源像素特效（https://www.spritefusion.com/pixel-art-vfx ，MIT，© 2026 Hugo Duprez）。本作用到了这些文件，所以这条归属同样适用。

## 用到的核心

| PixelFX 模块 | 内容 |
|---|---|
| `src/core/fx.js` | `createFx`、`createFrame`、`clearFrame`、`painterFor`：效果实例、120 Hz 子步更新、分层绘制 |
| `src/core/painter.js` | `line`、`rect`、`poly`、`tri`、`arc`、`beam`、`shade`、`circle`、`ellipse`、`px` |
| `src/core/sim.js` | `Particles` 及其类型：`MOTE`、`SPARK`、`DUST`、`GRIT`、`SMOKE`、`VAPOUR`、`FLAKE` |
| `src/core/math.js`、`color.js` | `Rng`、`rng`（随机数全部带种子），`fit`、`hex` |
| `src/fx/lightning/_lib.js` | `crackle()` |
| `tools/lib/fxlib.mjs` | `loadEffect`（加载下列效果库），以及预览用的 `canvas`、`upscale`、`writeVideo` |
| `tools/lib/png.mjs` | `encodePNG` |

## 直接调用的效果库（按选项换色、按时间采样）

| 效果 | 用在哪里 | Hugo 署名 |
|---|---|---|
| `projectiles/energy-disc` | 弹壁飞盘 起手、命中 | 是 |
| `impacts/ricochet` | 弹壁飞盘 命中 | 是 |
| `impacts/hit-spark` | 超载弹匣 命中、钩锁 命中 | 是 |
| `ice/frost-nova` | 冷凝地面 命中 | — |
| `explosions/smoke-bomb` | 遮断烟幕 命中 | — |
| `lightning/zap-hit` | 电栅 命中 | 是 |
| `scifi/warp-in` | 诱饵全息 命中 | 是 |
| `status/stunned` | 钩锁 命中 | 是 |
| `arcane/blink-out`、`arcane/blink-in` | 相位步 起手、命中 | 是 |
| `impacts/heavy-landing` | 喷射跃升 起手、命中 | 是 |
| `scifi/jetpack` | 喷射跃升 命中 | — |
| `impacts/blunt-hit` | 滑铲推进 命中 | 是 |
| `impacts/parry` | 偏转护板 命中 | 是 |
| `explosions/implosion` | 吸弹球 命中 | — |
| `pickups/collect-burst` | 速修 起手 | — |
| `holy/heal-aoe` | 速修 命中 | — |
| `scifi/force-field` | 掩体立墙 命中 | 是 |
| `scifi/emp-blast` | 磁轨换弹 命中 | 是 |
| `impacts/crit-hit` | 处决视界 命中 | 是 |
| `projectiles/muzzle-smg` | 随从炮台 命中 | — |

`lightning/zap-hit` 和 `explosions/grenade` 还被用作碎片雷、电弧枪刺试样的编排参考，那两招是按参考重写的，没有直接调用这两个效果。

## 自写部分（不属于上游）

- `src/fx/*.mjs`：碎片雷、电弧枪刺的四个手绘效果，以及硬折电弧 `hardBolt()`、硬边描线 `ink()`。
- `src/skills/*.mjs`：其余 18 招的分层编排：调用哪个库效果、在什么时间窗内采样、放在哪个位置，以及所有自绘的起手动作和叠加层。`src/skills/_parts.mjs` 是这些编排共用的小图形。
- `src/kit.mjs`：
  - 分层渲染器。
  - `bake()`：为亮厂房改写 PixelFX 的透明合成。光照只提亮已有颜色的像素，不生成柔光晕；半透明部分只保留 3 级透明度；外缘 2 px 清空；每个实心形状外侧加 1 px 深色描边。
  - 按技能色生成调色板，以及质检和预览。

这些自写部分和生成的 PNG 属于本项目。因为它们由 PixelFX 绘制，并在运行时导入 PixelFX 的代码，所以保留上面的 MIT 归属和许可证全文。
