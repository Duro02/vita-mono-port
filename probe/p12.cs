using System;
using System.IO;
using System.Threading;
class P12 {
    static StreamWriter log;
    static int tc;
    static void W(string s) { log.WriteLine(s); Console.WriteLine("P12:" + s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p12-result.txt", false);
        log.AutoFlush = true;
        try {
            W("begin");
            tc = 0;
            Thread[] ts = new Thread[4];
            for (int i = 0; i < 4; i++) {
                int id = i;
                ts[i] = new Thread(delegate() {
                    for (int j = 0; j < 250; j++) { tc++; }
                    Console.WriteLine("P12:worker-" + id + "-done");
                });
                ts[i].Start();
            }
            W("all-started");
            for (int i = 0; i < 4; i++) { ts[i].Join(); W("joined-" + i); }
            W("tc=" + tc);
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
