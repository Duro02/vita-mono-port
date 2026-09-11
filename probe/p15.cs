using System;
class P15 {
    static void W(string s) { Console.WriteLine("P15:" + s); }
    static double NoFold(long bits) { return BitConverter.Int64BitsToDouble(bits); }
    static int Main() {
        double r = Math.Floor(NoFold(0x4004000000000000L));
        W("floor2_5=" + r + " bits=" + BitConverter.DoubleToInt64Bits(r).ToString("X"));
        W("floor2_9=" + Math.Floor(NoFold(0x4003333333333333L)));
        W("floor3_0=" + Math.Floor(NoFold(0x4008000000000000L)));
        W("floor_neg25=" + Math.Floor(NoFold(unchecked((long)0xC004000000000000UL))));
        W("ALL-DONE");
        return 0;
    }
}
