function DetectRunningSpinningMomo() {
    try {
        var wmi = GetObject("winmgmts:{impersonationLevel=impersonate}!\\\\.\\root\\cimv2");
        var results = wmi.ExecQuery("SELECT ProcessId FROM Win32_Process WHERE Name='SpinningMomo.exe'");
        var hasRunningProcess = false;

        var enumerator = new Enumerator(results);
        for (; !enumerator.atEnd(); enumerator.moveNext()) {
            hasRunningProcess = true;
            break;
        }

        Session.Property("SPINNINGMOMO_RUNNING") = hasRunningProcess ? "1" : "";
        return 1;
    }
    catch (e) {
        Session.Log("DetectRunningSpinningMomo script error: " + e.message);
        return 1;
    }
}
