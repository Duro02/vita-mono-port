using System;
class P14 {
    static void W(string s) { Console.WriteLine("P14:" + s); }
    static int Main() {
        double a = BitConverter.Int64BitsToDouble(0x4008000000000000L);
        double b = BitConverter.Int64BitsToDouble(0x4000000000000000L);
        double r = a % b;
        W("fmod3_2=" + r + " bits=" + BitConverter.DoubleToInt64Bits(r).ToString("X"));
        double c = BitConverter.Int64BitsToDouble(0x4014000000000000L);
        W("fmod5_2=" + (c % b));
        W("round25=" + Math.Round(a));
        W("round35=" + Math.Round(3.5));
        W("ALL-DONE");
        return 0;
    }
}
