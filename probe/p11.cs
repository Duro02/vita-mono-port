using System;
using System.IO;
using System.Threading;
class P11 {
    static StreamWriter log;
    static object lk = new object();
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P11:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p11-result.txt", false);
        log.AutoFlush = true;
        try {
            W("start");
            lock (lk) {
                W("main-holds-lock");
                Thread t = new Thread(delegate() {
                    int t0 = Environment.TickCount;
                    bool r = Monitor.TryEnter(lk, 5000);
                    int ms = Environment.TickCount - t0;
                    try { W("tryenter ret=" + r + " ms=" + ms); } finally { if (r) Monitor.Exit(lk); }
                });
                t.Start();
                Thread.Sleep(6000);
            }
            W("main-released");
            Thread.Sleep(1000);
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
