using System;
class P17 {
    static void W(string s) { Console.WriteLine("P17:" + s); }
    static double N(long bits) { return BitConverter.Int64BitsToDouble(bits); }
    static int Main() {
        W("lit=" + Math.Round(2.5));
        W("bits=" + Math.Round(N(0x4004000000000000L)));
        W("lit35=" + Math.Round(3.5));
        W("var=" + Math.Round(double.Parse("2.5")));
        W("ALL-DONE");
        return 0;
    }
}
