using System;
class P16 {
    static void W(string s) { Console.WriteLine("P16:" + s); }
    static double N(long bits) { return BitConverter.Int64BitsToDouble(bits); }
    static int Main() {
        W("r05=" + Math.Round(N(0x3FE0000000000000L)));
        W("r15=" + Math.Round(N(0x3FF8000000000000L)));
        W("r25=" + Math.Round(N(0x4004000000000000L)));
        W("r35=" + Math.Round(N(0x400C000000000000L)));
        W("r45=" + Math.Round(N(0x4012000000000000L)));
        W("rn25=" + Math.Round(N(unchecked((long)0xC004000000000000UL))));
        W("r24=" + Math.Round(N(0x4003333333333333L)));
        W("r26=" + Math.Round(N(0x4004CCCCCCCCCCCDL)));
        W("ALL-DONE");
        return 0;
    }
}
