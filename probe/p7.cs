using System;
using System.IO;
using System.Collections;
class P7 {
    static StreamWriter log;
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P7:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p7-result.txt", false);
        log.AutoFlush = true;
        try {
            W("start");
            ArrayList keep = new ArrayList();
            for (int i = 0; i < 20000; i++) {
                keep.Add(new byte[64]);
                if ((i % 5000) == 4999) W("alloc-" + (i + 1));
            }
            W("small-ok");
            W("pre-clear");
            keep.Clear();
            W("clear-ok");
            keep = null;
            GC.Collect();
            W("collect1-ok");
            GC.WaitForPendingFinalizers();
            W("finalizers-ok");
            byte[] big = new byte[1024 * 1024];
            big[1000] = 7;
            W("large-ok v=" + big[1000]);
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
