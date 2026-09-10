using System;
using System.IO;
using System.Threading;
class P2 {
    static StreamWriter log;
    static volatile int tcounter;
    static object lk = new object();
    static void W(string s) { log.WriteLine(s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p2-result.txt", false);
        log.AutoFlush = true;
        try {
            W("step1-begin");
            Thread t = new Thread(delegate() { });
            t.Start(); t.Join();
            W("step1-single-thread-ok");
            W("step2-begin-4threads");
            tcounter = 0;
            Thread[] ts = new Thread[4];
            for (int i = 0; i < 4; i++) {
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) { tcounter++; }
                });
                ts[i].Start();
            }
            for (int i = 0; i < 4; i++) ts[i].Join();
            W("step2-join-ok tcounter=" + tcounter);
            W("step3-begin-lock");
            tcounter = 0;
            for (int i = 0; i < 4; i++) {
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) { lock (lk) { tcounter++; } }
                });
                ts[i].Start();
            }
            for (int i = 0; i < 4; i++) ts[i].Join();
            W("step3-lock-ok tcounter=" + tcounter);
            W("step4-begin-interlocked");
            tcounter = 0;
            for (int i = 0; i < 4; i++) {
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) { Interlocked.Increment(ref tcounter); }
                });
                ts[i].Start();
            }
            for (int i = 0; i < 4; i++) ts[i].Join();
            W("step4-interlocked-ok tcounter=" + tcounter);
            W("step5-begin-sleep");
            Thread.Sleep(50);
            W("step5-sleep-ok");
            W("step6-begin-pool");
            bool done = false;
            ThreadPool.QueueUserWorkItem(delegate(object o) { done = true; }, null);
            for (int i = 0; i < 200 && !done; i++) Thread.Sleep(10);
            W("step6-pool done=" + done);
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
