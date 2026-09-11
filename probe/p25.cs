using System;
using System.Threading;
class P25 {
    static void W(string s) { Console.WriteLine("P25:" + s); }
    static volatile int tc;
    static int Main() {
        for (int round = 0; round < 15; round++) {
            tc = 0;
            object lk = new object();
            Thread[] ts = new Thread[4];
            for (int i = 0; i < 4; i++) {
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) {
                        lock (lk) { tc++; }
                        Interlocked.Increment(ref tc);
                    }
                });
                ts[i].Start();
            }
            for (int i = 0; i < 4; i++) ts[i].Join();
            int v1 = tc;
            if (v1 != 2000) {
                Thread.Sleep(100);
                int v2 = tc;
                Thread.MemoryBarrier();
                int v3 = tc;
                W("round" + round + " v1=" + v1 + " v2=" + v2 + " v3=" + v3);
            }
        }
        W("ALL-DONE");
        return 0;
    }
}
