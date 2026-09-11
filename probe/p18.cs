using System;
class P18 {
    static void W(string s) { Console.WriteLine("P18:" + s); }
    static int Main() {
        double lit = 2.5;
        W("bits=" + BitConverter.DoubleToInt64Bits(lit).ToString("X16"));
        double lit35 = 3.5;
        W("bits35=" + BitConverter.DoubleToInt64Bits(lit35).ToString("X16"));
        double lit29 = 2.9;
        W("bits29=" + BitConverter.DoubleToInt64Bits(lit29).ToString("X16"));
        W("ALL-DONE");
        return 0;
    }
}
