// 彻底卸载时递归删除 %LocalAppData%\SpinningMomo（含 webview2、缩略图等）。
// 注意：MSI 嵌入 JScript 的 Session 对象不支持 Session.Log，调用会报 1720 且中断脚本。

function PrepareRemoveAppDataRoot() {
  try {
    var path = Session.Property("APPDATAROOT");
    if (!path || path === "") {
      var sh = new ActiveXObject("WScript.Shell");
      path = sh.ExpandEnvironmentStrings("%LOCALAPPDATA%\\SpinningMomo");
    }
    // 与 deferred CustomAction Id 同名且全大写，MSI 才会把值写入 CustomActionData（Impersonate=yes）
    Session.Property("REMOVEAPPDATADIR") = path;
    return 1;
  } catch (e) {
    Session.Property("REMOVEAPPDATADIR") = "";
    return 1;
  }
}

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
