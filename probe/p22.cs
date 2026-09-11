using System;
using System.Threading;
class P22 {
    static void W(string s) { Console.WriteLine("P22:" + s); }
    static volatile int tc;
    static object slk = new object();
    static int Main() {
        double r = Math.Round(2.5);
        W("roundbits=" + BitConverter.DoubleToInt64Bits(r).ToString("X16"));
        for (int round = 0; round < 10; round++) {
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
            W("round" + round + " tc=" + tc);
        }
        W("ALL-DONE");
        return 0;
    }
}
