using System;
using System.IO;
using System.Threading;
using System.Collections;
class P56 {
    static StreamWriter log;
    static void W(string s) { log.WriteLine(s); }
    static int Main() {
        log = new StreamWriter("ux0:/data/monoapp/p56-result.txt", false);
        log.AutoFlush = true;
        try {
            object o = new object();
            int t0 = Environment.TickCount;
            bool r;
            lock (o) { r = Monitor.Wait(o, 100); }
            W("monitor-wait100 ret=" + r + " ms=" + (Environment.TickCount - t0));
            ManualResetEvent ev = new ManualResetEvent(false);
            t0 = Environment.TickCount; bool r2 = ev.WaitOne(100);
            W("event-wait100 ret=" + r2 + " ms=" + (Environment.TickCount - t0));
            SemaphoreSlim ss = new SemaphoreSlim(0);
            t0 = Environment.TickCount; bool r3 = ss.Wait(100);
            W("semslim-wait100 ret=" + r3 + " ms=" + (Environment.TickCount - t0));
            W("waits-done");
            ArrayList keep = new ArrayList();
            for (int i = 0; i < 20000; i++) keep.Add(new byte[64]);
            W("gc-small-ok");
            keep.Clear();
            byte[] big = new byte[1024 * 1024];
            big[1000] = 7;
            W("gc-large-ok v=" + big[1000]);
            big = null;
            object x = new object();
            WeakReference w = new WeakReference(x);
            x = null;
            GC.Collect();
            W("gc-collect-ok");
            GC.WaitForPendingFinalizers();
            W("gc-finalizers-ok");
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        log.Close();
        return 0;
    }
}
