using System;
using System.IO;
using System.Collections;
class P8 {
    static StreamWriter log;
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P8:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p8-result.txt", false);
        log.AutoFlush = true;
        try {
            W("start");
            ArrayList keep = new ArrayList();
            for (int i = 0; i < 3000; i++) {
                keep.Add(new byte[64]);
                if ((i % 1000) == 999) W("alloc-" + (i + 1));
            }
            W("small-ok");
            keep.Clear();
            W("clear-ok");
            keep = null;
            GC.Collect();
            W("collect-ok");
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
