using System;
using System.IO;
using System.Threading;
class P10 {
    static StreamWriter log;
    static volatile int tcounter;
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P10:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p10-result.txt", false);
        log.AutoFlush = true;
        try {
            W("t02-begin");
            tcounter = 0;
            object lk = new object();
            Thread[] ts = new Thread[4];
            for (int i = 0; i < 4; i++) {
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) {
                        lock (lk) { tcounter++; }
                        Interlocked.Increment(ref tcounter);
                    }
                });
                W("created-" + i);
                ts[i].Start();
                W("started-" + i);
            }
            for (int i = 0; i < 4; i++) { ts[i].Join(); W("joined-" + i); }
            W("tc=" + tcounter + " expect=2000");
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
