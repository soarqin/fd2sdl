# Changelog

本文件按 [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) 的格式记录重要改动。

## [Unreleased]

## [0.2.1] - 2026-11-26

### Added

- Windows x64 发布包提供 `play.cmd` 启动脚本。运行时需自备原版游戏数据。
- 新增浏览器版。页面可从原版数据 ZIP 启动游戏，并在浏览器支持时保留存档。
- 启动设置提供简体中文和繁体中文，可按 F2 或点击右上角切换。
- 增加升级成长、罗德曼招募、30 级经验归零、AI 攻击高防御目标和休息回血的规则选项。

### Changed

- 桌面版优先从当前工作目录读取原版数据，仍支持原有的 `original_game/` 布局。可在启动设置的「路径」页指定其他目录。

### Fixed

- 读取菜单遇到缺失存档时会返回原菜单。
- 城镇服务提示遵循逐字速度设置。

### Internal

- GitHub Actions 自动构建、检验并发布 Windows x64 压缩包。
- 补齐数据路径、启动、战斗和原版对照的回归覆盖。

## [0.2.0] - 2026-11-23

开发里程碑。

### Added

- 新增启动设置，可调整画面、音量、数据和存档路径、键位及游戏节奏。
- 支持手柄热插拔、自定义绑定和摇杆死区。

### Changed

- 默认节奏和规则沿用原版流程，启用第 23 章罗德曼招募条件和 30 级经验归零修正。

[Unreleased]: https://github.com/soarqin/fd2sdl/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/soarqin/fd2sdl/releases/tag/v0.2.1
[0.2.0]: https://github.com/soarqin/fd2sdl/tree/b66843bc2005bf78666b1a45efa83bed7de1eb89
