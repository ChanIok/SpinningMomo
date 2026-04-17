// 彻底卸载时递归删除 %LocalAppData%\SpinningMomo（含 webview2、缩略图等）。
// 路径由 Package.wxs 中 Type 51 CustomAction（SetRemoveAppDataDir）写入 REMOVEAPPDATADIR，再经 deferred 传入 CustomActionData。
// 注意：MSI 嵌入 JScript 的 Session 对象不支持 Session.Log，调用会报 1720 且中断脚本。

function RemoveAppDataRootDeferred() {
  try {
    var folder = Session.Property("CustomActionData");
    if (!folder || folder === "") {
      return 1;
    }
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    if (fso.FolderExists(folder)) {
      fso.DeleteFolder(folder, true);
    }
    return 1;
  } catch (e) {
    return 1;
  }
}
