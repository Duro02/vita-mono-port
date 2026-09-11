using System;
class P19 {
    static void W(string s) { Console.WriteLine("P19:" + s); }
    static int Main() {
        double a = BitConverter.Int64BitsToDouble(0x4008000000000000L);
        double b = BitConverter.Int64BitsToDouble(0x4000000000000000L);
        for (int i = 0; i < 5; i++) {
            double r = a % b;
            W("fmod[" + i + "]=" + r);
        }
        W("ALL-DONE");
        return 0;
    }
}
