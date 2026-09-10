using System;
using System.IO;
using System.Threading;
class P4 {
    static StreamWriter log;
    static volatile int tc;
    static object lk = new object();
    static void W(string s) { log.WriteLine(s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p4-result.txt", false);
        log.AutoFlush = true;
        try {
            for (int round = 0; round < 3; round++) {
                tc = 0;
                Thread[] ts = new Thread[4];
                for (int i = 0; i < 4; i++) {
                    ts[i] = new Thread(delegate() {
                        for (int j = 0; j < 20000; j++) { lock (lk) { tc++; } }
                    });
                    ts[i].Start();
                }
                for (int i = 0; i < 4; i++) ts[i].Join();
                W("lock-only round" + round + " tc=" + tc + " expect=80000");
            }
            for (int round = 0; round < 3; round++) {
                tc = 0;
                Thread[] ts = new Thread[4];
                for (int i = 0; i < 4; i++) {
                    ts[i] = new Thread(delegate() {
                        for (int j = 0; j < 20000; j++) { Interlocked.Increment(ref tc); }
                    });
                    ts[i].Start();
                }
                for (int i = 0; i < 4; i++) ts[i].Join();
                W("interlocked-only round" + round + " tc=" + tc + " expect=80000");
            }
            tc = 0;
            {
                Thread[] ts = new Thread[4];
                for (int i = 0; i < 4; i++) {
                    ts[i] = new Thread(delegate() {
                        for (int j = 0; j < 250; j++) { lock (lk) { tc++; } Interlocked.Increment(ref tc); }
                    });
                    ts[i].Start();
                }
                for (int i = 0; i < 4; i++) ts[i].Join();
            }
            W("combined tc=" + tc + " expect=2000");
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
