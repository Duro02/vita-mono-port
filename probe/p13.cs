using System;
using System.IO;
class P13 {
    static void W(string s) { Console.WriteLine("P13:" + s); }
    static int Main() {
        try {
            W("start");
            string dir = "ux0:/data/monoapp/t13/";
            Directory.CreateDirectory(dir);
            W("mkdir-ok");
            File.WriteAllText(dir + "f1.txt", "1");
            File.WriteAllText(dir + "f2.txt", "2");
            File.WriteAllText(dir + "f3.txt", "3");
            W("files-ok");
            string[] files = Directory.GetFiles(dir);
            W("count=" + files.Length);
            foreach (string f in files) W("e:" + f);
            W("ALL-DONE");
        } catch (Exception e) { W("FATAL " + e.GetType().Name + ": " + e.Message); }
        return 0;
    }
}
