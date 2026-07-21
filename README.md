# A-TAS

## 启动

通过 `Start-A-TAS.cmd` 启动 A-TAS，启动器会在打开管理器前自动检查更新。

## 发布

将功能修改合并到 `main` 并更新 `A_TAS_VERSION` 后，在 `main` 上创建并推送最后一个提交：

```bash
git commit -m "release 202607211500"
git push origin main
```

版本必须是与 `A_TAS_VERSION` 一致的十二位数字。GitHub Actions 会自动更新 `manifest.sha256`、打包 `A-TAS.zip`，并创建同版本的 GitHub Release。
