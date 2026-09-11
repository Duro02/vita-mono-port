using System;
using System.Threading;
class P24 {
    static void W(string s) { Console.WriteLine("P24:" + s); }
    static volatile int tc;
    static int Div(int a, int b) { return a / b; }
    static int RunRound() {
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
        return tc;
    }
    static int Main() {
        int fails = 0;
        for (int round = 0; round < 30; round++) {
            int v = RunRound();
            if (v != 2000) { fails++; W("plain" + round + "=" + v); }
        }
        W("plain-done fails=" + fails);
        for (int round = 0; round < 30; round++) {
            try { throw new InvalidOperationException("x"); } catch (InvalidOperationException) { }
            try { r_Div(); } catch (DivideByZeroException) { }
            int v = RunRound();
            if (v != 2000) { fails++; W("withT01-" + round + "=" + v); }
        }
        W("ALL-DONE fails=" + fails);
        return 0;
    }
    static int r_Div() { return Div(10, 0); }
}
