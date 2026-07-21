# A-TAS

## 使用

通过 `Start-A-TAS.cmd` 启动 A-TAS，启动器会在打开管理器前自动检查更新。

- `settings.dat`、`keybindings.ini`：用户配置，首次使用后生成在根目录。
- `replay/`：默认录像目录。
- `app/A-TAS-Manager.exe`：管理器主体，建议始终通过 `Start-A-TAS.cmd` 调用。
- `app/libavz.dll`：A-TAS 的 AVZ 脚本模块。
- `app/`：管理器、脚本模块、更新器和压缩工具，请勿手动修改。
- `src/`：A-TAS 脚本源码。
- `docs/`：更新日志和许可证。

## 发布

替换发布用的 `app/libavz.dll` 前，先将 `app/7z.exe` 和 `app/7z.dll` 复制到用于编译 A-TAS 的 AVZ 工作区 `bin/` 目录。编译完成后，再将该目录生成的 `libavz.dll` 复制到本仓库的 `app/libavz.dll`。

将功能修改合并到 `main` 并更新 `A_TAS_VERSION` 后，在 `main` 上创建并推送最后一个提交：

```bash
git commit -m "release 202607211500"
git push origin main
```

版本必须是与 `A_TAS_VERSION` 一致的十二位数字。GitHub Actions 会自动打包 `A-TAS-4.0.zip`、生成 `manifest.sha256`，并创建同版本的 GitHub Release。
