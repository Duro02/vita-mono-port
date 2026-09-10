using System;
using System.IO;
class P9 {
    static StreamWriter log;
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P9:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p9-result.txt", false);
        log.AutoFlush = true;
        try {
            W("start");
            GC.Collect(0);
            W("minor-ok");
            GC.Collect();
            W("major-ok");
            GC.WaitForPendingFinalizers();
            W("finalizers-ok");
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
