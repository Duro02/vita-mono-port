using System;
using System.Threading;
class P23 {
    static void W(string s) { Console.WriteLine("P23:" + s); }
    static volatile int tc;
    static int Div(int a, int b) { return a / b; }
    static int Main() {
        double d = 2.5;
        W("via-local=" + Math.Round(d));
        W("direct=" + Math.Round(2.5));
        try { throw new InvalidOperationException("x"); } catch (InvalidOperationException) { }
        try { throw new Exception("i"); } catch { }
        int r = 0;
        try { r = Div(10, 0); } catch (DivideByZeroException) { r = -1; }
        W("div=" + r);
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
            W("t" + round + "=" + tc);
        }
        W("ALL-DONE");
        return 0;
    }
}
