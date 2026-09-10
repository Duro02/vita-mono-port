using System;
using System.Threading;
class P1 {
    static volatile int tcounter;
    static void Main() {
        Console.WriteLine("P1 start");
        Thread t = new Thread(delegate() { Console.WriteLine("P1 worker ran"); });
        Console.WriteLine("P1 created");
        t.Start();
        Console.WriteLine("P1 started");
        t.Join();
        Console.WriteLine("P1 joined");
    }
}
