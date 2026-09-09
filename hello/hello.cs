using System;
using System.IO;

class Hello {
    static int Main() {
        File.WriteAllText("ux0:/data/monoapp/out.txt",
            "Hello from C# on PS Vita!\nMono " + Environment.Version + "\n");
        Console.WriteLine("Hello from C# on PS Vita!");
        return 42;
    }
}
