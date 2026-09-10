using System;
class P0 {
    static int Div(int a, int b) { return a / b; }
    static int Main() {
        Console.WriteLine("P0 start");
        try { throw new InvalidOperationException("x"); }
        catch (InvalidOperationException) { Console.WriteLine("P0 catch-ok"); }
        try { try { throw new Exception("inner"); } finally { Console.WriteLine("P0 finally-ok"); } }
        catch { Console.WriteLine("P0 unreach"); }
        int r = 0;
        try { r = Div(10, 0); } catch (DivideByZeroException) { r = -1; }
        Console.WriteLine("P0 divzero r=" + r);
        Console.WriteLine("P0 done");
        return 42;
    }
}
